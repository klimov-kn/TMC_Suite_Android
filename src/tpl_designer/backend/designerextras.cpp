/// @file designerextras.cpp
/// @brief TPL Designer на Android: «Что считать», память телефона, выбор нескольких фигур, образцы.
///
/// Части `TplDesignerModel`, которые на настольной версии живут в отдельных окнах (`RunPlanPage`,
/// `MemoryDonut`, `SamplesPage`). Правила перенесены без изменений; разница только в подаче: страницы
/// QML получают простые карты. Подписи — через `tpl::tr`, как в ядре генератора.
/// @see https://www.tamic.ru/docs/code/tpl-designer.html#gui-run
/// @see https://www.tamic.ru/docs/platforms/android.html#designer

#include "designermodel.h"

#include "emitter.h"
#include "lang.h"
#include "meshimport.h"
#include "meshio.h"
#include "physics.h"
#include "ramestimate.h"
#include "sample.h"
#include "sysmemory.h"
#include "usersamples.h"

#include <QDir>
#include <QFileInfo>
#include <QLineF>
#include <QMetaObject>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace tpl;

namespace {

/// Подпись материала для списков страницы «Что считать».
QString materialText(Material m)
{
    switch (m) {
    case Material::Metal:      return tpl::tr("металл", "metal");
    case Material::Magnetic:   return tpl::tr("магнитная стенка", "magnetic wall");
    case Material::Absorber:   return tpl::tr("поглотитель", "absorber");
    case Material::Dielectric: return tpl::tr("диэлектрик", "dielectric");
    }
    return QString();
}

/// Строки паспорта для QML: [{label, value}].
QVariantList rowsList(const QVector<samples::Row> &rows)
{
    QVariantList out;
    for (const samples::Row &r : rows)
        out << QVariantMap{{QStringLiteral("label"), r.label}, {QStringLiteral("value"), r.value}};
    return out;
}

/// Заполняет углы свипа «от, до, шаг» (`RunPlanPage::pullSweep`).
void fillAngles(AngleSweep *s, double a0, double a1, double st)
{
    s->angles.clear();
    if (st > 0.0 && a1 >= a0) {
        for (double a = a0; a <= a1 + st * 1e-6 && s->angles.size() < 400; a += st)
            s->angles << std::round(a * 1000.0) / 1000.0;
    }
}

} // namespace

// ------------------------------------------------------------------ «Что считать»

QVariantMap TplDesignerModel::runPlan() const
{
    const Project &p = m_project;
    QVariantMap out;

    // Строка входа подписана тем, что она даёт: столбец S-матрицы. Один прогон отдаёт целый
    // столбец, отдельный элемент из него не вынуть, поэтому выбор идёт по входам.
    QVariantList ports;
    for (const Port &port : p.ports) {
        const int j = p.portOrdinal(port.id);
        QStringList gives;
        for (const Port &q : p.ports)
            gives << QStringLiteral("S%1%2").arg(p.portOrdinal(q.id)).arg(j);
        QVariantMap m;
        m.insert(QStringLiteral("id"), port.id);
        m.insert(QStringLiteral("gives"), gives.join(QStringLiteral(", ")));
        m.insert(QStringLiteral("text"), tpl::tr("возбудить вход %1 «%2»", "excite port %1 \"%2\"")
                                             .arg(j).arg(port.name));
        m.insert(QStringLiteral("checked"), p.exciters.contains(port.id));
        ports << m;
    }
    out.insert(QStringLiteral("ports"), ports);

    QVariantList variants;
    for (int i = 0; i < p.variants.size(); ++i) {
        const Variant &v = p.variants.at(i);
        QStringList off;
        int on = 0;
        QVariantList shapes;
        for (const Shape &s : p.shapes) {
            if (v.isEnabled(s.id))
                ++on;
            else
                off << s.name;
            shapes << QVariantMap{{QStringLiteral("id"), s.id},
                                  {QStringLiteral("text"), QStringLiteral("%1 — %2").arg(s.name, materialText(s.material))},
                                  {QStringLiteral("on"), v.isEnabled(s.id)}};
        }
        const bool empty = on == 0 && !p.shapes.isEmpty();
        QString what = off.isEmpty() ? tpl::tr("вся геометрия", "all geometry")
                                     : tpl::tr("без: %1", "without: %1").arg(off.join(QStringLiteral(", ")));
        if (empty)
            what = tpl::tr("ПУСТО — ни одной фигуры", "EMPTY: no shapes at all");
        variants << QVariantMap{{QStringLiteral("index"), i},
                                {QStringLiteral("name"), v.name},
                                {QStringLiteral("tag"), v.tag},
                                {QStringLiteral("what"), what},
                                {QStringLiteral("empty"), empty},
                                {QStringLiteral("shapes"), shapes}};
    }
    out.insert(QStringLiteral("variants"), variants);

    // --- итог ---
    const int nv = std::max(1, int(p.variants.size()));
    const int ne = std::max(1, int(p.exciters.size()));
    const int nf = std::max(1, int(p.freqs.size()));
    const int total = p.stepCount();
    QString summary = p.sweep.active()
        ? tpl::tr("Шагов расчёта: %1 (поворот объекта: %2 углов%3).", "Run steps: %1 (object rotation: %2 angles%3).")
              .arg(total).arg(p.sweep.angles.size())
              .arg(p.sweep.referenceStep ? tpl::tr(" + опорный шаг", " + reference step") : QString())
        : tpl::tr("Шагов расчёта: %1 варианта x %2 возбуждения x %3 частоты = %4.",
                  "Run steps: %1 variants x %2 excitations x %3 frequencies = %4.")
              .arg(nv).arg(ne).arg(nf).arg(total);
    if (total > 1 && !p.sweep.active()) {
        summary += tpl::tr("\nБез этой страницы столько же данных потребовало бы %1 отдельных заданий и %1 запусков.",
                           "\nWithout this page the same data would need %1 separate tasks and %1 launches.")
                       .arg(nv * ne);
    }
    out.insert(QStringLiteral("summary"), summary);
    out.insert(QStringLiteral("summaryHeavy"), total > 200);

    // --- какие элементы матрицы получатся ---
    QString hint;
    QString level;
    if (p.sweep.active()) {
        const int n = int(p.exciters.size());
        hint = n == 0 ? tpl::tr("Поворот объекта: на всех шагах работают входы, помеченные активными в свойствах порта. "
                                "Можно и отметить вход здесь.",
                                "Object rotation: at all steps the ports marked active in the port properties work. "
                                "You can also tick a port here.")
             : n == 1 ? tpl::tr("Поворот объекта: на всех шагах возбуждается этот вход.",
                                "Object rotation: this port is excited at all steps.")
                      : tpl::tr("Поворот объекта: оставьте отмеченным ОДИН вход (сейчас %1).",
                                "Object rotation: leave ONE port ticked (now %1).").arg(n);
        level = n > 1 ? QStringLiteral("warn") : QString();
    } else if (p.exciters.isEmpty()) {
        hint = tpl::tr("Ничего не отмечено — будет один шаг с портами, помеченными активными в свойствах порта.",
                       "Nothing ticked: there will be one step with the ports marked active in the port properties.");
    } else {
        QStringList got;
        for (int id : p.exciters) {
            const int j = p.portOrdinal(id);
            for (const Port &q : p.ports)
                got << QStringLiteral("S%1%2").arg(p.portOrdinal(q.id)).arg(j);
        }
        const bool full = p.exciters.size() == p.ports.size();
        hint = tpl::tr("Получится: %1.%2", "Result: %1.%2")
                   .arg(got.join(QStringLiteral(", ")),
                        full ? tpl::tr(" Это полная матрица.", " This is the full matrix.")
                             : tpl::tr(" Остальные столбцы останутся пустыми.", " The other columns stay empty."));
        level = full ? QStringLiteral("ok") : QStringLiteral("warn");
    }
    out.insert(QStringLiteral("hint"), hint);
    out.insert(QStringLiteral("hintLevel"), level);

    // --- поворот объекта ---
    const AngleSweep &s = p.sweep;
    QVariantMap sw;
    sw.insert(QStringLiteral("on"), s.enabled);
    QVariantList swShapes;
    for (const Shape &sh : p.shapes) {
        swShapes << QVariantMap{{QStringLiteral("id"), sh.id},
                                {QStringLiteral("text"), QStringLiteral("%1 — %2").arg(sh.name, materialText(sh.material))},
                                {QStringLiteral("on"), s.shapeIds.contains(sh.id)}};
    }
    sw.insert(QStringLiteral("shapes"), swShapes);
    const QPointF pivot = sweepPivot(p);
    sw.insert(QStringLiteral("center"), s.pivotAtCenter);
    sw.insert(QStringLiteral("pivotX"), num(pivot.x()));
    sw.insert(QStringLiteral("pivotY"), num(pivot.y()));
    sw.insert(QStringLiteral("from"), num(s.angles.isEmpty() ? 0.0 : s.angles.first()));
    sw.insert(QStringLiteral("to"), num(s.angles.isEmpty() ? 180.0 : s.angles.last()));
    sw.insert(QStringLiteral("step"), num(s.angles.size() > 1 ? std::fabs(s.angles.at(1) - s.angles.at(0)) : 5.0));
    sw.insert(QStringLiteral("reference"), s.referenceStep);

    QString swHint = tpl::tr("Выключено: объект стоит на месте.", "Off: the object stays in place.");
    QString swLevel;
    if (s.enabled) {
        swHint = tpl::tr("Будет %1 шаг(ов): %2 углов%3. Варианты геометрии на это время отключены.",
                         "There will be %1 step(s): %2 angles%3. Geometry variants are disabled for this time.")
                     .arg(p.stepCount()).arg(s.angles.size())
                     .arg(s.referenceStep ? tpl::tr(" + опорный шаг без объекта", " + reference step without the object")
                                          : QString());
        QStringList names;
        for (const Shape &sh : p.shapes) {
            if (s.rotates(sh.id))
                names << QStringLiteral("«%1»").arg(sh.name);
        }
        if (!names.isEmpty())
            swHint = tpl::tr("Поворачивается: %1. ", "Rotating: %1. ").arg(names.join(QStringLiteral(", "))) + swHint;
        QStringList fix;
        if (s.shapeIds.isEmpty())
            fix << tpl::tr("отметьте фигуры, которые поворачиваются", "tick the shapes that rotate");
        if (s.angles.isEmpty())
            fix << tpl::tr("угол «до» должен быть не меньше угла «от»", "the \"to\" angle must not be less than the \"from\" angle");
        if (p.exciters.size() > 1)
            fix << tpl::tr("оставьте отмеченным один вход (сейчас %1)", "leave one port ticked (now %1)").arg(p.exciters.size());
        if (p.freqs.size() > 1)
            fix << tpl::tr("на шаге «Задание» оставьте одну частоту (сейчас %1)", "keep one frequency on the Task step (now %1)")
                       .arg(p.freqs.size());
        if (!fix.isEmpty())
            swHint += tpl::tr("\nЧтобы сохранить задание: ", "\nTo save the task: ") + fix.join(QStringLiteral("; ")) + QLatin1Char('.');
        swLevel = fix.isEmpty() ? QStringLiteral("ok") : QStringLiteral("warn");
    }
    sw.insert(QStringLiteral("hint"), swHint);
    sw.insert(QStringLiteral("hintLevel"), swLevel);
    out.insert(QStringLiteral("sweep"), sw);
    return out;
}

void TplDesignerModel::setExciter(int portId, bool on)
{
    snapshot();
    // Порядок очереди — порядок портов в задании, как у флажков настольного списка.
    QVector<int> keep;
    for (const Port &p : m_project.ports) {
        const bool was = m_project.exciters.contains(p.id);
        if (p.id == portId ? on : was)
            keep << p.id;
    }
    m_project.exciters = keep;
    refresh();
}

void TplDesignerModel::addVariant()
{
    snapshot();
    Variant v;
    v.name = tpl::tr("Вариант %1", "Variant %1").arg(m_project.variants.size() + 1);
    v.tag = QStringLiteral("v%1").arg(m_project.variants.size() + 1);
    m_project.variants << v;
    refresh();
}

void TplDesignerModel::presetVariants()
{
    // Заготовка: в первом варианте выключены все заливки, во втором включено всё.
    snapshot();
    m_project.variants.clear();
    Variant empty;
    empty.name = tpl::tr("пустой", "empty");
    empty.tag = QStringLiteral("wg");
    for (const Shape &s : m_project.shapes) {
        if (!s.isContour())
            empty.disabledShapes << s.id;
    }
    Variant full;
    full.name = tpl::tr("с заполнением", "filled");
    full.tag = QStringLiteral("fill");
    m_project.variants << empty << full;
    refresh();
}

void TplDesignerModel::removeVariant(int index)
{
    if (index < 0 || index >= m_project.variants.size())
        return;
    snapshot();
    m_project.variants.remove(index);
    refresh();
}

void TplDesignerModel::setVariant(int index, const QString &key, const QString &value)
{
    if (index < 0 || index >= m_project.variants.size())
        return;
    snapshot();
    if (key == QLatin1String("name"))
        m_project.variants[index].name = value;
    else if (key == QLatin1String("tag"))
        m_project.variants[index].tag = value;
    refresh();
}

void TplDesignerModel::setVariantShape(int index, int shapeId, bool on)
{
    if (index < 0 || index >= m_project.variants.size())
        return;
    snapshot();
    QVector<int> &off = m_project.variants[index].disabledShapes;
    off.removeAll(shapeId);
    if (!on)
        off << shapeId;
    refresh();
}

void TplDesignerModel::setSweep(const QString &key, const QVariant &value)
{
    snapshot();
    AngleSweep &s = m_project.sweep;
    bool ok = false;
    const double d = value.toString().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    if (key == QLatin1String("on")) {
        const bool on = value.toBool();
        // Включили впервые — разумное начало: весь объект без поглотителей, 0...180 шагом 5.
        if (on && s.angles.isEmpty()) {
            s.referenceStep = true;
            for (double a = 0; a <= 180.0 + 1e-9; a += 5.0)
                s.angles << a;
            for (const Shape &sh : m_project.shapes) {
                if (sh.material != Material::Absorber)
                    s.shapeIds << sh.id;
            }
            s.pivotAtCenter = true;
        }
        s.enabled = on;
    } else if (key == QLatin1String("center")) {
        const bool on = value.toBool();
        if (s.pivotAtCenter && !on)
            s.pivot = sweepPivot(m_project);   // ось остаётся там, где была центром
        s.pivotAtCenter = on;
    } else if (key == QLatin1String("reference")) {
        s.referenceStep = value.toBool();
    } else if (ok && key == QLatin1String("pivotX")) {
        s.pivot.setX(d);
    } else if (ok && key == QLatin1String("pivotY")) {
        s.pivot.setY(d);
    } else if (ok && (key == QLatin1String("from") || key == QLatin1String("to") || key == QLatin1String("step"))) {
        const double a0 = key == QLatin1String("from") ? d : (s.angles.isEmpty() ? 0.0 : s.angles.first());
        const double a1 = key == QLatin1String("to") ? d : (s.angles.isEmpty() ? 180.0 : s.angles.last());
        const double st = key == QLatin1String("step") ? d
                          : (s.angles.size() > 1 ? std::fabs(s.angles.at(1) - s.angles.at(0)) : 5.0);
        fillAngles(&s, a0, a1, st);
    }
    refresh();
}

void TplDesignerModel::setSweepShape(int shapeId, bool on)
{
    snapshot();
    QVector<int> &ids = m_project.sweep.shapeIds;
    ids.removeAll(shapeId);
    if (on)
        ids << shapeId;
    refresh();
}

// ------------------------------------------------------------------ память

void TplDesignerModel::updateMemory()
{
    const sysmem::Info mem = sysmem::query();
    // Свободная память идёт в оценку: по ней ядро решает, копить ДН одной частоты по всей области
    // или по краю (`tpl::ramEstimate`).
    const RamEstimate ram = ramEstimate(m_project, mem.ok ? mem.available : 0.0);
    const double need = ram.total();

    QString text;
    QString level;
    if (!mem.ok) {
        text = tpl::tr("Узнать память телефона не удалось. Расчёту нужно около %1.",
                       "The phone memory could not be read. The run needs about %1.").arg(formatBytes(need));
    } else if (need > mem.available) {
        text = tpl::tr("На этом телефоне задание может не поместиться в память: расчёту нужно около %1, а свободно %2 "
                       "(всего %3). Сохранить файл можно — посчитать его стоит на компьютере.",
                       "The task may not fit into the memory of this phone: the run needs about %1, while %2 is free "
                       "(%3 total). The file can be saved: it is better to run it on a computer.")
                   .arg(formatBytes(need), formatBytes(mem.available), formatBytes(mem.total));
        level = QStringLiteral("danger");
    } else if (need > 0.8 * mem.available) {
        text = tpl::tr("Впритык: расчёту нужно около %1 из %2 свободных. Закройте лишние приложения перед расчётом.",
                       "Tight: the run needs about %1 of %2 free. Close unneeded apps before the run.")
                   .arg(formatBytes(need), formatBytes(mem.available));
        level = QStringLiteral("warn");
    } else {
        text = tpl::tr("Поместится: расчёту нужно около %1, свободно %2 из %3.",
                       "It fits: the run needs about %1, %2 of %3 is free.")
                   .arg(formatBytes(need), formatBytes(mem.available), formatBytes(mem.total));
        level = QStringLiteral("ok");
    }

    QVariantMap m;
    m.insert(QStringLiteral("known"), mem.ok);
    m.insert(QStringLiteral("total"), mem.total);
    m.insert(QStringLiteral("used"), mem.used());
    m.insert(QStringLiteral("grid"), ram.gridBytes);
    m.insert(QStringLiteral("pattern"), ram.patternBytes);
    m.insert(QStringLiteral("patternOn"), ram.pattern);
    m.insert(QStringLiteral("patternBorder"), ram.patternBorder);
    m.insert(QStringLiteral("tones"), ram.patternTones);
    m.insert(QStringLiteral("fits"), !mem.ok || mem.used() + need <= mem.total);
    m.insert(QStringLiteral("text"), text);
    m.insert(QStringLiteral("level"), level);
    m.insert(QStringLiteral("usedText"), mem.ok ? formatBytes(mem.used()) : QString());
    m.insert(QStringLiteral("gridText"), formatBytes(ram.gridBytes));
    m.insert(QStringLiteral("patternText"), formatBytes(ram.patternBytes));
    m.insert(QStringLiteral("freeText"), mem.ok ? formatBytes(mem.available) : QString());
    if (m != m_memory) {
        m_memory = m;
        emit memoryChanged();
    }
}

// ------------------------------------------------------------------ несколько фигур

QVariantList TplDesignerModel::selectedIds() const
{
    QVariantList out;
    for (int id : m_multi)
        out << id;
    if (out.isEmpty() && m_selectedId >= 0)
        out << m_selectedId;
    return out;
}

bool TplDesignerModel::isSelected(int id) const
{
    return id >= 0 && (id == m_selectedId || m_multi.contains(id));
}

void TplDesignerModel::toggleSelected(int id)
{
    if (id < 0)
        return;
    if (portById(id)) {
        // Порты в группу не входят: группой правят фигуры.
        select(id);
        return;
    }
    QVector<int> ids = m_multi;
    if (ids.isEmpty() && m_selectedId >= 0 && !portById(m_selectedId))
        ids << m_selectedId;
    if (ids.contains(id))
        ids.removeAll(id);
    else
        ids << id;
    m_multi = ids.size() > 1 ? ids : QVector<int>{};
    m_selectedId = ids.size() == 1 ? ids.first() : -1;
    ++m_revision;
    emit changed();
}

QString TplDesignerModel::mergeSelected()
{
    // Сшиваются только списки точек: у прямоугольника и круга своя запись в языке
    // (`GeometryView::mergeShapes`).
    QVector<int> order;
    for (int i = 0; i < m_project.shapes.size(); ++i) {
        const Shape &s = m_project.shapes.at(i);
        if (!m_multi.contains(s.id))
            continue;
        if (s.kind != ShapeKind::Polyline && s.kind != ShapeKind::Polygon)
            return tpl::tr("«%1» — не линия и не контур: соединять можно только линии, дуги и контуры.",
                           "\"%1\" is neither a line nor a polygon: only lines, arcs and polygons can be joined.").arg(s.name);
        if (s.points.size() < 2)
            return tpl::tr("У «%1» меньше двух точек.", "\"%1\" has fewer than two points.").arg(s.name);
        order << i;
    }
    if (order.size() < 2)
        return tpl::tr("Выберите хотя бы две линии.", "Select at least two lines.");

    QVector<QVector<QPointF>> parts;
    for (int i : order) {
        const Shape &s = m_project.shapes.at(i);
        QVector<QPointF> pts = s.points;
        if (s.kind == ShapeKind::Polygon && pts.first() != pts.last())
            pts << pts.first();
        parts << pts;
    }

    const double tol = std::max(1e-6, m_project.delta);
    QVector<QPointF> chain = parts.takeFirst();
    double worstGap = 0.0;
    while (!parts.isEmpty()) {
        int best = -1;
        double bestD = std::numeric_limits<double>::max();
        bool atTail = true;
        bool flip = false;
        for (int i = 0; i < parts.size(); ++i) {
            const QPointF a = parts.at(i).first();
            const QPointF b = parts.at(i).last();
            const struct { double d; bool tail; bool rev; } cases[] = {
                {QLineF(chain.last(), a).length(), true, false},
                {QLineF(chain.last(), b).length(), true, true},
                {QLineF(chain.first(), b).length(), false, false},
                {QLineF(chain.first(), a).length(), false, true},
            };
            for (const auto &c : cases) {
                if (c.d < bestD) {
                    bestD = c.d;
                    best = i;
                    atTail = c.tail;
                    flip = c.rev;
                }
            }
        }
        QVector<QPointF> piece = parts.takeAt(best);
        if (flip)
            std::reverse(piece.begin(), piece.end());
        worstGap = std::max(worstGap, bestD);
        if (atTail) {
            if (QLineF(chain.last(), piece.first()).length() <= tol)
                piece.removeFirst();
            chain += piece;
        } else {
            if (!piece.isEmpty() && QLineF(piece.last(), chain.first()).length() <= tol)
                piece.removeLast();
            chain = piece + chain;
        }
    }
    if (chain.size() < 2)
        return tpl::tr("После сшивания не осталось точек.", "No points are left after joining.");

    const bool closed = chain.size() >= 4 && QLineF(chain.first(), chain.last()).length() <= tol;
    if (closed)
        chain.removeLast();

    snapshot();
    // Свойства берутся у первой части по порядку в задании, иначе объединение молча меняло бы среду.
    Shape merged = m_project.shapes.at(order.first());
    merged.kind = closed ? ShapeKind::Polygon : ShapeKind::Polyline;
    merged.points = chain;
    merged.xMin = merged.xMax = merged.yMin = merged.yMax = 0.0;

    QVector<int> del = order;
    std::sort(del.begin(), del.end(), std::greater<int>());
    QVector<int> gone;
    for (int i : del) {
        if (m_project.shapes.at(i).id != merged.id)
            gone << m_project.shapes.at(i).id;
        m_project.shapes.remove(i);
    }
    m_project.shapes.insert(order.first(), merged);

    // Варианты выключают фигуры по номеру. Была выключена хоть одна часть — выключаем и результат.
    for (Variant &v : m_project.variants) {
        const bool hid = std::any_of(gone.begin(), gone.end(), [&v](int id) { return v.disabledShapes.contains(id); });
        for (int id : gone)
            v.disabledShapes.removeAll(id);
        if (hid && !v.disabledShapes.contains(merged.id))
            v.disabledShapes.append(merged.id);
    }
    m_multi.clear();
    m_selectedId = merged.id;
    refresh();
    return worstGap > tol
        ? tpl::tr("Соединено в одну фигуру: вершин %1. ⚠ Самый большой разрыв между кусками — %2 мм, он замкнут прямой.",
                  "Joined into one shape: %1 vertices. ⚠ The largest gap between the pieces is %2 mm, it is closed by a straight line.")
              .arg(chain.size()).arg(worstGap, 0, 'g', 3)
        : tpl::tr("Соединено в одну фигуру: вершин %1.", "Joined into one shape: %1 vertices.").arg(chain.size());
}

QString TplDesignerModel::filletSelected(const QString &radiusText)
{
    bool ok = false;
    const double radius = QString(radiusText).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    Shape *s = shapeById(m_selectedId);
    if (!s || !ok || radius <= 0.0)
        return tpl::tr("Укажите радиус больше нуля.", "Enter a radius above zero.");
    if (s->kind != ShapeKind::Rectangle)
        return tpl::tr("Скругляются углы прямоугольника.", "Rectangle corners are rounded.");
    const double r = std::min({radius, (s->xMax - s->xMin) / 2, (s->yMax - s->yMin) / 2});
    if (r <= 0.0)
        return tpl::tr("Прямоугольник слишком мал.", "The rectangle is too small.");
    snapshot();
    // Каждая дуга начинается там, где кончается прямая сторона (`GeometryView::filletSelected`).
    const auto arc = [](const QPointF &c, double rad, double a0, double a1) {
        const int n = std::clamp(int(std::fabs(a1 - a0) * rad / 1.5), 3, 96);
        QVector<QPointF> pts;
        for (int i = 0; i <= n; ++i) {
            const double a = a0 + (a1 - a0) * i / n;
            pts << c + QPointF(rad * std::cos(a), rad * std::sin(a));
        }
        return pts;
    };
    QVector<QPointF> pts;
    pts += arc({s->xMax - r, s->yMin + r}, r, -M_PI / 2, 0.0);
    pts += arc({s->xMax - r, s->yMax - r}, r, 0.0, M_PI / 2);
    pts += arc({s->xMin + r, s->yMax - r}, r, M_PI / 2, M_PI);
    pts += arc({s->xMin + r, s->yMin + r}, r, M_PI, 3 * M_PI / 2);
    s->kind = ShapeKind::Polygon;
    s->points = pts;
    for (EdgeSpec &e : s->edges)
        e = EdgeSpec{};
    refresh();
    return QString();
}

// ------------------------------------------------------------------ калькулятор и импорт

QVariantMap TplDesignerModel::calculate(const QVariantMap &in) const
{
    const auto val = [&in](const char *k, double def) {
        bool ok = false;
        const double d = in.value(QLatin1String(k)).toString().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
        return ok ? d : def;
    };
    const double a = val("a", 72.0), wgEps = val("wgEps", 1.0);
    const double eps = val("eps", 4.0), f = val("freq", 4.0);
    const double n = val("n", 1.24e18), b = val("b", 0.0);
    QVariantMap out;
    out.insert(QStringLiteral("fc10"), QStringLiteral("%1 %2").arg(num(std::round(cutoffH10(a, wgEps) * 1e4) / 1e4), tpl::tr("ГГц", "GHz")));
    out.insert(QStringLiteral("fc20"), QStringLiteral("%1 %2").arg(num(std::round(cutoffH20(a, wgEps) * 1e4) / 1e4), tpl::tr("ГГц", "GHz")));
    if (f > 0.0 && eps > 0.0) {
        out.insert(QStringLiteral("lambda"), QStringLiteral("%1 %2").arg(num(std::round(kLightSpeed / f / std::sqrt(eps) * 1e3) / 1e3), tpl::tr("мм", "mm")));
    }
    const PlasmaBands pb = plasmaBands(n, b);
    const auto g = [](double v) { return num(std::round(v * 1e4) / 1e4); };
    out.insert(QStringLiteral("plasma"),
               tpl::tr("Плазменная частота %1 ГГц, циклотронная %2 ГГц.\nОтсечки X-моды: нижняя %3 ГГц, верхняя %4 ГГц, гибридный резонанс %5 ГГц.",
                       "Plasma frequency %1 GHz, cyclotron %2 GHz.\nX-mode cutoffs: lower %3 GHz, upper %4 GHz, hybrid resonance %5 GHz.")
                   .arg(g(pb.fp), g(pb.fce), g(pb.fL), g(pb.fR), g(pb.fUH)));
    return out;
}

QStringList TplDesignerModel::meshFiles() const
{
    QStringList out;
    for (const QString &f : folderFiles()) {
        if (f.endsWith(QLatin1String(".stl"), Qt::CaseInsensitive) || f.endsWith(QLatin1String(".obj"), Qt::CaseInsensitive))
            out << f;
    }
    return out;
}

QString TplDesignerModel::importMesh(const QString &fileName, const QString &plane, const QString &offset)
{
    QString path = localFolder();
    if (path.isEmpty())
        return tpl::tr("Папка задачи не выбрана.", "The task folder is not selected.");
    path = QDir(path).filePath(fileName);
    const MeshReadResult mr = readMeshFile(path);
    if (!mr.ok())
        return tpl::tr("Модель не прочитана: %1", "Model not read: %1").arg(mr.error);
    const SlicePlane sp = plane == QLatin1String("yz") ? SlicePlane::YZ
                          : plane == QLatin1String("xz") ? SlicePlane::XZ : SlicePlane::XY;
    bool ok = false;
    const double off = QString(offset).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    SliceResult slice = sliceMesh(mr.mesh, sp, ok ? off : 0.0);
    if (slice.bodies.isEmpty())
        return tpl::tr("Плоскость не задевает модель: поменяйте положение плоскости.",
                       "The plane does not cross the model: change the plane position.");
    simplifySlice(&slice, std::max(1e-6, m_project.delta / 4.0));
    ImportPlacement pl;
    pl.shift = centeringShift(slice, pl, m_project.box);
    const QVector<BodyChoice> choices(mr.mesh.bodies.size());
    const ImportShapes res = buildImportShapes(mr.mesh, slice, choices, pl, m_project.nextFreeId());
    if (res.shapes.isEmpty())
        return tpl::tr("Из среза не получилось ни одной фигуры.", "The slice gave no shapes.");
    snapshot();
    QVector<int> ids;
    for (const Shape &s : res.shapes) {
        m_project.shapes << s;
        ids << s.id;
    }
    const QString prov = sliceProvenance(QFileInfo(fileName).fileName(), sp, ok ? off : 0.0);
    if (!prov.isEmpty() && !m_project.description.contains(prov)) {
        m_project.description = m_project.description.isEmpty() ? prov
                                                                : m_project.description + QStringLiteral(". ") + prov;
    }
    m_selectedId = ids.size() == 1 ? ids.first() : -1;
    m_multi = ids.size() > 1 ? ids : QVector<int>{};
    refresh();
    QString msg = tpl::tr("Добавлено фигур: %1. Материал — металл, его можно поменять в свойствах фигуры.",
                          "Shapes added: %1. The material is metal, it can be changed in the shape properties.").arg(res.shapes.size());
    if (!res.notes.isEmpty())
        msg += QLatin1Char('\n') + res.notes.join(QLatin1Char('\n'));
    return msg;
}

// ------------------------------------------------------------------ образцы

QVariantList TplDesignerModel::sampleSections()
{
    m_userSamples = samples::userSamples();
    QVariantList out;
    for (const samples::Section &sec : samples::sections()) {
        QVariantList items;
        for (const samples::Sample *smp : samples::inSection(sec.id)) {
            items << QVariantMap{{QStringLiteral("key"), QStringLiteral("b:") + smp->id},
                                 {QStringLiteral("title"), smp->title},
                                 {QStringLiteral("summary"), smp->summary},
                                 {QStringLiteral("standard"), smp->standard},
                                 {QStringLiteral("kernel"), smp->kernel == Kernel::X ? QStringLiteral("X") : QStringLiteral("H")}};
        }
        if (!items.isEmpty()) {
            out << QVariantMap{{QStringLiteral("id"), sec.id},
                               {QStringLiteral("title"), sec.title},
                               {QStringLiteral("summary"), sec.summary},
                               {QStringLiteral("items"), items}};
        }
    }
    QVariantList mine;
    for (int i = 0; i < m_userSamples.size(); ++i) {
        const samples::UserSample &u = m_userSamples.at(i);
        mine << QVariantMap{{QStringLiteral("key"), QStringLiteral("u:%1").arg(i)},
                            {QStringLiteral("title"), u.title},
                            {QStringLiteral("summary"), u.summary},
                            {QStringLiteral("standard"), QString()},
                            {QStringLiteral("kernel"), u.kernel == Kernel::X ? QStringLiteral("X") : QStringLiteral("H")}};
    }
    if (!mine.isEmpty()) {
        out << QVariantMap{{QStringLiteral("id"), QStringLiteral("mine")},
                           {QStringLiteral("title"), tpl::tr("Мои образцы", "My samples")},
                           {QStringLiteral("summary"), QString()},
                           {QStringLiteral("items"), mine}};
    }
    return out;
}

samples::Build TplDesignerModel::buildSample(const QString &key, const samples::Values *values) const
{
    if (key.startsWith(QLatin1String("u:"))) {
        const int i = key.mid(2).toInt();
        return (i >= 0 && i < m_userSamples.size()) ? samples::loadUserSample(m_userSamples.at(i)) : samples::Build{};
    }
    const samples::Sample *s = samples::find(key.mid(2));
    if (!s)
        return {};
    return samples::make(*s, values ? *values : samples::defaults(*s));
}

void TplDesignerModel::openSampleCard(const QString &key)
{
    const samples::Sample *s = key.startsWith(QLatin1String("b:")) ? samples::find(key.mid(2)) : nullptr;
    m_cardKey = key;
    m_cardValues = s ? samples::defaults(*s) : samples::Values{};
    rebuildCard();
}

void TplDesignerModel::setSampleParam(const QString &key, const QVariant &value)
{
    bool ok = false;
    const double d = value.toString().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    if (value.typeId() == QMetaType::Bool)
        m_cardValues.insert(key, value.toBool() ? 1.0 : 0.0);
    else if (ok)
        m_cardValues.insert(key, d);
    rebuildCard();
}

void TplDesignerModel::resetSampleParams()
{
    const samples::Sample *s = samples::find(m_cardKey.mid(2));
    if (s)
        m_cardValues = samples::defaults(*s);
    rebuildCard();
}

void TplDesignerModel::rebuildCard()
{
    QVariantMap card;
    m_cardBuild = buildSample(m_cardKey, &m_cardValues);
    const bool user = m_cardKey.startsWith(QLatin1String("u:"));
    const samples::Sample *s = user ? nullptr : samples::find(m_cardKey.mid(2));
    const Project &p = m_cardBuild.project;
    const Diagnostics d = validate(p);

    card.insert(QStringLiteral("key"), m_cardKey);
    card.insert(QStringLiteral("user"), user);
    if (s) {
        card.insert(QStringLiteral("title"), s->title);
        card.insert(QStringLiteral("summary"), s->summary);
        card.insert(QStringLiteral("help"), s->help);
        card.insert(QStringLiteral("meta"), s->standard);
        card.insert(QStringLiteral("canReset"), !samples::isDefault(*s, m_cardValues));
    } else if (user) {
        const samples::UserSample u = m_userSamples.value(m_cardKey.mid(2).toInt());
        card.insert(QStringLiteral("title"), u.title);
        card.insert(QStringLiteral("summary"), u.summary);
        card.insert(QStringLiteral("help"), p.description);
    }

    QVariantList params;
    if (s) {
        for (const samples::Param &q : s->params) {
            const double v = m_cardValues.value(q.key);
            QVariantMap m;
            m.insert(QStringLiteral("key"), q.key);
            m.insert(QStringLiteral("label"), q.label);
            m.insert(QStringLiteral("unit"), q.unit);
            m.insert(QStringLiteral("hint"), q.hint);
            m.insert(QStringLiteral("kind"), q.kind == samples::Param::Flag ? QStringLiteral("flag")
                                             : q.kind == samples::Param::Choice ? QStringLiteral("choice")
                                                                                : QStringLiteral("number"));
            m.insert(QStringLiteral("value"), num(v));
            m.insert(QStringLiteral("checked"), v > 0.5);
            m.insert(QStringLiteral("index"), int(v));
            m.insert(QStringLiteral("choices"), q.choices);
            params << m;
        }
    }
    card.insert(QStringLiteral("params"), params);

    QStringList problems;
    QString check;
    if (d.ok()) {
        check = tpl::tr("Задание корректно. %1, шагов расчёта: %2, сетка %3 x %4 (шаг %5 мм).",
                        "The task is correct. %1, run steps: %2, grid %3 x %4 (step %5 mm).")
                    .arg(needsMacro(p) ? tpl::tr("Макро-версия (.tpl + _step.h)", "Macro version (.tpl + _step.h)")
                                       : tpl::tr("Один файл .tpl", "Single .tpl file"))
                    .arg(p.stepCount())
                    .arg(int(std::lround(p.box.width() / p.delta)))
                    .arg(int(std::lround(p.box.height() / p.delta)))
                    .arg(QString::number(p.delta, 'g', 4));
    } else {
        check = tpl::tr("Ядро такое задание не примет.", "The kernel will not accept such a task.");
        problems = d.errors;
    }
    card.insert(QStringLiteral("ok"), d.ok());
    card.insert(QStringLiteral("check"), check);
    card.insert(QStringLiteral("errors"), problems);
    card.insert(QStringLiteral("warnings"), QStringList(d.warnings));
    card.insert(QStringLiteral("canOpen"), !p.shapes.isEmpty() || !p.ports.isEmpty());
    card.insert(QStringLiteral("theory"), rowsList(m_cardBuild.theory));

    const samples::Baked *bk = s ? samples::baked(s->id) : nullptr;
    QString kernelNote;
    QVariantList kernelRows;
    if (user) {
        kernelNote = tpl::tr("Свои образцы программа ядром не считает: посчитайте сохранённое задание сами.",
                             "The app does not compute your own samples with the kernel: compute the saved task yourself.");
    } else if (!bk) {
        kernelNote = tpl::tr("Этот образец ядром ещё не считали.", "This sample has not been computed by the kernel yet.");
    } else if (s && !samples::isDefault(*s, m_cardValues)) {
        kernelNote = tpl::tr("Параметры изменены: числа ядра относятся к значениям по умолчанию.",
                             "Parameters changed: the kernel numbers refer to the defaults.");
    } else {
        kernelRows = rowsList(bk->rows);
        kernelNote = tpl::tr("Прогон %1: %2.", "Run %1: %2.").arg(bk->when, bk->setup);
    }
    card.insert(QStringLiteral("kernelRows"), kernelRows);
    card.insert(QStringLiteral("kernelNote"), kernelNote);

    QStringList notes = m_cardBuild.notes;
    if (!m_cardBuild.companions.isEmpty()) {
        notes << tpl::tr("Рядом с заданием будут записаны: %1.", "Written next to the task: %1.")
                     .arg(m_cardBuild.companions.keys().join(QStringLiteral(", ")));
    }
    card.insert(QStringLiteral("notes"), notes);
    m_card = card;
    ++m_cardRevision;
    emit sampleCardChanged();
}

QString TplDesignerModel::openCardInEditor()
{
    if (m_cardKey.isEmpty())
        return QString();
    snapshot();
    m_project = m_cardBuild.project;
    m_companions = m_cardBuild.companions;
    m_selectedId = -1;
    m_multi.clear();
    pushToForm();
    refresh();
    return tpl::tr("Открыт образец «%1».", "Sample \"%1\" opened.").arg(m_card.value(QStringLiteral("title")).toString());
}

QVariantMap TplDesignerModel::saveAsSample(const QString &title, const QString &summary)
{
    QVariantMap r;
    const Diagnostics d = validate(m_project);
    if (!d.ok() || title.trimmed().isEmpty()) {
        r.insert(QStringLiteral("ok"), false);
        r.insert(QStringLiteral("message"), title.trimmed().isEmpty()
                     ? tpl::tr("Укажите название образца.", "Enter the sample title.")
                     : tpl::tr("В образцы кладутся только задания, которые ядро примет.",
                               "Only tasks the kernel accepts can be saved as samples."));
        return r;
    }
    const WriteResult w = samples::saveUserSample(title.trimmed(), summary.trimmed(), m_project, m_companions);
    r.insert(QStringLiteral("ok"), w.ok);
    r.insert(QStringLiteral("message"), w.ok ? tpl::tr("Образец сохранён в «Моих образцах».", "The sample is saved to My samples.")
                                             : w.error);
    return r;
}

bool TplDesignerModel::deleteUserSample()
{
    if (!m_cardKey.startsWith(QLatin1String("u:")))
        return false;
    const int i = m_cardKey.mid(2).toInt();
    if (i < 0 || i >= m_userSamples.size())
        return false;
    const bool ok = samples::removeUserSample(m_userSamples.at(i));
    m_cardKey.clear();
    return ok;
}

void TplDesignerModel::setViewProject(const Project &p)
{
    m_project = p;
    ++m_revision;
    emit changed();
}
