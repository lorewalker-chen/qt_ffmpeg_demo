#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "decoder.h"

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

  private:
    Ui::MainWindow* ui;

    Decoder decoder_;

  private slots:
    void GotImage(const QImage& image);
};
#endif // MAINWINDOW_H
