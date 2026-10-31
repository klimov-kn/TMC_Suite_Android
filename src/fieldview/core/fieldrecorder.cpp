/// @file fieldrecorder.cpp
/// @brief Запись серии тактов поля — описание в fieldrecorder.h.
/// @see https://www.tamic.ru/docs/platforms/android.html#field

#include "fieldrecorder.h"
#include "fieldfile.h"

#include <QDir>
#include <QFile>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace fieldview {

namespace {

bool isExFile(const QString &name)
{
    return name.endsWith(QLatin1String(".ex"), Qt::CaseInsensitive);
}

/// Прочитать файл целиком так, чтобы ядро не переписало его во время чтения.
bool readStable(const QString &path, QByteArray *out)
{
#ifdef Q_OS_WIN
    // Без FILE_SHARE_WRITE: fopen(..., "wb") ядра на это время получает отказ и
    // повторяет попытку через 10 мс (TmcRTHRectNode.cpp, PutField).
    const std::wstring w = QDir::toNativeSeparators(path).toStdWString();
    HANDLE h = CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER size;
    bool ok = GetFileSizeEx(h, &size) != 0 && size.QuadPart > 0 && size.QuadPart < (1LL << 31);
    if (ok) {
        out->resize(qsizetype(size.QuadPart));
        DWORD got = 0;
        ok = ReadFile(h, out->data(), DWORD(size.QuadPart), &got, nullptr) != 0
             && qint64(got) == size.QuadPart;
    }
    CloseHandle(h);
    return ok;
#else
    // Android, Linux, macOS: ядро (compat tmc_fopen) пишет кадр во временный файл и
    // подменяет им файл поля переименованием — открытый здесь файл
    // всегда цельный кадр, ядро при этом не ждёт.
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    *out = f.readAll();
    return !out->isEmpty();
#endif
}

/// Заголовок кадра без замка (только номер такта и размеры): ядро не задерживается.
/// Ложь — файл сейчас переписывается или ещё не создан.
bool peekHeader(const QString &path, FieldHeader *h, qint64 *frameBytes)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = f.read(4096);
    QString err;
    const qint64 dataStart = parseFieldHeader(head.constData(), head.size(), h, &err);
    if (dataStart < 0)
        return false;
    *frameBytes = dataStart + fieldDataBytes(*h);
    return true;
}

/// Кадр в одинарной точности: экран рисует поле во float, а серия тактов так вдвое
/// меньше (сетка 342×322 — 0,44 МБ на кадр вместо 0,88). Заголовок тот же, кроме
/// «#TMCGROFLD_Accuracy 4» (длина строки не меняется); значения — float(double).
QByteArray toSinglePrecision(const QByteArray &frame, qint64 dataStart, const FieldHeader &h)
{
    QByteArray out = frame.left(dataStart);
    const QByteArray from("#TMCGROFLD_Accuracy 8"), to("#TMCGROFLD_Accuracy 4");
    const qsizetype at = out.indexOf(from);
    if (at < 0)
        return frame;
    out.replace(at, from.size(), to);
    out.reserve(dataStart + qint64(h.nY) * h.nX * 4 + qint64(h.nY) * 2);
    const char *p = frame.constData() + dataStart;
    for (int i = 0; i < h.nY; ++i) {
        if (i > 0) {
            out.append("\r\n", 2);
            p += 2;
        }
        for (int j = 0; j < h.nX; ++j) {
            double v;
            std::memcpy(&v, p, 8);
            p += 8;
            const float f = float(v);
            out.append(reinterpret_cast<const char *>(&f), 4);
        }
    }
    return out;
}

} // namespace

FieldRecorder::FieldRecorder(QObject *parent) : QObject(parent) {}

FieldRecorder::~FieldRecorder()
{
    m_stop = true;
    if (m_thread.joinable())
        m_thread.join();
}

void FieldRecorder::start(const QString &workDir)
{
    stop();
    m_dir = workDir;
    m_tracks.clear();
    m_frames = 0;
    m_bytes = 0;
    m_status.clear();
    const QDir d(workDir);
    const QStringList old = d.entryList({QStringLiteral("*.frames")}, QDir::Files);
    for (const QString &n : old) {
        if (n.endsWith(QLatin1String(".ex.frames"), Qt::CaseInsensitive))
            QFile::remove(d.filePath(n));
    }
    m_stop = false;
    m_thread = std::thread([this]() {
        while (!m_stop) {
            poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(std::max(1, m_intervalMs.load())));
        }
    });
}

void FieldRecorder::stop()
{
    if (!m_thread.joinable())
        return;
    m_stop = true;
    m_thread.join();
    poll();   // финальный такт
}

void FieldRecorder::poll()
{
    if (m_dir.isEmpty())
        return;
    const QDir d(m_dir);
    const QStringList names = d.entryList(QDir::Files);
    for (const QString &n : names) {
        if (!isExFile(n))
            continue;
        if (m_bytes >= m_budget) {
            m_status = tr("Запись тактов остановлена: достигнут объём %1 МБ")
                           .arg(m_budget / (1024 * 1024));
            return;
        }
        captureOne(d.filePath(n), &m_tracks[n]);
    }
}

int FieldRecorder::stepFor(qint64 frameBytes) const
{
    int step = std::max(1, m_minStep.load());
    const int ticks = m_expectedTicks;
    if (ticks <= 0 && m_requireTicks)
        return std::numeric_limits<int>::max();
    if (ticks > 0 && frameBytes > 0) {
        const qint64 fit = std::max<qint64>(1, m_budget / frameBytes);
        const qint64 frames = std::max<qint64>(1, std::min<qint64>(m_maxFrames, fit));
        step = std::max<int>(step, int((ticks + frames - 1) / frames));
    }
    return step;
}

bool FieldRecorder::captureOne(const QString &exPath, Track *t)
{
    FieldHeader peek;
    qint64 frameBytes = 0;
    if (!peekHeader(exPath, &peek, &frameBytes))
        return false;
    // Новый шаг расчёта начинает такты заново — такой кадр берётся сразу.
    if (peek.accuracy == 8)   // в серию кадр уходит в одинарной точности
        frameBytes -= qint64(peek.nX) * peek.nY * 4;
    const bool due = t->lastNT < 0 || peek.nT < t->lastNT || peek.nT - t->lastNT >= stepFor(frameBytes);
    if (!due || peek.nT == t->lastNT)
        return false;
    QByteArray bytes;
    if (!readStable(exPath, &bytes))
        return false;
    FieldHeader h;
    QString err;
    const qint64 dataStart = parseFieldHeader(bytes.constData(), bytes.size(), &h, &err);
    if (dataStart < 0)
        return false;
    // Ядро пишет кадр без завершающего \r\n; больше — не наш формат, меньше — недописан.
    if (bytes.size() != dataStart + fieldDataBytes(h) || h.nT == t->lastNT)
        return false;
    if (h.accuracy == 8)
        bytes = toSinglePrecision(bytes, dataStart, h);
    QFile out(exPath + QStringLiteral(".frames"));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Append))
        return false;
    if (out.write(bytes) != bytes.size())
        return false;
    out.close();
    t->lastNT = h.nT;
    ++m_frames;
    m_bytes += bytes.size();
    Q_EMIT frameRecorded(exPath, h.nT);
    return true;
}

} // namespace fieldview
