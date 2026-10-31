// GifWriter — анимированный GIF из кадров QImage (экспорт анимации поля, «Поделиться»).
//
// Свой кодировщик (GIF89a, LZW), без сторонних библиотек: в Qt записи GIF нет. У каждого
// кадра своя палитра — 256 самых частых цветов кадра (цвета сводятся к 5-6-5 бит, затем
// каждому цвету кадра — ближайший из палитры). Шкалы поля плавные, поэтому этого
// хватает; переходы без сглаживания (dither) — анимация не «шумит».
#pragma once

#include <QFile>
#include <QImage>
#include <QString>

namespace fieldview {

class GifWriter
{
public:
    /// Открыть файл; width×height — размер всех кадров (другие масштабируются).
    bool open(const QString &path, int width, int height);
    /// Добавить кадр; delayCs — показ кадра, сотые секунды.
    bool addFrame(const QImage &image, int delayCs);
    /// Дописать конец файла. false — ошибка записи.
    bool close();
    QString error() const { return m_error; }

private:
    void writeLzw(const QByteArray &indices);

    QFile m_file;
    int m_w = 0, m_h = 0;
    QString m_error;
};

} // namespace fieldview
