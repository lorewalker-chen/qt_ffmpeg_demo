#include "decoder.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

#include <QUrl>
#include <QThread>
#include <QTimer>
#include <QImage>
#include <QDebug>

Decoder::Decoder(QObject* parent): QObject(parent) {
    //移动到线程
    decode_thread_ = new QThread;
    this->moveToThread(decode_thread_);
    decode_thread_->start();
}

Decoder::~Decoder() {
    if (decode_timer_) {
        decode_timer_->deleteLater();
    }
    DeinitFFmpeg();
    decode_thread_->quit();
    decode_thread_->wait();
    decode_thread_->deleteLater();
}
//设置Url
void Decoder::SetUrl(const QString& url) {
    //如果正在播放,不进行设置
    if (!is_stop_) {
        return;
    }
    QUrl temp(url);
    //判断url是否有效,有效则设置
    if (temp.isValid()) {
        url_ = url;
        //判断url是否为本地文件
        is_local_ = isLocalFile(url);
    }
}
//设置输出图片尺寸
void Decoder::SetOutImageSize(uint width, uint height) {
    out_image_width_ = width;
    out_image_height_ = height;
}
//开始解码
void Decoder::Start() {
    //判断url是否为空
    if ("" == url_) {
        qDebug() << "url is invalid";
        return;
    }
    //初始化FFmpeg解码库
    if (!InitFFmpeg()) {
        qDebug() << "init ffmpeg failed";
        return;
    }
    //初始化解码定时器
    decode_timer_ = new QTimer;
    connect(decode_timer_, SIGNAL(timeout()), this, SLOT(DecodeOnePacket()));
    //如果是本地视频,25ms解一帧,如果是网络流,不限定时间
    int delay = 0;
    if (is_local_) {
        delay = 25;
    }
    //将停止标志置为false
    is_stop_ = false;
    //打开解码定时器
    decode_timer_->start(delay);
}
//暂停
void Decoder::Pause() {
    is_pausing_ = true;
}
//继续
void Decoder::Goon() {
    is_pausing_ = false;
}
//关闭视频
void Decoder::Stop() {
    //发送一帧空白图片
    QImage image;
    emit GotImage(image);
    //停止定时器
    if (decode_timer_) {
        decode_timer_->stop();
        decode_timer_->deleteLater();
    }
    is_stop_ = true;
    //释放空间
    DeinitFFmpeg();
}
//判断一个url是否为本地文件
bool Decoder::IsLocalFile(const QString& url) {
    QUrl temp(url);
    temp.isValid();
    return temp.isLocalFile();
}
//判断一个正确的url地址是否为网络地址
bool Decoder::isLocalFile(const QString& url) {
    //linux下文件路径开头为/,windows下文件路径第二个字符为:
    if (url.indexOf("/") == 0 || url.indexOf(":") == 1) {
        return true;
    }
    return false;
}
//初始化FFmpeg组件，分配空间
bool Decoder::InitFFmpeg() {
    //申请空间
    format_context_ = avformat_alloc_context();

    //不是本地文件需要初始化网络库
    if (!is_local_) {
        avformat_network_init();
    }

    //接口返回值
    int result = 0;

    //打开url
#if 0
    //参数设置
    AVDictionary* opts = nullptr;
    //设置rtsp传输方式
//    av_dict_set(&opts, "rtsp_transport", "tcp");
//    av_dict_set(&opts, "rtsp_transport", "udp");
    //设置rtsp超时5秒
//    av_dict_set(&opts, "stimeout", "5000000", 0);
    //设置udp,http超时5秒
    av_dict_set(&opts, "timeout", "5000000", 0);
    //打开url
    result = avformat_open_input(&format_context_, url_.toStdString().c_str(), nullptr, &opts);
    av_dict_free(&opts);
#else
    //不设置参数
    result = avformat_open_input(&format_context_, url_.toStdString().c_str(), nullptr, nullptr);
#endif
    if (result < 0) {
        qDebug() << "Open url failed";
        return false;
    }

    //查找流媒体信息
    result = avformat_find_stream_info(format_context_, nullptr);
    if (result < 0) {
        qDebug() << "Find stream info failed";
        return false;
    }

    //寻找视频流索引
    for (uint i = 0; i < format_context_->nb_streams; i++) {
        //查找到视频流则跳出
        if (format_context_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }

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
    result = avcodec_parameters_to_context(codec_context_, codec_param);
    if (result < 0) {
        qDebug() << "Trans to context failed";
        avcodec_parameters_free(&codec_param);
        return false;
    }
    //打开解码器上下文
    result = avcodec_open2(codec_context_, codec, nullptr);
    if (result < 0) {
        qDebug() << "Open codec context failed";
        if (!codec) {
            delete codec;
            codec = nullptr;
        }
        return false;
    }
    //包
    packet_ = av_packet_alloc();
    //帧结构
    frame_ = av_frame_alloc();
    //输出帧采用RGB32格式
    out_frame_ = av_frame_alloc();
    out_frame_->format = AV_PIX_FMT_RGB32;
    out_frame_->width = out_image_width_;
    out_frame_->height = out_image_height_;
    av_frame_get_buffer(out_frame_, 0);
    //图像格式转换器上下文
    if (out_image_width_ == 0 || out_image_height_ == 0) {
        out_image_width_ = codec_context_->width;
        out_image_height_ = codec_context_->height;
    }
    sws_context_ = sws_getContext(codec_context_->width, codec_context_->height, codec_context_->pix_fmt,
                                  out_image_width_, out_image_height_, AV_PIX_FMT_RGB32,
                                  SWS_BICUBIC, NULL, NULL, NULL);
    return true;
}
//释放FFmpeg空间
void Decoder::DeinitFFmpeg() {
    if (!out_frame_) {
        av_frame_free(&out_frame_);
    }
    if (!frame_) {
        av_frame_free(&frame_);
    }
    if (!packet_) {
        av_packet_free(&packet_);
    }
    //图像格式转换器
    if (!sws_context_) {
        sws_freeContext(sws_context_);
    }
    //解码器
    if (!codec_context_) {
        avcodec_close(codec_context_);
        avcodec_free_context(&codec_context_);
    }
    //格式
    if (!format_context_) {
        avformat_close_input(&format_context_);
        avformat_free_context(format_context_);
    }
    //如果不是本地文件,关闭网络库
    if (!is_local_) {
        avformat_network_deinit();
    }
}
//解码一包数据
void Decoder::DecodeOnePacket() {
    //如果是本地流,在此处暂停,以实现从暂停处继续
    if (is_local_ && is_pausing_) {
        return;
    }
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
    if (avcodec_receive_frame(codec_context_, frame_) == 0) {
        //如果是网络流,在此处暂停,以实现实时解码
        if (!is_local_ && is_pausing_) {
            //释放空间
            av_packet_unref(packet_);
            return;
        }
        //YUV转RGB32
        sws_scale(sws_context_,
                  frame_->data, frame_->linesize,
                  0, frame_->height,
                  out_frame_->data, out_frame_->linesize);
        //创建QImage
        QImage image(out_frame_->data[0],
                     out_image_width_, out_image_height_, QImage::Format_RGB32);
        //发送信号
        emit GotImage(image);
    }
    //释放空间
    av_packet_unref(packet_);
}
