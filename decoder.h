#ifndef DECODER_H
#define DECODER_H

#include <QObject>

class AVDictionary;
class AVFormatContext;
class AVCodecContext;
class SwsContext;
class AVPacket;
class AVFrame;

class QTimer;
class QThread;

class Decoder : public QObject {
    Q_OBJECT
  public:
    explicit Decoder(QObject* parent = nullptr);
    ~Decoder();

  public slots:
    //参数设置
    void SetUrl(const QString& url);
    void SetOutImageSize(uint width, uint height);
    //解码控制
    void Start();
//    void Pause();
//    void Goon();
//    void Stop();


    void OpenVideo();
    void CloseVideo();

  private slots:
    bool IsLocalFile(const QString& url);
    bool isNetworkUrl(const QString& url);

    bool InitFFmpeg();
    void DeinitFFmpeg();

    void InitSwsContext();
    void DeinitSwsContext();

    void InitOutFrame();
    void DeinitOutFrame();

    void DecodeOnePacket();

  private:
    //线程
    QThread* decode_thread_ = nullptr;
    //播放状态
    bool is_stop_ = true;
    bool is_pausing_ = false;
    //设置
    QString url_ = "";
    bool is_local_ = false;
    uint out_image_width_;
    uint out_image_height_;
    //解码定时器
    uint decode_delay_msec_ = 30;
    QTimer* decode_timer_ = nullptr;
    //FFmpeg相关结构体
    AVFormatContext* format_context_ = NULL;
    int video_stream_index_;
    AVCodecContext* codec_context_ = NULL;
    SwsContext* sws_context_ = NULL;
    AVPacket* packet_ = NULL;
    AVFrame* frame_YUV_ = NULL;
    AVFrame* frame_RGB32_ = NULL;
    uint8_t* out_buffer_ = NULL;

  signals:
    void GotImage(const QImage& image);
};

#endif // DECODER_H
