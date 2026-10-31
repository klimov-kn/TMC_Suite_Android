// Раздел «Среды, плазма, ЭПР»: неоднородная eps, плазма и рассеяние на телах.

#include "emitter.h"
#include "epsfile.h"
#include "kit.h"
#include "rf.h"
#include "sample.h"
#include "standards.h"

#include "physics.h"

#include <QByteArray>

#include <cmath>
#include <cstring>

namespace tpl::samples {

namespace {

using namespace kit;

constexpr double kPi = rf::kPi;

/// Канал плоской волны: две магнитные стенки ТОЧНО по краям апертуры. Между ними
/// однородная волна Ez распространяется как плоская волна при нормальном падении.
void temChannel(Builder &b, double L, double ylo, double yhi)
{
    b.polyline(QStringLiteral("канал, низ"), Material::Magnetic,
               {QPointF(0, ylo), QPointF(L / 2, ylo), QPointF(L, ylo)});
    b.polyline(QStringLiteral("канал, верх"), Material::Magnetic,
               {QPointF(0, yhi), QPointF(L / 2, yhi), QPointF(L, yhi)});
}

QString helpTem()
{
    return QStringLiteral(
        "\n\nПлоская волна при нормальном падении задана каналом с магнитными стенками: в нём "
        "однородное поле распространяется ровно так, как плоская волна в свободном "
        "пространстве. У входа плоской волны в ядре есть остаточное отражение около -25 дБ.");
}

QString helpOpen()
{
    return QStringLiteral(
        "\n\nОткрытое пространство — коробка с поглотителем по краям, плоская волна приходит "
        "слева. Первый вариант считает ту же сцену без объекта: его отклик вычитают, чтобы "
        "отделить рассеяние от отражений самой области.");
}

// ------------------------------------------------------------ слой

Build buildSlab(const Values &v)
{
    const double eps = v.value(QStringLiteral("eps"));
    const double t = v.value(QStringLiteral("t"));
    const double fLo = v.value(QStringLiteral("flo")), fHi = v.value(QStringLiteral("fhi"));

    Builder b(QStringLiteral("slab"));
    const double lamMin = rf::lambda0(fHi) / std::sqrt(eps);
    b.p.delta = chooseDelta(lamMin, t, 20.0, 6.0);
    const double d = b.p.delta;
    const double W = snap(rf::lambda0(fHi) / 2.0, d);
    const double lead = snap(rf::lambda0(fLo), d);
    const double tt = snap(t, d);
    const double L = 2 * lead + tt;
    const double ylo = 3 * d, yhi = ylo + W;
    b.p.box = {0.0, L, 0.0, yhi + 3 * d};
    b.p.description = QStringLiteral("Плоская волна на диэлектрическом слое: eps = %1, толщина %2 мм.")
                          .arg(num(eps), num(tt));
    temChannel(b, L, ylo, yhi);
    b.dielectric(QStringLiteral("слой"), eps, lead, lead + tt, ylo, yhi);
    b.port(QStringLiteral("вход 1"), PortSide::Left, ylo, yhi, true, PortMode::Plane);
    b.port(QStringLiteral("вход 2"), PortSide::Right, ylo, yhi, false, PortMode::Plane);
    b.freqs(fLo, fHi, 33);
    b.outputs(true);
    b.time(settleTime(L + tt * std::sqrt(eps) * 4, fLo, 0.0));

    QVector<Row> rows;
    rows << Row{QStringLiteral("Слой"), QStringLiteral("eps = %1, толщина %2").arg(num(eps), mm(tt))};
    const double fT = kLightSpeed / (2.0 * tt * std::sqrt(eps));
    rows << Row{QStringLiteral("Слой прозрачен на"),
                QStringLiteral("%1 ГГц и кратных (толщина = k λ/2 в слое)").arg(num(fT, 4))};
    const double f0 = (fLo + fHi) / 2.0;
    for (double f : {fLo, f0, fHi}) {
        const double r = std::abs(rf::slabReflection(0.0, eps, tt, f));
        rows << Row{QStringLiteral("Точно, %1 ГГц").arg(num(f, 4)),
                    QStringLiteral("|S11| = %1, |S21| = %2").arg(db(r), db(std::sqrt(1 - r * r)))};
    }
    rows << Row{QStringLiteral("Одна граница (полупространство)"),
                QStringLiteral("|Г| = %1").arg(num((std::sqrt(eps) - 1) / (std::sqrt(eps) + 1)))};
    return b.done(rows);
}

// ------------------------------------------------------------ градиентный переход

Build buildGrad(const Values &v)
{
    const double epsMax = v.value(QStringLiteral("eps"));
    const double lenLam = v.value(QStringLiteral("len"));
    const double f0 = v.value(QStringLiteral("f0"));
    const double lam = rf::lambda0(f0);

    Builder b(QStringLiteral("grad"));
    b.p.delta = chooseDelta(lam / std::sqrt(epsMax), lam, 20.0, 4.0);
    const double d = b.p.delta;
    const double W = snap(lam / 2.0, d);
    const double lead = snap(lam, d);
    const double T = snap(lenLam * lam, d);
    const double tail = snap(lam, d);
    const double L = lead + T + tail;
    const double ylo = 3 * d, yhi = ylo + W;
    b.p.box = {0.0, L, 0.0, yhi + 3 * d};
    b.p.description = QStringLiteral("Плавный переход из вакуума в диэлектрик eps = %1: профиль "
                                     "eps формулой против резкой границы.")
                          .arg(num(epsMax));
    temChannel(b, L, ylo, yhi);
    // Резкая граница: сразу eps_max. Плавный переход: eps растёт линейно по длине T.
    const int abrupt = b.dielectric(QStringLiteral("резкая граница"), epsMax, lead, lead + T, ylo, yhi).id;
    Shape &g = b.dielectric(QStringLiteral("плавный профиль"), epsMax, lead, lead + T, ylo, yhi);
    g.epsProfile.enabled = true;
    // x — от центра прямоугольника: -T/2 ... T/2.
    g.epsProfile.formula = QStringLiteral("1+(%1)*(x+%2)/%3")
                               .arg(formatNumber(epsMax - 1.0), formatNumber(T / 2.0),
                                    formatNumber(T));
    g.epsProfile.file = QStringLiteral("grad");
    const int graded = g.id;
    b.dielectric(QStringLiteral("диэлектрик за переходом"), epsMax, lead + T, L, ylo, yhi);
    b.port(QStringLiteral("вход 1 (вакуум)"), PortSide::Left, ylo, yhi, true, PortMode::Plane);
    Port &p2 = b.port(QStringLiteral("вход 2 (в диэлектрике)"), PortSide::Right, ylo, yhi, false,
                      PortMode::Plane);
    p2.eps = epsMax;

    Variant va;
    va.name = QStringLiteral("резкая граница");
    va.tag = QStringLiteral("step");
    va.disabledShapes = {graded};
    Variant vb;
    vb.name = QStringLiteral("плавный профиль");
    vb.tag = QStringLiteral("taper");
    vb.disabledShapes = {abrupt};
    b.p.variants << va << vb;

    b.freqs(std::round(f0 * 0.5 * 100) / 100, std::round(f0 * 2.0 * 100) / 100, 16);
    b.outputs(true);
    b.time(settleTime(L * std::sqrt(epsMax) * 2, f0 * 0.5, 0.0));

    QVector<Row> rows;
    rows << Row{QStringLiteral("Переход"),
                QStringLiteral("eps от 1 до %1 на длине %2 (%3 λ на %4 ГГц)")
                    .arg(num(epsMax), mm(T), num(T / lam), num(f0, 3))};
    rows << Row{QStringLiteral("Резкая граница, точно"),
                QStringLiteral("|S11| = %1 на всех частотах")
                    .arg(db((std::sqrt(epsMax) - 1) / (std::sqrt(epsMax) + 1)))};
    rows << Row{QStringLiteral("Плавный профиль"),
                QStringLiteral("отражение падает с ростом частоты: переход длиннее волны")};
    rows << Row{QStringLiteral("Файл профиля"),
                QStringLiteral("grad.eps пишется ядром на первом шаге; готовый файл "
                               "подключают блоком FILE — см. образец «Среда из файла .eps»")};
    return b.done(rows, {QStringLiteral("Один профиль задан формулой от x: так в ядро уходит "
                                        "любая неоднородная eps. Градиент виден и на холсте.")});
}

// ------------------------------------------------------------ среда из файла

Build buildEpsFile(const Values &v)
{
    const double epsMax = v.value(QStringLiteral("eps"));
    const double f0 = v.value(QStringLiteral("f0"));
    const double lam = rf::lambda0(f0);

    Builder b(QStringLiteral("epsfile"));
    b.p.delta = chooseDelta(lam / std::sqrt(epsMax), lam, 20.0, 4.0);
    const double d = b.p.delta;
    const double R = snap(1.5 * lam, d);      // полуширина «линзы»
    const double X = snap(2 * R + 5 * lam, d);
    const double Y = snap(2 * R + 4 * lam, d);
    b.p.box = {0.0, X, 0.0, Y};
    b.p.description = QStringLiteral("Среда из готового файла lens.eps: гауссов бугор eps до %1.")
                          .arg(num(epsMax));
    b.absorberFrame(false, true, true, true);
    const QPointF c(snap(X / 2.0, d), snap(Y / 2.0, d));
    Shape &s = b.rect(QStringLiteral("распределение из файла"), Material::Dielectric, c.x() - R,
                      c.x() + R, c.y() - R, c.y() + R);
    s.epsProfile.enabled = true;
    s.epsProfile.fromFile = true;
    s.epsProfile.file = QStringLiteral("lens");
    const double t = 5 * d;
    b.port(QStringLiteral("плоская волна"), PortSide::Left, t + d, Y - t - d, true, PortMode::Plane);
    b.freq(f0);
    b.outputs(true);
    b.time(settleTime(X * 2, f0, 0.0));

    const double w = R / 1.5;
    Build out = b.done(
        {Row{QStringLiteral("Файл"), QStringLiteral("lens.eps рядом с заданием, сетка %1 мм").arg(num(d))},
         Row{QStringLiteral("Распределение"),
             QStringLiteral("eps = 1 + %1 exp(-(x²+y²)/%2²), квадрат %3").arg(num(epsMax - 1), mm(w), mm(2 * R))}},
        {QStringLiteral("Файл программа пишет сама в формате ядра; координаты блока ядро не читает "
                        "— распределение ставится на место анкером."),
         QStringLiteral("Блок FILE читается только ядром, собранным после 6 сентября 2026 "
                        "(исправление бага #19). Старые сборки останавливаются на «Missing "
                        "separator ;».")});
    out.companions.insert(QStringLiteral("lens.eps"),
                          epsFileBytes(d, 2 * R, 2 * R, [&](double x, double y) {
                              return (epsMax - 1.0) * std::exp(-(x * x + y * y) / (w * w));
                          }));
    return out;
}

// ------------------------------------------------------------ Люнеберг

Build buildLuneburg(const Values &v)
{
    const double f0 = v.value(QStringLiteral("f0"));
    const double lam = rf::lambda0(f0);
    const double R = v.value(QStringLiteral("r")) * lam;

    Builder b(QStringLiteral("luneburg"));
    b.p.delta = chooseDelta(lam / std::sqrt(2.0), lam, 20.0, 4.0);
    const double d = b.p.delta;
    const double X = snap(2 * R + 5 * lam, d);
    const double Y = snap(2 * R + 4 * lam, d);
    b.p.box = {0.0, X, 0.0, Y};
    b.p.description = QStringLiteral("Линза Люнеберга радиусом %1 мм: eps = 2 - (r/R)^2.")
                          .arg(num(R, 4));
    b.absorberFrame(false, true, true, true);
    const QPointF c(snap(2 * lam + R, d), snap(Y / 2.0, d));
    Shape &s = b.circle(QStringLiteral("линза"), Material::Dielectric, c, R);
    s.epsProfile.enabled = true;
    // r — расстояние от анкера, а анкер круга — его центр.
    s.epsProfile.formula = QStringLiteral("2-(r/%1)*(r/%1)").arg(formatNumber(R));
    const double t = 5 * d;
    b.port(QStringLiteral("плоская волна"), PortSide::Left, t + d, Y - t - d, true, PortMode::Plane);
    b.freq(f0);
    b.outputs(true, true);
    b.time(settleTime(X * 2, f0, 0.0));

    return b.done({Row{QStringLiteral("Радиус"), QStringLiteral("%1 (%2 λ)").arg(mm(R), num(R / lam))},
                   Row{QStringLiteral("Профиль"), QStringLiteral("eps = 2 в центре, 1 на краю")},
                   Row{QStringLiteral("Фокус"),
                       QStringLiteral("на противоположном краю: x = %1 мм, y = %2 мм")
                           .arg(num(c.x() + R, 4), num(c.y(), 4))}},
                  {QStringLiteral("Чтобы увидеть фокусировку, включите выгрузку поля на первом шаге "
                                  "(мастер, «Вывод данных») и откройте его в FieldView: плоская волна "
                                  "собирается в точку на тыльной стороне линзы.")});
}

// ------------------------------------------------------------ плазма

Param stdParam(int def)
{
    return Param::choice(QStringLiteral("std"), QStringLiteral("Сечение волновода"),
                         waveguideChoices(), def);
}

int stdIndex(const char *name)
{
    const QVector<WaveguideStd> &t = waveguides();
    for (int i = 0; i < t.size(); ++i) {
        if (t.at(i).name.startsWith(QLatin1String(name)))
            return i;
    }
    return defaultWaveguide();
}

/// Волновод X-моды с плазменным слоем поперёк — постановка лабораторной работы ЛР2.
struct PlasmaGuide {
    WaveguideStd std;
    double d = 0.0, ylo = 0.0, yhi = 0.0, L = 0.0, x0 = 0.0, x1 = 0.0;
};

PlasmaGuide plasmaGuide(Builder &b, int stdIdx, double thick)
{
    PlasmaGuide g;
    g.std = waveguides().value(stdIdx);
    b.p.kernel = Kernel::X;
    b.p.delta = std::round(g.std.a / 40.0 * 1e6) / 1e6;
    g.d = b.p.delta;
    g.ylo = 3 * g.d;
    g.yhi = g.ylo + g.std.a;
    const double lead = snap(2.0 * rf::lambdaGuide(g.std.a, g.std.fLo), g.d);
    const double t = std::max(2 * g.d, snap(thick, g.d));
    g.x0 = lead;
    g.x1 = lead + t;
    g.L = g.x1 + lead;
    b.p.box = {0.0, g.L, 0.0, g.std.a + 6 * g.d};
    b.guideX(QStringLiteral("волновод"), 0.0, g.L, g.ylo, g.yhi);
    return g;
}

Build buildPlasmaSlab(const Values &v)
{
    const double fp = v.value(QStringLiteral("fp"));
    const double thick = v.value(QStringLiteral("t"));
    Builder b(QStringLiteral("plslab"));
    const PlasmaGuide g = plasmaGuide(b, int(v.value(QStringLiteral("std"))), thick);
    b.p.description = QStringLiteral("Плазменный слой в волноводе %1: fp = %2 ГГц (X-мода, "
                                     "ядро tmc_rtx).")
                          .arg(g.std.name, num(fp, 4));
    Shape &s = b.rect(QStringLiteral("плазма"), Material::Dielectric, g.x0, g.x1, g.ylo, g.yhi);
    s.plasma.hasN = true;
    s.plasma.n = concentrationForFreqGHz(fp);
    b.port(QStringLiteral("вход 1"), PortSide::Left, g.ylo, g.yhi, true);
    b.port(QStringLiteral("вход 2"), PortSide::Right, g.ylo, g.yhi, false);
    b.freqs(g.std.fLo, g.std.fHi, 21);
    b.outputs(true);
    b.time(settleTime(g.L, g.std.fLo, g.std.fc10()));

    const double t = g.x1 - g.x0;
    QVector<Row> rows;
    rows << Row{QStringLiteral("Волновод"), QStringLiteral("%1, a = %2 мм, отсечка H10 %3 ГГц")
                                                .arg(g.std.name, num(g.std.a, 5), num(g.std.fc10(), 4))};
    rows << Row{QStringLiteral("Плазма"), QStringLiteral("fp = %1 ГГц, N = %2 м⁻³, слой %3")
                                              .arg(num(fp, 4), num(s.plasma.n, 4), mm(t))};
    rows << Row{QStringLiteral("Отсечка волновода с плазмой"),
                QStringLiteral("√(fp² + fc²) = %1 ГГц").arg(num(std::hypot(fp, g.std.fc10()), 4))};
    for (double f : {g.std.fLo, (g.std.fLo + g.std.fHi) / 2, g.std.fHi}) {
        const double eps = 1.0 - fp * fp / (f * f);
        const double r = std::abs(rf::slabReflection(g.std.a, eps, t, f));
        rows << Row{QStringLiteral("Теория, %1 ГГц").arg(num(f, 4)),
                    QStringLiteral("eps = %1, |S11| = %2").arg(num(eps), db(r))};
    }
    return b.done(rows, {QStringLiteral("Концентрация пишется в СИ один раз: на частоту её "
                                        "пересчитывает само ядро на каждом шаге.")});
}

Build buildMagnetized(const Values &v)
{
    const double fp = v.value(QStringLiteral("fp"));
    const double fce = v.value(QStringLiteral("fce"));
    const double thick = v.value(QStringLiteral("t"));
    Builder b(QStringLiteral("plmag"));
    const PlasmaGuide g = plasmaGuide(b, int(v.value(QStringLiteral("std"))), thick);
    b.p.description = QStringLiteral("Замагниченный плазменный слой в волноводе %1: fp = %2 ГГц, "
                                     "fce = %3 ГГц.")
                          .arg(g.std.name, num(fp, 4), num(fce, 4));
    Shape &s = b.rect(QStringLiteral("плазма"), Material::Dielectric, g.x0, g.x1, g.ylo, g.yhi);
    s.plasma.hasN = true;
    s.plasma.n = concentrationForFreqGHz(fp);
    s.plasma.hasB = true;
    s.plasma.b = fieldForFreqGHz(fce);
    b.port(QStringLiteral("вход 1"), PortSide::Left, g.ylo, g.yhi, true);
    b.port(QStringLiteral("вход 2"), PortSide::Right, g.ylo, g.yhi, false);
    b.freqs(g.std.fLo, g.std.fHi, 21);
    b.outputs(true);
    b.time(settleTime(g.L, g.std.fLo, g.std.fc10(), 5.0));

    const PlasmaBands pb = plasmaBands(s.plasma.n, s.plasma.b);
    QVector<Row> rows;
    rows << Row{QStringLiteral("Волновод"), QStringLiteral("%1, a = %2 мм").arg(g.std.name, num(g.std.a, 5))};
    rows << Row{QStringLiteral("Плазма"), QStringLiteral("N = %1 м⁻³, B = %2 Тл, слой %3")
                                              .arg(num(s.plasma.n, 4), num(s.plasma.b, 4), mm(g.x1 - g.x0))};
    rows << Row{QStringLiteral("fp / fce"), QStringLiteral("%1 / %2 ГГц").arg(num(pb.fp, 4), num(pb.fce, 4))};
    rows << Row{QStringLiteral("Отсечки X-моды fL / fR"),
                QStringLiteral("%1 / %2 ГГц").arg(num(pb.fL, 4), num(pb.fR, 4))};
    rows << Row{QStringLiteral("Верхний гибридный fUH"), ghz(pb.fUH)};
    rows << Row{QStringLiteral("Непрозрачно для X-моды"),
                QStringLiteral("f < %1 ГГц и %2 < f < %3 ГГц").arg(num(pb.fL, 4), num(pb.fUH, 4), num(pb.fR, 4))};
    return b.done(rows, {QStringLiteral("Подмагничивание действует только там, где есть "
                                        "концентрация: поэтому N и B заданы одной фигуре.")});
}

Build buildColumn(const Values &v)
{
    const double fp0 = v.value(QStringLiteral("fp"));
    const double rRel = v.value(QStringLiteral("r"));
    Builder b(QStringLiteral("plcol"));
    const PlasmaGuide g = plasmaGuide(b, int(v.value(QStringLiteral("std"))), 0.0);
    const double R = std::max(2 * g.d, rRel * g.std.a);
    const QPointF c((g.x0 + g.x1) / 2.0, (g.ylo + g.yhi) / 2.0);
    b.p.description = QStringLiteral("Плазменный столб с параболическим профилем в волноводе %1: "
                                     "fp в центре %2 ГГц.")
                          .arg(g.std.name, num(fp0, 4));
    Shape &s = b.circle(QStringLiteral("плазменный столб"), Material::Dielectric, c, R);
    s.plasma.hasN = true;
    const double n0 = concentrationForFreqGHz(fp0);
    s.plasma.n = n0;
    // Профиль от центра: N0 * (1 - (r/Rp)^2), r — расстояние от анкера, то есть от центра.
    //
    // ⚠️ Rp чуть больше радиуса столба: на самом краю концентрация обязана остаться
    // ПОЛОЖИТЕЛЬНОЙ. При N = 0 в узле ядро останавливает шаг сообщением «abnormal
    // function» (проверено прогоном 2026-09-23: r = R давало ровно ноль).
    const double rp = R * 1.1;
    s.plasma.nFormula = QStringLiteral("%1*(1-(r/%2)*(r/%2))").arg(formatNumber(n0), formatNumber(rp));
    b.port(QStringLiteral("вход 1"), PortSide::Left, g.ylo, g.yhi, true);
    b.port(QStringLiteral("вход 2"), PortSide::Right, g.ylo, g.yhi, false);
    b.freqs(g.std.fLo, g.std.fHi, 21);
    b.outputs(true);
    b.time(settleTime(g.L, g.std.fLo, g.std.fc10(), 5.0));
    return b.done({Row{QStringLiteral("Столб"), QStringLiteral("радиус %1 (%2 a)").arg(mm(R), num(R / g.std.a))},
                   Row{QStringLiteral("Профиль"),
                       QStringLiteral("N = N0 (1 - (r/1.1R)²), N0 = %1 м⁻³; на краю N = 0.17 N0 — "
                                      "нулевую концентрацию ядро не принимает")
                           .arg(num(n0, 4))},
                   Row{QStringLiteral("fp в центре"), ghz(fp0)},
                   Row{QStringLiteral("Ожидаемо"),
                       QStringLiteral("столб отражает сильнее на частотах ниже fp: там его середина "
                                      "непрозрачна")}},
                  {QStringLiteral("Концентрация задана формулой от координат — ядро считает её в "
                                  "каждом узле. Так же заданы профили плотности в авторских "
                                  "токамаковых заданиях.")});
}

// ------------------------------------------------------------ ЭПР

/// Открытая сцена рассеяния: коробка с поглотителем сверху, снизу и справа, плоская
/// волна слева почти на всю высоту.
void scatterScene(Builder &b, double X, double Y)
{
    const double d = b.p.delta;
    b.p.box = {0.0, X, 0.0, Y};
    b.absorberFrame(false, true, true, true);
    const double t = 5 * d;
    b.port(QStringLiteral("плоская волна"), PortSide::Left, t + d, Y - t - d, true, PortMode::Plane);
}

Build buildCylinder(const Values &v)
{
    const double a = v.value(QStringLiteral("r"));
    const double fLo = v.value(QStringLiteral("flo")), fHi = v.value(QStringLiteral("fhi"));
    const double lamMax = rf::lambda0(fLo), lamMin = rf::lambda0(fHi);

    Builder b(QStringLiteral("cyl"));
    b.p.delta = chooseDelta(lamMin, a, 20.0, 8.0);
    const double d = b.p.delta;
    const double X = snap(2 * a + 3 * lamMax, d);
    const double Y = snap(2 * a + 3 * lamMax, d);
    b.p.description = QStringLiteral("Рассеяние плоской волны на металлическом цилиндре радиусом "
                                     "%1 мм.").arg(num(a, 4));
    scatterScene(b, X, Y);
    const int cyl = b.circle(QStringLiteral("цилиндр"), Material::Metal,
                             QPointF(snap(X / 2, d), snap(Y / 2, d)), a).id;
    Variant empty;
    empty.name = QStringLiteral("без объекта");
    empty.tag = QStringLiteral("ref");
    empty.disabledShapes = {cyl};
    Variant body;
    body.name = QStringLiteral("цилиндр");
    body.tag = QStringLiteral("body");
    b.p.variants << empty << body;
    b.freqs(fLo, fHi, 13);
    b.outputs(true);
    b.time(settleTime(X * 2, fLo, 0.0));

    QVector<Row> rows;
    rows << Row{QStringLiteral("Цилиндр"), QStringLiteral("радиус %1, ka = %2 ... %3")
                                               .arg(mm(a), num(2 * kPi * a / lamMax), num(2 * kPi * a / lamMin))};
    for (double f : {fLo, (fLo + fHi) / 2, fHi}) {
        const double s = rf::cylinderEchoWidth(a, f, true);
        rows << Row{QStringLiteral("Ряд Ми (E вдоль оси), %1 ГГц").arg(num(f, 4)),
                    QStringLiteral("σ = %1 мм (%2 дБ·м)").arg(num(s, 4), num(10 * std::log10(s / 1000.0), 3))};
    }
    rows << Row{QStringLiteral("Оценка по S11"),
                QStringLiteral("σ ≈ 2π W² |S11 - S11 опоры|² / λ, W — апертура входа")};
    return b.done(rows, {QStringLiteral("Погонная ЭПР — рассеяние бесконечно длинного цилиндра "
                                        "(задача двумерная). Оценка по S11 — порядок величины, "
                                        "а не калибровка: область конечна, поглотитель "
                                        "отражает на уровне -30 дБ.")});
}

/// Сцена ЭПР с поворотом объекта. Углы — от \p a0 до \p a1 с шагом \p step.
void rotateObject(Builder &b, const QVector<int> &ids, const QPointF &pivot, double a0, double a1,
                  double step)
{
    b.p.sweep.enabled = true;
    b.p.sweep.shapeIds = ids;
    b.p.sweep.pivot = pivot;
    b.p.sweep.referenceStep = true;
    for (double a = a0; a <= a1 + 1e-9; a += step)
        b.p.sweep.angles << std::round(a * 1000.0) / 1000.0;
}

Build buildStrip(const Values &v)
{
    const double f0 = v.value(QStringLiteral("f0"));
    const double lam = rf::lambda0(f0);
    const double w = v.value(QStringLiteral("w")) * lam;
    const double step = v.value(QStringLiteral("step"));

    Builder b(QStringLiteral("strip"));
    b.p.delta = chooseDelta(lam, w, 30.0, 8.0);
    const double d = b.p.delta;
    const double X = snap(w + 4 * lam, d);
    const double Y = snap(w + 4 * lam, d);
    b.p.description = QStringLiteral("ЭПР металлической полосы шириной %1 мм на %2 ГГц: поворот "
                                     "по шагам.").arg(num(w, 4), num(f0, 4));
    scatterScene(b, X, Y);
    const QPointF c(snap(X / 2, d), snap(Y / 2, d));
    const int strip = b.polyline(QStringLiteral("полоса"), Material::Metal,
                                 {QPointF(c.x(), c.y() - w / 2), QPointF(c.x(), c.y() + w / 2)}).id;
    rotateObject(b, {strip}, c, 0.0, 180.0, step);
    b.freq(f0);
    b.outputs(true);
    b.time(settleTime(X * 2, f0, 0.0));

    QVector<Row> rows;
    rows << Row{QStringLiteral("Полоса"), QStringLiteral("ширина %1 (%2 λ)").arg(mm(w), num(w / lam))};
    rows << Row{QStringLiteral("Углы"), QStringLiteral("0...180° с шагом %1°, шагов %2")
                                            .arg(num(step)).arg(b.p.stepCount())};
    for (double rot : {0.0, 10.0, 30.0, 60.0}) {
        const double s = rf::stripEchoWidth(w, f0, 90.0 - rot);
        rows << Row{QStringLiteral("Физ. оптика, поворот %1°").arg(num(rot)),
                    QStringLiteral("σ = %1 мм (%2 дБ·м)").arg(num(s, 4), num(10 * std::log10(std::max(s, 1e-9) / 1000.0), 3))};
    }
    return b.done(rows, {QStringLiteral("При повороте 0° полоса стоит поперёк волны — зеркальное "
                                        "отражение, максимум ЭПР k w². Точка .s подписана углом: "
                                        "в TMCGROUT по горизонтали будет угол, а не частота.")});
}

Build buildAirfoil(const Values &v)
{
    const double f0 = v.value(QStringLiteral("f0"));
    const double lam = rf::lambda0(f0);
    const double chord = v.value(QStringLiteral("c")) * lam;
    const double thick = v.value(QStringLiteral("t")) / 100.0;
    const double step = v.value(QStringLiteral("step"));

    Builder b(QStringLiteral("airfoil"));
    b.p.delta = chooseDelta(lam, chord * thick, 30.0, 6.0);
    const double d = b.p.delta;
    const double X = snap(chord + 4 * lam, d);
    const double Y = snap(chord + 4 * lam, d);
    b.p.description = QStringLiteral("ЭПР профиля NACA 00%1 с хордой %2 мм на %3 ГГц: поворот по "
                                     "шагам.").arg(int(std::lround(thick * 100))).arg(num(chord, 4), num(f0, 4));
    scatterScene(b, X, Y);
    const QPointF c(snap(X / 2, d), snap(Y / 2, d));
    // NACA 4-digit, симметричный: y = 5 t c [0.2969 √x - 0.1260 x - 0.3516 x² + 0.2843 x³ - 0.1036 x⁴]
    // (последний коэффициент — вариант с замкнутой задней кромкой). Носок — навстречу волне.
    QVector<QPointF> upper, lower;
    const int N = 24;
    for (int i = 0; i <= N; ++i) {
        const double beta = kPi * i / N;
        const double xr = (1.0 - std::cos(beta)) / 2.0; // сгущение к кромкам
        const double yt = 5.0 * thick * (0.2969 * std::sqrt(xr) - 0.1260 * xr - 0.3516 * xr * xr
                                         + 0.2843 * xr * xr * xr - 0.1036 * xr * xr * xr * xr);
        upper << QPointF(c.x() - chord / 2 + xr * chord, c.y() + yt * chord);
        lower << QPointF(c.x() - chord / 2 + xr * chord, c.y() - yt * chord);
    }
    QVector<QPointF> contour = upper;
    for (int i = lower.size() - 2; i >= 1; --i)
        contour << lower.at(i);
    const int body = b.polygon(QStringLiteral("профиль"), Material::Metal, contour).id;
    rotateObject(b, {body}, c, 0.0, 180.0, step);
    b.freq(f0);
    b.outputs(true);
    b.time(settleTime(X * 2, f0, 0.0));

    return b.done({Row{QStringLiteral("Профиль"), QStringLiteral("NACA 00%1, хорда %2 (%3 λ)")
                                                     .arg(int(std::lround(thick * 100))).arg(mm(chord), num(chord / lam))},
                   Row{QStringLiteral("Углы"), QStringLiteral("0...180° с шагом %1°, шагов %2")
                                                   .arg(num(step)).arg(b.p.stepCount())},
                   Row{QStringLiteral("Ожидаемо"),
                       QStringLiteral("максимум ЭПР при 90° (волна на плоскую сторону), минимум — "
                                      "с носка и с кромки")}},
                  {QStringLiteral("Точной формулы для профиля нет — результат даёт ядро. "
                                  "На сетке повёрнутый профиль — «лесенка», поэтому шаг сетки "
                                  "λ/30.")});
}

Build buildCoated(const Values &v)
{
    const double a = v.value(QStringLiteral("r"));
    const double t = v.value(QStringLiteral("t"));
    const double fp = v.value(QStringLiteral("fp"));
    const double fLo = v.value(QStringLiteral("flo")), fHi = v.value(QStringLiteral("fhi"));
    const double lamMax = rf::lambda0(fLo), lamMin = rf::lambda0(fHi);

    Builder b(QStringLiteral("coated"));
    b.p.kernel = Kernel::X;
    b.p.delta = chooseDelta(lamMin, t, 20.0, 4.0);
    const double d = b.p.delta;
    const double X = snap(2 * (a + t) + 3 * lamMax, d);
    const double Y = X;
    b.p.description = QStringLiteral("Металлический цилиндр в плазменной оболочке: радиус %1 мм, "
                                     "оболочка %2 мм, fp = %3 ГГц (X-мода).")
                          .arg(num(a, 4), num(t, 4), num(fp, 4));
    scatterScene(b, X, Y);
    const QPointF c(snap(X / 2, d), snap(Y / 2, d));
    const int cyl = b.circle(QStringLiteral("цилиндр"), Material::Metal, c, a).id;
    Shape &sh = b.circle(QStringLiteral("плазменная оболочка"), Material::Dielectric, c, a + t);
    sh.rMin = a;
    sh.plasma.hasN = true;
    sh.plasma.n = concentrationForFreqGHz(fp);
    const int sheath = sh.id;
    Variant empty;
    empty.name = QStringLiteral("без объекта");
    empty.tag = QStringLiteral("ref");
    empty.disabledShapes = {cyl, sheath};
    Variant bare;
    bare.name = QStringLiteral("металл");
    bare.tag = QStringLiteral("metal");
    bare.disabledShapes = {sheath};
    Variant coated;
    coated.name = QStringLiteral("металл + плазма");
    coated.tag = QStringLiteral("plasma");
    b.p.variants << empty << bare << coated;
    b.freqs(fLo, fHi, 9);
    b.outputs(true);
    b.time(settleTime(X * 2, fLo, 0.0));

    QVector<Row> rows;
    rows << Row{QStringLiteral("Цилиндр / оболочка"), QStringLiteral("радиус %1, оболочка %2").arg(mm(a), mm(t))};
    rows << Row{QStringLiteral("Плазма"), QStringLiteral("fp = %1 ГГц, N = %2 м⁻³").arg(num(fp, 4), num(sh.plasma.n, 4))};
    for (double f : {fLo, fHi}) {
        const double s = rf::cylinderEchoWidth(a, f, false);
        rows << Row{QStringLiteral("Голый цилиндр, ряд Ми (E ⟂ оси), %1 ГГц").arg(num(f, 4)),
                    QStringLiteral("σ = %1 мм").arg(num(s, 4))};
    }
    rows << Row{QStringLiteral("Ожидаемо"),
                QStringLiteral("разница вариантов «металл» и «металл + плазма» — вклад оболочки. "
                               "Для тонкой оболочки при E в плоскости (X-мода) она невелика: "
                               "прогон ядра при умолчаниях даёт расхождение меньше 1 дБ — "
                               "утолщайте оболочку или поднимайте fp")};
    return b.done(rows, {QStringLiteral("Три варианта одним заданием: без объекта (опора), голый "
                                        "металл и металл в плазме — так видно, что меняет "
                                        "оболочка. X-мода: поле E лежит в плоскости рисунка.")});
}

} // namespace

void addMedia(QVector<Sample> *out)
{
    const QString sec = QStringLiteral("media");
    {
        Sample s;
        s.id = QStringLiteral("med_slab");
        s.section = sec;
        s.title = QStringLiteral("Плоская волна на диэлектрическом слое");
        s.summary = QStringLiteral("Нормальное падение: точная формула Френеля для проверки");
        s.standard = QStringLiteral("формулы Френеля, слой в однородной среде");
        s.help = QStringLiteral(
                     "Плоская волна падает нормально на диэлектрический слой. Отражения от двух "
                     "граней интерферируют: на частотах, где толщина кратна половине длины волны "
                     "в слое, слой становится прозрачным. Формула точная — паспорт даёт |S11| и "
                     "|S21| на трёх частотах.")
                 + helpTem();
        s.params = {Param::number(QStringLiteral("eps"), QStringLiteral("Проницаемость eps"), {}, 4.0,
                                  1.1, 30.0, 0.1, 2),
                    Param::number(QStringLiteral("t"), QStringLiteral("Толщина"), QStringLiteral("мм"),
                                  10.0, 0.5, 200.0, 0.5, 2),
                    Param::number(QStringLiteral("flo"), QStringLiteral("Частота от"), QStringLiteral("ГГц"),
                                  2.0, 0.3, 50.0, 0.5, 2),
                    Param::number(QStringLiteral("fhi"), QStringLiteral("Частота до"), QStringLiteral("ГГц"),
                                  12.0, 0.5, 100.0, 0.5, 2)};
        s.build = buildSlab;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("med_grad");
        s.section = sec;
        s.title = QStringLiteral("Плавный переход: eps формулой");
        s.summary = QStringLiteral("Профиль eps от координат против резкой границы");
        s.standard = QStringLiteral("неоднородная среда, профиль формулой");
        s.help = QStringLiteral(
                     "Переход из вакуума в плотный диэлектрик. Резкая граница отражает на всех "
                     "частотах одинаково, а плавный — линейный по длине — профиль тем лучше, чем "
                     "длиннее переход по сравнению с волной. Профиль задан формулой от x: так в "
                     "ядро уходит любая неоднородная eps, а на холсте она видна градиентом.\n\n"
                     "Ядро на первом шаге сохранит посчитанный профиль в файл grad.eps — этот же "
                     "файл можно потом подключить блоком FILE.")
                 + helpTem();
        s.params = {Param::number(QStringLiteral("eps"), QStringLiteral("eps за переходом"), {}, 6.0,
                                  1.5, 20.0, 0.5, 1),
                    Param::number(QStringLiteral("len"), QStringLiteral("Длина перехода"),
                                  QStringLiteral("λ"), 1.0, 0.2, 5.0, 0.1, 1),
                    Param::number(QStringLiteral("f0"), QStringLiteral("Частота f0"), QStringLiteral("ГГц"),
                                  5.0, 0.5, 50.0, 0.5, 2)};
        s.build = buildGrad;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("med_eps_file");
        s.section = sec;
        s.title = QStringLiteral("Среда из готового файла .eps");
        s.summary = QStringLiteral("Распределение eps, посчитанное заранее, подключается блоком FILE");
        s.standard = QStringLiteral("формат распределения eps ядра TMC (V2.00)");
        s.help = QStringLiteral(
                     "Распределение eps не обязательно задавать формулой: его можно посчитать "
                     "заранее — своей программой или самим ядром — и подключить готовым файлом. "
                     "Здесь программа сама пишет файл lens.eps (гауссов бугор проницаемости) "
                     "рядом с заданием, а задание читает его блоком FILE.\n\nКоординаты блока "
                     "ядро не читает — сетка лежит в самом файле, а на место распределение "
                     "ставит анкер. Шаг сетки в файле обязан совпадать с DELTA задания.\n\n"
                     "Внимание: блок FILE читает только ядро, собранное после 6 сентября 2026 "
                     "(исправление бага #19).")
                 + helpOpen();
        s.params = {Param::number(QStringLiteral("eps"), QStringLiteral("eps в центре"), {}, 3.0, 1.2,
                                  10.0, 0.1, 2),
                    Param::number(QStringLiteral("f0"), QStringLiteral("Частота"), QStringLiteral("ГГц"),
                                  10.0, 1.0, 50.0, 0.5, 2)};
        s.build = buildEpsFile;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("med_luneburg");
        s.section = sec;
        s.title = QStringLiteral("Линза Люнеберга");
        s.summary = QStringLiteral("Градиентная линза: eps = 2 - (r/R)² фокусирует плоскую волну");
        s.standard = QStringLiteral("линза Люнеберга");
        s.help = QStringLiteral(
                     "Круглая линза с проницаемостью, плавно спадающей от 2 в центре до 1 на "
                     "краю. Плоская волна с любого направления собирается в точку на "
                     "противоположном краю линзы. Профиль задан формулой от расстояния r до "
                     "центра — на холсте он виден градиентом, а поле первого шага в FieldView (выгрузку "
                     "поля включите на странице «Вывод данных») "
                     "показывает фокусировку.")
                 + helpOpen();
        s.params = {Param::number(QStringLiteral("r"), QStringLiteral("Радиус"), QStringLiteral("λ"),
                                  2.5, 1.0, 6.0, 0.5, 1),
                    Param::number(QStringLiteral("f0"), QStringLiteral("Частота"), QStringLiteral("ГГц"),
                                  10.0, 1.0, 50.0, 0.5, 2)};
        s.build = buildLuneburg;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("pl_slab");
        s.section = sec;
        s.title = QStringLiteral("Плазменный слой в волноводе");
        s.summary = QStringLiteral("Изотропная плазма: отсечка √(fp² + fc²), точная теория");
        s.standard = QStringLiteral("холодная бесстолкновительная плазма");
        s.kernel = Kernel::X;
        s.help = QStringLiteral(
                     "Слой плазмы поперёк волновода (ядро tmc_rtx, X-мода). Проницаемость "
                     "плазмы eps = 1 - fp²/f² отрицательна ниже плазменной частоты — там слой "
                     "отражает. Волновод с плазмой пропускает волну выше √(fp² + fc²). "
                     "Концентрация N пересчитывается из fp и пишется в СИ; на каждую частоту её "
                     "переводит само ядро.");
        s.params = {stdParam(stdIndex("WR-284")),
                    Param::number(QStringLiteral("fp"), QStringLiteral("Плазменная частота fp"),
                                  QStringLiteral("ГГц"), 3.0, 0.1, 100.0, 0.1, 2),
                    Param::number(QStringLiteral("t"), QStringLiteral("Толщина слоя"), QStringLiteral("мм"),
                                  20.0, 1.0, 500.0, 1.0, 1)};
        s.build = buildPlasmaSlab;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("pl_magnetized");
        s.section = sec;
        s.title = QStringLiteral("Замагниченная плазма");
        s.summary = QStringLiteral("Слой с N и B: полосы непрозрачности X-моды");
        s.standard = QStringLiteral("холодная замагниченная плазма, X-мода");
        s.kernel = Kernel::X;
        s.help = QStringLiteral(
                     "Та же постановка, что в лабораторной работе ЛР2: слой плазмы в волноводе, "
                     "подмагниченный полем B, перпендикулярным плоскости. У X-моды две полосы "
                     "непрозрачности: ниже fL и между fUH и fR. Все характерные частоты — в "
                     "паспорте.\n\nПодмагничивание действует только в области с концентрацией, "
                     "поэтому N и B заданы одной и той же фигуре.");
        s.params = {stdParam(stdIndex("WR-284")),
                    Param::number(QStringLiteral("fp"), QStringLiteral("Плазменная частота fp"),
                                  QStringLiteral("ГГц"), 3.0, 0.1, 100.0, 0.1, 2),
                    Param::number(QStringLiteral("fce"), QStringLiteral("Циклотронная частота fce"),
                                  QStringLiteral("ГГц"), 1.5, 0.1, 100.0, 0.1, 2),
                    Param::number(QStringLiteral("t"), QStringLiteral("Толщина слоя"), QStringLiteral("мм"),
                                  20.0, 1.0, 500.0, 1.0, 1)};
        s.build = buildMagnetized;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("pl_column");
        s.section = sec;
        s.title = QStringLiteral("Плазменный столб с профилем");
        s.summary = QStringLiteral("Концентрация задана формулой от координат");
        s.standard = QStringLiteral("параболический профиль плотности");
        s.kernel = Kernel::X;
        s.help = QStringLiteral(
                     "Плазменный столб в волноводе с плотностью, спадающей к краю по параболе. "
                     "Концентрация задана не числом, а формулой от расстояния до центра — ядро "
                     "считает её в каждом узле. Так заданы профили плотности в авторских "
                     "заданиях по токамакам.");
        s.params = {stdParam(stdIndex("WR-284")),
                    Param::number(QStringLiteral("fp"), QStringLiteral("fp в центре"), QStringLiteral("ГГц"),
                                  4.0, 0.1, 100.0, 0.1, 2),
                    Param::number(QStringLiteral("r"), QStringLiteral("Радиус, доля a"), {}, 0.2, 0.05,
                                  0.45, 0.05, 2)};
        s.build = buildColumn;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("rcs_cylinder");
        s.section = sec;
        s.title = QStringLiteral("ЭПР металлического цилиндра");
        s.summary = QStringLiteral("Эталон: точное решение рядом Ми");
        s.standard = QStringLiteral("ряд Ми для идеально проводящего цилиндра");
        s.help = QStringLiteral(
                     "Плоская волна рассеивается на металлическом цилиндре. Для цилиндра есть "
                     "точное решение — ряд Ми, — поэтому это эталон, на котором проверяют саму "
                     "постановку расчёта ЭПР. Поворачивать круглое тело бессмысленно, поэтому "
                     "здесь свип по частоте: вариант без объекта (опора) и с цилиндром.\n\n"
                     "ЭПР двумерная — погонная, в дБ относительно метра.")
                 + helpOpen();
        s.params = {Param::number(QStringLiteral("r"), QStringLiteral("Радиус"), QStringLiteral("мм"),
                                  20.0, 2.0, 500.0, 1.0, 1),
                    Param::number(QStringLiteral("flo"), QStringLiteral("Частота от"), QStringLiteral("ГГц"),
                                  2.0, 0.3, 50.0, 0.5, 2),
                    Param::number(QStringLiteral("fhi"), QStringLiteral("Частота до"), QStringLiteral("ГГц"),
                                  8.0, 0.5, 100.0, 0.5, 2)};
        s.build = buildCylinder;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("rcs_strip");
        s.section = sec;
        s.title = QStringLiteral("ЭПР полосы: поворот по шагам");
        s.summary = QStringLiteral("Угловой свип: объект поворачивается, источник стоит");
        s.standard = QStringLiteral("физическая оптика");
        s.help = QStringLiteral(
                     "Моностатическая ЭПР металлической полосы в зависимости от угла. Плоская "
                     "волна приходит всегда слева, а полоса на каждом шаге повёрнута на следующий "
                     "угол — повернуть сам источник язык не позволяет, а для рассеяния важен "
                     "только угол между волной и телом.\n\nШаг описан ОДИН раз макросом с углом в "
                     "параметре; первым идёт шаг без объекта (опора). Точка .s подписана углом, "
                     "поэтому TMCGROUT покажет зависимость от угла. Угол 0 подписан как 360: "
                     "точку с подписью 0 ядро теряет.")
                 + helpOpen();
        s.params = {Param::number(QStringLiteral("w"), QStringLiteral("Ширина полосы"), QStringLiteral("λ"),
                                  2.0, 0.5, 10.0, 0.5, 1),
                    Param::number(QStringLiteral("f0"), QStringLiteral("Частота"), QStringLiteral("ГГц"),
                                  5.0, 0.5, 50.0, 0.5, 2),
                    // От 2°: 0...180 при шаге 2 — 91 угол, а больше 99 точек .s не примет.
                    Param::number(QStringLiteral("step"), QStringLiteral("Шаг по углу"),
                                  QStringLiteral("град."), 5.0, 2.0, 45.0, 1.0, 0)};
        s.build = buildStrip;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("rcs_airfoil");
        s.section = sec;
        s.title = QStringLiteral("ЭПР профиля крыла");
        s.summary = QStringLiteral("Профиль NACA, угловой свип 0...180°");
        s.standard = QStringLiteral("NACA 4-digit (симметричный профиль)");
        s.help = QStringLiteral(
                     "Сечение крыла (профиль NACA 00xx) из металла, повёрнутое по шагам "
                     "относительно падающей волны. Точной формулы нет — это как раз та задача, "
                     "ради которой ЭПР считают численно. Профиль задан контуром из вершин и "
                     "поворачивается целиком.")
                 + helpOpen();
        s.params = {Param::number(QStringLiteral("c"), QStringLiteral("Хорда"), QStringLiteral("λ"),
                                  3.0, 1.0, 10.0, 0.5, 1),
                    Param::number(QStringLiteral("t"), QStringLiteral("Толщина"), QStringLiteral("% хорды"),
                                  12.0, 6.0, 24.0, 1.0, 0),
                    Param::number(QStringLiteral("f0"), QStringLiteral("Частота"), QStringLiteral("ГГц"),
                                  5.0, 0.5, 50.0, 0.5, 2),
                    // От 2°: 0...180 при шаге 2 — 91 угол, а больше 99 точек .s не примет.
                    Param::number(QStringLiteral("step"), QStringLiteral("Шаг по углу"),
                                  QStringLiteral("град."), 5.0, 2.0, 45.0, 1.0, 0)};
        s.build = buildAirfoil;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("rcs_coated");
        s.section = sec;
        s.title = QStringLiteral("Цилиндр в плазменной оболочке");
        s.summary = QStringLiteral("Как плазма меняет рассеяние: три варианта сразу");
        s.standard = QStringLiteral("X-мода, холодная плазма");
        s.kernel = Kernel::X;
        s.help = QStringLiteral(
                     "Металлический цилиндр, окружённый слоем плазмы, — упрощённая модель тела в "
                     "плазменной оболочке (как у гиперзвукового аппарата). Одно задание считает "
                     "три варианта: пустую область (опору), голый металл и металл в плазме.")
                 + helpOpen();
        s.params = {Param::number(QStringLiteral("r"), QStringLiteral("Радиус цилиндра"),
                                  QStringLiteral("мм"), 20.0, 2.0, 500.0, 1.0, 1),
                    Param::number(QStringLiteral("t"), QStringLiteral("Толщина оболочки"),
                                  QStringLiteral("мм"), 10.0, 1.0, 200.0, 1.0, 1),
                    Param::number(QStringLiteral("fp"), QStringLiteral("fp оболочки"), QStringLiteral("ГГц"),
                                  4.0, 0.1, 100.0, 0.1, 2),
                    Param::number(QStringLiteral("flo"), QStringLiteral("Частота от"), QStringLiteral("ГГц"),
                                  2.0, 0.3, 50.0, 0.5, 2),
                    Param::number(QStringLiteral("fhi"), QStringLiteral("Частота до"), QStringLiteral("ГГц"),
                                  8.0, 0.5, 100.0, 0.5, 2)};
        s.build = buildCoated;
        *out << s;
    }
}

} // namespace tpl::samples
