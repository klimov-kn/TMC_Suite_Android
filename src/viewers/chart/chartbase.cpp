#include "chartbase.h"

#include "theme.h"
#include "viewers.h"

#include <QDir>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

namespace viewers {

namespace {

// Размеры (логические пиксели).
constexpr qreal kTickFont = 12;      // подписи делений — JetBrains Mono
constexpr qreal kTitleFont = 13;     // названия осей — Inter
constexpr qreal kPickFont = 13;      // значение точки — JetBrains Mono
constexpr qreal kPad = 10;
constexpr qreal kTouchRadius = 48;   // касание точки: не дальше 48 пикселей
constexpr qreal kSeriesWidth = 2.0;

double niceStep(double raw, double *mantissa)
{
    if (!(raw > 0) || !std::isfinite(raw))
        raw = 1;
    const double e = std::floor(std::log10(raw));
    const double p = std::pow(10.0, e);
    const double f = raw / p;
    double n;
    if (f <= 1.0) n = 1;
    else if (f <= 2.0) n = 2;
    else if (f <= 2.5) n = 2.5;
    else if (f <= 5.0) n = 5;
    else n = 10;
    if (mantissa)
        *mantissa = n;
    return n * p;
}

std::vector<double> ticksFor(double lo, double hi, double step)
{
    std::vector<double> t;
    if (!(step > 0) || !std::isfinite(lo) || !std::isfinite(hi))
        return t;
    double v = std::ceil(lo / step - 1e-9) * step;
    for (int guard = 0; v <= hi + step * 1e-9 && guard < 200; ++guard, v += step)
        t.push_back(std::fabs(v) < step * 1e-9 ? 0.0 : v);
    return t;
}

QString tickLabel(double v, double step, double mantissa)
{
    const double a = std::max(std::fabs(v), step);
    QString s;
    if (a >= 1e6 || step < 1e-5) {
        s = QString::number(v, 'g', 3);
    } else {
        int dec = int(std::max(0.0, -std::floor(std::log10(step) + 1e-9)));
        if (mantissa == 2.5 && step < 10)
            dec += 1;
        s = QString::number(v, 'f', std::min(dec, 8));
    }
    return localizeNumber(s);
}

struct Axis
{
    std::vector<double> ticks;
    QStringList labels;
    qreal widest = 0;
};

// Шаг делений: не меньше minGapPx между соседними подписями (подписи не налезают).
Axis makeAxis(double lo, double hi, qreal lengthPx, qreal targetPx, const QFontMetricsF &fm, bool horizontal)
{
    Axis a;
    const double span = hi - lo;
    if (!(span > 0) || lengthPx <= 0)
        return a;
    // Начинаем с частого шага и укрупняем, пока подписи не перестанут налезать.
    double raw = span / std::max(1.0, double(lengthPx / (targetPx * 0.5)));
    for (int iter = 0; iter < 24; ++iter) {
        double mant = 1;
        const double step = niceStep(raw, &mant);
        a.ticks = ticksFor(lo, hi, step);
        a.labels.clear();
        a.widest = 0;
        for (double t : a.ticks) {
            const QString l = tickLabel(t, step, mant);
            a.labels << l;
            a.widest = std::max(a.widest, fm.horizontalAdvance(l));
        }
        const qreal need = horizontal ? std::max<qreal>(a.widest + 16, targetPx * 0.62)
                                      : std::max<qreal>(fm.height() * 2.0, targetPx * 0.62);
        if (step / span * lengthPx >= need || a.ticks.size() <= 2)
            break;
        raw = step * 1.05;
    }
    return a;
}

} // namespace

ChartBase::ChartBase(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(true);
    // Касания разбирает TouchGesture; мышь на ПК — обработчики QML в ChartScreen.
    setAcceptTouchEvents(true);
    m_touch.onDrag = [this](const QPointF &d) { panBy(d.x(), d.y()); };
    m_touch.onPinch = [this](double f, const QPointF &c, const QPointF &d) {
        panBy(d.x(), d.y());
        zoomAt(f, c.x(), c.y());
    };
    m_touch.onTap = [this](const QPointF &p) { pick(p.x(), p.y()); };
    m_touch.onDoubleTap = [this](const QPointF &) {
        clearPick();
        resetView();
    };
    connect(this, &QQuickItem::widthChanged, this, [this] { update(); });
    connect(this, &QQuickItem::heightChanged, this, [this] { update(); });
    // Смена языка: подписи осей, рядов и сводки — заново.
    connect(languageNotifier(), &LanguageNotifier::changed, this, [this] {
        retranslate();
        applyRebuild(true);
    });
}

ChartBase::~ChartBase() = default;

// --- Источник и загрузка ----------------------------------------------------------

void ChartBase::setFolder(const QString &v)
{
    if (m_folder == v)
        return;
    m_folder = v;
    emit sourceChanged();
    scheduleLoad();
}

void ChartBase::setFile(const QString &v)
{
    if (m_file == v)
        return;
    m_file = v;
    emit sourceChanged();
    scheduleLoad();
}

void ChartBase::setFiles(const QStringList &v)
{
    if (m_files == v)
        return;
    m_files = v;
    emit sourceChanged();
    scheduleLoad();
}

void ChartBase::setCompare(const QVariantList &v)
{
    if (m_compare == v)
        return;
    m_compare = v;
    emit sourceChanged();
    scheduleLoad();
}

void ChartBase::reload() { scheduleLoad(); }

void ChartBase::scheduleLoad()
{
    if (m_loadQueued)
        return;
    m_loadQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_loadQueued = false;
        startLoad();
    });
}

void ChartBase::startLoad()
{
    const quint64 gen = ++m_generation;
    if (m_file.isEmpty()) {
        m_doc.reset();
        m_series.clear();
        m_error.clear();
        m_loading = false;
        emit stateChanged();
        emit seriesChanged();
        update();
        return;
    }
    QStringList names = m_files;
    names.removeAll(m_file);
    names.prepend(m_file);   // открытый файл — первым (его ряды видны по умолчанию)

    m_loading = true;
    m_error.clear();
    emit stateChanged();

    struct Source { QString folder, file, label; bool compared; };
    std::vector<Source> sources;
    for (const QString &n : std::as_const(names))
        sources.push_back({ m_folder, n, n, false });
    for (const QVariant &v : std::as_const(m_compare)) {
        const QVariantMap m = v.toMap();
        const QString file = m.value(QStringLiteral("file")).toString();
        const QString label = m.value(QStringLiteral("label")).toString();
        if (!file.isEmpty())
            sources.push_back({ m.value(QStringLiteral("folder")).toString(), file,
                                label.isEmpty() ? file : label, true });
    }

    const Loader load = loader();
    const FileReader read = fileReader();
    QPointer<ChartBase> self(this);
    QThreadPool::globalInstance()->start([=]() {
        std::vector<InputFile> in;
        QString err;
        for (const Source &s : sources) {
            InputFile f{ s.label, read(s.folder, s.file), s.compared };
            if (f.bytes.isEmpty()) {
                if (&s == &sources.front()) {
                    err = tr("не удалось прочитать %1").arg(s.file);
                    break;
                }
                continue;
            }
            in.push_back(std::move(f));
        }
        std::shared_ptr<Doc> doc;
        if (err.isEmpty())
            doc = load(in, &err);
        QMetaObject::invokeMethod(qApp, [self, gen, doc, err]() {
            if (!self || self->m_generation != gen)
                return;
            self->m_loading = false;
            self->m_error = doc ? QString() : (err.isEmpty() ? tr("файл не прочитан") : err);
            self->m_doc = doc;
            self->m_pickSeries = -1;
            self->m_pickIndex = -1;
            self->applyRebuild(false);
            emit self->stateChanged();
        }, Qt::QueuedConnection);
    });
}

// --- Вид оси и ряды -------------------------------------------------------------

void ChartBase::setModes(const QStringList &modes)
{
    m_modes = modes;
    if (m_mode >= m_modes.size())
        m_mode = 0;
}

void ChartBase::setInfo(const QString &info) { m_info = info; }

void ChartBase::setMode(int m)
{
    if (m < 0 || m == m_mode || (!m_modes.isEmpty() && m >= m_modes.size()))
        return;
    m_mode = m;
    emit modeChanged();
    applyRebuild(true);
}

void ChartBase::setPolar(bool p)
{
    if (p == m_polar || (p && !polarAvailable()))
        return;
    m_polar = p;
    emit modeChanged();
    applyRebuild(false);
}

void ChartBase::applyRebuild(bool keepX)
{
    std::vector<Series> before;
    before.swap(m_series);
    if (m_doc)
        rebuild();
    // Видимость: выбор пользователя по ключу; если он ещё ничего не трогал — по умолчанию.
    // Ряды, которых раньше не было (добавлен файл сравнения), — тоже по умолчанию.
    for (Series &s : m_series) {
        if (!m_selectionTouched || !m_seen.contains(s.key)) {
            s.visible = s.defaultVisible;
            if (s.visible)
                m_selected.insert(s.key);
            else
                m_selected.remove(s.key);
        } else {
            s.visible = m_selected.contains(s.key);
        }
        m_seen.insert(s.key);
    }
    if (m_pickSeries >= int(m_series.size()) || (m_pickSeries >= 0 && !m_series[size_t(m_pickSeries)].visible)) {
        m_pickSeries = -1;
        m_pickIndex = -1;
    }

    const QRectF autoRect = autoView();
    if (keepX && m_zoomed && !m_polar) {
        // Смена вида оси Y: окно по X сохраняется, Y — заново по видимой части.
        QRectF r = m_view;
        r.setTop(autoRect.top());
        r.setHeight(autoRect.height());
        m_view = r;
    } else {
        m_view = autoRect;
        m_zoomed = false;
    }
    emit dataChanged();
    emit seriesChanged();
    emit viewChanged();
    update();
}

QVariantList ChartBase::seriesModel() const
{
    QVariantList out;
    for (const Series &s : m_series) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), s.name);
        m.insert(QStringLiteral("group"), s.group);
        m.insert(QStringLiteral("color"), theme::series(s.colorIndex));
        m.insert(QStringLiteral("visible"), s.visible);
        m.insert(QStringLiteral("compared"), s.compared);
        out.append(m);
    }
    return out;
}

int ChartBase::visibleCount() const
{
    return int(std::count_if(m_series.begin(), m_series.end(), [](const Series &s) { return s.visible; }));
}

void ChartBase::setSeriesVisible(int index, bool visible)
{
    if (index < 0 || index >= int(m_series.size()) || m_series[size_t(index)].visible == visible)
        return;
    m_selectionTouched = true;
    m_series[size_t(index)].visible = visible;
    if (visible)
        m_selected.insert(m_series[size_t(index)].key);
    else
        m_selected.remove(m_series[size_t(index)].key);
    if (!visible && m_pickSeries == index) {
        m_pickSeries = -1;
        m_pickIndex = -1;
    }
    if (!m_zoomed) {
        m_view = autoView();
    }
    emit seriesChanged();
    emit viewChanged();
    update();
}

QString ChartBase::polarRingLabel(double r) const { return fmt(r, 3); }

QString ChartBase::fmt(double v, int significant)
{
    if (!std::isfinite(v))
        return v > 0 ? QStringLiteral("∞") : v < 0 ? QStringLiteral("−∞") : QStringLiteral("—");
    return localizeNumber(QString::number(v, 'g', significant));
}

// --- Геометрия ------------------------------------------------------------------

QPointF ChartBase::pointOf(const Series &s, size_t i) const
{
    if (m_polar) {
        const double r = std::max(0.0, polarRadius(s.y[i]));
        return { r * std::cos(s.x[i]), r * std::sin(s.x[i]) };
    }
    return { s.x[i], s.y[i] };
}

QRectF ChartBase::autoView() const
{
    if (m_polar) {
        double rmax = 0;
        for (double r : polarRings())
            rmax = std::max(rmax, r);
        if (rmax <= 0)
            rmax = 1;
        const double e = rmax * 1.2;
        return QRectF(-e, -e, 2 * e, 2 * e);
    }
    double x0 = std::numeric_limits<double>::infinity(), x1 = -x0, y0 = x0, y1 = -x0;
    for (const Series &s : m_series) {
        if (!s.visible)
            continue;
        for (size_t i = 0; i < s.x.size(); ++i) {
            if (!std::isfinite(s.x[i]) || !std::isfinite(s.y[i]))
                continue;
            x0 = std::min(x0, s.x[i]);
            x1 = std::max(x1, s.x[i]);
            y0 = std::min(y0, s.y[i]);
            y1 = std::max(y1, s.y[i]);
        }
    }
    if (!std::isfinite(x0)) {
        return QRectF(0, 0, 1, 1);
    }
    const double span = yAutoFloorSpan();
    if (span > 0 && y0 < y1 - span)
        y0 = y1 - span;
    if (x1 - x0 <= 0) {
        const double d = std::max(std::fabs(x0) * 0.05, 1e-9);
        x0 -= d;
        x1 += d;
    }
    double dy = (y1 - y0) * 0.06;
    if (dy <= 0)
        dy = std::max(std::fabs(y0) * 0.1, 1e-6);
    return QRectF(x0, y0 - dy, x1 - x0, (y1 - y0) + 2 * dy);
}

QRectF ChartBase::plotRect(const QSizeF &size, bool forExport) const
{
    const QFontMetricsF tick(theme::monoFont(int(kTickFont)));
    const QFontMetricsF title(theme::uiFont(int(kTitleFont)));
    qreal top = kPad + title.height() + 6;     // строка: название оси Y слева, сводка справа
    qreal bottom = kPad;
    if (forExport) {
        // Легенда снизу: по строке на ряд, в несколько колонок.
        int visible = visibleCount();
        const int perRow = std::max(1, int(size.width() / 220));
        const int rows = (visible + perRow - 1) / perRow;
        bottom += rows * (title.height() + 4) + 6;
        top += title.height() + 4;               // заголовок снимка
    }
    if (m_polar) {
        return QRectF(kPad, top, std::max<qreal>(10, size.width() - 2 * kPad),
                      std::max<qreal>(10, size.height() - top - bottom));
    }
    bottom += tick.height() + 6 + title.height() + 2;   // подписи делений X и название оси X
    const qreal plotH = std::max<qreal>(10, size.height() - top - bottom);
    // Ширина подписей Y — от делений при этой высоте.
    const Axis ya = makeAxis(m_view.top(), m_view.bottom(), plotH, 56, tick, false);
    const qreal left = kPad + std::max<qreal>(ya.widest, tick.horizontalAdvance(QStringLiteral("0,0"))) + 8;
    const qreal right = kPad + tick.horizontalAdvance(QStringLiteral("00,0")) / 2;   // последняя подпись X
    return QRectF(left, top, std::max<qreal>(10, size.width() - left - right), plotH);
}

QRectF ChartBase::effectiveView(const QRectF &plot) const
{
    if (!m_polar || plot.width() <= 0 || plot.height() <= 0)
        return m_view;
    // Полярные: масштаб по осям одинаковый — окно подгоняется под пропорции поля.
    const double s = std::min(plot.width() / m_view.width(), plot.height() / m_view.height());
    const double w = plot.width() / s, h = plot.height() / s;
    const QPointF c = m_view.center();
    return QRectF(c.x() - w / 2, c.y() - h / 2, w, h);
}

QPointF ChartBase::toScreen(const QRectF &plot, const QRectF &view, double x, double y) const
{
    const double sx = plot.width() / view.width();
    const double sy = plot.height() / view.height();
    return { plot.left() + (x - view.left()) * sx, plot.bottom() - (y - view.top()) * sy };
}

QPointF ChartBase::toData(const QRectF &plot, const QRectF &view, QPointF p) const
{
    const double sx = plot.width() / view.width();
    const double sy = plot.height() / view.height();
    return { view.left() + (p.x() - plot.left()) / sx, view.top() + (plot.bottom() - p.y()) / sy };
}

// --- Жесты ------------------------------------------------------------------------

void ChartBase::touchEvent(QTouchEvent *e)
{
    m_touch.process(e);
}

void ChartBase::touchUngrabEvent()
{
    m_touch.reset();
}

void ChartBase::zoomAt(qreal factor, qreal x, qreal y)
{
    if (!(factor > 0) || !std::isfinite(factor) || m_series.empty())
        return;
    const QRectF plot = plotRect(size(), false);
    const QRectF view = effectiveView(plot);
    const QRectF home = autoView();
    // Не дальше 1e6 крупнее и 4 раз мельче исходного вида.
    const double w = view.width() / factor;
    if (w < home.width() * 1e-6 || w > home.width() * 4)
        return;
    const QPointF d = toData(plot, view, QPointF(x, y));
    const double nl = d.x() - (d.x() - view.left()) / factor;
    const double nb = d.y() - (d.y() - view.top()) / factor;
    m_view = QRectF(nl, nb, view.width() / factor, view.height() / factor);
    m_zoomed = true;
    emit viewChanged();
    update();
}

void ChartBase::panBy(qreal dx, qreal dy)
{
    if (m_series.empty())
        return;
    const QRectF plot = plotRect(size(), false);
    const QRectF view = effectiveView(plot);
    const double sx = plot.width() / view.width();
    const double sy = plot.height() / view.height();
    m_view = QRectF(view.left() - dx / sx, view.top() + dy / sy, view.width(), view.height());
    m_zoomed = true;
    emit viewChanged();
    update();
}

void ChartBase::resetView()
{
    m_view = autoView();
    m_zoomed = false;
    emit viewChanged();
    update();
}

void ChartBase::pick(qreal x, qreal y)
{
    const QRectF plot = plotRect(size(), false);
    const QRectF view = effectiveView(plot);
    double best = kTouchRadius * kTouchRadius;
    int bs = -1;
    qint64 bi = -1;
    for (size_t k = 0; k < m_series.size(); ++k) {
        const Series &s = m_series[k];
        if (!s.visible)
            continue;
        for (size_t i = 0; i < s.x.size(); ++i) {
            if (!std::isfinite(s.y[i]))
                continue;
            const QPointF pd = pointOf(s, i);
            const QPointF ps = toScreen(plot, view, pd.x(), pd.y());
            const double d = (ps.x() - x) * (ps.x() - x) + (ps.y() - y) * (ps.y() - y);
            if (d < best) {
                best = d;
                bs = int(k);
                bi = qint64(i);
            }
        }
    }
    m_pickSeries = bs;
    m_pickIndex = bi;
    emit viewChanged();
    update();
}

void ChartBase::pickMax()
{
    for (size_t k = 0; k < m_series.size(); ++k) {
        const Series &s = m_series[k];
        if (!s.visible)
            continue;
        qint64 bi = -1;
        for (size_t i = 0; i < s.y.size(); ++i)
            if (std::isfinite(s.y[i]) && (bi < 0 || s.y[i] > s.y[size_t(bi)]))
                bi = qint64(i);
        m_pickSeries = int(k);
        m_pickIndex = bi;
        break;
    }
    emit viewChanged();
    update();
}

void ChartBase::clearPick()
{
    m_pickSeries = -1;
    m_pickIndex = -1;
    emit viewChanged();
    update();
}

QString ChartBase::pickText() const
{
    if (m_pickSeries < 0 || m_pickIndex < 0)
        return {};
    const Series &s = m_series[size_t(m_pickSeries)];
    return s.name + QStringLiteral(": ") + m_xTitle + QStringLiteral(" = ") + fmt(s.x[size_t(m_pickIndex)])
         + QStringLiteral("; ") + m_yTitle + QStringLiteral(" = ") + fmt(s.y[size_t(m_pickIndex)]);
}

// --- Рисование --------------------------------------------------------------------

void ChartBase::paint(QPainter *painter)
{
    draw(painter, size(), false);
}

QImage ChartBase::renderImage(const QSizeF &size, qreal dpr, bool forExport) const
{
    QImage img((size * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(theme::bg());
    QPainter p(&img);
    draw(&p, size, forExport);
    p.end();
    return img;
}

QString ChartBase::saveImage(const QString &fileName)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/charts");
    QDir().mkpath(dir);
    const QString path = dir + QLatin1Char('/') + fileName;
    QSizeF sz = size();
    if (sz.width() < 50 || sz.height() < 50)
        sz = QSizeF(1080 / 3.0, 720 / 3.0);
    qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 2.0;
    dpr = std::max<qreal>(dpr, 2.0);
    const QImage img = renderImage(sz, dpr, true);
    return img.save(path, "PNG") ? path : QString();
}

void ChartBase::draw(QPainter *p, const QSizeF &size, bool forExport) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);
    p->fillRect(QRectF(QPointF(0, 0), size), theme::bg());

    const QFont titleFont = theme::uiFont(int(kTitleFont), QFont::DemiBold);
    const QFont monoFont = theme::monoFont(int(kTickFont));
    const QFontMetricsF tfm(titleFont);

    if (m_series.empty() || m_loading || !m_error.isEmpty()) {
        p->setFont(theme::uiFont(15));
        p->setPen(m_error.isEmpty() ? theme::textMute() : theme::danger());
        const QString msg = m_loading ? tr("Читаю %1…").arg(m_file)
                          : !m_error.isEmpty() ? m_error
                          : m_file.isEmpty() ? tr("Нет файла") : tr("Нет данных");
        p->drawText(QRectF(QPointF(0, 0), size).adjusted(20, 20, -20, -20),
                    Qt::AlignCenter | Qt::TextWordWrap, msg);
        p->restore();
        return;
    }

    const QRectF plot = plotRect(size, forExport);
    const QRectF view = effectiveView(plot);

    qreal y = kPad;
    if (forExport) {
        // Заголовок снимка: вид графика · файл · вид оси.
        p->setFont(theme::uiFont(int(kTitleFont) + 1, QFont::Bold));
        p->setPen(theme::text());
        QString head = kindTitle() + QStringLiteral(" · ") + m_file;
        if (m_mode >= 0 && m_mode < m_modes.size())
            head += QStringLiteral(" · ") + m_modes.at(m_mode);
        if (m_polar)
            head += tr(" · полярные");
        p->drawText(QRectF(kPad, y, size.width() - 2 * kPad, tfm.height() + 2), Qt::AlignLeft | Qt::AlignVCenter, head);
        y += tfm.height() + 4;
    }
    // Строка над полем: название оси Y слева, сводка справа.
    p->setFont(titleFont);
    p->setPen(theme::text());
    const QString yTitle = m_polar ? QString() : m_yTitle;
    p->drawText(QRectF(kPad, y, size.width() / 2, tfm.height()), Qt::AlignLeft | Qt::AlignVCenter, yTitle);
    // Сводка — только на снимке (на экране она в шапке).
    p->setFont(monoFont);
    p->setPen(theme::textDim());
    if (forExport) {
    const qreal infoLeft = kPad + (yTitle.isEmpty() ? 0 : QFontMetricsF(titleFont).horizontalAdvance(yTitle) + 16);
    p->drawText(QRectF(infoLeft, y, size.width() - kPad - infoLeft, tfm.height()),
                Qt::AlignRight | Qt::AlignVCenter,
                QFontMetricsF(monoFont).elidedText(m_info, Qt::ElideLeft, size.width() - kPad - infoLeft));
    }

    if (m_polar)
        drawPolarGrid(p, plot, view);
    else
        drawCartesianGrid(p, plot, view);

    p->save();
    p->setClipRect(plot.adjusted(-1, -1, 1, 1));
    drawSeries(p, plot, view);
    drawPick(p, plot, view);
    p->restore();

    if (forExport) {
        // Легенда — под полем (и под подписями оси X в декартовых координатах).
        const qreal below = m_polar ? 6 : QFontMetricsF(monoFont).height() + 6 + tfm.height() + 8;
        drawLegend(p, QRectF(kPad, plot.bottom() + below, size.width() - 2 * kPad,
                             size.height() - plot.bottom() - below));
    }
    p->restore();
}

void ChartBase::drawCartesianGrid(QPainter *p, const QRectF &plot, const QRectF &view) const
{
    const QFont mono = theme::monoFont(int(kTickFont));
    const QFontMetricsF fm(mono);
    const Axis xa = makeAxis(view.left(), view.right(), plot.width(), 90, fm, true);
    const Axis ya = makeAxis(view.top(), view.bottom(), plot.height(), 56, fm, false);

    QPen grid(theme::line());
    grid.setWidthF(1);
    grid.setCosmetic(true);
    p->setPen(grid);
    for (double t : xa.ticks) {
        const qreal X = toScreen(plot, view, t, 0).x();
        p->drawLine(QPointF(X, plot.top()), QPointF(X, plot.bottom()));
    }
    for (double t : ya.ticks) {
        const qreal Y = toScreen(plot, view, 0, t).y();
        p->drawLine(QPointF(plot.left(), Y), QPointF(plot.right(), Y));
    }
    // Оси — text-dim (style-guide, «Графики»).
    QPen axis(theme::textDim());
    axis.setWidthF(1);
    axis.setCosmetic(true);
    p->setPen(axis);
    p->drawLine(plot.bottomLeft(), plot.bottomRight());
    p->drawLine(plot.bottomLeft(), plot.topLeft());

    p->setFont(mono);
    p->setPen(theme::textDim());
    // Подписи X: по центру деления, крайние прижаты внутрь поля; пропуск, если налезает.
    qreal lastRight = -1e9;
    for (size_t i = 0; i < xa.ticks.size(); ++i) {
        const qreal X = toScreen(plot, view, xa.ticks[i], 0).x();
        const qreal w = fm.horizontalAdvance(xa.labels.at(int(i)));
        qreal left = X - w / 2;
        left = std::max(left, plot.left() - 8);
        if (left < lastRight + 8)
            continue;
        p->drawText(QRectF(left, plot.bottom() + 4, w + 2, fm.height()), Qt::AlignLeft | Qt::AlignTop,
                    xa.labels.at(int(i)));
        lastRight = left + w;
    }
    qreal lastTop = 1e9;
    for (size_t i = 0; i < ya.ticks.size(); ++i) {
        const qreal Y = toScreen(plot, view, 0, ya.ticks[i]).y();
        const qreal top = Y - fm.height() / 2;
        if (top + fm.height() > lastTop - 2)
            continue;
        p->drawText(QRectF(0, top, plot.left() - 6, fm.height()), Qt::AlignRight | Qt::AlignVCenter,
                    ya.labels.at(int(i)));
        lastTop = top;
    }
    // Название оси X — справа под подписями.
    p->setFont(theme::uiFont(int(kTitleFont), QFont::DemiBold));
    p->setPen(theme::text());
    const QFontMetricsF tfm(p->font());
    p->drawText(QRectF(plot.left(), plot.bottom() + 4 + fm.height() + 2, plot.width(), tfm.height()),
                Qt::AlignRight | Qt::AlignVCenter, m_xTitle);
}

void ChartBase::drawPolarGrid(QPainter *p, const QRectF &plot, const QRectF &view) const
{
    const QFont mono = theme::monoFont(int(kTickFont));
    const QFontMetricsF fm(mono);
    const QPointF c = toScreen(plot, view, 0, 0);
    const double scale = plot.width() / view.width();
    const std::vector<double> rings = polarRings();
    double rmax = 0;
    for (double r : rings)
        rmax = std::max(rmax, r);

    QPen grid(theme::line());
    grid.setWidthF(1);
    grid.setCosmetic(true);
    p->setPen(grid);
    p->setBrush(Qt::NoBrush);
    for (double r : rings)
        p->drawEllipse(c, r * scale, r * scale);
    for (int deg = 0; deg < 360; deg += 30) {
        const double a = deg * 3.141592653589793 / 180.0;
        p->drawLine(c, QPointF(c.x() + rmax * scale * std::cos(a), c.y() - rmax * scale * std::sin(a)));
    }
    QPen axis(theme::textDim());
    axis.setCosmetic(true);
    p->setPen(axis);
    p->drawEllipse(c, rmax * scale, rmax * scale);

    p->setFont(mono);
    p->setPen(theme::textDim());
    // Подписи углов снаружи круга: 0°, ±30° … 180°.
    for (int deg = -150; deg <= 180; deg += 30) {
        const double a = deg * 3.141592653589793 / 180.0;
        const QString l = (deg < 0 ? QString(QChar(0x2212)) : QString()) + QString::number(std::abs(deg)) + QChar(0x00B0);
        const qreal w = fm.horizontalAdvance(l);
        const qreal R = rmax * scale + 6 + std::max(w, fm.height()) / 2;
        const QPointF at(c.x() + R * std::cos(a), c.y() - R * std::sin(a));
        p->drawText(QRectF(at.x() - w / 2, at.y() - fm.height() / 2, w + 2, fm.height()), Qt::AlignCenter, l);
    }
    // Подписи колец — вдоль луча 112,5° (между лучами сетки; главный лепесток ДН обычно
    // у 0°, там подписи мешали бы).
    const double la = 112.5 * 3.141592653589793 / 180.0;
    qreal lastY = 1e9;
    for (double r : rings) {
        const QString l = polarRingLabel(r);
        const qreal w = fm.horizontalAdvance(l);
        const QPointF at(c.x() + r * scale * std::cos(la), c.y() - r * scale * std::sin(la));
        if (at.y() > lastY - fm.height())
            continue;
        p->drawText(QRectF(at.x() - w - 4, at.y() - fm.height(), w + 2, fm.height()), Qt::AlignRight | Qt::AlignVCenter, l);
        lastY = at.y();
    }
}

void ChartBase::drawSeries(QPainter *p, const QRectF &plot, const QRectF &view) const
{
    for (const Series &s : m_series) {
        if (!s.visible || s.x.empty())
            continue;
        const QColor color = theme::series(s.colorIndex);
        QPen pen(color);
        pen.setWidthF(kSeriesWidth);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        if (s.compared)
            pen.setStyle(Qt::DashLine);   // файл сравнения — пунктиром
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);

        // Прореживание: в одном столбце пикселей — только крайние значения (вид не меняется,
        // а рисовать 50 тысяч отрезков на каждый кадр жеста не нужно).
        QPolygonF poly;
        poly.reserve(int(std::min<size_t>(s.x.size(), size_t(plot.width() * 4 + 8))));
        auto flush = [&]() {
            if (poly.size() > 1)
                p->drawPolyline(poly);
            else if (poly.size() == 1)
                p->drawPoint(poly.first());
            poly.clear();
        };
        const bool decimate = !m_polar && s.x.size() > size_t(plot.width() * 2);
        int col = std::numeric_limits<int>::min();
        qreal cMin = 0, cMax = 0, cLast = 0, cFirst = 0, cX = 0;
        auto emitColumn = [&]() {
            if (col == std::numeric_limits<int>::min())
                return;
            poly << QPointF(cX, cFirst);
            if (cMin != cFirst) poly << QPointF(cX, cMin);
            if (cMax != cMin) poly << QPointF(cX, cMax);
            if (cLast != cMax) poly << QPointF(cX, cLast);
        };
        for (size_t i = 0; i < s.x.size(); ++i) {
            if (!std::isfinite(s.y[i]) || !std::isfinite(s.x[i])) {
                emitColumn();
                col = std::numeric_limits<int>::min();
                flush();
                continue;
            }
            const QPointF d = pointOf(s, i);
            const QPointF q = toScreen(plot, view, d.x(), d.y());
            if (!decimate) {
                poly << q;
                continue;
            }
            const int c = int(std::floor(q.x()));
            if (c != col) {
                emitColumn();
                col = c;
                cX = q.x();
                cFirst = cMin = cMax = cLast = q.y();
            } else {
                cMin = std::min(cMin, q.y());
                cMax = std::max(cMax, q.y());
                cLast = q.y();
            }
        }
        emitColumn();
        flush();

        // Точки — если их немного (S-матрица на нескольких частотах).
        if (s.x.size() <= 64) {
            p->setPen(QPen(theme::bg(), 1.5));
            p->setBrush(color);
            for (size_t i = 0; i < s.x.size(); ++i) {
                if (!std::isfinite(s.y[i]))
                    continue;
                const QPointF d = pointOf(s, i);
                p->drawEllipse(toScreen(plot, view, d.x(), d.y()), 4, 4);
            }
            p->setBrush(Qt::NoBrush);
        }
    }
}

void ChartBase::drawPick(QPainter *p, const QRectF &plot, const QRectF &view) const
{
    if (m_pickSeries < 0 || m_pickIndex < 0 || m_pickSeries >= int(m_series.size()))
        return;
    const Series &s = m_series[size_t(m_pickSeries)];
    if (size_t(m_pickIndex) >= s.x.size())
        return;
    const size_t i = size_t(m_pickIndex);
    const QPointF d = pointOf(s, i);
    const QPointF q = toScreen(plot, view, d.x(), d.y());
    const QColor color = theme::series(s.colorIndex);

    QPen guide(theme::textDim());
    guide.setStyle(Qt::DashLine);
    guide.setCosmetic(true);
    p->setPen(guide);
    if (m_polar)
        p->drawLine(toScreen(plot, view, 0, 0), q);
    else
        p->drawLine(QPointF(q.x(), plot.top()), QPointF(q.x(), plot.bottom()));
    p->setPen(QPen(theme::bg(), 2));
    p->setBrush(color);
    p->drawEllipse(q, 6, 6);

    // Метка: имя ряда и значения (JetBrains Mono), внутри поля графика.
    const QFont mono = theme::monoFont(int(kPickFont));
    const QFont monoBold = theme::monoFont(int(kPickFont), QFont::Bold);
    const QFontMetricsF fm(mono), fmb(monoBold);
    const QStringList lines = pointText(s, i);
    qreal w = fmb.horizontalAdvance(s.name);
    for (const QString &l : lines)
        w = std::max(w, fm.horizontalAdvance(l));
    const qreal lh = fm.height() + 2;
    const QSizeF box(w + 20, lh * (lines.size() + 1) + 12);
    QPointF at(q.x() + 14, q.y() - box.height() - 10);
    if (at.x() + box.width() > plot.right()) at.setX(q.x() - 14 - box.width());
    if (at.x() < plot.left()) at.setX(plot.left() + 4);
    if (at.y() < plot.top()) at.setY(q.y() + 14);
    if (at.y() + box.height() > plot.bottom()) at.setY(plot.bottom() - box.height() - 4);
    const QRectF r(at, box);
    p->setPen(QPen(theme::line(), 1));
    p->setBrush(theme::surface2());
    p->drawRoundedRect(r, 8, 8);
    p->setFont(monoBold);
    p->setPen(color);
    p->drawText(QRectF(r.left() + 10, r.top() + 6, w + 4, lh), Qt::AlignLeft | Qt::AlignVCenter, s.name);
    p->setFont(mono);
    p->setPen(theme::text());
    for (int k = 0; k < lines.size(); ++k)
        p->drawText(QRectF(r.left() + 10, r.top() + 6 + lh * (k + 1), w + 4, lh), Qt::AlignLeft | Qt::AlignVCenter,
                    lines.at(k));
}

QStringList ChartBase::pointText(const Series &s, size_t i) const
{
    return { m_xTitle + QStringLiteral(" = ") + fmt(s.x[i]), m_yTitle + QStringLiteral(" = ") + fmt(s.y[i]) };
}

void ChartBase::drawLegend(QPainter *p, const QRectF &area) const
{
    const QFont f = theme::uiFont(int(kTitleFont));
    const QFontMetricsF fm(f);
    p->setFont(f);
    const int perRow = std::max(1, int(area.width() / 220));
    const qreal colW = area.width() / perRow;
    // Ряды из нескольких файлов (сравнение) подписываются с именем файла.
    QSet<QString> groups;
    for (const Series &s : m_series)
        if (s.visible)
            groups.insert(s.group);
    int k = 0;
    for (const Series &s : m_series) {
        if (!s.visible)
            continue;
        const QString label = groups.size() > 1 && s.group != s.name ? s.group + QStringLiteral(" · ") + s.name
                                                                     : s.name;
        const qreal x = area.left() + (k % perRow) * colW;
        const qreal y = area.top() + (k / perRow) * (fm.height() + 4);
        p->setPen(Qt::NoPen);
        p->setBrush(theme::series(s.colorIndex));
        if (s.compared) {
            p->drawRoundedRect(QRectF(x, y + fm.height() / 2 - 2, 5, 4), 2, 2);
            p->drawRoundedRect(QRectF(x + 9, y + fm.height() / 2 - 2, 5, 4), 2, 2);
        } else {
            p->drawRoundedRect(QRectF(x, y + fm.height() / 2 - 2, 14, 4), 2, 2);
        }
        p->setPen(theme::textDim());
        p->drawText(QRectF(x + 20, y, colW - 24, fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    fm.elidedText(label, Qt::ElideMiddle, colW - 24));
        ++k;
    }
}

} // namespace viewers
