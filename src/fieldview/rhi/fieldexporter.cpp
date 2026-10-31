// FieldExporter — описание в fieldexporter.h.
#include "fieldexporter.h"

#include "fieldscene.h"
#include "gifwriter.h"

#include <QDir>
#include <QFileInfo>
#include <QQuickWindow>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace fieldview {

FieldExporter::FieldExporter(QObject *parent) : QObject(parent) {}

FieldExporter::~FieldExporter() = default;

bool FieldExporter::start(QQuickItem *area, FieldScene *scene, const QString &path, int maxFrames,
                          int fps, int width)
{
    if (m_running || !area || !scene || !area->window() || scene->frameCount() < 1 || path.isEmpty())
        return false;
    m_area = area;
    m_scene = scene;
    m_path = path;
    m_png = !path.endsWith(QLatin1String(".gif"), Qt::CaseInsensitive);
    m_fps = std::clamp(fps, 1, 50);
    // Не шире, чем элемент на экране в точках: растянутый кадр только размоется.
    const qreal dpr = area->window()->effectiveDevicePixelRatio();
    m_width = std::max(64, std::min(width, int(std::lround(area->width() * dpr))) & ~1);
    m_restoreFrame = scene->frame();
    // Каждый k-й такт, чтобы уложиться в maxFrames; последний — всегда.
    const int n = scene->frameCount();
    const int stride = std::max(1, int(std::ceil(double(n) / std::max(1, maxFrames))));
    m_frames.clear();
    for (int i = 0; i < n; i += stride)
        m_frames.push_back(i);
    if (m_frames.back() != n - 1)
        m_frames.push_back(n - 1);
    m_pos = 0;
    m_gif.reset();
    m_error.clear();
    if (m_png) {
        QDir().mkpath(path);
    } else {
        QDir().mkpath(QFileInfo(path).absolutePath());
        m_gif = std::make_unique<GifWriter>();
        const QSizeF s = area->size();
        const int h = int(std::lround(m_width * s.height() / std::max(1.0, s.width()))) & ~1;
        if (!m_gif->open(path, m_width, h)) {
            m_error = m_gif->error();
            m_gif.reset();
            return false;
        }
    }
    m_running = true;
    m_progress = 0.0;
    Q_EMIT stateChanged();
    Q_EMIT progressChanged();
    QTimer::singleShot(0, this, &FieldExporter::step);
    return true;
}

void FieldExporter::cancel()
{
    if (m_running)
        finish(false, tr("Отменено"));
}

void FieldExporter::step()
{
    if (!m_running)
        return;
    if (!m_area || !m_scene || !m_area->window()) {
        finish(false, tr("Экран поля закрыт"));
        return;
    }
    if (m_pos >= m_frames.size()) {
        finish(true, QString());
        return;
    }
    m_scene->setFrame(m_frames[m_pos]);
    // Снимок окна рисует кадр заново (сцена RHI и надписи поверх) — уже с новым тактом.
    QQuickWindow *w = m_area->window();
    const QImage shot = w->grabWindow();
    const qreal dpr = shot.devicePixelRatio() > 0 ? shot.devicePixelRatio() : 1.0;
    const QRectF r = m_area->mapRectToScene(QRectF(0, 0, m_area->width(), m_area->height()));
    const QRect px = QRectF(r.x() * dpr, r.y() * dpr, r.width() * dpr, r.height() * dpr).toAlignedRect()
                         .intersected(QRect(QPoint(0, 0), shot.size()));
    QImage frame = shot.copy(px);
    frame.setDevicePixelRatio(1.0);
    if (frame.width() > m_width)
        frame = frame.scaledToWidth(m_width, Qt::SmoothTransformation);
    bool ok = true;
    if (m_png) {
        // Папка «…_jpg» — кадры JPEG (в 5–10 раз меньше PNG; для видео этого хватает).
        const bool jpg = m_path.endsWith(QLatin1String("_jpg"));
        ok = frame.save(QDir(m_path).filePath(QStringLiteral("frame_%1.%2").arg(m_pos + 1, 4, 10, QLatin1Char('0'))
                                                  .arg(jpg ? QStringLiteral("jpg") : QStringLiteral("png"))),
                        nullptr, jpg ? 92 : -1);
    }
    else
        ok = m_gif->addFrame(frame, std::max(2, int(std::lround(100.0 / m_fps))));
    if (!ok) {
        finish(false, m_gif ? m_gif->error() : tr("Не записать кадр"));
        return;
    }
    ++m_pos;
    m_progress = double(m_pos) / double(m_frames.size());
    Q_EMIT progressChanged();
    QTimer::singleShot(0, this, &FieldExporter::step);
}

void FieldExporter::finish(bool ok, const QString &error)
{
    if (m_gif) {
        if (!m_gif->close() && ok) {
            ok = false;
            m_error = m_gif->error();
        }
        m_gif.reset();
    }
    if (!ok && error.size())
        m_error = error;
    if (!ok && !m_png)
        QFile::remove(m_path);
    if (m_scene)
        m_scene->setFrame(m_restoreFrame);
    m_running = false;
    Q_EMIT stateChanged();
    Q_EMIT finished(ok, m_path);
}

} // namespace fieldview
