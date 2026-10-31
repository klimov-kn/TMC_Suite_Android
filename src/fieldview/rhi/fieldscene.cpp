/// @file fieldscene.cpp
/// @brief FieldScene — описание в fieldscene.h.
/// @see https://www.tamic.ru/docs/platforms/android.html#field

#include "fieldscene.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace fieldview {

namespace {

/// Файлы поля в папке результатов: .ex (FIELDS), .AMP/.FAZ (FIELD_DISTRIBUTION) и
/// записанные серии .ex.frames. Регистр расширений — любой (Android).
bool isFieldName(const QString &n)
{
    return n.endsWith(QLatin1String(".ex"), Qt::CaseInsensitive)
           || n.endsWith(QLatin1String(".ex.frames"), Qt::CaseInsensitive)
           || n.endsWith(QLatin1String(".amp"), Qt::CaseInsensitive)
           || n.endsWith(QLatin1String(".faz"), Qt::CaseInsensitive);
}

/// SetTplFileName: путь без последнего расширения + ".tt" (у серии — без ".frames").
QString topologyPathFor(const QString &fieldPath)
{
    QString p = fieldPath;
    if (p.endsWith(QLatin1String(".frames"), Qt::CaseInsensitive))
        p.chop(7);
    const int dot = p.lastIndexOf(QLatin1Char('.'));
    const int slash = std::max(p.lastIndexOf(QLatin1Char('/')), p.lastIndexOf(QLatin1Char('\\')));
    if (dot <= slash)
        return QString();
    const QString base = p.left(dot);
    for (const char *ext : {".tt", ".TT", ".Tt", ".tT"}) {
        const QString t = base + QLatin1String(ext);
        if (QFileInfo::exists(t))
            return t;
    }
    return QString();
}

} // namespace

FieldScene::FieldScene(QObject *parent) : QObject(parent)
{
    m_watchTimer.setInterval(250);
    connect(&m_watchTimer, &QTimer::timeout, this, &FieldScene::pollWatch);
}

FieldScene::~FieldScene()
{
    stopIndexing();
}

void FieldScene::setFolder(const QString &f)
{
    QString path = f;
    if (path.startsWith(QLatin1String("file:")))
        path = QUrl(path).toLocalFile();
    if (path == m_folder)
        return;
    m_folder = path;
    Q_EMIT folderChanged();
    scanFolder();
}

void FieldScene::setFile(const QString &f)
{
    QString path = f;
    if (path.startsWith(QLatin1String("file:")))
        path = QUrl(path).toLocalFile();
    if (path == m_file)
        return;
    m_file = path;
    Q_EMIT fileChanged();
    if (!m_file.isEmpty())
        load(m_file);
}

void FieldScene::scanFolder()
{
    m_sourcePaths.clear();
    m_sourceNames.clear();
    const QDir d(m_folder);
    if (!m_folder.isEmpty() && d.exists()) {
        QStringList names = d.entryList(QDir::Files, QDir::Name | QDir::IgnoreCase);
        // Серия тактов заменяет одиночный .ex того же имени.
        QStringList frames, singles, others;
        for (const QString &n : names) {
            if (!isFieldName(n))
                continue;
            if (n.endsWith(QLatin1String(".frames"), Qt::CaseInsensitive))
                frames << n;
            else if (n.endsWith(QLatin1String(".ex"), Qt::CaseInsensitive))
                singles << n;
            else
                others << n;
        }
        for (const QString &n : frames) {
            m_sourcePaths << d.filePath(n);
            m_sourceNames << n.left(n.size() - 7);
        }
        for (const QString &n : singles) {
            if (m_sourceNames.contains(n, Qt::CaseInsensitive))
                continue;
            m_sourcePaths << d.filePath(n);
            m_sourceNames << n;
        }
        for (const QString &n : others) {
            m_sourcePaths << d.filePath(n);
            m_sourceNames << n;
        }
    }
    Q_EMIT sourcesChanged();
    m_sourceIndex = -1;
    if (m_file.isEmpty())
        setSourceIndex(m_sourcePaths.isEmpty() ? -1 : 0);
}

void FieldScene::setSourceIndex(int i)
{
    if (i == m_sourceIndex && i >= 0)
        return;
    m_sourceIndex = i;
    Q_EMIT sourceIndexChanged();
    if (i >= 0 && i < m_sourcePaths.size()) {
        load(m_sourcePaths.at(i));
    } else {
        stopIndexing();
        m_series.close();
        m_error = m_folder.isEmpty() ? QString()
                                     : tr("В папке нет файлов поля (.ex). Включите FIELDS и TOPOLOGY в задании.");
        m_current.reset();
        m_render = RenderData();
        Q_EMIT loadedChanged();
        Q_EMIT frameChanged();
        Q_EMIT topologyChanged();
        Q_EMIT renderDataChanged();
    }
}

void FieldScene::reload()
{
    const QString p = m_series.path();
    if (!p.isEmpty())
        load(p);
    else if (!m_folder.isEmpty())
        scanFolder();
}

void FieldScene::load(const QString &path)
{
    stopIndexing();
    m_cache.clear();
    m_lru.clear();
    m_cacheBytes = 0;
    m_current.reset();
    m_error.clear();
    QString err;
    if (!m_series.open(path, &err)) {
        m_error = QStringLiteral("Error : %1").arg(err);   // как OnDraw оригинала
        m_series.close();
    }
    m_stats.assign(size_t(m_series.count()), FieldStats());
    m_statsKnown.assign(size_t(m_series.count()), 0);
    m_frame = 0;
    m_layerValues.reset();
    m_layerValuesLayer = -1;
    // Топология необязательна и должна совпадать с сеткой поля (В-32, В-33).
    m_hasTopology = false;
    m_topology = TopologyFile();
    m_bounds.clear();
    const QString tt = m_series.count() > 0 ? topologyPathFor(path) : QString();
    if (!tt.isEmpty()) {
        QString terr;
        TopologyFile t;
        if (t.read(tt, &terr) && t.nX() == m_series.nX() && t.nY() == m_series.nY()) {
            m_topology = std::move(t);
            m_hasTopology = true;
            m_bounds = buildBoundaries(m_topology.blocks(), m_series.entry(0).header.delta, true);
        }
    }
    if (m_layer != 0 && !(m_hasTopology && (m_layer == 1 || m_topology.hasPlasma()))) {
        m_layer = 0;
        Q_EMIT layerChanged();
    }
    if (m_watch) {
        const QFileInfo fi(path);
        m_watchStamp = fi.lastModified();
        m_watchSize = fi.size();
    }
    Q_EMIT loadedChanged();
    updateTopology();
    updateValues();
    startIndexing();
}

QString FieldScene::fileName() const
{
    return QFileInfo(m_series.path()).fileName();
}

std::shared_ptr<const FieldFrame> FieldScene::frameData(int i)
{
    const auto it = m_cache.find(i);
    if (it != m_cache.end()) {
        m_lru.remove(i);
        m_lru.push_front(i);
        return it->second;
    }
    auto f = std::make_shared<FieldFrame>();
    QString err;
    if (!m_series.readFrame(i, f.get(), &err)) {
        m_error = QStringLiteral("Error : %1").arg(err);
        return nullptr;
    }
    m_stats[size_t(i)] = f->stats;
    m_statsKnown[size_t(i)] = 1;
    const qint64 bytes = qint64(f->values.size() * sizeof(float));
    m_cache.emplace(i, f);
    m_lru.push_front(i);
    m_cacheBytes += bytes;
    while (m_cacheBytes > m_cacheBudget && m_lru.size() > 1) {
        const int old = m_lru.back();
        m_lru.pop_back();
        const auto o = m_cache.find(old);
        if (o != m_cache.end()) {
            m_cacheBytes -= qint64(o->second->values.size() * sizeof(float));
            m_cache.erase(o);
        }
    }
    return f;
}

void FieldScene::setFrame(int f)
{
    if (m_series.count() == 0)
        return;
    f = std::clamp(f, 0, m_series.count() - 1);
    if (f == m_frame && m_current)
        return;
    m_frame = f;
    updateValues();
}

void FieldScene::stepFrame(int delta)
{
    const int n = m_series.count();
    if (n == 0)
        return;
    setFrame(((m_frame + delta) % n + n) % n);
}

int FieldScene::nT() const
{
    return m_series.count() > 0 ? m_series.entry(m_frame).header.nT : 0;
}

QString FieldScene::timeText() const
{
    if (m_series.count() == 0)
        return QString();
    const FieldHeader &h = m_series.entry(m_frame).header;
    // Время в единицах задания (TIME_UNIT): t / rTimeUnit, как в строке состояния Windows.
    const double unit = h.timeUnitValue > 0 ? h.timeUnitValue : 1.0;
    return QStringLiteral("%1 %2").arg(QString::number(h.time / unit, 'g', 6), h.timeUnit);
}

QString FieldScene::gridText() const
{
    if (m_series.count() == 0)
        return QString();
    const FieldHeader &h = m_series.entry(0).header;
    const double unit = h.longUnitValue > 0 ? h.longUnitValue : 1.0;
    return QStringLiteral("%1×%2 · Δ = %3 %4")
        .arg(h.nX)
        .arg(h.nY)
        .arg(QString::number(h.delta / unit, 'g', 6), h.longUnit);
}

void FieldScene::setLayer(int l)
{
    l = std::clamp(l, 0, 4);
    if (l != 0 && !m_hasTopology)
        l = 0;
    if (l >= 2 && !m_topology.hasPlasma())
        l = 0;
    if (l == m_layer)
        return;
    m_layer = l;
    Q_EMIT layerChanged();
    updateValues();
}

void FieldScene::setModulus(bool on)
{
    if (on == m_modulus)
        return;
    m_modulus = on;
    updateRange();
}

void FieldScene::setGlobalScale(bool on)
{
    if (on == m_globalScale)
        return;
    m_globalScale = on;
    updateRange();
}

void FieldScene::setTopologyVisible(bool on)
{
    if (on == m_topologyVisible)
        return;
    m_topologyVisible = on;
    updateTopology();
}

void FieldScene::setWatch(bool on)
{
    if (on == m_watch)
        return;
    m_watch = on;
    Q_EMIT watchChanged();
    if (on) {
        const QFileInfo fi(m_series.path());
        m_watchStamp = fi.lastModified();
        m_watchSize = fi.size();
        m_watchTimer.start();
    } else {
        m_watchTimer.stop();
    }
}

void FieldScene::pollWatch()
{
    // Как OnTimer Windows-FieldView: файл изменился — перечитать (живой расчёт).
    const QString p = m_series.path().isEmpty() && m_sourceIndex >= 0 && m_sourceIndex < m_sourcePaths.size()
                          ? m_sourcePaths.at(m_sourceIndex)
                          : m_series.path();
    if (p.isEmpty()) {
        if (!m_folder.isEmpty())
            scanFolder();
        return;
    }
    const QFileInfo fi(p);
    if (fi.lastModified() != m_watchStamp || fi.size() != m_watchSize) {
        const int keep = m_frame;
        const bool atEnd = m_frame >= m_series.count() - 1;
        load(p);
        setFrame(atEnd ? m_series.count() - 1 : keep);
    }
}

void FieldScene::updateValues()
{
    if (m_series.count() == 0)
        return;
    if (m_layer == 0) {
        m_current = frameData(m_frame);
        if (m_current) {
            // Держим данные кадра живыми, пока ими пользуется отрисовка (алиасинг shared_ptr).
            m_render.values = std::shared_ptr<const std::vector<float>>(m_current, &m_current->values);
        } else {
            m_render.values.reset();
        }
    } else {
        if (!m_layerValues || m_layerValuesLayer != m_layer) {
            const std::vector<double> &g = m_topology.grid(m_layer - 1);
            auto v = std::make_shared<std::vector<float>>(g.size());
            for (size_t i = 0; i < g.size(); ++i)
                (*v)[i] = float(g[i]);
            m_layerValues = v;
            m_layerValuesLayer = m_layer;
        }
        m_render.values = m_layerValues;
    }
    m_render.nX = m_series.nX();
    m_render.nY = m_series.nY();
    // XtoGLWinCoord / YtoGLWinCoord при bProportionally = TRUE (по умолчанию).
    const double lx = double(m_render.nX - 1), ly = double(m_render.nY - 1);
    m_render.sizeX = 2.5 * (lx < ly ? lx / ly : 1.0);
    m_render.sizeY = 2.5 * (lx < ly ? 1.0 : ly / lx);
    ++m_render.valuesVersion;
    updateRange();
    Q_EMIT frameChanged();
}

void FieldScene::setColorGain(double g)
{
    g = std::clamp(g, 0.1, 50.0);
    if (g == m_colorGain)
        return;
    m_colorGain = g;
    updateRange();
}

void FieldScene::updateRange()
{
    double zmin = 0.0, zmax = 0.0;
    Scale scale = Scale::Wave;
    int colorMode = 0;
    if (m_layer == 0) {
        FieldStats s;
        if (m_globalScale) {
            bool any = false;
            for (size_t i = 0; i < m_stats.size(); ++i) {
                if (!m_statsKnown[i])
                    continue;
                const FieldStats &f = m_stats[i];
                if (!any) {
                    s = f;
                    any = true;
                } else {
                    s.min = std::min(s.min, f.min);
                    s.max = std::max(s.max, f.max);
                    s.maxAbs = std::max(s.maxAbs, f.maxAbs);
                }
            }
        } else if (m_current) {
            s = m_current->stats;
        }
        // «Волна»: симметрично относительно нуля. «Модуль» (bDrawFieldModulFlag):
        // dZmax = max|v|, dZmin = −dZmax, цвет по |h| — шкала «Свечение».
        double a = s.maxAbs / m_colorGain;
        if (a < double(FLT_MIN))
            a = double(FLT_MIN);   // С-56: постоянное поле
        zmin = -a;
        zmax = a;
        if (m_modulus) {
            scale = Scale::Glow;
            colorMode = 1;
            m_scaleMin = 0.0;
            m_scaleMax = a;
        } else {
            m_scaleMin = -a;
            m_scaleMax = a;
        }
    } else {
        // PrepareTpl: пределы — минимум и максимум сетки, цвет 10 + z·10 из 20 →
        // (h + 1)/2; шкала «Свечение» (неотрицательные ε, N, Y, B).
        const std::vector<double> &g = m_topology.grid(m_layer - 1);
        if (!g.empty()) {
            zmin = zmax = g.front();
            for (double v : g) {
                zmin = std::min(zmin, v);
                zmax = std::max(zmax, v);
            }
        }
        m_scaleMin = zmin;
        m_scaleMax = zmax;
        if (std::fabs(zmax - zmin) < double(FLT_MIN)) {   // как в PrepareTpl
            const double r = zmax;
            zmax = r + double(FLT_MIN);
            zmin = r - double(FLT_MIN);
            if (zmax == zmin) {
                zmax = r + std::max(1.0, std::fabs(r)) * 1e-6;
                zmin = r - std::max(1.0, std::fabs(r)) * 1e-6;
            }
        }
        scale = Scale::Glow;
        colorMode = 0;
    }
    // ZtoGLWinCoord: h = 2((v − zmin)/(zmax − zmin) − 0,5) = v·k + off.
    m_render.k = 2.0 / (zmax - zmin);
    m_render.off = -2.0 * zmin / (zmax - zmin) - 1.0;
    m_render.colorMode = colorMode;
    if (scale != m_render.scale) {
        m_render.scale = scale;
        ++m_render.scaleVersion;
    }
    Q_EMIT scaleChanged();
    Q_EMIT renderDataChanged();
}

QString FieldScene::scaleUnit() const
{
    switch (m_layer) {
    case 1: return QStringLiteral("ε");
    case 2: return tr("N, м⁻³");
    case 3: return QStringLiteral("Y");
    case 4: return tr("B, Тл");
    default: return m_modulus ? QStringLiteral("|E|") : QStringLiteral("E");
    }
}

QVariantList FieldScene::scaleColors() const
{
    QVariantList out;
    const int n = 48;
    for (int i = 0; i < n; ++i)
        out << QVariant::fromValue(scaleColor(m_render.scale, double(i) / (n - 1)));
    return out;
}

void FieldScene::updateTopology()
{
    m_render.quads.clear();
    m_scenePorts.clear();
    m_dataPorts.clear();
    if (m_hasTopology && m_series.count() > 0) {
        const FieldHeader &h = m_series.entry(0).header;
        // Показываемая область — 0 … (n−1)·Delta (bXsizeFlag / bYsizeFlag = TRUE).
        const double xMax = (h.nX - 1) * h.delta, yMax = (h.nY - 1) * h.delta;
        TopologyFilter filter;
        std::vector<TopQuad> quads;
        visibleTopology(m_bounds, 0.0, xMax, 0.0, yMax, h.delta, filter, &quads, &m_dataPorts);
        const double lx = double(h.nX - 1), ly = double(h.nY - 1);
        const double sx = 2.5 * (lx < ly ? lx / ly : 1.0), sy = 2.5 * (lx < ly ? 1.0 : ly / lx);
        const auto toX = [&](double x) { return (x / xMax - 0.5) * sx; };
        const auto toY = [&](double y) { return (y / yMax - 0.5) * sy; };
        if (m_topologyVisible) {
            for (TopQuad q : quads) {
                for (int k = 0; k < 4; ++k) {
                    q.x[k] = toX(q.x[k]);
                    q.y[k] = toY(q.y[k]);
                }
                m_render.quads.push_back(q);
            }
        }
        for (TopPort p : m_dataPorts) {
            p.x = toX(p.x);
            p.y = toY(p.y);
            m_scenePorts.push_back(p);
        }
    }
    ++m_render.topologyVersion;
    Q_EMIT topologyChanged();
    Q_EMIT renderDataChanged();
}

QVariantList FieldScene::ports() const
{
    QVariantList out;
    for (const TopPort &p : m_dataPorts) {
        QVariantMap m;
        m.insert(QStringLiteral("number"), p.number);
        m.insert(QStringLiteral("x"), p.x);
        m.insert(QStringLiteral("y"), p.y);
        out << m;
    }
    return out;
}

double FieldScene::valueAt(double u, double v) const
{
    const auto &vals = m_render.values;
    if (!vals || m_render.nX < 2)
        return 0.0;
    const int j = std::clamp(int(std::lround(u * (m_render.nX - 1))), 0, m_render.nX - 1);
    const int i = std::clamp(int(std::lround(v * (m_render.nY - 1))), 0, m_render.nY - 1);
    return double((*vals)[size_t(i) * size_t(m_render.nX) + size_t(j)]);
}

void FieldScene::startIndexing()
{
    const int n = m_series.count();
    m_indexProgress = n <= 1 ? 1.0 : 0.0;
    Q_EMIT indexProgressChanged();
    if (n <= 1)
        return;
    // Фоновый проход по всем кадрам: размах для шкалы «по всем тактам».
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    m_cancel = cancel;
    const FieldSeries series = m_series;
    QPointer<FieldScene> self(this);
    m_indexer = QThread::create([series, cancel, self, n]() {
        std::vector<FieldStats> stats(static_cast<size_t>(n));
        FieldFrame f;
        QString err;
        for (int i = 0; i < n && !cancel->load(); ++i) {
            if (series.readFrame(i, &f, &err))
                stats[size_t(i)] = f.stats;
            if ((i % 16) == 15 || i == n - 1) {
                const double prog = double(i + 1) / n;
                QMetaObject::invokeMethod(self, [self, prog]() {
                    if (self) {
                        self->m_indexProgress = prog;
                        Q_EMIT self->indexProgressChanged();
                    }
                }, Qt::QueuedConnection);
            }
        }
        if (cancel->load())
            return;
        QMetaObject::invokeMethod(self, [self, stats, cancel]() {
            if (!self || cancel->load())
                return;
            self->m_stats = stats;
            self->m_statsKnown.assign(stats.size(), 1);
            self->updateRange();
        }, Qt::QueuedConnection);
    });
    m_indexer->start(QThread::LowPriority);
}

void FieldScene::stopIndexing()
{
    if (m_cancel)
        m_cancel->store(true);
    if (m_indexer) {
        m_indexer->wait();
        delete m_indexer;
        m_indexer = nullptr;
    }
    m_cancel.reset();
}

} // namespace fieldview
