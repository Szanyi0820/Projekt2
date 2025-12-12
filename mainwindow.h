#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#pragma once
#include <QImage>
#include <QFutureWatcher>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onLoadImage();
    void onApplyBlur();
    void onBlurFinished();

private:
    Ui::MainWindow *ui;
    QImage inputImage;
    QImage outputImage;
    std::vector<double> threadTimes;
    QFutureWatcher<QImage> watcher;

    QImage gaussianBlur(const QImage& img, float sigma, int threads);
};
#endif // MAINWINDOW_H
