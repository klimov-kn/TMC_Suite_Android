/// @file fieldexporter.h
/// @brief FieldExporter — выгрузка анимации поля: анимированный GIF (поделиться с телефона) или
///
/// кадры PNG (на ПК — для видео).
///
/// Такты перебираются по одному: FieldScene::setFrame, снимок окна (grabWindow — Qt Quick
/// рисует кадр, сцену RHI вместе с надписями и шкалой поверх), вырезается область
/// заданного элемента и уменьшается до нужной ширины. Работает по таймеру, экран живой.
/// @see https://www.tamic.ru/docs/platforms/android.html#field

#pragma once

#include "fieldscene.h"

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

namespace fieldview {

class GifWriter;

class FieldExporter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY stateChanged)

public:
    explicit FieldExporter(QObject *parent = nullptr);
    ~FieldExporter() override;

    bool running() const { return m_running; }
    double progress() const { return m_progress; }
    QString error() const { return m_error; }
    QString outputPath() const { return m_path; }

    /// Начать выгрузку. area — что снимать (сцена с надписями); path — файл .gif или
    /// папка для кадров PNG (frame_0001.png …); maxFrames — не больше стольких кадров
    /// (берётся каждый k-й такт); fps — скорость GIF; width — ширина картинки, точек.
    Q_INVOKABLE bool start(QQuickItem *area, fieldview::FieldScene *scene, const QString &path,
                           int maxFrames, int fps, int width);
    Q_INVOKABLE void cancel();

Q_SIGNALS:
    void stateChanged();
    void progressChanged();
    void finished(bool ok, const QString &path);

private:
    void step();
    void finish(bool ok, const QString &error);

    QPointer<QQuickItem> m_area;
    QPointer<FieldScene> m_scene;
    QString m_path;
    bool m_png = false;
    std::vector<int> m_frames;
    size_t m_pos = 0;
    int m_fps = 10;
    int m_width = 540;
    int m_restoreFrame = 0;
    std::unique_ptr<GifWriter> m_gif;
    bool m_running = false;
    double m_progress = 0.0;
    QString m_error;
};

} // namespace fieldview
