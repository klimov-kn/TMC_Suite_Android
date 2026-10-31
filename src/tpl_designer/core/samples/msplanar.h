#pragma once

#include "kit.h"
#include "rf.h"
#include "sample.h"
#include "standards.h"

#include <QPointF>
#include <QString>
#include <QVector>

#include <algorithm>
#include <cmath>

/// Детали планарной волноводной модели — общие для всех рецептов раздела
/// «Микрополосковые».
///
/// Микрополоска — трёхмерная структура, а ядро двумерное. Классический выход —
/// планарная волноводная модель (Весселя-Кольхоффа): полоска над экраном заменяется
/// плоским волноводом ширины W_eff с магнитными боковыми стенками, заполненным средой
/// с eps_eff. Поле Ez в нём однородно по ширине — это и есть основная волна линии,
/// а H-мода ядра (поле Ez, Hx, Hy в плоскости) решает ровно такую задачу.
///
/// Проверено прогоном 2026-09-23: вход плоской волны (MAGNETIC) между магнитными
/// стенками работает, но у самого входа ядра есть остаточное отражение около -25 дБ,
/// не зависящее от шага сетки. Уровни ниже этого в результате не различить — в
/// паспортах так и написано. Лучше всего вход сидит, когда стенки проходят ТОЧНО по
/// краям апертуры: так и строятся все образцы раздела.
///
/// ⚠️ Чего этой моделью выразить НЕЛЬЗЯ, и потому в разделе нет: связанных линий.
/// Боковые стенки канала магнитные, то есть поле за них не выходит вовсе — а связь
/// двух полосок живёт именно в краевом поле. Направленный ответвитель на связанных
/// линиях и фильтр на связанных резонаторах в двумерной постановке не получаются;
/// их место занимают шлейфовые схемы, где связь идёт по самому каналу.
namespace tpl::samples::msp {

using namespace kit;

constexpr double kPi = rf::kPi;

/// Микрополосковая линия заданного волнового сопротивления на подложке.
struct Line {
    double z0 = 50.0;
    double w = 0.0;      ///< ширина полоски, мм
    double epsEff = 1.0;
    double weff = 0.0;   ///< ширина планарной модели, мм

    double lambdaG(double fGHz) const { return rf::lambda0(fGHz) / std::sqrt(epsEff); }
};

struct Board {
    Substrate sub;
    double h = 1.0;

    Line line(double z0) const
    {
        Line l;
        l.z0 = z0;
        const double u = rf::msWidthRatio(z0, sub.eps);
        l.w = u * h;
        l.epsEff = rf::msEpsEff(u, sub.eps);
        l.weff = rf::msPlanarWidth(z0, l.epsEff, h);
        return l;
    }
};

inline Board board(const Values &v)
{
    Board b;
    const QVector<Substrate> &t = substrates();
    b.sub = t.value(std::clamp(int(v.value(QStringLiteral("sub"))), 0, int(t.size()) - 1));
    const double h = v.value(QStringLiteral("h"));
    b.h = h > 0.0 ? h : b.sub.h;
    return b;
}

inline QVector<Param> boardParams()
{
    return {Param::choice(QStringLiteral("sub"), QStringLiteral("Подложка"), substrateChoices(), 0),
            Param::number(QStringLiteral("h"), QStringLiteral("Толщина h (0 — по таблице)"),
                          QStringLiteral("мм"), 0.0, 0.0, 5.0, 0.1, 3)};
}

inline QVector<Row> boardRows(const Board &b)
{
    return {{QStringLiteral("Подложка"),
             QStringLiteral("%1: eps = %2, h = %3 мм").arg(b.sub.name, num(b.sub.eps), num(b.h))}};
}

inline Row lineRow(const QString &what, const Line &l, double f0)
{
    return {what, QStringLiteral("w = %1 мм, eps_eff = %2, W_eff = %3 мм, λв = %4 мм на %5 ГГц")
                      .arg(num(l.w), num(l.epsEff), num(l.weff), num(l.lambdaG(f0), 4), num(f0, 3))};
}

/// Канал планарной модели: заливка eps_eff и магнитные стенки строятся отдельно.
inline void fill(Builder &b, const QString &name, const Line &l, double x0, double x1, double y0,
                 double y1)
{
    b.dielectric(name, l.epsEff, x0, x1, y0, y1);
}

inline Port &msPort(Builder &b, const QString &name, PortSide side, double from, double to,
                    bool active, const Line &l)
{
    Port &p = b.port(name, side, from, to, active, PortMode::Plane);
    // Среда в сечении входа — та же eps_eff: иначе вход рассогласован с линией.
    p.eps = l.epsEff;
    return p;
}

inline double gridFor(const Line &narrowest, double f0)
{
    return chooseDelta(rf::lambda0(f0) / std::sqrt(narrowest.epsEff), narrowest.weff, 20.0, 10.0);
}

/// Стенка-отрезок тремя точками, а не двумя.
///
/// `POLYGON_STAT` не грузится меньше чем с тремя строками `L` — середину всё равно
/// добивал бы эмиттер, так что ставим её сами и видим в задании ровно то, что писали.
inline QVector<QPointF> seg(const QPointF &a, const QPointF &b)
{
    return {a, (a + b) / 2.0, b};
}

inline QString helpPlanar()
{
    return QStringLiteral(
        "\n\nПланарная модель: полоска заменена плоским волноводом ширины W_eff с магнитными "
        "стенками и заполнением eps_eff (квазистатика Хаммерстада). Так микрополоску считают "
        "в двумерной постановке; дисперсию eps_eff модель не учитывает. У входа плоской волны "
        "в ядре есть остаточное отражение около -25 дБ — уровни ниже не различить.");
}

/// Поправка длин на неоднородности, %.
///
/// В планарной модели полоски широкие: у 50-омной линии на FR-4 W_eff около 6 мм при
/// λв/4 около 14 мм. Соединения таких полосок электрически «короче», чем их рисуют по
/// осям, и учебниковый расчёт уводит центр полосы. Поправка подобрана прогоном ядра для
/// умолчаний; при 0 — расчёт по осям, как в учебнике.
inline Param corrParam(double def)
{
    return Param::number(QStringLiteral("corr"), QStringLiteral("Поправка на соединения"),
                         QStringLiteral("%"), def, -40.0, 40.0, 1.0, 0,
                         QStringLiteral("Масштаб длин отрезков. Подобран прогоном ядра для "
                                        "умолчаний; 0 — расчёт по осям, как в учебнике"));
}

inline Param freqParam(double def)
{
    return Param::number(QStringLiteral("f0"), QStringLiteral("Частота f0"), QStringLiteral("ГГц"),
                         def, 0.3, 30.0, 0.1, 2);
}

inline Param z0Param(const QString &label, double def)
{
    return Param::number(QStringLiteral("z0"), label, QStringLiteral("Ом"), def, 20.0, 120.0, 5.0,
                         1);
}

} // namespace tpl::samples::msp
