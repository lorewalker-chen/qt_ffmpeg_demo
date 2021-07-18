#include "decoder.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

#include <QThread>
#include <QTimer>
#include <QImage>
#include <QDebug>

Decoder::Decoder(const QString& url, uint out_width, uint out_height, QObject* parent): QObject(parent) {
    url_ = url;
    if (isNetworkUrl(url)) {
        decode_delay_msec_ = 1;
    }

    out_width_ = out_width;
    out_height_ = out_height;

    //移动到线程
    decode_thread_ = new QThread;
    this->moveToThread(decode_thread_);
    decode_thread_->start();
}

Decoder::~Decoder() {
    decode_thread_->quit();
    decode_thread_->wait();
    decode_thread_->deleteLater();
}
//打开视频
void Decoder::OpenVideo() {
    is_stop_ = false;
    //初始化FFmpeg组件
    InitFFmpeg();
    //打开Url
    if (!OpenUrl()) {
        DeinitFFmpeg();
        return;
    }
    //寻找视频流索引
    FindVideoStreamIndex();
    //初始化解码器
    if (!InitCodecContext()) {
        DeinitFFmpeg();
        return;
    }
    //初始化图像转换器上下文
    InitSwsContext();
    if (!sws_context_) {
        DeinitSwsContext();
        return;
    }
    //初始化输出结构体
    InitOutFrame();
    //打开解码器定时器
    decode_timer_ = new QTimer;
    connect(decode_timer_, SIGNAL(timeout()), this, SLOT(DecodeOnePacket()));
    decode_timer_->start(decode_delay_msec_);
}
//关闭视频
void Decoder::CloseVideo() {
    //发送一帧空白图片
    QImage image;
    emit GotImage(image);
    decode_timer_->stop();
    decode_timer_->deleteLater();
    DeinitFFmpeg();
}
//判断一个正确的url地址是否为网络地址
bool Decoder::isNetworkUrl(const QString& url) {
    if (url == "") {
        return false;
    }
    //linux下文件路径开头为/,windows下文件路径第二个字符为:
    if (url.indexOf("/") == 0 || url.indexOf(":") == 1) {
        return false;
    }
    return true;
}
//初始化FFmpeg组件，分配空间
void Decoder::InitFFmpeg() {
    //初始化网络库
    InitNetWork();
    //初始化参数列表
    InitParameters();
    //格式上下文
    format_context_ = avformat_alloc_context();
}
//释放FFmpeg空间
void Decoder::DeinitFFmpeg() {
    DeinitOutFrame();
    DeinitSwsContext();
    DeinitCodecContext();
    CloseUrl();
    DeinitParameters();
    DeInitNetwork();
}
//初始化网络模块，以使用网络url
void Decoder::InitNetWork() {
    avformat_network_init();
}
//注销FFmpeg组件
void Decoder::DeInitNetwork() {
    avformat_network_deinit();
}
//初始化参数
void Decoder::InitParameters() {
    //超时3s
    av_dict_set(&opts_, "timeout", "3000000", 0);
}
//释放参数列表
void Decoder::DeinitParameters() {
    av_dict_free(&opts_);
}
//打开Url
bool Decoder::OpenUrl() {
    //打开Url
    if (avformat_open_input(&format_context_, url_.toStdString().c_str(), NULL, &opts_) < 0) {
        qDebug() << "Open url failed";
        return false;
    }
    //查找流媒体信息
    if (avformat_find_stream_info(format_context_, NULL) < 0) {
        qDebug() << "Find stream info failed";
        return false;
    }
    return true;
}
//关闭Url
void Decoder::CloseUrl() {
    if (format_context_ != NULL) {
        //关闭输入
        avformat_close_input(&format_context_);
        //释放格式上下文空间
        avformat_free_context(format_context_);
    }
}
//寻找视频流索引
void Decoder::FindVideoStreamIndex() {
    for (uint i = 0; i < format_context_->nb_streams; i++) {
        //查找到视频流则跳出
        if (format_context_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }
}
//初始化解码器
bool Decoder::InitCodecContext() {
    //查找视频流对应的解码器
    AVCodec* codec = avcodec_find_decoder(format_context_->streams[video_stream_index_]->codecpar->codec_id);
    if (!codec) {
        qDebug() << "Find codec failed";
        return false;
    }
    //初始化对应解码器上下文
    codec_context_ = avcodec_alloc_context3(codec);
    //初始化并获取对应的解码器参数
    AVCodecParameters* codec_param = avcodec_parameters_alloc();
    codec_param = format_context_->streams[video_stream_index_]->codecpar;
    //将解码器参数转换为上下文
    if (avcodec_parameters_to_context(codec_context_, codec_param) < 0) {
        qDebug() << "Trans to context failed";
        avcodec_parameters_free(&codec_param);
        return false;
    }
    //打开解码器上下文
    if (avcodec_open2(codec_context_, codec, NULL) < 0) {
        qDebug() << "Open codec context failed";
        if (codec != NULL) {
            delete codec;
            codec = NULL;
        }
        return false;
    }
    return true;
}
//销毁解码器
void Decoder::DeinitCodecContext() {
    if (codec_context_ != NULL) {
        //关闭解码器
        avcodec_close(codec_context_);
        //释放空间
        avcodec_free_context(&codec_context_);
    }
}
//初始化图像转换器上下文
void Decoder::InitSwsContext() {
    if (out_width_ == 0 || out_height_ == 0) {
        out_width_ = codec_context_->width;
        out_height_ = codec_context_->height;
    }
    sws_context_ = sws_getContext(codec_context_->width, codec_context_->height, codec_context_->pix_fmt,
                                  out_width_, out_height_, AV_PIX_FMT_RGB32,
                                  SWS_BICUBIC, NULL, NULL, NULL);
}
//释放图像转换器空间
void Decoder::DeinitSwsContext() {
    if (sws_context_ != NULL) {
        sws_freeContext(sws_context_);
    }
}
//初始化输出帧结构
void Decoder::InitOutFrame() {
    //包
    packet_ = av_packet_alloc();
    //帧结构
    frame_YUV_ = av_frame_alloc();
    frame_RGB32_ = av_frame_alloc();
    frame_RGB32_->format = AV_PIX_FMT_RGB32;
    frame_RGB32_->width = out_width_;
    frame_RGB32_->height = out_height_;
    av_frame_get_buffer(frame_RGB32_, 0);
//    //输出缓冲空间
//    int out_buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB32,
//                          out_width_, out_height_, 1);
//    out_buffer_ = (uint8_t*)av_malloc(out_buffer_size);
//    //填充RGB32帧结构
//    av_image_fill_arrays(frame_RGB32_->data, frame_RGB32_->linesize,
//                         out_buffer_, AV_PIX_FMT_RGB32,
//                         out_width_, out_height_, 1);
}
//释放输出结构体空间
void Decoder::DeinitOutFrame() {
    if (frame_RGB32_ != NULL) {
        av_frame_free(&frame_RGB32_);
    }
    if (out_buffer_ != NULL) {
        av_free(out_buffer_);
    }
    if (frame_YUV_ != NULL) {
        av_frame_free(&frame_YUV_);
    }
    if (packet_ != NULL) {
        av_packet_free(&packet_);
    }
}
//解码一包数据
void Decoder::DecodeOnePacket() {
    //读取一帧未解码的视频数据
    int read_result = -1;
    int read_count = 0;
    do {
        read_result = av_read_frame(format_context_, packet_);
        //读取次数+1
        read_count++;
        //如果读取连续50帧没有读到视频数据,则认为视频流结束,发送一帧空白图片,停止定时器并返回
        if (read_count > 50) {
            QImage image;
            emit GotImage(image);
            decode_timer_->stop();
            return;
        }
    } while ((read_result < 0) || (packet_->stream_index != video_stream_index_));
    //发送一帧数据到解码器
    int ret = avcodec_send_packet(codec_context_, packet_);
    if (ret < 0) {
        qDebug() << "send packet failed";
    }
    //接收解码后的数据
    if (avcodec_receive_frame(codec_context_, frame_YUV_) == 0) {
        //YUV转RGB32
        sws_scale(sws_context_,
                  frame_YUV_->data, frame_YUV_->linesize,
                  0, frame_YUV_->height,
                  frame_RGB32_->data, frame_RGB32_->linesize);
        //创建QImage
        QImage image(frame_RGB32_->data[0],
                     out_width_, out_height_, QImage::Format_RGB32);
        //发送信号
        emit GotImage(image);
    }
    //释放空间
    av_packet_unref(packet_);
}
