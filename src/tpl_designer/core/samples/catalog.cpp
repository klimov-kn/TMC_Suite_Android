#include "sample.h"

#include <cmath>

namespace tpl::samples {

Param Param::number(const QString &key, const QString &label, const QString &unit, double def,
                    double min, double max, double step, int decimals, const QString &hint)
{
    Param p;
    p.key = key;
    p.label = label;
    p.unit = unit;
    p.kind = Number;
    p.def = def;
    p.min = min;
    p.max = max;
    p.step = step;
    p.decimals = decimals;
    p.hint = hint;
    return p;
}

Param Param::integer(const QString &key, const QString &label, int def, int min, int max,
                     const QString &hint)
{
    Param p;
    p.key = key;
    p.label = label;
    p.kind = Integer;
    p.def = def;
    p.min = min;
    p.max = max;
    p.step = 1;
    p.decimals = 0;
    p.hint = hint;
    return p;
}

Param Param::choice(const QString &key, const QString &label, const QStringList &items, int def,
                    const QString &hint)
{
    Param p;
    p.key = key;
    p.label = label;
    p.kind = Choice;
    p.choices = items;
    p.def = def;
    p.min = 0;
    p.max = items.size() - 1;
    p.hint = hint;
    return p;
}

Param Param::flag(const QString &key, const QString &label, bool def, const QString &hint)
{
    Param p;
    p.key = key;
    p.label = label;
    p.kind = Flag;
    p.def = def ? 1.0 : 0.0;
    p.min = 0;
    p.max = 1;
    p.hint = hint;
    return p;
}

const QVector<Section> &sections()
{
    static const QVector<Section> list = {
        {QStringLiteral("waveguide"), QStringLiteral("Волноводы и узлы"),
         QStringLiteral("Стандартные сечения, неоднородности, фильтры")},
        {QStringLiteral("antenna"), QStringLiteral("Антенны"),
         QStringLiteral("Излучатели, рупоры, линзы, отражатели, решётки")},
        {QStringLiteral("microstrip"), QStringLiteral("Микрополосковые"),
         QStringLiteral("Планарная модель: линии, шлейфы, фильтры, ответвители")},
        {QStringLiteral("media"), QStringLiteral("Среды, плазма, ЭПР"),
         QStringLiteral("Неоднородная eps, плазма, рассеяние на телах")},
        {QStringLiteral("lesson"), QStringLiteral("Учебные задания"),
         QStringLiteral("Авторские задания лабораторных работ")},
    };
    return list;
}

const QVector<Sample> &all()
{
    static const QVector<Sample> list = [] {
        QVector<Sample> v;
        addWaveguides(&v);
        addAntennas(&v);
        addMicrostrip(&v);
        addMedia(&v);
        addLessons(&v);
        return v;
    }();
    return list;
}

const Sample *find(const QString &id)
{
    for (const Sample &s : all()) {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

QVector<const Sample *> inSection(const QString &sectionId)
{
    QVector<const Sample *> out;
    for (const Sample &s : all()) {
        if (s.section == sectionId)
            out << &s;
    }
    return out;
}

Values defaults(const Sample &s)
{
    Values v;
    for (const Param &p : s.params)
        v.insert(p.key, p.def);
    return v;
}

bool isDefault(const Sample &s, const Values &v)
{
    for (const Param &p : s.params) {
        const double x = v.value(p.key, p.def);
        if (std::fabs(x - p.def) > 1e-9 * std::max(1.0, std::fabs(p.def)))
            return false;
    }
    return true;
}

const Baked *baked(const QString &sampleId)
{
    for (const Baked &b : bakedTable()) {
        if (b.sampleId == sampleId)
            return &b;
    }
    return nullptr;
}

Build make(const Sample &s, const Values &v)
{
    Values full = defaults(s);
    for (auto it = v.constBegin(); it != v.constEnd(); ++it)
        full.insert(it.key(), it.value());
    // Выход за пределы поля — не повод строить неверное задание: прижимаем.
    for (const Param &p : s.params) {
        double x = full.value(p.key);
        if (p.max > p.min)
            x = std::min(p.max, std::max(p.min, x));
        if (p.kind != Param::Number)
            x = std::round(x);
        full.insert(p.key, x);
    }
    Build b = s.build(full);
    if (b.project.description.isEmpty())
        b.project.description = s.title;
    return b;
}

} // namespace tpl::samples
