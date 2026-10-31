// Раздел «Волноводы и узлы»: стандартные сечения и неоднородности в H-плоскости.
//
// Общее у всех рецептов раздела: волновод идёт вдоль X, стенки — металл, вход H10
// слева и справа (или сверху у изгиба и тройника). Шаг сетки берётся так, чтобы в
// широкую стенку укладывалось целое число ячеек: тогда стенки лежат в узлах, и
// ширина волновода по сетке совпадает с номиналом стандарта.

#include "kit.h"
#include "rf.h"
#include "sample.h"
#include "standards.h"

#include "physics.h"

#include <cmath>

namespace tpl::samples {

namespace {

using namespace kit;

constexpr double kPi = rf::kPi;

/// Раскладка волноводной сцены по сечению стандарта.
struct Guide {
    WaveguideStd std;
    double a = 0.0;    ///< ширина по сетке
    double d = 0.0;    ///< шаг сетки
    double ylo = 0.0;  ///< нижняя стенка
    double yhi = 0.0;  ///< верхняя
    double fc = 0.0;   ///< отсечка H10
    double f0 = 0.0;   ///< середина рабочей полосы
};

/// \p cells — сколько ячеек в широкой стенке. 40 хватает, чтобы отсечка по сетке
/// разошлась с формулой меньше чем на процент.
Guide layout(int stdIndex, int cells = 40)
{
    Guide g;
    const QVector<WaveguideStd> &t = waveguides();
    g.std = t.value(std::clamp(stdIndex, 0, int(t.size()) - 1));
    g.a = g.std.a;
    g.d = std::round(g.a / cells * 1e6) / 1e6;
    g.ylo = 3 * g.d;
    g.yhi = g.ylo + g.a;
    g.fc = kLightSpeed / (2 * g.a);
    g.f0 = std::round((g.std.fLo + g.std.fHi) / 2.0 * 100.0) / 100.0;
    return g;
}

Param stdParam()
{
    return Param::choice(QStringLiteral("std"), QStringLiteral("Сечение волновода"),
                         waveguideChoices(), defaultWaveguide(),
                         QStringLiteral("EIA/IEC — размеры и полоса по стандарту; ГОСТ 20900-75 — "
                                        "сечение по стандарту, полоса расчётная 1.25-1.9 fc. "
                                        "Ядро двумерное: в расчёт идёт только широкая стенка a."));
}

Param pointsParam(int def = 21)
{
    return Param::integer(QStringLiteral("n"), QStringLiteral("Частот в свипе"), def, 2, 99,
                          QStringLiteral("Больше 99 точек в один .s не помещается — предел пакета"));
}

/// Общие строки паспорта волновода.
QVector<Row> guideRows(const Guide &g)
{
    return {
        {QStringLiteral("Стандарт"), g.std.name + QStringLiteral(" (") + g.std.family
                                         + QStringLiteral(")")},
        {QStringLiteral("Сечение a x b"),
         QStringLiteral("%1 x %2 мм").arg(num(g.std.a, 5), num(g.std.b, 5))},
        {QStringLiteral("Рабочая полоса"),
         QStringLiteral("%1 - %2 ГГц%3")
             .arg(num(g.std.fLo, 4), num(g.std.fHi, 4),
                  g.std.bandFromStandard ? QString() : QStringLiteral(" (расчётная)"))},
        {QStringLiteral("Отсечка H10 / H20"),
         QStringLiteral("%1 / %2 ГГц").arg(num(g.fc, 4), num(2 * g.fc, 4))},
    };
}

QString lgText(double a, double f, double eps = 1.0)
{
    const double lg = rf::lambdaGuide(a, f, eps);
    return lg > 0.0 ? mm(lg) : QStringLiteral("ниже отсечки");
}

/// Стандартная шапка описания задания (идёт в .tpl комментарием, поэтому без
/// греческих букв и стрелок — их нет в CP1251).
QString describe(const QString &what, const Guide &g)
{
    return QStringLiteral("%1. Волновод %2, a = %3 мм, H10.")
        .arg(what, g.std.name, num(g.std.a, 5));
}

// ------------------------------------------------------------ отрезок волновода

Build buildSection(const Values &v)
{
    Guide g = layout(int(v.value(QStringLiteral("std"))));
    const double eps = v.value(QStringLiteral("eps"));
    const double lenLg = v.value(QStringLiteral("len"));
    const int n = int(v.value(QStringLiteral("n")));

    // Заполнение опускает отсечку в sqrt(eps) раз — вместе с ней уезжает и полоса.
    const double fcEff = g.fc / std::sqrt(eps);
    const double lo = eps > 1.0 ? std::round(1.25 * fcEff * 100) / 100 : g.std.fLo;
    const double hi = eps > 1.0 ? std::round(1.9 * fcEff * 100) / 100 : g.std.fHi;
    const double f0 = (lo + hi) / 2.0;

    Builder b(QStringLiteral("wg"));
    b.p.delta = g.d;
    const double lg0 = rf::lambdaGuide(g.a, f0, eps);
    const double L = snap(lenLg * lg0, g.d);
    b.p.box = {0.0, L, 0.0, g.a + 6 * g.d};
    b.p.description = describe(QStringLiteral("Отрезок волновода"), g);

    b.guideX(QStringLiteral("волновод"), 0.0, L, g.ylo, g.yhi);
    if (eps > 1.0)
        b.dielectric(QStringLiteral("заполнение"), eps, 0.0, L, g.ylo, g.yhi);
    Port &p1 = b.port(QStringLiteral("вход 1"), PortSide::Left, g.ylo, g.yhi, true);
    Port &p2 = b.port(QStringLiteral("вход 2"), PortSide::Right, g.ylo, g.yhi, false);
    p1.eps = eps;
    p2.eps = eps;
    b.freqs(lo, hi, n);
    b.outputs(true);
    b.time(settleTime(L, lo, fcEff));

    QVector<Row> t = guideRows(g);
    if (eps > 1.0) {
        t << Row{QStringLiteral("С заполнением eps = %1").arg(num(eps)),
                 QStringLiteral("отсечка %1 ГГц, полоса %2 - %3 ГГц")
                     .arg(num(fcEff, 4), num(lo, 4), num(hi, 4))};
    }
    t << Row{QStringLiteral("λв на %1 / %2 / %3 ГГц").arg(num(lo, 4), num(f0, 4), num(hi, 4)),
             QStringLiteral("%1 / %2 / %3")
                 .arg(lgText(g.a, lo, eps), lgText(g.a, f0, eps), lgText(g.a, hi, eps))};
    t << Row{QStringLiteral("Длина отрезка"),
             QStringLiteral("%1 (%2 λв на %3 ГГц)").arg(mm(L), num(L / lg0), num(f0, 4))};
    t << Row{QStringLiteral("Ожидаемо"), QStringLiteral("|S11| = 0, |S21| = 1 во всей полосе")};
    return b.done(t, {QStringLiteral("Стенки — металл толщиной в шаг сетки, как в авторских "
                                     "заданиях. Шаг сетки a/40 = %1 мм.").arg(num(g.d, 4))});
}

// ------------------------------------------------------------ диафрагма

Build buildIris(const Values &v)
{
    Guide g = layout(int(v.value(QStringLiteral("std"))));
    const double ratio = v.value(QStringLiteral("d"));
    const bool sym = v.value(QStringLiteral("sym")) > 0.5;
    const double tRel = v.value(QStringLiteral("t"));

    Builder b(QStringLiteral("iris"));
    b.p.delta = g.d;
    const double lg0 = rf::lambdaGuide(g.a, g.f0);
    const double L = snap(4.0 * lg0, g.d);
    const double xc = snap(L / 2.0, g.d);
    const double t = std::max(2 * g.d, snap(tRel * g.a, g.d));
    // Окно — по узлам сетки: у симметричной диафрагмы каждая «щека» — целое число ячеек.
    const double cheek = sym ? snap((1.0 - ratio) * g.a / 2.0, g.d) : snap((1.0 - ratio) * g.a, g.d);
    const double dWin = sym ? g.a - 2 * cheek : g.a - cheek;
    b.p.box = {0.0, L, 0.0, g.a + 6 * g.d};
    b.p.description = describe(sym ? QStringLiteral("Симметричная индуктивная диафрагма")
                                   : QStringLiteral("Несимметричная индуктивная диафрагма"),
                               g);

    b.guideX(QStringLiteral("волновод"), 0.0, L, g.ylo, g.yhi);
    b.rect(QStringLiteral("диафрагма, нижняя щека"), Material::Metal, xc - t / 2, xc + t / 2, g.ylo,
           g.ylo + cheek);
    if (sym)
        b.rect(QStringLiteral("диафрагма, верхняя щека"), Material::Metal, xc - t / 2, xc + t / 2,
               g.yhi - cheek, g.yhi);
    b.port(QStringLiteral("вход 1"), PortSide::Left, g.ylo, g.yhi, true);
    b.port(QStringLiteral("вход 2"), PortSide::Right, g.ylo, g.yhi, false);
    b.freqs(g.std.fLo, g.std.fHi, int(v.value(QStringLiteral("n"))));
    b.outputs(true);
    b.time(settleTime(L, g.std.fLo, g.fc));

    QVector<Row> rows = guideRows(g);
    rows << Row{QStringLiteral("Окно d (по сетке)"),
                QStringLiteral("%1 (d/a = %2)").arg(mm(dWin), num(dWin / g.a))};
    rows << Row{QStringLiteral("Толщина"), mm(t)};
    if (sym) {
        for (double f : {g.std.fLo, g.f0, g.std.fHi}) {
            const double bb = rf::irisSusceptance(g.a, dWin, f);
            rows << Row{QStringLiteral("Тонкая диафрагма, %1 ГГц").arg(num(f, 4)),
                        QStringLiteral("B/Y0 = %1, |S11| = %2")
                            .arg(num(bb), db(rf::shuntReflection(bb)))};
        }
    }
    QStringList notes;
    notes << QStringLiteral("Теория — приближение бесконечно тонкой диафрагмы (Маркувиц); "
                            "толщина делает диафрагму «сильнее», |S11| ядра будет выше.");
    if (!sym)
        notes << QStringLiteral("Для несимметричной диафрагмы простой формулы нет — смотрите "
                                "результат ядра.");
    return b.done(rows, notes);
}

// ------------------------------------------------------------ штырь

Build buildPost(const Values &v)
{
    Guide g = layout(int(v.value(QStringLiteral("std"))), 60);
    const double dRel = v.value(QStringLiteral("dia"));
    const double off = v.value(QStringLiteral("off"));

    Builder b(QStringLiteral("post"));
    b.p.delta = g.d;
    const double lg0 = rf::lambdaGuide(g.a, g.f0);
    const double L = snap(4.0 * lg0, g.d);
    const double dia = std::max(2 * g.d, dRel * g.a);
    const QPointF c(snap(L / 2.0, g.d), (g.ylo + g.yhi) / 2.0 + off * g.a);
    b.p.box = {0.0, L, 0.0, g.a + 6 * g.d};
    b.p.description = describe(QStringLiteral("Индуктивный штырь"), g);

    b.guideX(QStringLiteral("волновод"), 0.0, L, g.ylo, g.yhi);
    b.circle(QStringLiteral("штырь"), Material::Metal, c, dia / 2.0);
    b.port(QStringLiteral("вход 1"), PortSide::Left, g.ylo, g.yhi, true);
    b.port(QStringLiteral("вход 2"), PortSide::Right, g.ylo, g.yhi, false);
    b.freqs(g.std.fLo, g.std.fHi, int(v.value(QStringLiteral("n"))));
    b.outputs(true);
    b.time(settleTime(L, g.std.fLo, g.fc));

    QVector<Row> rows = guideRows(g);
    rows << Row{QStringLiteral("Диаметр штыря"), QStringLiteral("%1 (d/a = %2)").arg(mm(dia), num(dia / g.a))};
    rows << Row{QStringLiteral("Смещение от оси"), mm(off * g.a)};
    if (std::fabs(off) < 1e-9) {
        for (double f : {g.std.fLo, g.f0, g.std.fHi}) {
            const double x = rf::postReactance(g.a, dia, f);
            rows << Row{QStringLiteral("Тонкий штырь, %1 ГГц").arg(num(f, 4)),
                        QStringLiteral("X/Z0 = %1, |S11| = %2")
                            .arg(num(x), db(1.0 / std::sqrt(1.0 + 4.0 * x * x)))};
        }
    }
    return b.done(rows,
                  {QStringLiteral("Теория — тонкий штырь в центре (Маркувиц); у толстого штыря и "
                                  "при смещении отражение другое — его и показывает ядро."),
                   QStringLiteral("Шаг сетки мельче обычного, a/60: круг на сетке — «лесенка».")});
}

// ------------------------------------------------------------ ступенька

Build buildStep(const Values &v)
{
    Guide g = layout(int(v.value(QStringLiteral("std"))));
    const double ratio = v.value(QStringLiteral("ratio"));

    Builder b(QStringLiteral("step"));
    b.p.delta = g.d;
    const double a2 = g.a - 2.0 * snap((g.a - ratio * g.a) / 2.0, g.d);
    const double fc2 = kLightSpeed / (2 * a2);
    // Полоса — там, где обе половины одномодовые. Стык симметричный, поэтому H20 он
    // не возбуждает; следующая — H30.
    const double lo = std::max(g.std.fLo, std::round(1.1 * fc2 * 100) / 100);
    const double hi = std::min(g.std.fHi, std::round(0.95 * 3.0 * fc2 * 100) / 100);
    const double lg0 = rf::lambdaGuide(g.a, (lo + hi) / 2.0);
    const double half = snap(2.0 * lg0, g.d);
    const double L = 2 * half;
    const double wMax = std::max(g.a, a2);
    const double yc = 3 * g.d + wMax / 2.0;
    const double y1lo = yc - g.a / 2, y1hi = yc + g.a / 2;
    const double y2lo = yc - a2 / 2, y2hi = yc + a2 / 2;
    b.p.box = {0.0, L, 0.0, wMax + 6 * g.d};
    b.p.description = describe(QStringLiteral("Ступенька по ширине волновода"), g);

    b.polyline(QStringLiteral("нижняя стенка"), Material::Metal,
               {QPointF(0, y1lo), QPointF(half, y1lo), QPointF(half, y2lo), QPointF(L, y2lo)});
    b.polyline(QStringLiteral("верхняя стенка"), Material::Metal,
               {QPointF(0, y1hi), QPointF(half, y1hi), QPointF(half, y2hi), QPointF(L, y2hi)});
    const int id1 = b.port(QStringLiteral("вход 1 (a1)"), PortSide::Left, y1lo, y1hi, true).id;
    const int id2 = b.port(QStringLiteral("вход 2 (a2)"), PortSide::Right, y2lo, y2hi, false).id;
    b.p.exciters = {id1, id2};
    b.freqs(lo, hi, 21);
    b.outputs(true);
    b.time(settleTime(L, lo, std::max(g.fc, fc2)));

    QVector<Row> rows = guideRows(g);
    rows << Row{QStringLiteral("Ширины a1 -> a2"), QStringLiteral("%1 -> %2 мм").arg(num(g.a, 5), num(a2, 5))};
    rows << Row{QStringLiteral("Отсечки H10"), QStringLiteral("%1 / %2 ГГц").arg(num(g.fc, 4), num(fc2, 4))};
    rows << Row{QStringLiteral("Полоса расчёта"),
                QStringLiteral("%1 - %2 ГГц (обе половины одномодовые)").arg(num(lo, 4), num(hi, 4))};
    return b.done(rows, {QStringLiteral("Возбуждаются оба входа по очереди — в .s полная матрица "
                                        "2x2. Входы разной ширины, поэтому S11 и S22 разные.")});
}

// ------------------------------------------------------------ изгиб

Build buildBend(const Values &v)
{
    Guide g = layout(int(v.value(QStringLiteral("std"))));
    const double cut = v.value(QStringLiteral("cut"));

    Builder b(QStringLiteral("bend"));
    b.p.delta = g.d;
    const double lg0 = rf::lambdaGuide(g.a, g.f0);
    const double arm = snap(2.0 * lg0, g.d);
    const double xc = arm;                 // начало угла
    const double ytop = g.yhi + arm;       // конец вертикального плеча
    const double m = snap(cut * g.a, g.d); // срез наружного угла по каждой стороне
    b.p.box = {0.0, xc + g.a + 3 * g.d, 0.0, ytop};
    b.p.description = describe(QStringLiteral("H-изгиб 90 град. со срезанным углом"), g);

    QVector<QPointF> outer;
    outer << QPointF(0, g.ylo);
    if (m > 0.0)
        outer << QPointF(xc + g.a - m, g.ylo) << QPointF(xc + g.a, g.ylo + m);
    else
        outer << QPointF(xc + g.a, g.ylo);
    outer << QPointF(xc + g.a, ytop);
    b.polyline(QStringLiteral("наружная стенка"), Material::Metal, outer);
    b.polyline(QStringLiteral("внутренняя стенка"), Material::Metal,
               {QPointF(0, g.yhi), QPointF(xc, g.yhi), QPointF(xc, ytop)});
    b.port(QStringLiteral("вход 1"), PortSide::Left, g.ylo, g.yhi, true);
    b.port(QStringLiteral("вход 2"), PortSide::Top, xc, xc + g.a, false);
    b.freqs(g.std.fLo, g.std.fHi, int(v.value(QStringLiteral("n"))));
    b.outputs(true);
    b.time(settleTime(2 * arm + g.a, g.std.fLo, g.fc));

    QVector<Row> rows = guideRows(g);
    rows << Row{QStringLiteral("Срез угла"),
                m > 0.0 ? QStringLiteral("%1 по каждой стенке (%2 a)").arg(mm(m), num(m / g.a))
                        : QStringLiteral("нет — прямой угол")};
    rows << Row{QStringLiteral("Плечи"), QStringLiteral("по %1 (2 λв)").arg(mm(arm))};
    return b.done(rows, {QStringLiteral("Формулы для срезанного изгиба нет — его подбирают. "
                                        "Поменяйте долю среза и сравните |S11|: прямой угол "
                                        "отражает заметно сильнее.")});
}

// ------------------------------------------------------------ тройник

Build buildTee(const Values &v)
{
    Guide g = layout(int(v.value(QStringLiteral("std"))));

    Builder b(QStringLiteral("tee"));
    b.p.delta = g.d;
    const double lg0 = rf::lambdaGuide(g.a, g.f0);
    const double arm = snap(2.0 * lg0, g.d);
    const double L = 2 * arm + g.a;
    const double xl = arm, xr = arm + g.a;
    const double ytop = g.yhi + arm;
    b.p.box = {0.0, L, 0.0, ytop};
    b.p.description = describe(QStringLiteral("H-плоскостной тройник"), g);

    b.polyline(QStringLiteral("нижняя стенка"), Material::Metal,
               {QPointF(0, g.ylo), QPointF(L / 2, g.ylo), QPointF(L, g.ylo)});
    b.polyline(QStringLiteral("стенка слева от ответвления"), Material::Metal,
               {QPointF(0, g.yhi), QPointF(xl, g.yhi), QPointF(xl, ytop)});
    b.polyline(QStringLiteral("стенка справа от ответвления"), Material::Metal,
               {QPointF(xr, ytop), QPointF(xr, g.yhi), QPointF(L, g.yhi)});
    const Port &p1 = b.port(QStringLiteral("вход 1"), PortSide::Left, g.ylo, g.yhi, true);
    const int id1 = p1.id;
    const int id2 = b.port(QStringLiteral("вход 2"), PortSide::Right, g.ylo, g.yhi, false).id;
    const int id3 = b.port(QStringLiteral("вход 3 (ответвление)"), PortSide::Top, xl, xr, false).id;
    b.p.exciters = {id1, id2, id3};
    b.freqs(g.std.fLo, g.std.fHi, int(v.value(QStringLiteral("n"))));
    b.outputs(true);
    b.time(settleTime(L + arm, g.std.fLo, g.fc));

    QVector<Row> rows = guideRows(g);
    rows << Row{QStringLiteral("Плечи"), QStringLiteral("по %1").arg(mm(arm))};
    rows << Row{QStringLiteral("Шагов расчёта"),
                QStringLiteral("%1 = 3 возбуждения x %2 частот")
                    .arg(b.p.stepCount()).arg(b.p.freqs.size())};
    return b.done(rows, {QStringLiteral("Все три входа возбуждаются по очереди — в .s полная "
                                        "матрица 3x3. Идеально согласовать все три плеча "
                                        "тройника без потерь нельзя — это видно по S11, S22, S33.")});
}

// ------------------------------------------------------------ фильтр

Build buildFilter(const Values &v)
{
    Guide g = layout(int(v.value(QStringLiteral("std"))), 48);
    const int n = int(v.value(QStringLiteral("order")));
    const double fbw = v.value(QStringLiteral("fbw")) / 100.0;
    const double ripple = v.value(QStringLiteral("ripple"));
    double f0 = v.value(QStringLiteral("f0"));
    QStringList notes;
    // Центр полосы обязан лежать в рабочей полосе выбранного сечения.
    const double fMin = g.std.fLo / (1.0 - fbw);
    const double fMax = g.std.fHi / (1.0 + fbw);
    if (f0 < fMin || f0 > fMax) {
        const double fixed = std::clamp(f0, fMin, fMax);
        notes << QStringLiteral("Частота %1 ГГц вне полосы сечения — взята %2 ГГц.")
                     .arg(num(f0, 4), num(fixed, 4));
        f0 = std::round(fixed * 100) / 100;
    }

    // --- расчёт по Маттею (фильтр на связанных резонаторах с индуктивными окнами) ---
    const double f1 = f0 * (1.0 - fbw / 2.0), f2 = f0 * (1.0 + fbw / 2.0);
    const double lg0 = rf::lambdaGuide(g.a, f0);
    const double w = (rf::lambdaGuide(g.a, f1) - rf::lambdaGuide(g.a, f2)) / lg0;
    const QVector<double> gp = rf::chebyshev(n, ripple);
    QVector<double> K(n + 1), X(n + 1), dWin(n + 1);
    for (int j = 0; j <= n; ++j) {
        if (j == 0)
            K[j] = std::sqrt(kPi * w / (2.0 * gp[0] * gp[1]));
        else if (j == n)
            K[j] = std::sqrt(kPi * w / (2.0 * gp[n] * gp[n + 1]));
        else
            K[j] = kPi * w / (2.0 * std::sqrt(gp[j] * gp[j + 1]));
        X[j] = K[j] / (1.0 - K[j] * K[j]);
        dWin[j] = rf::irisWindowForReactance(g.a, X[j], f0);
    }
    QVector<double> len(n + 1);
    for (int j = 1; j <= n; ++j) {
        const double theta = kPi - 0.5 * (std::atan(2.0 * X[j - 1]) + std::atan(2.0 * X[j]));
        len[j] = theta * lg0 / (2.0 * kPi);
    }

    Builder b(QStringLiteral("filter"));
    b.p.delta = g.d;
    const double t = 2 * g.d;
    const double lead = snap(1.5 * lg0, g.d);
    QVector<double> xs;
    double x = lead;
    xs << x;
    for (int j = 1; j <= n; ++j) {
        x += len[j];
        xs << x;
    }
    const double L = snap(x + lead, g.d);
    b.p.box = {0.0, L, 0.0, g.a + 6 * g.d};
    b.p.description = QStringLiteral("Фильтр Чебышёва %1-го порядка на индуктивных диафрагмах: "
                                     "f0 = %2 ГГц, полоса %3 %. Волновод %4.")
                          .arg(n).arg(num(f0, 4), num(fbw * 100.0), g.std.name);

    b.guideX(QStringLiteral("волновод"), 0.0, L, g.ylo, g.yhi);
    for (int j = 0; j <= n; ++j) {
        const double cheek = (g.a - dWin[j]) / 2.0;
        b.rect(QStringLiteral("диафрагма %1, низ").arg(j + 1), Material::Metal, xs[j] - t / 2,
               xs[j] + t / 2, g.ylo, g.ylo + cheek);
        b.rect(QStringLiteral("диафрагма %1, верх").arg(j + 1), Material::Metal, xs[j] - t / 2,
               xs[j] + t / 2, g.yhi - cheek, g.yhi);
    }
    b.port(QStringLiteral("вход 1"), PortSide::Left, g.ylo, g.yhi, true);
    b.port(QStringLiteral("вход 2"), PortSide::Right, g.ylo, g.yhi, false);
    const double bw = fbw * f0;
    const double lo = std::max(g.std.fLo, f0 - 2.0 * bw);
    const double hi = std::min(g.std.fHi, f0 + 2.0 * bw);
    b.freqs(lo, hi, int(v.value(QStringLiteral("n"))));
    b.outputs(true);
    b.time(settleTime(L, lo, g.fc, 5.0 * n / (kPi * bw)));

    QVector<Row> rows = guideRows(g);
    rows << Row{QStringLiteral("Полоса пропускания"),
                QStringLiteral("%1 - %2 ГГц, пульсации %3 дБ").arg(num(f1, 4), num(f2, 4), num(ripple))};
    QStringList gs;
    for (int j = 1; j <= n; ++j)
        gs << num(gp[j], 4);
    rows << Row{QStringLiteral("Прототип g1...g%1").arg(n), gs.join(QStringLiteral(", "))};
    for (int j = 0; j <= n; ++j)
        rows << Row{QStringLiteral("Окно %1").arg(j + 1),
                    QStringLiteral("%1 (X/Z0 = %2)").arg(mm(dWin[j]), num(X[j]))};
    for (int j = 1; j <= n; ++j)
        rows << Row{QStringLiteral("Резонатор %1").arg(j), mm(len[j])};
    notes << QStringLiteral("Расчёт по Маттею в приближении бесконечно тонких диафрагм. У "
                            "диафрагм толщиной %1 на сетке полоса ядра уходит ВВЕРХ примерно на "
                            "3 %, а согласование в полосе хуже расчётного (проверено прогоном: "
                            "для умолчаний полоса 10.05-10.55 ГГц вместо 9.75-10.25). Дальше "
                            "фильтр настраивают — удлиняют резонаторы и подбирают окна; это и "
                            "есть учебная задача.")
                 .arg(mm(t));
    return b.done(rows, notes);
}

// ------------------------------------------------------------ полуволновое окно

Build buildWindow(const Values &v)
{
    Guide g = layout(int(v.value(QStringLiteral("std"))));
    const double eps = v.value(QStringLiteral("eps"));
    const double k = v.value(QStringLiteral("thick"));

    Builder b(QStringLiteral("window"));
    b.p.delta = g.d;
    const double lgEps = rf::lambdaGuide(g.a, g.f0, eps);
    const double t = std::max(g.d, snap(k * lgEps / 2.0, g.d));
    const double lg0 = rf::lambdaGuide(g.a, g.f0);
    const double L = snap(4.0 * lg0 + t, g.d);
    const double x0 = snap((L - t) / 2.0, g.d);
    b.p.box = {0.0, L, 0.0, g.a + 6 * g.d};
    b.p.description = describe(QStringLiteral("Диэлектрическое окно (полуволновая пластина)"), g);

    b.guideX(QStringLiteral("волновод"), 0.0, L, g.ylo, g.yhi);
    b.dielectric(QStringLiteral("пластина"), eps, x0, x0 + t, g.ylo, g.yhi);
    b.port(QStringLiteral("вход 1"), PortSide::Left, g.ylo, g.yhi, true);
    b.port(QStringLiteral("вход 2"), PortSide::Right, g.ylo, g.yhi, false);
    b.freqs(g.std.fLo, g.std.fHi, int(v.value(QStringLiteral("n"))));
    b.outputs(true);
    b.time(settleTime(L, g.std.fLo, g.fc));

    QVector<Row> rows = guideRows(g);
    rows << Row{QStringLiteral("Пластина"),
                QStringLiteral("eps = %1, толщина %2 (%3 λв/2 в диэлектрике на %4 ГГц)")
                    .arg(num(eps), mm(t), num(2 * t / lgEps), num(g.f0, 4))};
    for (double f : {g.std.fLo, g.f0, g.std.fHi}) {
        const double r = std::abs(rf::slabReflection(g.a, eps, t, f));
        rows << Row{QStringLiteral("Точно, %1 ГГц").arg(num(f, 4)),
                    QStringLiteral("|S11| = %1 (%2)").arg(num(r), db(r))};
    }
    return b.done(rows, {QStringLiteral("Слой толщиной в полволны прозрачен: отражения от двух "
                                        "его граней гасят друг друга. Формула точная — хорошая "
                                        "проверка ядра.")});
}

// ------------------------------------------------------------ нагрузка

Build buildLoad(const Values &v)
{
    Guide g = layout(int(v.value(QStringLiteral("std"))));
    const double wedge = v.value(QStringLiteral("wedge"));

    Builder b(QStringLiteral("load"));
    b.p.delta = g.d;
    const double lg0 = rf::lambdaGuide(g.a, g.f0);
    const double lead = snap(2.0 * lg0, g.d);
    const double lw = snap(wedge * lg0, g.d);
    const double xEnd = lead + lw;
    const double L = xEnd + 3 * g.d;
    const double yc = (g.ylo + g.yhi) / 2.0;
    b.p.box = {0.0, L, 0.0, g.a + 6 * g.d};
    b.p.description = describe(QStringLiteral("Нагрузка волновода: короткое замыкание, "
                                              "плоский поглотитель и клин"),
                               g);

    b.guideX(QStringLiteral("волновод"), 0.0, xEnd, g.ylo, g.yhi);
    const QVector<QPointF> across{QPointF(xEnd, g.ylo), QPointF(xEnd, yc), QPointF(xEnd, g.yhi)};
    const int shortId = b.polyline(QStringLiteral("короткозамыкатель"), Material::Metal, across).id;
    const int flatId = b.polyline(QStringLiteral("поглотитель в торце"), Material::Absorber, across).id;
    const int wedgeId = b.polygon(QStringLiteral("клин поглотителя"), Material::Absorber,
                                  {QPointF(xEnd, g.ylo), QPointF(xEnd, g.yhi), QPointF(lead, yc)})
                            .id;
    b.port(QStringLiteral("вход"), PortSide::Left, g.ylo, g.yhi, true);

    // Три варианта в одном задании: короткое замыкание (|S11| = 1 — проверка
    // постановки), плоский поглотитель в торце и клин перед короткозамыкателем.
    Variant sc;
    sc.name = QStringLiteral("короткое замыкание");
    sc.tag = QStringLiteral("short");
    sc.disabledShapes = {flatId, wedgeId};
    Variant flat;
    flat.name = QStringLiteral("плоский поглотитель");
    flat.tag = QStringLiteral("flat");
    flat.disabledShapes = {shortId, wedgeId};
    Variant wedgeV;
    wedgeV.name = QStringLiteral("клин");
    wedgeV.tag = QStringLiteral("wedge");
    wedgeV.disabledShapes = {flatId};
    b.p.variants << sc << flat << wedgeV;

    b.freqs(g.std.fLo, g.std.fHi, int(v.value(QStringLiteral("n"))));
    b.outputs(true);
    b.time(settleTime(L, g.std.fLo, g.fc));

    QVector<Row> rows = guideRows(g);
    rows << Row{QStringLiteral("Клин"), QStringLiteral("длина %1 (%2 λв)").arg(mm(lw), num(lw / lg0))};
    // Поглотитель ядра согласован для НОРМАЛЬНОГО падения. Волна H10 — две плоские
    // волны под углом к оси, у которого cos = λ/λв, и от плоского поглотителя они
    // отражаются как от сопротивления, согласованного не под тем углом.
    for (double f : {g.std.fLo, g.f0, g.std.fHi}) {
        const double c = rf::lambda0(f) / rf::lambdaGuide(g.a, f);
        rows << Row{QStringLiteral("Плоский поглотитель, %1 ГГц").arg(num(f, 4)),
                    QStringLiteral("|S11| = (1 - λ/λв)/(1 + λ/λв) = %1").arg(db((1 - c) / (1 + c)))};
    }
    rows << Row{QStringLiteral("Короткое замыкание"), QStringLiteral("|S11| = 1 (0 дБ)")};
    return b.done(rows, {QStringLiteral("Варианты считаются одним заданием: load_short.s, "
                                        "load_flat.s и load_wedge.s. Поглотитель ядра согласован "
                                        "для нормального падения, а волна H10 бежит к торцу "
                                        "наклонными плоскими волнами — отсюда формула для "
                                        "плоского поглотителя. Для клина формулы нет.")});
}

QString helpCommon()
{
    return QStringLiteral(
        "\n\nКак считать: сохраните задание и запустите ядро tmc_rth. Перед повторным "
        "прогоном удалите старый .s — ядро дописывает его, а не перезаписывает. "
        "Частотные характеристики открываются в TMCGROUT, сигналы — в TMCROS.");
}

} // namespace

void addWaveguides(QVector<Sample> *out)
{
    const QString sec = QStringLiteral("waveguide");
    const QString stdRef = QStringLiteral("EIA RS-261, IEC 60153-2, ГОСТ 20900-75");

    {
        Sample s;
        s.id = QStringLiteral("wg_section");
        s.section = sec;
        s.title = QStringLiteral("Отрезок прямоугольного волновода");
        s.summary = QStringLiteral("Стандартное сечение, свип по рабочей полосе");
        s.standard = stdRef;
        s.help = QStringLiteral(
                     "Отрезок регулярного волновода с волной H10 на входе и выходе. Это «нулевой» "
                     "образец: на нём проверяют постановку — |S11| должен быть около нуля, а |S21| "
                     "— единица во всей полосе. Фаза S21 показывает длину волны в волноводе: чем "
                     "ближе к отсечке, тем длиннее λв.\n\nЗаполнение диэлектриком опускает отсечку "
                     "в sqrt(eps) раз, и программа сама сдвигает полосу свипа. Проницаемость "
                     "в сечении входа ставится той же — иначе вход окажется рассогласован.")
                 + helpCommon();
        s.params = {stdParam(),
                    Param::number(QStringLiteral("len"), QStringLiteral("Длина"),
                                  QStringLiteral("λв"), 3.0, 0.5, 20.0, 0.5, 1,
                                  QStringLiteral("В длинах волны в волноводе на середине полосы")),
                    Param::number(QStringLiteral("eps"), QStringLiteral("Заполнение, eps"), {},
                                  1.0, 1.0, 12.0, 0.1, 2,
                                  QStringLiteral("1 — пустой волновод")),
                    pointsParam()};
        s.build = buildSection;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("wg_iris");
        s.section = sec;
        s.title = QStringLiteral("Индуктивная диафрагма");
        s.summary = QStringLiteral("Окно в поперечной стенке — шунтирующая индуктивность");
        s.standard = stdRef;
        s.help = QStringLiteral(
                     "Тонкая металлическая стенка поперёк волновода с окном по ширине. Для волны "
                     "H10 это шунтирующая индуктивность: чем уже окно, тем сильнее отражение. "
                     "Диафрагмы — основной элемент волноводных фильтров и согласующих устройств.\n\n"
                     "В паспорте — нормированная реактивная проводимость B/Y0 по формуле "
                     "Маркувица для бесконечно тонкой диафрагмы. Ядро считает диафрагму реальной "
                     "толщины, поэтому отражение выйдет несколько сильнее.")
                 + helpCommon();
        s.params = {stdParam(),
                    Param::number(QStringLiteral("d"), QStringLiteral("Окно, доля a"), {}, 0.5,
                                  0.2, 0.95, 0.05, 2),
                    Param::flag(QStringLiteral("sym"), QStringLiteral("Симметричная"), true),
                    Param::number(QStringLiteral("t"), QStringLiteral("Толщина, доля a"), {}, 0.05,
                                  0.05, 0.5, 0.01, 2),
                    pointsParam()};
        s.build = buildIris;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("wg_post");
        s.section = sec;
        s.title = QStringLiteral("Индуктивный штырь");
        s.summary = QStringLiteral("Металлический стержень поперёк волновода");
        s.standard = stdRef;
        s.help = QStringLiteral(
                     "Металлический штырь, перекрывающий волновод по высоте. В H-плоскости — "
                     "круг. Действует как шунтирующая индуктивность; смещение от оси ослабляет "
                     "связь с волной H10. Штыри применяют в фильтрах и для настройки.\n\n"
                     "Теория — формула Маркувица для тонкого центрального штыря.")
                 + helpCommon();
        s.params = {stdParam(),
                    Param::number(QStringLiteral("dia"), QStringLiteral("Диаметр, доля a"), {}, 0.1,
                                  0.04, 0.4, 0.01, 2),
                    Param::number(QStringLiteral("off"), QStringLiteral("Смещение от оси, доля a"),
                                  {}, 0.0, -0.3, 0.3, 0.05, 2),
                    pointsParam()};
        s.build = buildPost;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("wg_step");
        s.section = sec;
        s.title = QStringLiteral("Ступенька по ширине");
        s.summary = QStringLiteral("Стык двух волноводов разной ширины");
        s.standard = stdRef;
        s.help = QStringLiteral(
                     "Симметричный стык двух волноводов разной ширины. Отсечки у половин разные, "
                     "поэтому свип идёт только там, где обе половины одномодовые: программа "
                     "вычисляет эту полосу сама. Оба входа возбуждаются по очереди — получается "
                     "полная матрица 2x2.")
                 + helpCommon();
        s.params = {stdParam(),
                    Param::number(QStringLiteral("ratio"), QStringLiteral("Отношение a2/a1"), {},
                                  0.85, 0.7, 1.4, 0.05, 2)};
        s.build = buildStep;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("wg_bend");
        s.section = sec;
        s.title = QStringLiteral("H-изгиб 90°");
        s.summary = QStringLiteral("Поворот волновода со срезанным наружным углом");
        s.standard = stdRef;
        s.help = QStringLiteral(
                     "Поворот волновода на 90 градусов в H-плоскости. Прямой угол заметно отражает; "
                     "срез наружного угла уменьшает отражение. Оптимальный срез подбирают — "
                     "меняйте долю среза и сравнивайте |S11|.\n\nВторой вход стоит на верхней "
                     "границе области: вход вдоль оси X (INPUT_Y) программа выбирает сама.")
                 + helpCommon();
        s.params = {stdParam(),
                    Param::number(QStringLiteral("cut"), QStringLiteral("Срез угла, доля a"), {},
                                  0.6, 0.0, 0.95, 0.05, 2),
                    pointsParam()};
        s.build = buildBend;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("wg_tee");
        s.section = sec;
        s.title = QStringLiteral("H-плоскостной тройник");
        s.summary = QStringLiteral("Разветвление на три плеча, матрица 3x3");
        s.standard = stdRef;
        s.help = QStringLiteral(
                     "Ответвление в H-плоскости. Все три входа возбуждаются по очереди в ОДНОМ "
                     "задании — ядро заполняет полную матрицу рассеяния 3x3. Без согласующих "
                     "элементов тройник отражает: три плеча без потерь одновременно согласовать "
                     "нельзя.")
                 + helpCommon();
        s.params = {stdParam(), pointsParam(11)};
        s.build = buildTee;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("wg_filter");
        s.section = sec;
        s.title = QStringLiteral("Фильтр на индуктивных диафрагмах");
        s.summary = QStringLiteral("Полосовой фильтр Чебышёва, расчёт по Маттею");
        s.standard = QStringLiteral("Маттей, Янг, Джонс; ") + stdRef;
        s.help = QStringLiteral(
                     "Полосовой фильтр из цепочки полуволновых резонаторов, связанных "
                     "индуктивными диафрагмами. Размеры считаются по классической методике "
                     "Маттея: прототип Чебышёва -> инверторы сопротивления -> реактивности "
                     "диафрагм -> ширины окон и длины резонаторов. Весь расчёт — в паспорте.\n\n"
                     "Методика предполагает бесконечно тонкие диафрагмы; ядро считает реальные, "
                     "поэтому полоса немного сдвинется — ровно так выглядит настройка реального "
                     "фильтра.")
                 + helpCommon();
        s.params = {stdParam(),
                    Param::number(QStringLiteral("f0"), QStringLiteral("Центральная частота"),
                                  QStringLiteral("ГГц"), 10.0, 0.5, 110.0, 0.1, 3),
                    Param::number(QStringLiteral("fbw"), QStringLiteral("Полоса пропускания"),
                                  QStringLiteral("%"), 5.0, 1.0, 15.0, 0.5, 1),
                    Param::integer(QStringLiteral("order"), QStringLiteral("Порядок"), 3, 1, 7),
                    Param::number(QStringLiteral("ripple"), QStringLiteral("Пульсации"),
                                  QStringLiteral("дБ"), 0.1, 0.01, 1.0, 0.05, 2),
                    pointsParam(31)};
        s.build = buildFilter;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("wg_window");
        s.section = sec;
        s.title = QStringLiteral("Диэлектрическое окно λ/2");
        s.summary = QStringLiteral("Полуволновая пластина: точная формула для проверки ядра");
        s.standard = stdRef;
        s.help = QStringLiteral(
                     "Диэлектрическая пластина поперёк волновода — так делают герметичные окна. "
                     "При толщине в половину длины волны (в диэлектрике, с учётом волновода) "
                     "отражения от двух граней гасят друг друга, и окно прозрачно на центральной "
                     "частоте.\n\nФормула однородного слоя точная, поэтому паспорт даёт |S11| на "
                     "трёх частотах — сравните с тем, что посчитает ядро.")
                 + helpCommon();
        s.params = {stdParam(),
                    Param::number(QStringLiteral("eps"), QStringLiteral("Проницаемость eps"), {},
                                  2.1, 1.1, 20.0, 0.1, 2,
                                  QStringLiteral("2.1 — фторопласт, 4 — кварц, 9.8 — поликор")),
                    Param::number(QStringLiteral("thick"), QStringLiteral("Толщина, доля λв/2"), {},
                                  1.0, 0.1, 3.0, 0.1, 2),
                    pointsParam()};
        s.build = buildWindow;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("wg_load");
        s.section = sec;
        s.title = QStringLiteral("Нагрузка волновода");
        s.summary = QStringLiteral("Короткое замыкание, плоский поглотитель и клин — три варианта");
        s.standard = stdRef;
        s.help = QStringLiteral(
                     "Чем закончить волновод. Задание считает три варианта сразу: короткое "
                     "замыкание (полное отражение, |S11| = 1 — проверка постановки), плоский "
                     "поглотитель в торце и клин поглотителя перед короткозамыкателем.\n\n"
                     "Поглотитель ядра согласован для нормального падения. Волна H10 — это две "
                     "плоские волны, бегущие под углом к оси, поэтому плоский поглотитель "
                     "частично отражает: |S11| = (1 - λ/λв)/(1 + λ/λв), и тем сильнее, чем "
                     "ближе частота к отсечке. Эту формулу ядро и проверяет.")
                 + helpCommon();
        s.params = {stdParam(),
                    Param::number(QStringLiteral("wedge"), QStringLiteral("Длина клина"),
                                  QStringLiteral("λв"), 3.0, 0.3, 8.0, 0.1, 1),
                    pointsParam(11)};
        s.build = buildLoad;
        *out << s;
    }
}

} // namespace tpl::samples
