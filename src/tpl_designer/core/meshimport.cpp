#include "meshimport.h"

#include "convexsplit.h"
#include "cp1251.h"
#include "emitter.h"
#include "sigfile.h"

#include <algorithm>
#include <cmath>

namespace tpl {

namespace {

/// Имя из CAD может нести знаки, которых в CP1251 нет (греческие, иероглифы, ²). В файл
/// задания имя уходит комментарием, и такой знак запретил бы запись — заменяем его
/// вопросом сразу, при вставке, чтобы человек видел на холсте то же, что в файле.
QString cp1251Safe(const QString &s)
{
    return fromCp1251(toCp1251(s)).trimmed();
}

} // namespace

QPointF placePoint(const QPointF &uv, const ImportPlacement &pl)
{
    QPointF p = uv;
    if (pl.mirror)
        p.setX(-p.x());
    const int k = ((pl.rot90 % 4) + 4) % 4;
    for (int i = 0; i < k; ++i)
        p = QPointF(-p.y(), p.x());
    return p + pl.shift;
}

QPointF centeringShift(const SliceResult &r, ImportPlacement pl, const Box &box)
{
    pl.shift = QPointF();
    double x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    bool any = false;
    for (const SliceBody &b : r.bodies)
        for (const SliceLoop &l : b.loops)
            for (const QPointF &q : l.pts) {
                const QPointF p = placePoint(q, pl);
                if (!any) {
                    x0 = x1 = p.x();
                    y0 = y1 = p.y();
                    any = true;
                }
                x0 = std::min(x0, p.x());
                x1 = std::max(x1, p.x());
                y0 = std::min(y0, p.y());
                y1 = std::max(y1, p.y());
            }
    if (!any)
        return {};
    return QPointF((box.xMin + box.xMax) / 2 - (x0 + x1) / 2, (box.yMin + box.yMax) / 2 - (y0 + y1) / 2);
}

ImportShapes buildImportShapes(const Mesh &mesh, const SliceResult &slice,
                               const QVector<BodyChoice> &choices, const ImportPlacement &pl,
                               int firstId)
{
    ImportShapes out;
    int id = firstId;
    for (const SliceBody &sb : slice.bodies) {
        if (sb.body < 0 || sb.body >= choices.size())
            continue;
        const BodyChoice &c = choices.at(sb.body);
        if (!c.include)
            continue;
        QString name = cp1251Safe(mesh.bodies.at(sb.body).name);
        if (name.isEmpty())
            name = QStringLiteral("тело %1").arg(sb.body + 1);

        const auto placed = [&](const QVector<QPointF> &pts) {
            QVector<QPointF> q;
            q.reserve(pts.size());
            for (const QPointF &p : pts)
                q << placePoint(p, pl);
            return q;
        };

        if (c.material == Material::Dielectric) {
            QVector<QVector<QPointF>> loops;
            int open = 0;
            for (const SliceLoop &l : sb.loops) {
                if (l.closed && l.pts.size() >= 3)
                    loops << placed(l.pts);
                else
                    ++open;
            }
            if (open > 0) {
                out.notes << QStringLiteral("«%1»: %2 не замкнулось — залить такое нельзя, они не "
                                            "вставлены. Модель, видимо, «не герметична»: в "
                                            "CAD проверьте, что тело твёрдое")
                                 .arg(name, plural(open, QStringLiteral("контур"),
                                                   QStringLiteral("контура"),
                                                   QStringLiteral("контуров")));
            }
            const QVector<QVector<QPointF>> pieces = convexPieces(loops);
            for (int i = 0; i < pieces.size(); ++i) {
                Shape s;
                s.id = id++;
                s.kind = ShapeKind::Polygon;
                s.material = Material::Dielectric;
                s.eps = c.eps;
                s.points = padTriangle(pieces.at(i));
                s.name = pieces.size() == 1
                             ? name
                             : QStringLiteral("%1 — часть %2 из %3").arg(name).arg(i + 1).arg(pieces.size());
                out.shapes << s;
            }
            out.dielectricPieces += int(pieces.size());
            if (pieces.size() > 1) {
                out.notes << QStringLiteral("«%1» невыпуклое: ядро заливает только выпуклый "
                                            "многоугольник, поэтому сечение разбито на %2 "
                                            "с той же eps")
                                 .arg(name, plural(int(pieces.size()), QStringLiteral("выпуклый кусок"),
                                                   QStringLiteral("выпуклых куска"),
                                                   QStringLiteral("выпуклых кусков")));
            }
            continue;
        }

        int k = 0;
        for (const SliceLoop &l : sb.loops) {
            Shape s;
            s.material = c.material;
            if (l.closed && l.pts.size() >= 3) {
                s.kind = ShapeKind::Polygon;
            } else if (l.pts.size() >= 2) {
                s.kind = ShapeKind::Polyline;
            } else {
                continue;
            }
            s.id = id++;
            s.points = placed(l.pts);
            ++k;
            s.name = sb.loops.size() == 1 ? name : QStringLiteral("%1 — контур %2").arg(name).arg(k);
            out.shapes << s;
        }
    }
    if (slice.openLoops > 0) {
        out.notes << QStringLiteral("контуров, которые не замкнулись: %1. Металл и стенки "
                                    "вставлены ломаными — проверьте разрывы на холсте")
                         .arg(slice.openLoops);
    }
    return out;
}

QString sliceProvenance(const QString &fileName, SlicePlane plane, double offset)
{
    return cp1251Safe(QStringLiteral("Геометрия: срез модели %1 плоскостью %2 при %3 = %4 мм")
                          .arg(fileName, slicePlaneName(plane), normalAxisName(plane),
                               formatCompact(offset)));
}

} // namespace tpl
