// Раздел «Антенны»: излучение в открытое пространство.
//
// Открытое пространство в двумерном ядре — это коробка, обложенная поглотителем по
// краям (как в авторских `LR1_rupor.tpl` и `hyper_sheath_0p4.TPL`). Питающий волновод
// входит в коробку слева; там, где он проходит, поглотителя нет. Диаграмму
// направленности ядро пишет строкой FIELD_DISTRIBUTION_M — её показывает TMC_DN.

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

Param freqParam(double def = 10.0)
{
    return Param::number(QStringLiteral("f0"), QStringLiteral("Частота"), QStringLiteral("ГГц"),
                         def, 1.0, 100.0, 0.5, 2,
                         QStringLiteral("Питающий волновод программа подбирает по стандарту сама — "
                                        "тот, в рабочей полосе которого лежит частота"));
}

/// Открытая сцена с питающим волноводом слева.
struct Feed {
    WaveguideStd std;
    double a = 0.0;
    double ylo = 0.0, yhi = 0.0;
    double fc = 0.0;
};

/// Коробка, поглотитель по краям и питающий волновод от левого края до \p xEnd.
/// \p yc — ось волновода. Возвращает раскладку питания.
Feed openScene(Builder &b, double f0, double X, double Y, double yc, double xEnd,
               const QString &feedName = QStringLiteral("питающий волновод"))
{
    Feed f;
    f.std = waveguides().value(waveguideForFreq(f0));
    f.a = f.std.a;
    f.fc = f.std.fc10();
    const double d = b.p.delta;
    f.ylo = snap(yc - f.a / 2.0, d);
    f.yhi = f.ylo + f.a;
    b.p.box = {0.0, X, 0.0, Y};

    const double t = 5 * d;
    // Поглотитель: справа, сверху, снизу целиком; слева — выше и ниже волновода.
    b.rect(QStringLiteral("поглотитель справа"), Material::Absorber, X - t, X, 0.0, Y);
    b.rect(QStringLiteral("поглотитель сверху"), Material::Absorber, t, X - t, Y - t, Y);
    b.rect(QStringLiteral("поглотитель снизу"), Material::Absorber, t, X - t, 0.0, t);
    // Узкая полоска поглотителя (тоньше 2*DELTA) ядру не нужна и валидатору не нравится.
    if (f.ylo - 3 * d > 3 * d)
        b.rect(QStringLiteral("поглотитель слева, низ"), Material::Absorber, 0.0, t, 0.0,
               f.ylo - 3 * d);
    if (Y - (f.yhi + 3 * d) > 3 * d)
        b.rect(QStringLiteral("поглотитель слева, верх"), Material::Absorber, 0.0, t,
               f.yhi + 3 * d, Y);
    b.guideX(feedName, 0.0, xEnd, f.ylo, f.yhi);
    b.port(QStringLiteral("вход"), PortSide::Left, f.ylo, f.yhi, true);
    return f;
}

QVector<Row> feedRows(const Feed &f, double f0)
{
    return {
        {QStringLiteral("Питание"),
         QStringLiteral("%1, a = %2 мм, отсечка %3 ГГц").arg(f.std.name, num(f.a, 5), num(f.fc, 4))},
        {QStringLiteral("Длина волны λ"), mm(rf::lambda0(f0))},
    };
}

QString helpDn()
{
    return QStringLiteral(
        "\n\nЧто смотреть: |S11| питания — в TMCGROUT; диаграмму направленности (файлы "
        ".DAT/.AMP/.FAZ) — в TMC_DN. Картинку поля для FieldView образец не пишет — она "
        "замедляет счёт в разы; включите её в мастере на странице «Вывод данных». "
        "Поглотитель по краям области изображает открытое пространство.");
}

// ------------------------------------------------------------ открытый конец

Build buildOpenWg(const Values &v)
{
    const double f0 = v.value(QStringLiteral("f0"));
    const double lam = rf::lambda0(f0);
    const WaveguideStd w = waveguides().value(waveguideForFreq(f0));
    Builder b(QStringLiteral("openwg"));
    b.p.delta = deltaFitting(w.a, std::min(lam / 20.0, w.a / 16.0));
    const double d = b.p.delta;
    const double stub = snap(1.5 * rf::lambdaGuide(w.a, f0), d);
    const double X = snap(stub + 5.0 * lam, d);
    const double Y = snap(8.0 * lam, d);
    b.p.description = QStringLiteral("Излучение из открытого конца волновода %1 на %2 ГГц.")
                          .arg(w.name, num(f0, 4));
    const Feed f = openScene(b, f0, X, Y, Y / 2.0, stub);
    b.freq(f0);
    b.outputs(true, true);
    b.time(settleTime(X * 1.5, f0, 0.0));

    QVector<Row> rows = feedRows(f, f0);
    rows << Row{QStringLiteral("Раскрыв a/λ"), num(f.a / lam)};
    rows << Row{QStringLiteral("Ожидаемо"),
                QStringLiteral("широкая ДН без выраженного лепестка: раскрыв меньше длины волны")};
    return b.done(rows);
}

// ------------------------------------------------------------ рупор

/// Рупор: стенки от горловины (xThroat) до раскрыва (xAp), раскрыв A.
void hornWalls(Builder &b, const Feed &f, double xThroat, double xAp, double A)
{
    const double yc = (f.ylo + f.yhi) / 2.0;
    b.polyline(QStringLiteral("рупор, нижняя стенка"), Material::Metal,
               {QPointF(xThroat, f.ylo), QPointF((xThroat + xAp) / 2.0, (f.ylo + yc - A / 2) / 2.0),
                QPointF(xAp, yc - A / 2)});
    b.polyline(QStringLiteral("рупор, верхняя стенка"), Material::Metal,
               {QPointF(xThroat, f.yhi), QPointF((xThroat + xAp) / 2.0, (f.yhi + yc + A / 2) / 2.0),
                QPointF(xAp, yc + A / 2)});
}

Build buildHorn(const Values &v)
{
    const double f0 = v.value(QStringLiteral("f0"));
    const double lam = rf::lambda0(f0);
    const double A = v.value(QStringLiteral("ap")) * lam;
    const WaveguideStd w = waveguides().value(waveguideForFreq(f0));

    // Оптимальный H-секторальный рупор: A = sqrt(3 lambda R), R — от вершины до раскрыва.
    const double R = A * A / (3.0 * lam);
    const double l = R * (A - w.a) / A; // осевая длина раструба

    Builder b(QStringLiteral("horn"));
    b.p.delta = deltaFitting(w.a, std::min(lam / 20.0, w.a / 16.0));
    const double d = b.p.delta;
    const double lf = snap(rf::lambdaGuide(w.a, f0), d);
    const double xAp = snap(lf + l, d);
    const double X = snap(xAp + 4.0 * lam, d);
    const double Y = snap(A + 4.0 * lam, d);
    b.p.description = QStringLiteral("Оптимальный H-секторальный рупор: раскрыв %1 мм на %2 ГГц.")
                          .arg(num(A, 4), num(f0, 4));
    const Feed f = openScene(b, f0, X, Y, Y / 2.0, lf);
    hornWalls(b, f, lf, xAp, A);
    b.freq(f0);
    b.outputs(true, true);
    b.time(settleTime(X * 1.5, f0, 0.0));

    QVector<Row> rows = feedRows(f, f0);
    rows << Row{QStringLiteral("Раскрыв A"), QStringLiteral("%1 (%2 λ)").arg(mm(A), num(A / lam))};
    rows << Row{QStringLiteral("Оптимальная длина R = A²/3λ"), mm(R)};
    rows << Row{QStringLiteral("Длина раструба"), mm(xAp - lf)};
    rows << Row{QStringLiteral("Угол раскрыва"),
                QStringLiteral("%1°").arg(num(2.0 * std::atan((A - w.a) / 2.0 / l) * 180.0 / kPi))};
    rows << Row{QStringLiteral("Ширина ДН по уровню -3 дБ"),
                QStringLiteral("≈ %1° (косинусный раскрыв, 68 λ/A)").arg(num(68.0 * lam / A))};
    return b.done(rows, {QStringLiteral("«Оптимальный» — при заданном раскрыве даёт наибольший "
                                        "КНД: фазовая ошибка на краю раскрыва около 3/8 λ.")});
}

// ------------------------------------------------------------ рупор с линзой

Build buildLensHorn(const Values &v)
{
    const double f0 = v.value(QStringLiteral("f0"));
    const double lam = rf::lambda0(f0);
    const double A = v.value(QStringLiteral("ap")) * lam;
    const double eps = v.value(QStringLiteral("eps"));
    const double n = std::sqrt(eps);
    const WaveguideStd w = waveguides().value(waveguideForFreq(f0));

    // Короткий рупор: вдвое короче оптимального — с заметной фазовой ошибкой.
    const double R = A * A / (3.0 * lam) / 2.0;
    const double l = R * (A - w.a) / A;

    Builder b(QStringLiteral("lenshorn"));
    b.p.delta = deltaFitting(w.a, std::min(lam / 20.0, w.a / 16.0));
    const double d = b.p.delta;
    const double lf = snap(rf::lambdaGuide(w.a, f0), d);
    const double xAp = snap(lf + l, d);
    const double xApex = xAp - R;     // вершина раструба = фокус линзы
    const double F = xAp - xApex;

    // Гиперболическая линза: r(t) = F (n-1) / (n cos t - 1), фокус — в вершине рупора.
    // Со стороны рупора — гипербола, снаружи — плоскость.
    QVector<QPointF> hyper;
    double tMax = 0.0;
    {
        const int N = 24;
        // Угол, при котором гипербола поднимается на A/2.
        double lo = 0.0, hi = std::acos(1.0 / n) * 0.999;
        for (int it = 0; it < 60; ++it) {
            const double t = (lo + hi) / 2.0;
            const double r = F * (n - 1.0) / (n * std::cos(t) - 1.0);
            (r * std::sin(t) < A / 2.0 ? lo : hi) = t;
        }
        tMax = lo;
        for (int i = -N; i <= N; ++i) {
            const double t = tMax * i / N;
            const double r = F * (n - 1.0) / (n * std::cos(t) - 1.0);
            hyper << QPointF(xApex + r * std::cos(t), r * std::sin(t));
        }
    }
    const double xEdge = hyper.first().x();
    const double xBack = snap(xEdge + 2 * d, d);
    const double X = snap(xBack + 4.0 * lam, d);
    const double Y = snap(A + 4.0 * lam, d);
    b.p.description = QStringLiteral("Укороченный H-рупор с гиперболической линзой eps = %1 "
                                     "на %2 ГГц; варианты без линзы и с линзой.")
                          .arg(num(eps), num(f0, 4));
    const Feed f = openScene(b, f0, X, Y, Y / 2.0, lf);
    hornWalls(b, f, lf, xAp, A);

    const double yc = (f.ylo + f.yhi) / 2.0;
    QVector<QPointF> lens;
    for (const QPointF &p : hyper)
        lens << QPointF(p.x(), yc + p.y());
    lens << QPointF(xBack, yc + A / 2.0) << QPointF(xBack, yc - A / 2.0);
    Shape &ls = b.polygon(QStringLiteral("линза"), Material::Dielectric, lens);
    ls.eps = eps;
    const int lensId = ls.id;

    Variant bare;
    bare.name = QStringLiteral("без линзы");
    bare.tag = QStringLiteral("bare");
    bare.disabledShapes = {lensId};
    Variant withLens;
    withLens.name = QStringLiteral("с линзой");
    withLens.tag = QStringLiteral("lens");
    b.p.variants << bare << withLens;

    b.freq(f0);
    b.outputs(true, true);
    b.time(settleTime(X * 1.5, f0, 0.0));

    const double phaseErr = A * A / (8.0 * R * lam);
    QVector<Row> rows = feedRows(f, f0);
    rows << Row{QStringLiteral("Раскрыв A"), QStringLiteral("%1 (%2 λ)").arg(mm(A), num(A / lam))};
    rows << Row{QStringLiteral("Длина до вершины R"), QStringLiteral("%1 (вдвое короче оптимальной)").arg(mm(R))};
    rows << Row{QStringLiteral("Фазовая ошибка без линзы"),
                QStringLiteral("%1 λ на краю (%2°)").arg(num(phaseErr), num(phaseErr * 360.0))};
    rows << Row{QStringLiteral("Линза"),
                QStringLiteral("eps = %1 (n = %2), фокус %3, толщина в центре %4")
                    .arg(num(eps), num(n), mm(F), mm(xBack - xAp))};
    return b.done(rows, {QStringLiteral("Одно задание — два варианта: horn_bare и horn_lens. "
                                        "Сравните ДН в TMC_DN: линза выравнивает фазу в раскрыве, "
                                        "лепесток сужается и растёт.")});
}

// ------------------------------------------------------------ отражатель

Build buildReflector(const Values &v)
{
    const double f0 = v.value(QStringLiteral("f0"));
    const double lam = rf::lambda0(f0);
    const double D = v.value(QStringLiteral("dia")) * lam;
    const double F = v.value(QStringLiteral("fd")) * D;
    const WaveguideStd w = waveguides().value(waveguideForFreq(f0));

    Builder b(QStringLiteral("reflector"));
    b.p.delta = deltaFitting(w.a, std::min(lam / 20.0, w.a / 16.0));
    const double d = b.p.delta;
    const double xFocus = snap(4.0 * lam, d);
    const double xVertex = snap(xFocus + F, d);
    const double X = snap(xVertex + 2.0 * lam, d);
    const double Y = snap(D + 4.0 * lam, d);
    b.p.description = QStringLiteral("Параболический цилиндрический отражатель: D = %1 мм, "
                                     "F = %2 мм, %3 ГГц.")
                          .arg(num(D, 4), num(F, 4), num(f0, 4));
    const Feed f = openScene(b, f0, X, Y, Y / 2.0, xFocus, QStringLiteral("облучатель"));
    const double yc = (f.ylo + f.yhi) / 2.0;

    QVector<QPointF> par;
    const int N = 40;
    for (int i = 0; i <= N; ++i) {
        const double y = -D / 2.0 + D * i / N;
        par << QPointF(xVertex - y * y / (4.0 * F), yc + y);
    }
    b.polyline(QStringLiteral("отражатель"), Material::Metal, par);

    b.freq(f0);
    b.outputs(true, true);
    b.time(settleTime(X * 2.0, f0, 0.0));

    const double edge = 2.0 * std::atan(D / 2.0 / (F - D * D / (16.0 * F))) * 180.0 / kPi;
    QVector<Row> rows = feedRows(f, f0);
    rows << Row{QStringLiteral("Раскрыв D"), QStringLiteral("%1 (%2 λ)").arg(mm(D), num(D / lam))};
    rows << Row{QStringLiteral("Фокус F (F/D)"), QStringLiteral("%1 (%2)").arg(mm(F), num(F / D))};
    rows << Row{QStringLiteral("Угол облучения края"), QStringLiteral("%1°").arg(num(edge))};
    rows << Row{QStringLiteral("Ширина ДН по уровню -3 дБ"),
                QStringLiteral("≈ %1° (70 λ/D)").arg(num(70.0 * lam / D))};
    return b.done(rows, {QStringLiteral("Облучатель — открытый волновод в фокусе; его стенки "
                                        "затеняют часть раскрыва, как у настоящей антенны с "
                                        "прямым облучением.")});
}

// ------------------------------------------------------------ решётка

Build buildArray(const Values &v)
{
    const double f0 = v.value(QStringLiteral("f0"));
    const double lam = rf::lambda0(f0);
    const int N = int(v.value(QStringLiteral("count")));
    const double scan = v.value(QStringLiteral("scan"));
    double dRel = v.value(QStringLiteral("pitch"));

    // Элемент — короткий волновод H10: он излучает, только если шире lambda/2.
    const double ae = 0.6 * lam;
    Builder b(QStringLiteral("array"));
    b.p.delta = deltaFitting(ae, lam / 20.0);
    const double d = b.p.delta;
    QStringList notes;
    const double pitchMin = (ae + 3 * d) / lam;
    if (dRel < pitchMin) {
        notes << QStringLiteral("Шаг %1 λ меньше ширины излучателя — взят %2 λ.")
                     .arg(num(dRel), num(pitchMin));
        dRel = pitchMin;
    }
    const double pitch = snap(dRel * lam, d);
    const double stub = snap(0.75 * lam, d);
    const double span = pitch * (N - 1) + ae;
    const double X = snap(stub + 6.0 * lam, d);
    const double Y = snap(span + 6.0 * lam, d);
    b.p.box = {0.0, X, 0.0, Y};
    b.p.description = QStringLiteral("Решётка из %1 излучателей, шаг %2 мм, сканирование на "
                                     "%3 град., %4 ГГц.")
                          .arg(N).arg(num(pitch, 4), num(scan), num(f0, 4));

    const double t = 5 * d;
    b.rect(QStringLiteral("поглотитель справа"), Material::Absorber, X - t, X, 0.0, Y);
    b.rect(QStringLiteral("поглотитель сверху"), Material::Absorber, t, X - t, Y - t, Y);
    b.rect(QStringLiteral("поглотитель снизу"), Material::Absorber, t, X - t, 0.0, t);

    // Фазовый сдвиг между соседями: волна уходит под углом scan от оси X.
    const double k = 2.0 * kPi / lam;
    const double dphi = -k * pitch * std::sin(scan * kPi / 180.0);
    const double y0 = snap((Y - span) / 2.0, d);
    for (int i = 0; i < N; ++i) {
        const double lo = y0 + i * pitch;
        const double hi = lo + snap(ae, d);
        b.polyline(QStringLiteral("излучатель %1, низ").arg(i + 1), Material::Metal,
                   {QPointF(0, lo), QPointF(stub / 2, lo), QPointF(stub, lo)});
        b.polyline(QStringLiteral("излучатель %1, верх").arg(i + 1), Material::Metal,
                   {QPointF(0, hi), QPointF(stub / 2, hi), QPointF(stub, hi)});
        Port &p = b.port(QStringLiteral("излучатель %1").arg(i + 1), PortSide::Left, lo, hi, true);
        // Фаза — в радианах (ANGLE_UNIT radian), приведённая в (-pi; pi].
        p.signal.phase = std::remainder(dphi * i, 2.0 * kPi);
    }
    b.freq(f0);
    // Активны все излучатели сразу: .s здесь не матрица рассеяния, его не пишем.
    b.outputs(false, true);
    b.time(settleTime(X * 1.5, f0, 0.0));

    const double grating = 1.0 / (1.0 + std::fabs(std::sin(scan * kPi / 180.0)));
    QVector<Row> rows;
    rows << Row{QStringLiteral("Длина волны λ"), mm(lam)};
    rows << Row{QStringLiteral("Излучатели"),
                QStringLiteral("%1 шт., ширина %2, шаг %3 (%4 λ)")
                    .arg(N).arg(mm(snap(ae, d)), mm(pitch), num(pitch / lam))};
    rows << Row{QStringLiteral("Сдвиг фазы между соседями"),
                QStringLiteral("%1°").arg(num(dphi * 180.0 / kPi))};
    rows << Row{QStringLiteral("Ширина луча по уровню -3 дБ"),
                QStringLiteral("≈ %1° (50.8 λ / N d cos)")
                    .arg(num(50.8 * lam / (N * pitch * std::cos(scan * kPi / 180.0))))};
    rows << Row{QStringLiteral("Без дифракционных лепестков"),
                pitch / lam < grating
                    ? QStringLiteral("да: d/λ = %1 < %2").arg(num(pitch / lam), num(grating))
                    : QStringLiteral("НЕТ: d/λ = %1 >= %2 — появится второй луч")
                          .arg(num(pitch / lam), num(grating))};
    notes << QStringLiteral("Все излучатели активны одновременно, каждый со своей фазой, — "
                            "поэтому S-матрица не пишется: результат смотрят в ДН и в поле.");
    return b.done(rows, notes);
}

} // namespace

void addAntennas(QVector<Sample> *out)
{
    const QString sec = QStringLiteral("antenna");
    {
        Sample s;
        s.id = QStringLiteral("ant_open_wg");
        s.section = sec;
        s.title = QStringLiteral("Открытый конец волновода");
        s.summary = QStringLiteral("Простейший излучатель: волновод, обрывающийся в пространство");
        s.standard = QStringLiteral("питание по EIA RS-261 / IEC 60153-2");
        s.help = QStringLiteral(
                     "Волновод, открытый в свободное пространство. Раскрыв меньше длины волны, "
                     "поэтому излучение широкое, а часть мощности отражается обратно — это видно "
                     "по |S11|. С этого образца удобно начинать разговор об антеннах: дальше "
                     "раскрыв расширяют рупором.")
                 + helpDn();
        s.params = {freqParam()};
        s.build = buildOpenWg;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ant_horn");
        s.section = sec;
        s.title = QStringLiteral("H-секторальный рупор");
        s.summary = QStringLiteral("Оптимальный рупор: длина по заданному раскрыву");
        s.standard = QStringLiteral("оптимальный рупор, A = sqrt(3 λ R)");
        s.help = QStringLiteral(
                     "Рупор, расширяющий волновод в H-плоскости. Длина подобрана так, чтобы при "
                     "заданном раскрыве КНД был наибольшим: длиннее — лучше фаза, но больше "
                     "габарит; короче — фаза на краю раскрыва «разъезжается», и лепесток "
                     "расплывается. В паспорте — оптимальная длина и оценка ширины ДН.")
                 + helpDn();
        s.params = {freqParam(),
                    Param::number(QStringLiteral("ap"), QStringLiteral("Раскрыв"),
                                  QStringLiteral("λ"), 4.0, 1.5, 10.0, 0.5, 1)};
        s.build = buildHorn;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ant_lens_horn");
        s.section = sec;
        s.title = QStringLiteral("Рупор с диэлектрической линзой");
        s.summary = QStringLiteral("Короткий рупор и гиперболическая линза — два варианта");
        s.standard = QStringLiteral("гиперболическая линза, фокус в вершине рупора");
        s.help = QStringLiteral(
                     "Короткий рупор с большой фазовой ошибкой в раскрыве и гиперболическая "
                     "диэлектрическая линза, которая эту ошибку исправляет. Задание считает оба "
                     "варианта сразу — без линзы и с линзой, — в разные файлы. Сравнение ДН "
                     "показывает, зачем линза нужна.")
                 + helpDn();
        s.params = {freqParam(),
                    Param::number(QStringLiteral("ap"), QStringLiteral("Раскрыв"),
                                  QStringLiteral("λ"), 5.0, 2.0, 10.0, 0.5, 1),
                    Param::number(QStringLiteral("eps"), QStringLiteral("eps линзы"), {}, 2.5, 1.5,
                                  10.0, 0.1, 2, QStringLiteral("2.1 — фторопласт, 2.5 — полистирол"))};
        s.build = buildLensHorn;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ant_reflector");
        s.section = sec;
        s.title = QStringLiteral("Параболический отражатель");
        s.summary = QStringLiteral("Цилиндрическая парабола с облучателем в фокусе");
        s.standard = QStringLiteral("зеркальная антенна с прямым облучением");
        s.help = QStringLiteral(
                     "Параболический цилиндр и открытый волновод в его фокусе. Отражатель "
                     "превращает расходящуюся волну облучателя в плоскую — луч получается узким, "
                     "около 70 λ/D. Облучатель стоит перед зеркалом и затеняет его середину: "
                     "это видно по боковым лепесткам.")
                 + helpDn();
        s.params = {freqParam(),
                    Param::number(QStringLiteral("dia"), QStringLiteral("Раскрыв D"),
                                  QStringLiteral("λ"), 10.0, 4.0, 20.0, 1.0, 1),
                    Param::number(QStringLiteral("fd"), QStringLiteral("Отношение F/D"), {}, 0.4,
                                  0.25, 1.0, 0.05, 2)};
        s.build = buildReflector;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ant_array");
        s.section = sec;
        s.title = QStringLiteral("Фазированная решётка");
        s.summary = QStringLiteral("Линейка волноводных излучателей со сканированием луча");
        s.standard = QStringLiteral("линейная ФАР, равномерное возбуждение");
        s.help = QStringLiteral(
                     "Линейка открытых волноводов, возбуждаемых одновременно с линейным "
                     "набегом фазы. Набег поворачивает луч на заданный угол. Если шаг решётки "
                     "велик, появляется второй — дифракционный — луч: паспорт предупреждает об "
                     "этом заранее.\n\nФазы входов программа считает сама и пишет седьмым "
                     "параметром каждого входа.")
                 + helpDn();
        s.params = {freqParam(),
                    Param::integer(QStringLiteral("count"), QStringLiteral("Излучателей"), 6, 2, 16),
                    Param::number(QStringLiteral("pitch"), QStringLiteral("Шаг решётки"),
                                  QStringLiteral("λ"), 0.7, 0.5, 2.0, 0.05, 2),
                    Param::number(QStringLiteral("scan"), QStringLiteral("Угол сканирования"),
                                  QStringLiteral("град."), 20.0, -60.0, 60.0, 5.0, 1)};
        s.build = buildArray;
        *out << s;
    }
}

} // namespace tpl::samples
