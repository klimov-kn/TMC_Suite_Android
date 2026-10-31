#include "geometrycanvas.h"

#include "designermodel.h"

#include <QCoreApplication>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QTouchEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

using namespace tpl;

namespace {

// Допуски под палец, dp. У настольного оригинала — 7…11 пикселей мыши; подушечка
// пальца шире, поэтому крупнее, но не больше половины минимального касания (48 dp).
constexpr double kPickDp = 16.0;
constexpr double kHandleDp = 24.0;
constexpr double kSnapDp = 16.0;
constexpr double kPortMarkerDp = 11.0;
constexpr double kTapSlopDp = 10.0;   ///< сдвиг пальца, после которого касание — уже протяжка
constexpr int kRoleVertex = 100;
constexpr int kRolePortFrom = 50;
constexpr int kRolePortTo = 51;

double distToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 <= 1e-12)
        return QLineF(p, a).length();
    double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
    t = std::clamp(t, 0.0, 1.0);
    return QLineF(p, a + t * ab).length();
}

QPointF projectOnSegment(const QPointF &p, const QLineF &seg)
{
    const QPointF a = seg.p1();
    const QPointF ab = seg.p2() - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 <= 1e-12)
        return a;
    double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
    t = std::clamp(t, 0.0, 1.0);
    return a + t * ab;
}

/// Нормализовать прямоугольник: стороны переезжают вместе с гранями
/// (geometryview.cpp, normalizeRect).
void normalizeRect(Shape *s)
{
    if (s->xMin > s->xMax) {
        std::swap(s->xMin, s->xMax);
        std::swap(s->edges[EdgeLeft], s->edges[EdgeRight]);
    }
    if (s->yMin > s->yMax) {
        std::swap(s->yMin, s->yMax);
        std::swap(s->edges[EdgeBottom], s->edges[EdgeTop]);
    }
}

/// Дуга точками (geometryview.cpp, arcPoints): в языке .tpl дуги нет, только вершины.
QVector<QPointF> arcPoints(const QPointF &c, double r, double a0, double a1, double step = 1.5)
{
    const int n = std::clamp(int(std::fabs(a1 - a0) * r / std::max(0.1, step)), 3, 96);
    QVector<QPointF> pts;
    pts.reserve(n + 1);
    for (int i = 0; i <= n; ++i) {
        const double a = a0 + (a1 - a0) * i / n;
        pts << c + QPointF(r * std::cos(a), r * std::sin(a));
    }
    return pts;
}

/// Дуга ЧЕРЕЗ три точки (geometryview.cpp, arcThroughPoints): на одной прямой — отрезок.
QVector<QPointF> arcThroughPoints(const QPointF &a, const QPointF &m, const QPointF &b, double step)
{
    const double ax = a.x(), ay = a.y();
    const double bx = m.x(), by = m.y();
    const double cx = b.x(), cy = b.y();
    const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::fabs(d) < 1e-9)
        return {a, b};
    const double sa = ax * ax + ay * ay;
    const double sb = bx * bx + by * by;
    const double sc = cx * cx + cy * cy;
    const QPointF c((sa * (by - cy) + sb * (cy - ay) + sc * (ay - by)) / d,
                    (sa * (cx - bx) + sb * (ax - cx) + sc * (bx - ax)) / d);
    const double r = QLineF(c, a).length();
    if (r <= 1e-9 || r > 1e9)
        return {a, b};
    const auto angleOf = [&c](const QPointF &p) { return std::atan2(p.y() - c.y(), p.x() - c.x()); };
    const auto ccw = [](double v) {
        while (v < 0.0) v += 2 * M_PI;
        while (v >= 2 * M_PI) v -= 2 * M_PI;
        return v;
    };
    const double a0 = angleOf(a);
    const double toEnd = ccw(angleOf(b) - a0);
    const double toMid = ccw(angleOf(m) - a0);
    const double a1 = (toMid <= toEnd) ? a0 + toEnd : a0 - ccw(a0 - angleOf(b));
    return arcPoints(c, r, a0, a1, step);
}

/// Характерные точки фигуры (geometryview.cpp, keyPoints).
QVector<QPointF> keyPoints(const Shape &s)
{
    QVector<QPointF> pts;
    switch (s.kind) {
    case ShapeKind::Rectangle:
        pts << QPointF(s.xMin, s.yMin) << QPointF(s.xMax, s.yMin)
            << QPointF(s.xMax, s.yMax) << QPointF(s.xMin, s.yMax)
            << QPointF((s.xMin + s.xMax) / 2, s.yMin) << QPointF(s.xMax, (s.yMin + s.yMax) / 2)
            << QPointF((s.xMin + s.xMax) / 2, s.yMax) << QPointF(s.xMin, (s.yMin + s.yMax) / 2)
            << QPointF((s.xMin + s.xMax) / 2, (s.yMin + s.yMax) / 2);
        break;
    case ShapeKind::Circle:
        pts << s.center;
        for (int k = 0; k < 4; ++k) {
            const double a = k * M_PI / 2;
            pts << s.center + QPointF(s.rMax * std::cos(a), s.rMax * std::sin(a));
        }
        break;
    case ShapeKind::Polygon:
    case ShapeKind::Polyline:
        pts = s.points;
        break;
    }
    return pts;
}

QVector<QLineF> keySegments(const Shape &s)
{
    QVector<QLineF> segs;
    switch (s.kind) {
    case ShapeKind::Rectangle:
        for (int e = 0; e < 4; ++e)
            segs << QLineF(s.edgeFrom(e), s.edgeTo(e));
        break;
    case ShapeKind::Circle:
        break;
    case ShapeKind::Polygon:
    case ShapeKind::Polyline:
        for (int i = 0; i + 1 < s.points.size(); ++i)
            segs << QLineF(s.points.at(i), s.points.at(i + 1));
        if (s.kind == ShapeKind::Polygon && s.points.size() > 2)
            segs << QLineF(s.points.last(), s.points.first());
        break;
    }
    return segs;
}

/// Шаг сетки, кратный DELTA, но не мельче 10 dp (geometryview.cpp, niceGridStep).
double niceGridStep(double delta, double pxPerMm)
{
    double step = delta > 0.0 ? delta : 1.0;
    while (step * pxPerMm < 10.0)
        step *= 5.0;
    return step;
}

QPointF portEnd(const Port &p, const Box &b, bool second)
{
    const double plane = portPlane(p, b);
    const double v = second ? p.to : p.from;
    return portAlongY(p.side) ? QPointF(plane, v) : QPointF(v, plane);
}

QPointF portCenter(const Port &p, const Box &b)
{
    const double mid = (p.from + p.to) / 2.0;
    const double plane = portPlane(p, b);
    return portAlongY(p.side) ? QPointF(plane, mid) : QPointF(mid, plane);
}

QString fmt(double v)
{
    return TplDesignerModel::num(std::round(v * 1000.0) / 1000.0);
}

} // namespace

GeometryCanvas::GeometryCanvas(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton);
    setAcceptTouchEvents(true);
    setFlag(ItemHasContents, true);
    updateHint();
}

void GeometryCanvas::setModel(TplDesignerModel *m)
{
    if (m == m_model)
        return;
    if (m_model)
        disconnect(m_model, nullptr, this, nullptr);
    m_model = m;
    if (m_model) {
        connect(m_model, &TplDesignerModel::changed, this, [this] { update(); });
    }
    m_fitted = false;
    zoomToFit();
    emit modelChanged();
}

void GeometryCanvas::setTool(const QString &t)
{
    if (t == m_tool)
        return;
    cancelDrawing();
    m_tool = t;
    emit toolChanged();
    updateHint();
    update();
}

void GeometryCanvas::setGridVisible(bool on)
{
    if (on == m_grid)
        return;
    m_grid = on;
    emit viewChanged();
    update();
}

void GeometryCanvas::setSnapEnabled(bool on)
{
    if (on == m_snap)
        return;
    m_snap = on;
    emit viewChanged();
}

void GeometryCanvas::setColors(const QVariantMap &c)
{
    m_colors = c;
    emit viewChanged();
    update();
}

QColor GeometryCanvas::color(const char *key) const
{
    return m_colors.value(QString::fromLatin1(key)).value<QColor>();
}

QColor GeometryCanvas::materialColor(Material m) const
{
    switch (m) {
    case Material::Metal:      return color("metal");
    case Material::Magnetic:   return color("magnetic");
    case Material::Absorber:   return color("absorber");
    case Material::Dielectric: return color("dielectric");
    }
    return color("text");
}

QColor GeometryCanvas::shapeColor(const Shape &s) const
{
    // Ведущая величина плазмы — концентрация (geometryview.cpp, plasmaColor).
    if (s.plasma.any())
        return s.plasma.hasN ? color("plasma") : s.plasma.hasB ? color("plasmaB") : color("plasmaY");
    return materialColor(s.material);
}

// ------------------------------------------------------------------ вид

QPointF GeometryCanvas::toScreen(const QPointF &mm) const
{
    return QPointF(m_offset.x() + mm.x() * m_scale, m_offset.y() - mm.y() * m_scale);
}

QPointF GeometryCanvas::toMm(const QPointF &s) const
{
    return QPointF((s.x() - m_offset.x()) / m_scale, (m_offset.y() - s.y()) / m_scale);
}

double GeometryCanvas::gridStep() const
{
    return (m_model && m_model->project().delta > 0.0) ? m_model->project().delta : 1.0;
}

void GeometryCanvas::zoomToFit()
{
    if (!m_model || width() <= 1.0 || height() <= 1.0)
        return;
    const Project &p = m_model->project();
    double xLo = p.box.xMin, xHi = p.box.xMax, yLo = p.box.yMin, yHi = p.box.yMax;
    for (const Shape &s : p.shapes) {
        double a = 0, b = 0, c = 0, d = 0;
        s.bounds(&a, &b, &c, &d);
        xLo = std::min(xLo, a); xHi = std::max(xHi, b);
        yLo = std::min(yLo, c); yHi = std::max(yHi, d);
    }
    const double w = std::max(1e-6, xHi - xLo), h = std::max(1e-6, yHi - yLo);
    const double margin = 28.0;
    m_scale = std::max(1e-4, std::min((width() - 2 * margin) / w, (height() - 2 * margin) / h));
    const QPointF c((xLo + xHi) / 2.0, (yLo + yHi) / 2.0);
    m_offset = QPointF(width() / 2.0 - c.x() * m_scale, height() / 2.0 + c.y() * m_scale);
    m_viewSize = QSizeF(width(), height());
    m_fitted = true;
    emit viewChanged();
    update();
}

void GeometryCanvas::zoomBy(double factor, double x, double y)
{
    if (factor <= 0.0)
        return;
    const QPointF anchor(x, y);
    const QPointF mm = toMm(anchor);
    m_scale = std::clamp(m_scale * factor, 1e-3, 1e4);
    m_offset = QPointF(anchor.x() - mm.x() * m_scale, anchor.y() + mm.y() * m_scale);
    emit viewChanged();
    update();
}

void GeometryCanvas::panBy(double dx, double dy)
{
    m_offset += QPointF(dx, dy);
    update();
}

QPointF GeometryCanvas::viewCenterMm() const
{
    return toMm(QPointF(width() / 2.0, height() / 2.0));
}

double GeometryCanvas::viewSizeMm() const
{
    return std::min(width(), height()) / m_scale / 4.0;
}

void GeometryCanvas::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    // При перекладке экрана холст может на миг получить нулевой размер — такой
    // размер пропускаем и помним последний настоящий.
    if (newGeometry.width() < 2.0 || newGeometry.height() < 2.0)
        return;
    if (!m_fitted) {
        m_viewSize = newGeometry.size();
        zoomToFit();
        return;
    }
    if (newGeometry.size() != m_viewSize) {
        // Поворот телефона: масштаб тот же, а точка, бывшая в центре холста, остаётся
        // в центре — человек видит то же место, что и до поворота.
        const QPointF c((m_viewSize.width() / 2.0 - m_offset.x()) / m_scale,
                        (m_offset.y() - m_viewSize.height() / 2.0) / m_scale);
        m_offset = QPointF(newGeometry.width() / 2.0 - c.x() * m_scale,
                           newGeometry.height() / 2.0 + c.y() * m_scale);
        m_viewSize = newGeometry.size();
        update();
    }
}

// ------------------------------------------------------------------ привязка

GeometryCanvas::Snap GeometryCanvas::findSnap(const QPointF &raw, bool objects) const
{
    // Порядок как у оригинала (findSnap): вершина сильнее линии, линия сильнее сетки.
    Snap out;
    out.pos = raw;
    if (!m_model || !m_snap)
        return out;
    const Project &p = m_model->project();
    const double tol = m_scale > 1e-9 ? kSnapDp / m_scale : 0.0;
    const int exclude = (m_mode == Mode::Handle || m_mode == Mode::Move) ? m_model->selectedId() : -1;

    if (objects && tol > 0.0) {
        double best = tol;
        for (const Shape &s : p.shapes) {
            if (s.id == exclude)
                continue;
            for (const QPointF &v : keyPoints(s)) {
                const double d = QLineF(raw, v).length();
                if (d < best) {
                    best = d;
                    out.pos = v;
                    out.toObject = true;
                    out.what = (s.kind == ShapeKind::Circle && v == s.center)
                                   ? QCoreApplication::translate("TplDesigner", "центр «%1»").arg(s.name)
                                   : QCoreApplication::translate("TplDesigner", "вершина «%1»").arg(s.name);
                }
            }
        }
        const Box &b = p.box;
        for (const QPointF &v : {QPointF(b.xMin, b.yMin), QPointF(b.xMax, b.yMin),
                                 QPointF(b.xMax, b.yMax), QPointF(b.xMin, b.yMax)}) {
            const double d = QLineF(raw, v).length();
            if (d < best) {
                best = d;
                out.pos = v;
                out.toObject = true;
                out.what = QCoreApplication::translate("TplDesigner", "угол области");
            }
        }
        if (out.toObject)
            return out;

        best = tol;
        for (const Shape &s : p.shapes) {
            if (s.id == exclude)
                continue;
            for (const QLineF &seg : keySegments(s)) {
                const QPointF q = projectOnSegment(raw, seg);
                const double d = QLineF(raw, q).length();
                if (d < best) {
                    best = d;
                    out.pos = q;
                    out.toObject = true;
                    out.what = QCoreApplication::translate("TplDesigner", "на линии «%1»").arg(s.name);
                }
            }
            if (s.kind == ShapeKind::Circle && s.rMax > 0.0) {
                const QLineF r(s.center, raw);
                if (r.length() > 1e-9) {
                    const QPointF q = s.center + (raw - s.center) * (s.rMax / r.length());
                    const double d = QLineF(raw, q).length();
                    if (d < best) {
                        best = d;
                        out.pos = q;
                        out.toObject = true;
                        out.what = QCoreApplication::translate("TplDesigner", "на окружности «%1»").arg(s.name);
                    }
                }
            }
        }
        if (out.toObject) {
            // Точка на линии — тоже к сетке вдоль самой линии не тянем: она уже «на линии».
            return out;
        }
    }
    const double st = gridStep();
    out.pos = QPointF(std::round(raw.x() / st) * st, std::round(raw.y() / st) * st);
    return out;
}

// ------------------------------------------------------------------ попадания

int GeometryCanvas::pick(const QPointF &sp) const
{
    if (!m_model)
        return -1;
    const Project &p = m_model->project();
    const Box &b = p.box;
    for (const Port &port : p.ports) {
        if (QLineF(toScreen(portCenter(port, b)), sp).length() <= kPortMarkerDp + 6.0)
            return port.id;
    }
    for (const Port &port : p.ports) {
        if (distToSegment(sp, toScreen(portEnd(port, b, false)), toScreen(portEnd(port, b, true))) <= kPickDp)
            return port.id;
    }
    const QPointF mm = toMm(sp);
    const auto hit = [&](const Shape &s) {
        if (s.kind == ShapeKind::Rectangle) {
            for (int e = 0; e < 4; ++e) {
                if (distToSegment(sp, toScreen(s.edgeFrom(e)), toScreen(s.edgeTo(e))) <= kPickDp)
                    return true;
            }
            return QRectF(s.xMin, s.yMin, s.xMax - s.xMin, s.yMax - s.yMin).contains(mm);
        }
        if (s.kind == ShapeKind::Circle) {
            const double d = QLineF(s.center, mm).length();
            return d <= s.rMax || std::fabs(d - s.rMax) * m_scale <= kPickDp;
        }
        const int n = int(s.points.size());
        for (int k = 0; k + 1 < n; ++k) {
            if (distToSegment(sp, toScreen(s.points.at(k)), toScreen(s.points.at(k + 1))) <= kPickDp)
                return true;
        }
        if (s.kind == ShapeKind::Polygon && n >= 3) {
            if (distToSegment(sp, toScreen(s.points.last()), toScreen(s.points.first())) <= kPickDp)
                return true;
            QPainterPath path;
            path.moveTo(s.points.first());
            for (int i = 1; i < n; ++i)
                path.lineTo(s.points.at(i));
            path.closeSubpath();
            return path.contains(mm);
        }
        return false;
    };
    // 🔑 Выделенная фигура забирает касание у всех остальных (geometryview.cpp, pick).
    const int sel = m_model->selectedId();
    for (const Shape &s : p.shapes) {
        if (s.id == sel && hit(s))
            return s.id;
    }
    for (int i = int(p.shapes.size()) - 1; i >= 0; --i) {
        if (hit(p.shapes.at(i)))
            return p.shapes.at(i).id;
    }
    return -1;
}

QVector<QPair<QPointF, int>> GeometryCanvas::handles() const
{
    QVector<QPair<QPointF, int>> out;
    if (!m_model)
        return out;
    const Project &p = m_model->project();
    const int id = m_model->selectedId();
    for (const Port &port : p.ports) {
        if (port.id == id) {
            out << qMakePair(portEnd(port, p.box, false), kRolePortFrom)
                << qMakePair(portEnd(port, p.box, true), kRolePortTo);
            return out;
        }
    }
    for (const Shape &s : p.shapes) {
        if (s.id != id)
            continue;
        switch (s.kind) {
        case ShapeKind::Rectangle:
            out << qMakePair(QPointF(s.xMin, s.yMin), 0) << qMakePair(QPointF(s.xMax, s.yMin), 1)
                << qMakePair(QPointF(s.xMax, s.yMax), 2) << qMakePair(QPointF(s.xMin, s.yMax), 3);
            break;
        case ShapeKind::Circle: {
            const double mid = (s.fiMin + s.fiMax) / 2.0;
            out << qMakePair(s.center + QPointF(s.rMax * std::cos(mid), s.rMax * std::sin(mid)), 0);
            break;
        }
        case ShapeKind::Polygon:
        case ShapeKind::Polyline:
            for (int i = 0; i < s.points.size(); ++i)
                out << qMakePair(s.points.at(i), kRoleVertex + i);
            break;
        }
    }
    return out;
}

int GeometryCanvas::handleAt(const QPointF &sp) const
{
    int best = -1;
    double bestD = kHandleDp;
    for (const auto &h : handles()) {
        const double d = QLineF(toScreen(h.first), sp).length();
        if (d <= bestD) {
            bestD = d;
            best = h.second;
        }
    }
    return best;
}

// ------------------------------------------------------------------ правки

void GeometryCanvas::applyHandle(const QPointF &mm)
{
    Project &p = m_model->mutableProject();
    const int id = m_model->selectedId();
    if (m_handle == kRolePortFrom || m_handle == kRolePortTo) {
        for (Port &port : p.ports) {
            if (port.id != id)
                continue;
            const double v = portAlongY(port.side) ? mm.y() : mm.x();
            if (m_handle == kRolePortFrom)
                port.from = v;
            else
                port.to = v;
        }
        return;
    }
    for (Shape &s : p.shapes) {
        if (s.id != id)
            continue;
        if (m_handle >= kRoleVertex) {
            const int i = m_handle - kRoleVertex;
            if (i >= 0 && i < s.points.size())
                s.points[i] = mm;
            return;
        }
        if (s.kind == ShapeKind::Rectangle) {
            switch (m_handle) {
            case 0: s.xMin = mm.x(); s.yMin = mm.y(); break;
            case 1: s.xMax = mm.x(); s.yMin = mm.y(); break;
            case 2: s.xMax = mm.x(); s.yMax = mm.y(); break;
            case 3: s.xMin = mm.x(); s.yMax = mm.y(); break;
            default: break;
            }
        } else if (s.kind == ShapeKind::Circle) {
            s.rMax = std::max(QLineF(s.center, mm).length(), s.rMin + gridStep());
        }
        return;
    }
}

void GeometryCanvas::applyMove(const QPointF &delta)
{
    Project &p = m_model->mutableProject();
    const int id = m_model->selectedId();
    if (m_origIsPort) {
        for (Port &port : p.ports) {
            if (port.id != id)
                continue;
            // Вход двигается свободно ВНУТРИ области (geometryview.cpp, moveSelection):
            // вдоль апертуры — from/to, поперёк — плоскость; направление держим в
            // согласии с половиной коробки, иначе ядро молча развернёт волну.
            port = m_origPort;
            const bool alongY = portAlongY(port.side);
            port.from += alongY ? delta.y() : delta.x();
            port.to += alongY ? delta.y() : delta.x();
            const Box &b = p.box;
            const double lo = alongY ? b.xMin : b.yMin;
            const double hi = alongY ? b.xMax : b.yMax;
            const double moved = portPlane(m_origPort, b) + (alongY ? delta.x() : delta.y());
            port.plane = std::clamp(moved, lo, hi);
            port.side = directionForPlane(alongY, *port.plane, b);
        }
        return;
    }
    for (Shape &s : p.shapes) {
        if (s.id != id)
            continue;
        s = m_origShape;
        s.xMin += delta.x(); s.xMax += delta.x();
        s.yMin += delta.y(); s.yMax += delta.y();
        s.center += delta;
        for (QPointF &pt : s.points)
            pt += delta;
    }
}

QString GeometryCanvas::freshName(const QString &base, bool port) const
{
    QStringList names;
    if (m_model) {
        if (port) {
            for (const Port &p : m_model->project().ports)
                names << p.name;
        } else {
            for (const Shape &s : m_model->project().shapes)
                names << s.name;
        }
    }
    const QRegularExpression re(QStringLiteral("^%1 (\\d+)$").arg(QRegularExpression::escape(base)));
    int best = 0;
    for (const QString &n : names) {
        const QRegularExpressionMatch m = re.match(n);
        if (m.hasMatch())
            best = std::max(best, m.captured(1).toInt());
    }
    return QStringLiteral("%1 %2").arg(base).arg(best + 1);
}

/// Фигура по протяжке (geometryview.cpp, mouseReleaseEvent): нулевую не создаём.
void GeometryCanvas::createFromDrag(const QPointF &mm)
{
    const double step = gridStep();
    Project &pr = m_model->mutableProject();
    if (m_tool == QLatin1String("rect")) {
        const QRectF r = QRectF(m_startMm, mm).normalized();
        if (r.width() < step || r.height() < step)
            return;
        Shape s;
        s.id = pr.nextFreeId();
        s.name = freshName(QCoreApplication::translate("TplDesigner", "Прямоугольник"), false);
        s.kind = ShapeKind::Rectangle;
        s.material = Material::Metal;
        s.xMin = r.left(); s.xMax = r.right();
        s.yMin = r.top();  s.yMax = r.bottom();
        m_model->beginEdit();
        m_model->mutableProject().shapes << s;
        m_model->select(s.id);
        m_model->endEdit();
    } else if (m_tool == QLatin1String("circle")) {
        const double r = QLineF(m_startMm, mm).length();
        if (r < step)
            return;
        Shape s;
        s.id = pr.nextFreeId();
        s.name = freshName(QCoreApplication::translate("TplDesigner", "Круг"), false);
        s.kind = ShapeKind::Circle;
        s.material = Material::Metal;
        s.center = m_startMm;
        s.rMin = 0.0;
        s.rMax = r;
        s.fiMin = 0.0;
        s.fiMax = 2 * M_PI;
        m_model->beginEdit();
        m_model->mutableProject().shapes << s;
        m_model->select(s.id);
        m_model->endEdit();
    } else if (m_tool == QLatin1String("port")) {
        // Сторона — по НАПРАВЛЕНИЮ протяжки (sideForDrag), плоскость — поперечная
        // координата, направление — половина коробки (directionForPlane).
        const Box &box = pr.box;
        const double dx = std::fabs(mm.x() - m_startMm.x());
        const double dy = std::fabs(mm.y() - m_startMm.y());
        const bool alongY = dy >= dx;
        const double a = std::min(alongY ? m_startMm.y() : m_startMm.x(), alongY ? mm.y() : mm.x());
        const double b = std::max(alongY ? m_startMm.y() : m_startMm.x(), alongY ? mm.y() : mm.x());
        if (b - a < step)
            return;
        Port p;
        p.id = pr.nextFreeId();
        p.name = freshName(QCoreApplication::translate("TplDesigner", "Порт"), true);
        const double plane = std::clamp(alongY ? m_startMm.x() : m_startMm.y(),
                                        alongY ? box.xMin : box.yMin, alongY ? box.xMax : box.yMax);
        p.plane = plane;
        p.side = directionForPlane(alongY, plane, box);
        p.from = a;
        p.to = b;
        p.active = pr.activePorts().isEmpty();
        m_model->beginEdit();
        m_model->mutableProject().ports << p;
        m_model->select(p.id);
        m_model->endEdit();
    }
}

void GeometryCanvas::addChainPoint(const QPointF &mm)
{
    if (!m_chain.isEmpty() && QLineF(m_chain.last(), mm).length() < 1e-9)
        return;
    m_chain << mm;
    // Дуга: два конца и выгиб — третья точка завершает (Tool::ArcLine оригинала).
    if (m_tool == QLatin1String("arc") && m_chain.size() >= 3) {
        finishChain();
        return;
    }
    updateHint();
    update();
}

void GeometryCanvas::finishChain()
{
    if (!m_model || m_chain.isEmpty())
        return;
    Shape s;
    s.material = Material::Metal;
    if (m_tool == QLatin1String("arc")) {
        if (m_chain.size() < 3) {
            cancelDrawing();
            return;
        }
        // Разбивка с шагом сетки DELTA — мельче ядру нечего считать.
        const QVector<QPointF> pts = arcThroughPoints(m_chain.at(0), m_chain.at(2), m_chain.at(1),
                                                      gridStep());
        s.kind = ShapeKind::Polyline;
        s.name = freshName(QCoreApplication::translate("TplDesigner", "Дуга"), false);
        s.points = pts;
    } else {
        const bool closed = (m_tool == QLatin1String("contour"));
        if (m_chain.size() < (closed ? 3 : 2)) {
            cancelDrawing();
            return;
        }
        s.kind = closed ? ShapeKind::Polygon : ShapeKind::Polyline;
        s.name = freshName(closed ? QCoreApplication::translate("TplDesigner", "Контур") : QCoreApplication::translate("TplDesigner", "Линия"), false);
        s.points = m_chain;
    }
    m_chain.clear();
    m_model->beginEdit();
    s.id = m_model->project().nextFreeId();
    m_model->mutableProject().shapes << s;
    m_model->select(s.id);
    m_model->endEdit();
    updateHint();
    update();
}

void GeometryCanvas::cancelDrawing()
{
    m_chain.clear();
    if (m_mode == Mode::Create)
        m_mode = Mode::None;
    updateHint();
    update();
}

// ------------------------------------------------------------------ касания

void GeometryCanvas::pointerPress(const QPointF &pos)
{
    if (!m_model)
        return;
    m_pressScreen = pos;
    m_lastScreen = pos;
    m_edited = false;
    const Snap sn = findSnap(toMm(pos), true);
    m_lastSnap = sn;
    m_cursorMm = sn.pos;
    m_hasCursor = true;

    if (m_tool == QLatin1String("select")) {
        const int h = handleAt(pos);
        if (h >= 0) {
            m_mode = Mode::Handle;
            m_handle = h;
            return;
        }
        const int id = pick(pos);
        if (id >= 0) {
            const bool already = (id == m_model->selectedId());
            m_model->select(id);
            m_mode = already ? Mode::Move : Mode::Tap;
            m_startMm = toMm(pos);
            m_origIsPort = false;
            for (const Port &p : m_model->project().ports) {
                if (p.id == id) {
                    m_origPort = p;
                    m_origIsPort = true;
                }
            }
            for (const Shape &s : m_model->project().shapes) {
                if (s.id == id)
                    m_origShape = s;
            }
            return;
        }
        m_mode = Mode::Pan;
        return;
    }
    if (m_tool == QLatin1String("line") || m_tool == QLatin1String("contour")
        || m_tool == QLatin1String("arc")) {
        m_mode = Mode::Tap; // точка ставится по отпусканию; протяжка — сдвиг холста
        update();
        return;
    }
    m_mode = Mode::Create;
    m_startMm = sn.pos;
    setCursorText(QCoreApplication::translate("TplDesigner", "%1; %2 мм%3").arg(fmt(sn.pos.x()), fmt(sn.pos.y()),
                                                     sn.toObject ? QStringLiteral(" — ") + sn.what
                                                                 : QString()));
    update();
}

void GeometryCanvas::pointerMove(const QPointF &pos)
{
    if (!m_model)
        return;
    const QPointF d = pos - m_lastScreen;
    m_lastScreen = pos;
    const bool slop = QLineF(pos, m_pressScreen).length() > kTapSlopDp;

    switch (m_mode) {
    case Mode::Pan:
        panBy(d.x(), d.y());
        return;
    case Mode::Tap:
        if (!slop)
            return;
        if (m_tool == QLatin1String("select") && m_model->selectedId() >= 0) {
            // Тянут только что выбранную фигуру — сразу двигаем её.
            m_mode = Mode::Move;
        } else {
            m_mode = Mode::Pan;
            panBy(pos.x() - m_pressScreen.x(), pos.y() - m_pressScreen.y());
            return;
        }
        Q_FALLTHROUGH();
    case Mode::Move: {
        if (!slop && !m_edited)
            return;
        // Целиком фигуру ведём только по сетке — без прыжков на чужие вершины
        // (geometryview.cpp, toScene при m_dragging).
        const double st = gridStep();
        const QPointF raw = toMm(pos) - m_startMm;
        const QPointF delta = m_snap ? QPointF(std::round(raw.x() / st) * st,
                                               std::round(raw.y() / st) * st)
                                     : raw;
        if (!m_edited) {
            m_model->beginEdit();
            m_edited = true;
        }
        applyMove(delta);
        m_model->touch();
        setCursorText(QCoreApplication::translate("TplDesigner", "сдвиг %1; %2 мм").arg(fmt(delta.x()), fmt(delta.y())));
        return;
    }
    case Mode::Handle: {
        const Snap sn = findSnap(toMm(pos), true);
        m_lastSnap = sn;
        m_cursorMm = sn.pos;
        if (!m_edited) {
            m_model->beginEdit();
            m_edited = true;
        }
        applyHandle(sn.pos);
        m_model->touch();
        setCursorText(QCoreApplication::translate("TplDesigner", "%1; %2 мм%3").arg(fmt(sn.pos.x()), fmt(sn.pos.y()),
                                                         sn.toObject ? QStringLiteral(" — ") + sn.what
                                                                     : QString()));
        return;
    }
    case Mode::Create: {
        const Snap sn = findSnap(toMm(pos), true);
        m_lastSnap = sn;
        m_cursorMm = sn.pos;
        QString size;
        if (m_tool == QLatin1String("rect")) {
            size = QCoreApplication::translate("TplDesigner", "  %1 × %2 мм").arg(fmt(std::fabs(sn.pos.x() - m_startMm.x())),
                                                       fmt(std::fabs(sn.pos.y() - m_startMm.y())));
        } else if (m_tool == QLatin1String("circle")) {
            size = QCoreApplication::translate("TplDesigner", "  R = %1 мм").arg(fmt(QLineF(m_startMm, sn.pos).length()));
        }
        setCursorText(QCoreApplication::translate("TplDesigner", "%1; %2 мм%3%4").arg(fmt(sn.pos.x()), fmt(sn.pos.y()), size,
                                                           sn.toObject ? QStringLiteral(" — ") + sn.what
                                                                       : QString()));
        update();
        return;
    }
    case Mode::None:
        return;
    }
}

void GeometryCanvas::pointerRelease(const QPointF &pos)
{
    if (!m_model) {
        m_mode = Mode::None;
        return;
    }
    const Mode mode = m_mode;
    m_mode = Mode::None;
    switch (mode) {
    case Mode::Pan:
        if (QLineF(pos, m_pressScreen).length() <= kTapSlopDp && m_tool == QLatin1String("select"))
            m_model->select(-1); // касание мимо всего — снять выделение
        break;
    case Mode::Tap:
        if (m_tool == QLatin1String("select")) {
            // Касание по уже выделенному — свойства; по новому — просто выделение.
        } else {
            const Snap sn = findSnap(toMm(pos), true);
            m_lastSnap = sn;
            m_cursorMm = sn.pos;
            addChainPoint(sn.pos);
            setCursorText(QCoreApplication::translate("TplDesigner", "%1; %2 мм%3").arg(fmt(sn.pos.x()), fmt(sn.pos.y()),
                                                             sn.toObject ? QStringLiteral(" — ") + sn.what
                                                                         : QString()));
        }
        break;
    case Mode::Move:
        if (m_edited) {
            m_model->endEdit();
        } else {
            emit editRequested(m_model->selectedId());
        }
        break;
    case Mode::Handle:
        if (m_edited) {
            for (Shape &s : m_model->mutableProject().shapes) {
                if (s.id == m_model->selectedId() && s.kind == ShapeKind::Rectangle)
                    normalizeRect(&s);
            }
            m_model->endEdit();
        }
        break;
    case Mode::Create: {
        const Snap sn = findSnap(toMm(pos), true);
        createFromDrag(sn.pos);
        break;
    }
    case Mode::None:
        break;
    }
    m_edited = false;
    m_handle = -1;
    updateHint();
    update();
}

void GeometryCanvas::pointerCancel()
{
    // Палец ушёл в жест двумя пальцами или касание отобрали: начатая правка
    // завершается как есть, начатая фигура бросается.
    if ((m_mode == Mode::Move || m_mode == Mode::Handle) && m_edited && m_model)
        m_model->endEdit();
    m_mode = Mode::None;
    m_edited = false;
    m_handle = -1;
    update();
}

void GeometryCanvas::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton) {
        m_mode = Mode::Pan;
        m_pressScreen = m_lastScreen = e->position();
        e->accept();
        return;
    }
    pointerPress(e->position());
    e->accept();
}

void GeometryCanvas::mouseMoveEvent(QMouseEvent *e)
{
    pointerMove(e->position());
    e->accept();
}

void GeometryCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    pointerRelease(e->position());
    e->accept();
}

void GeometryCanvas::mouseUngrabEvent()
{
    if (m_mode != Mode::None)
        pointerCancel();
}

void GeometryCanvas::wheelEvent(QWheelEvent *e)
{
    const double steps = e->angleDelta().y() / 120.0;
    zoomBy(std::pow(1.2, steps), e->position().x(), e->position().y());
    e->accept();
}

void GeometryCanvas::touchEvent(QTouchEvent *e)
{
    const QList<QEventPoint> &pts = e->points();
    int down = 0;
    for (const QEventPoint &p : pts) {
        if (p.state() != QEventPoint::Released)
            ++down;
    }

    if (down >= 2 || m_pinching) {
        // Два пальца: щипок — масштаб вокруг середины, общий сдвиг — панорама.
        if (pts.size() >= 2 && down >= 2) {
            const QPointF a = pts.at(0).position(), b = pts.at(1).position();
            const QPointF c = (a + b) / 2.0;
            const double dist = QLineF(a, b).length();
            if (!m_pinching) {
                if (m_mode != Mode::None)
                    pointerCancel();
                if (m_mode == Mode::Create)
                    m_mode = Mode::None;
                m_pinching = true;
            } else if (m_pinchDist > 1.0) {
                panBy(c.x() - m_pinchCenter.x(), c.y() - m_pinchCenter.y());
                zoomBy(dist / m_pinchDist, c.x(), c.y());
            }
            m_pinchDist = dist;
            m_pinchCenter = c;
        }
        if (down == 0 || e->type() == QEvent::TouchEnd || e->type() == QEvent::TouchCancel) {
            m_pinching = false;
            m_pinchDist = 0.0;
        }
        e->accept();
        return;
    }

    if (pts.isEmpty()) {
        e->accept();
        return;
    }
    const QEventPoint &p = pts.first();
    switch (e->type()) {
    case QEvent::TouchBegin:
        pointerPress(p.position());
        break;
    case QEvent::TouchUpdate:
        if (p.state() == QEventPoint::Released)
            pointerRelease(p.position());
        else
            pointerMove(p.position());
        break;
    case QEvent::TouchEnd:
        pointerRelease(p.position());
        break;
    case QEvent::TouchCancel:
        pointerCancel();
        break;
    default:
        break;
    }
    e->accept();
}

void GeometryCanvas::touchUngrabEvent()
{
    m_pinching = false;
    m_pinchDist = 0.0;
    if (m_mode != Mode::None)
        pointerCancel();
}

// ------------------------------------------------------------------ подсказки

void GeometryCanvas::setCursorText(const QString &t)
{
    if (t == m_cursorText)
        return;
    m_cursorText = t;
    emit hintChanged();
}

void GeometryCanvas::updateHint()
{
    QString h;
    if (m_tool == QLatin1String("select")) {
        h = QCoreApplication::translate("TplDesigner", "Касание — выбрать, протяжка выбранной — сдвиг, за точку — размер. "
                           "Два пальца — масштаб.");
    } else if (m_tool == QLatin1String("rect")) {
        h = QCoreApplication::translate("TplDesigner", "Протяните пальцем по диагонали прямоугольника.");
    } else if (m_tool == QLatin1String("circle")) {
        h = QCoreApplication::translate("TplDesigner", "Поставьте палец в центр и протяните радиус.");
    } else if (m_tool == QLatin1String("line") || m_tool == QLatin1String("contour")) {
        h = m_chain.isEmpty()
                ? QCoreApplication::translate("TplDesigner", "Касайтесь — точка за точкой. Затем «Готово».")
                : QCoreApplication::translate("TplDesigner", "Точек: %1. Следующая — касанием, «Готово» — закончить.")
                      .arg(m_chain.size());
    } else if (m_tool == QLatin1String("arc")) {
        h = m_chain.isEmpty() ? QCoreApplication::translate("TplDesigner", "Дуга: коснитесь первого конца.")
            : m_chain.size() == 1 ? QCoreApplication::translate("TplDesigner", "Дуга: коснитесь второго конца.")
                                  : QCoreApplication::translate("TplDesigner", "Дуга: коснитесь точки, через которую идёт выгиб.");
    } else if (m_tool == QLatin1String("port")) {
        h = QCoreApplication::translate("TplDesigner", "Протяните вдоль границы области — это апертура входа.");
    }
    if (h != m_hint) {
        m_hint = h;
        emit hintChanged();
    } else {
        emit hintChanged(); // chainCount мог измениться
    }
}

// ------------------------------------------------------------------ рисование

void GeometryCanvas::drawShape(QPainter *p, const Shape &s, bool selected) const
{
    const QColor c = shapeColor(s);
    // Контурные материалы ядро рисует периметром, заливку — площадью (drawShape оригинала).
    const bool contour = s.isContour();
    QPen pen(contour || selected ? c : QColor(c.red(), c.green(), c.blue(), 150));
    pen.setWidthF(selected ? 3.0 : (contour ? 2.0 : 1.2));
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    p->setPen(pen);
    p->setBrush(contour ? QBrush(Qt::NoBrush) : QBrush(QColor(c.red(), c.green(), c.blue(), 62)));

    QPainterPath path;
    switch (s.kind) {
    case ShapeKind::Rectangle:
        path.addRect(QRectF(toScreen(QPointF(s.xMin, s.yMax)), toScreen(QPointF(s.xMax, s.yMin))));
        break;
    case ShapeKind::Circle: {
        const bool full = std::fabs(s.fiMax - s.fiMin) >= 2 * M_PI - 1e-6;
        if (full) {
            path.addEllipse(toScreen(s.center), s.rMax * m_scale, s.rMax * m_scale);
            if (s.rMin > 0.0)
                path.addEllipse(toScreen(s.center), s.rMin * m_scale, s.rMin * m_scale);
            break;
        }
        const QVector<QPointF> outer = arcPoints(s.center, s.rMax, s.fiMin, s.fiMax);
        path.moveTo(toScreen(outer.first()));
        for (int i = 1; i < outer.size(); ++i)
            path.lineTo(toScreen(outer.at(i)));
        if (s.rMin > 0.0) {
            for (const QPointF &pt : arcPoints(s.center, s.rMin, s.fiMax, s.fiMin))
                path.lineTo(toScreen(pt));
        } else {
            path.lineTo(toScreen(s.center));
        }
        path.closeSubpath();
        break;
    }
    case ShapeKind::Polygon:
    case ShapeKind::Polyline:
        if (!s.points.isEmpty()) {
            path.moveTo(toScreen(s.points.first()));
            for (int i = 1; i < s.points.size(); ++i)
                path.lineTo(toScreen(s.points.at(i)));
            if (s.kind == ShapeKind::Polygon)
                path.closeSubpath();
        }
        break;
    }

    if (s.kind == ShapeKind::Rectangle && contour) {
        // Стороны могут отличаться материалом или отсутствовать — каждая отдельно,
        // снятая — пунктиром: видно, что там дыра.
        for (int e = 0; e < 4; ++e) {
            QPen ep(s.edges[size_t(e)].present ? materialColor(s.edgeMaterial(e)) : color("box"));
            ep.setWidthF(selected ? 3.0 : 2.0);
            ep.setCapStyle(Qt::RoundCap);
            if (!s.edges[size_t(e)].present)
                ep.setStyle(Qt::DotLine);
            p->setPen(ep);
            p->drawLine(toScreen(s.edgeFrom(e)), toScreen(s.edgeTo(e)));
        }
    } else {
        p->drawPath(path);
    }

    // Подмагничивание и потери — штриховкой поверх заливки (как у оригинала).
    if (s.plasma.hasB || s.plasma.hasY) {
        const QRectF br = path.boundingRect();
        p->save();
        p->setClipPath(path);
        const double step = std::max(6.0, (br.width() + br.height()) / 14.0);
        const auto hatch = [&](const QColor &col, bool up) {
            QPen hp(col);
            hp.setWidthF(1.2);
            p->setPen(hp);
            for (double k = -br.height(); k <= br.width(); k += step) {
                if (up)
                    p->drawLine(QPointF(br.left() + k, br.bottom()), QPointF(br.left() + k + br.height(), br.top()));
                else
                    p->drawLine(QPointF(br.left() + k, br.top()), QPointF(br.left() + k + br.height(), br.bottom()));
            }
        };
        if (s.plasma.hasB)
            hatch(color("plasmaB"), true);
        if (s.plasma.hasY)
            hatch(color("plasmaY"), false);
        p->restore();
    }
}

void GeometryCanvas::drawPort(QPainter *p, const Port &port, bool selected) const
{
    const Box &b = m_model->project().box;
    const QColor col = port.active ? color("active") : color("passive");
    QPen ap(col);
    ap.setWidthF(selected ? 6.5 : 4.0);
    ap.setCapStyle(Qt::RoundCap);
    p->setPen(ap);
    p->drawLine(toScreen(portEnd(port, b, false)), toScreen(portEnd(port, b, true)));

    const QPointF c = toScreen(portCenter(port, b));
    // Стрелка направления падающей волны: направление ядро выводит по половине коробки.
    if (port.active) {
        const double len = 28.0;
        QPointF dir;
        switch (port.side) {
        case PortSide::Left:   dir = QPointF(len, 0); break;
        case PortSide::Right:  dir = QPointF(-len, 0); break;
        case PortSide::Bottom: dir = QPointF(0, -len); break;
        case PortSide::Top:    dir = QPointF(0, len); break;
        }
        QPen arrow(col);
        arrow.setWidthF(2.0);
        p->setPen(arrow);
        p->drawLine(c, c + dir);
        const QPointF n(-dir.y() * 0.3, dir.x() * 0.3);
        p->drawLine(c + dir, c + dir * 0.6 + n);
        p->drawLine(c + dir, c + dir * 0.6 - n);
    }
    const double r = kPortMarkerDp;
    p->setBrush(col);
    p->setPen(QPen(color("bg"), 2.0));
    p->drawEllipse(c, r, r);
    QPen mark(color("bg"));
    mark.setWidthF(2.2);
    mark.setCapStyle(Qt::RoundCap);
    p->setPen(mark);
    if (port.active) {
        p->drawLine(c + QPointF(-r * 0.42, 0), c + QPointF(-r * 0.1, r * 0.36));
        p->drawLine(c + QPointF(-r * 0.1, r * 0.36), c + QPointF(r * 0.45, -r * 0.38));
    } else {
        p->drawLine(c + QPointF(-r * 0.36, -r * 0.36), c + QPointF(r * 0.36, r * 0.36));
        p->drawLine(c + QPointF(-r * 0.36, r * 0.36), c + QPointF(r * 0.36, -r * 0.36));
    }
}

void GeometryCanvas::paint(QPainter *p)
{
    p->setRenderHint(QPainter::Antialiasing, true);
    p->fillRect(QRectF(0, 0, width(), height()), color("bg"));
    if (!m_model)
        return;
    const Project &pr = m_model->project();
    const Box &b = pr.box;

    if (m_grid && m_scale > 0.0) {
        const double step = niceGridStep(pr.delta, m_scale);
        QPen gp(color("grid"));
        gp.setWidthF(1.0);
        p->setPen(gp);
        const QPointF lo = toMm(QPointF(0, height()));
        const QPointF hi = toMm(QPointF(width(), 0));
        for (double x = std::floor(lo.x() / step) * step; x <= hi.x(); x += step) {
            const double sx = toScreen(QPointF(x, 0)).x();
            p->drawLine(QPointF(sx, 0), QPointF(sx, height()));
        }
        for (double y = std::floor(lo.y() / step) * step; y <= hi.y(); y += step) {
            const double sy = toScreen(QPointF(0, y)).y();
            p->drawLine(QPointF(0, sy), QPointF(width(), sy));
        }
    }

    // Счётная область — за её пределами поля не считаются.
    QColor fill = color("boxFill");
    fill.setAlpha(120);
    QPen boxPen(color("box"));
    boxPen.setWidthF(1.5);
    boxPen.setStyle(Qt::DashLine);
    p->setPen(boxPen);
    p->setBrush(fill);
    p->drawRect(QRectF(toScreen(QPointF(b.xMin, b.yMax)), toScreen(QPointF(b.xMax, b.yMin))));

    const int sel = m_model->selectedId();
    // Заливки снизу, стенки поверх (решение оригинала 2026-10-02).
    for (const Shape &s : pr.shapes) {
        if (!s.isContour())
            drawShape(p, s, s.id == sel);
    }
    for (const Shape &s : pr.shapes) {
        if (s.isContour())
            drawShape(p, s, s.id == sel);
    }
    for (const Port &port : pr.ports)
        drawPort(p, port, port.id == sel);

    // Выделение и ручки.
    const QColor accent = color("select");
    for (const auto &h : handles()) {
        const QPointF c = toScreen(h.first);
        p->setPen(QPen(color("bg"), 2.0));
        p->setBrush(accent);
        p->drawEllipse(c, 7.0, 7.0);
    }

    // Начатая фигура.
    QPen pv(accent);
    pv.setWidthF(2.0);
    pv.setStyle(Qt::DashLine);
    p->setPen(pv);
    p->setBrush(Qt::NoBrush);
    if (m_mode == Mode::Create && m_hasCursor) {
        if (m_tool == QLatin1String("rect")) {
            p->drawRect(QRectF(toScreen(m_startMm), toScreen(m_cursorMm)).normalized());
        } else if (m_tool == QLatin1String("circle")) {
            const double r = QLineF(m_startMm, m_cursorMm).length() * m_scale;
            p->drawEllipse(toScreen(m_startMm), r, r);
        } else if (m_tool == QLatin1String("port")) {
            const double dx = std::fabs(m_cursorMm.x() - m_startMm.x());
            const double dy = std::fabs(m_cursorMm.y() - m_startMm.y());
            const QPointF end = dy >= dx ? QPointF(m_startMm.x(), m_cursorMm.y())
                                         : QPointF(m_cursorMm.x(), m_startMm.y());
            QPen pp(color("active"));
            pp.setWidthF(4.0);
            pp.setCapStyle(Qt::RoundCap);
            p->setPen(pp);
            p->drawLine(toScreen(m_startMm), toScreen(end));
        }
    }
    if (!m_chain.isEmpty()) {
        p->setPen(pv);
        if (m_tool == QLatin1String("arc") && m_chain.size() == 2) {
            p->drawLine(toScreen(m_chain.at(0)), toScreen(m_chain.at(1)));
        } else {
            for (int i = 0; i + 1 < m_chain.size(); ++i)
                p->drawLine(toScreen(m_chain.at(i)), toScreen(m_chain.at(i + 1)));
            if (m_tool == QLatin1String("contour") && m_chain.size() >= 3)
                p->drawLine(toScreen(m_chain.last()), toScreen(m_chain.first()));
        }
        p->setPen(QPen(color("bg"), 2.0));
        p->setBrush(accent);
        for (const QPointF &pt : m_chain)
            p->drawEllipse(toScreen(pt), 5.0, 5.0);
    }

    // Точка привязки: к объекту — кольцом.
    if (m_mode != Mode::None && m_mode != Mode::Pan && m_lastSnap.toObject) {
        QPen sp(color("snap"));
        sp.setWidthF(2.0);
        p->setPen(sp);
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(toScreen(m_lastSnap.pos), 10.0, 10.0);
    }
}
