#include "main_window.h"
#include "ui_main_window.h"

#include <QThread>
#include <QDebug>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    qDebug() << "main:" << QThread::currentThreadId();
    connect(&decoder_, &Decoder::GotImage, this, &MainWindow::GotImage);
    //直接调用为同步调用
//    decoder_.SetUrl("./1.mp4");
//    decoder_.SetOutImageSize(800, 600);

    //异步调用
    QMetaObject::invokeMethod(&decoder_, "SetUrl", Q_ARG(QString, "./1.mp4"));
    QMetaObject::invokeMethod(&decoder_, "SetOutImageSize", Q_ARG(uint, 800), Q_ARG(uint, 600));
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::GotImage(const QImage& image) {
    ui->label->setPixmap(QPixmap::fromImage(image));
}

