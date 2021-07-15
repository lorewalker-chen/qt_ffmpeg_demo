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
    explicit Decoder(const QString& url, uint out_width = 0, uint out_height = 0, QObject* parent = nullptr);
    ~Decoder();

  public slots:
    void OpenVideo();
    void CloseVideo();

  private slots:
    bool isNetworkUrl(const QString& url);

    void InitFFmpeg();
    void DeinitFFmpeg();

    void InitNetWork();
    void DeInitNetwork();

    void InitParameters();
    void DeinitParameters();

    bool OpenUrl();
    void CloseUrl();

    void FindVideoStreamIndex();

    bool InitCodecContext();
    void DeinitCodecContext();

    void InitSwsContext();
    void DeinitSwsContext();

    void InitOutFrame();
    void DeinitOutFrame();

    void DecodeOnePacket();

  private:
    QString url_ = "";

    uint out_width_;
    uint out_height_;

    AVDictionary* opts_ = NULL;
    AVFormatContext* format_context_ = NULL;
    int video_stream_index_;
    AVCodecContext* codec_context_ = NULL;
    SwsContext* sws_context_ = NULL;
    AVPacket* packet_ = NULL;
    AVFrame* frame_YUV_ = NULL;
    AVFrame* frame_RGB32_ = NULL;
    uint8_t* out_buffer_ = NULL;

    uint decode_delay_msec_ = 30;
    QTimer* decode_timer_ = nullptr;

    QThread* decode_thread_ = nullptr;

    bool is_stop_ = false;

  signals:
    void GotImage(const QImage& image);
};

#endif // DECODER_H
