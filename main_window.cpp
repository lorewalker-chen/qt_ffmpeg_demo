#include "main_window.h"
#include "ui_main_window.h"

#include <QDebug>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    connect(&decoder_, &Decoder::GotImage, this, &MainWindow::GotImage);
    //直接调用为同步调用
//    decoder_.SetUrl("./1.mp4");
//    decoder_.SetOutImageSize(800, 600);

    //异步调用
    QMetaObject::invokeMethod(&decoder_, "SetUrl", Q_ARG(QString, "./1.mp4"));
    QMetaObject::invokeMethod(&decoder_, "SetOutImageSize", Q_ARG(uint, 800), Q_ARG(uint, 600));
    QMetaObject::invokeMethod(&decoder_, "Start");
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::GotImage(const QImage& image) {
    ui->label->setPixmap(QPixmap::fromImage(image));
}


void MainWindow::on_pushButton_pause_clicked() {
    QMetaObject::invokeMethod(&decoder_, "Pause");
}

void MainWindow::on_pushButton_goon_clicked() {
    QMetaObject::invokeMethod(&decoder_, "Goon");
}
