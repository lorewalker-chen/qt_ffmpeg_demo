#include "main_window.h"
#include "ui_main_window.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    decoder_.reset(new Decoder("./1.mp4", 800, 600));
    connect(decoder_.data(), &Decoder::GotImage, this, &MainWindow::GotImage);
    decoder_->OpenVideo();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::GotImage(const QImage& image) {
    ui->label->setPixmap(QPixmap::fromImage(image));
}

