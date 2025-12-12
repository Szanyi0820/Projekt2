#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <QtConcurrent/QtConcurrent>

#include <cmath>
#include <vector>

std::vector<float> makeKernel(float sigma)
{
    int radius = std::max(1, int(std::ceil(3 * sigma)));
    int size = radius * 2 + 1;
    std::vector<float> kernel(size);
    float sum = 0.0f;

    for (int i = -radius; i <= radius; i++) {
        float v = std::exp(-(i * i) / (2 * sigma * sigma));
        kernel[i + radius] = v;
        sum += v;
    }
    for (auto &k : kernel) k /= sum;
    return kernel;
}
inline int clamp(int v, int minV, int maxV)
{
    if (v < minV) return minV;
    if (v > maxV) return maxV;
    return v;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->btnLoad, &QPushButton::clicked, this, &MainWindow::onLoadImage);
    connect(ui->btnBlur, &QPushButton::clicked, this, &MainWindow::onApplyBlur);
    connect(&watcher, &QFutureWatcher<QImage>::finished, this, &MainWindow::onBlurFinished);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onLoadImage()
{
    QString path = QFileDialog::getOpenFileName(this, "Kép megnyitása", "", "Images (*.png *.jpg *.bmp)");
    if (path.isEmpty()) return;

    inputImage.load(path);
    ui->labelInput->setPixmap(QPixmap::fromImage(inputImage));
}

void MainWindow::onApplyBlur()
{
    if (inputImage.isNull()) return;

    float sigma = ui->sigmaSlider->value() / 10.0f;
    int threads = ui->threadSpin->value();

    auto future = QtConcurrent::run([=]() {
        return gaussianBlur(inputImage, sigma, threads);
    });

    watcher.setFuture(future);
}

void MainWindow::onBlurFinished()
{
    outputImage = watcher.result();
    ui->labelOutput->setPixmap(QPixmap::fromImage(outputImage));
}

QImage MainWindow::gaussianBlur(const QImage& img, float sigma, int threads)
{
    QImage src = img.convertToFormat(QImage::Format_RGBA8888);
    int w = src.width();
    int h = src.height();

    const uchar* pixels = src.constBits();

    std::vector<float> temp(w * h * 4);
    std::vector<uchar> out(w * h * 4);

    auto kernel = makeKernel(sigma);
    int radius = kernel.size() / 2;

    threadTimes.resize(threads);

    auto horizontal = [&](int startY, int endY) {
        for (int y = startY; y < endY; ++y) {
            for (int x = 0; x < w; ++x) {
                for (int c = 0; c < 4; ++c) {
                    float acc = 0.0f;
                    for (int k = -radius; k <= radius; ++k) {
                        int sx = clamp(x + k, 0, w - 1);
                        int idx = (y * w + sx) * 4 + c;
                        acc += kernel[k + radius] * pixels[idx];
                    }
                    temp[(y * w + x) * 4 + c] = acc;
                }
            }
        }
    };

    auto vertical = [&](int startY, int endY) {
        for (int y = startY; y < endY; ++y) {
            for (int x = 0; x < w; ++x) {
                for (int c = 0; c < 4; ++c) {
                    float acc = 0.0f;
                    for (int k = -radius; k <= radius; ++k) {
                        int sy = clamp(y + k, 0, h - 1);
                        acc += kernel[k + radius] * temp[(sy * w + x) * 4 + c];
                    }
                    out[(y * w + x) * 4 + c] = uchar(clamp(int(acc), 0, 255));
                }
            }
        }
    };

    QList<QFuture<void>> tasks;
    int rowsPerThread = h / threads;

    for (int t = 0; t < threads; t++) {
        int y1 = t * rowsPerThread;
        int y2 = (t == threads - 1) ? h : y1 + rowsPerThread;

        tasks << QtConcurrent::run([&, t, y1, y2]() {

            auto start = std::chrono::high_resolution_clock::now();

            horizontal(y1, y2);

            auto end = std::chrono::high_resolution_clock::now();
            threadTimes[t] = std::chrono::duration<double, std::milli>(end - start).count();
        });
    }
    for (auto &f : tasks) f.waitForFinished();
    tasks.clear();

    for (int t = 0; t < threads; t++) {
        int y1 = t * rowsPerThread;
        int y2 = (t == threads - 1) ? h : y1 + rowsPerThread;

        tasks << QtConcurrent::run([&, t, y1, y2]() {

            auto start = std::chrono::high_resolution_clock::now();

            vertical(y1, y2);

            auto end = std::chrono::high_resolution_clock::now();
            threadTimes[t] += std::chrono::duration<double, std::milli>(end - start).count();
        });
    }
    for (auto &f : tasks) f.waitForFinished();

    for (int t = 0; t < threads; t++) {
        qDebug() << "Thread" << t << "time:" << threadTimes[t] << "ms";
    }

    QImage result(out.data(), w, h, QImage::Format_RGBA8888);
    return result.copy();
}


