// Раздел «Микрополосковые», вторая половина: узлы, нагрузки и импульсные задачи.
//
// Здесь собрано то, чего разделу не хватало: задания с ПОГЛОТИТЕЛЕМ (согласованная
// нагрузка, рамка вокруг патча), задания, где магнитная стенка работает не стенкой
// канала, а элементом схемы (разомкнутый конец, патч-резонатор), и два задания с
// падающей волной ИЗ ФАЙЛА — импульсная рефлектометрия и линия задержки, где смысл
// целиком во временной картине, а не в частотной точке.
//
// Общие детали планарной модели — в `msplanar.h`. Там же сказано, чего этой моделью
// выразить нельзя: связанных линий, потому что магнитные стенки не пропускают
// краевое поле.

#include "msplanar.h"

#include "sigfile.h"

#include <algorithm>
#include <cmath>

namespace tpl::samples {

namespace {

using namespace msp;

// ------------------------------------------------------------ изгиб 90 градусов

Build buildBend(const Values &v)
{
    const Board bd = board(v);
    const double f0 = v.value(QStringLiteral("f0"));
    const Line l = bd.line(v.value(QStringLiteral("z0")));
    const double miter = v.value(QStringLiteral("miter")) / 100.0;

    Builder b(QStringLiteral("msbend"));
    b.p.delta = deltaFitting(l.weff, gridFor(l, f0));
    const double d = b.p.delta;
    const double w = l.weff;
    const double feed = snap(l.lambdaG(f0), d);

    // Канал приходит слева вдоль X и уходит вверх вдоль Y. Внешний угол — (xr, yb),
    // внутренний — (xl, yb+w): скос режет именно внешний.
    const double yb = 3 * d;
    const double xl = feed;
    const double xr = xl + w;
    const double yTop = yb + w + feed;
    const double cut = snap(miter * w, d);
    b.p.box = {0.0, xr + 3 * d, 0.0, yTop};
    b.p.description = QStringLiteral("Изгиб микрополосковой линии %1 Ом на 90 градусов, скос "
                                     "%2 процентов ширины, %3.")
                          .arg(num(l.z0), num(miter * 100.0, 3), bd.sub.name);

    QVector<QPointF> outer{QPointF(0, yb)};
    if (cut > 0.0)
        outer << QPointF(xr - cut, yb) << QPointF(xr, yb + cut);
    else
        outer << QPointF(xr, yb);
    outer << QPointF(xr, yTop);
    b.polyline(QStringLiteral("внешняя стенка со скосом"), Material::Magnetic, outer);
    b.polyline(QStringLiteral("внутренняя стенка"), Material::Magnetic,
               {QPointF(0, yb + w), QPointF(xl, yb + w), QPointF(xl, yTop)});

    fill(b, QStringLiteral("плечо по X"), l, 0.0, xr, yb, yb + w);
    fill(b, QStringLiteral("плечо по Y"), l, xl, xr, yb + w, yTop);

    msPort(b, QStringLiteral("вход 1"), PortSide::Left, yb, yb + w, true, l);
    msPort(b, QStringLiteral("вход 2"), PortSide::Top, xl, xr, false, l);
    b.freqs(std::round(f0 * 0.6 * 100) / 100, std::round(f0 * 1.6 * 100) / 100, 21);
    b.outputs(true);
    b.time(settleTime((xr + yTop) * std::sqrt(l.epsEff), f0, 0.0));

    QVector<Row> rows = boardRows(bd);
    rows << lineRow(QStringLiteral("Линия %1 Ом").arg(num(l.z0)), l, f0);
    rows << Row{QStringLiteral("Скос"),
                cut > 0.0 ? QStringLiteral("%1 % ширины канала, то есть %2")
                                .arg(num(miter * 100.0, 3), mm(cut))
                          : QStringLiteral("нет — прямой угол")};
    rows << Row{QStringLiteral("Ожидаемо"),
                QStringLiteral("|S21| около 0 дБ; без скоса |S11| заметно хуже — лишняя медь "
                               "в углу добавляет ёмкость")};
    return b.done(rows, {QStringLiteral("Выход стоит на ВЕРХНЕЙ границе области: в языке это "
                                        "INPUT_Y, и апертура у него задаётся по X, а не по Y."),
                         QStringLiteral("Диэлектрик залит двумя прямоугольниками, поэтому за "
                                        "скосом остаётся треугольник заполнения. Полю он не "
                                        "достаётся: магнитная стенка проходит по скосу.")});
}

// ------------------------------------------------------------ Т-образный делитель

Build buildTee(const Values &v)
{
    const Board bd = board(v);
    const double f0 = v.value(QStringLiteral("f0"));
    const double z0 = v.value(QStringLiteral("z0"));
    const Line lin = bd.line(z0);
    const Line arm = bd.line(std::min(120.0, 2.0 * z0)); // два плеча в параллель дают Z0

    Builder b(QStringLiteral("mstee"));
    b.p.delta = deltaFitting(arm.weff, gridFor(arm, f0));
    const double d = b.p.delta;
    const double w0 = lin.weff, w1 = arm.weff;
    const double yb = 3 * d;
    const double xj = snap(lin.lambdaG(f0), d);
    const double X = xj + snap(arm.lambdaG(f0), d);
    const double Y = yb + w0 + snap(arm.lambdaG(f0), d);
    b.p.box = {0.0, X, 0.0, Y};
    b.p.description = QStringLiteral("Т-образный делитель мощности: %1 Ом -> два плеча по %2 Ом, "
                                     "%3 ГГц, %4.")
                          .arg(num(z0), num(arm.z0), num(f0, 3), bd.sub.name);

    // Низ идёт насквозь: у входного канала и у правого плеча он общий.
    b.polyline(QStringLiteral("стенка снизу"), Material::Magnetic,
               seg(QPointF(0, yb), QPointF(X, yb)));
    // Верх входа переходит в левую стенку верхнего плеча.
    b.polyline(QStringLiteral("стенка сверху и слева от плеча"), Material::Magnetic,
               {QPointF(0, yb + w0), QPointF(xj - w1, yb + w0), QPointF(xj - w1, Y)});
    // Правая стенка верхнего плеча переходит в верх правого плеча.
    b.polyline(QStringLiteral("стенка справа от плеча и сверху"), Material::Magnetic,
               {QPointF(xj, Y), QPointF(xj, yb + w1), QPointF(X, yb + w1)});

    fill(b, QStringLiteral("вход %1 Ом").arg(num(z0)), lin, 0.0, xj, yb, yb + w0);
    fill(b, QStringLiteral("плечо вверх"), arm, xj - w1, xj, yb + w0, Y);
    fill(b, QStringLiteral("плечо вправо"), arm, xj, X, yb, yb + w1);

    msPort(b, QStringLiteral("вход 1"), PortSide::Left, yb, yb + w0, true, lin);
    msPort(b, QStringLiteral("вход 2 (вправо)"), PortSide::Right, yb, yb + w1, false, arm);
    msPort(b, QStringLiteral("вход 3 (вверх)"), PortSide::Top, xj - w1, xj, false, arm);
    b.freqs(std::round(f0 * 0.6 * 100) / 100, std::round(f0 * 1.4 * 100) / 100, 21);
    b.outputs(true);
    b.time(settleTime((X + Y) * std::sqrt(arm.epsEff), f0, 0.0, 5.0));

    QVector<Row> rows = boardRows(bd);
    rows << lineRow(QStringLiteral("Вход %1 Ом").arg(num(z0)), lin, f0);
    rows << lineRow(QStringLiteral("Плечи %1 Ом").arg(num(arm.z0)), arm, f0);
    rows << Row{QStringLiteral("Согласование на постоянном токе"),
                QStringLiteral("два плеча по %1 Ом в параллель = %2 Ом (вход %3 Ом)")
                    .arg(num(arm.z0), num(arm.z0 / 2.0), num(z0))};
    rows << Row{QStringLiteral("Ожидаемо"),
                QStringLiteral("|S21| = |S31| = -3 дБ, |S11| мал на f0")};
    return b.done(rows, {QStringLiteral("Развязки между плечами у такого делителя НЕТ — это не "
                                        "Вилкинсон: поглощающий резистор между плечами в "
                                        "планарной модели выразить нечем."),
                         QStringLiteral("Реактивность самого соединения учебниковой формулой не "
                                        "описывается — на неё и смотрят по |S11| ядра.")});
}

// ------------------------------------------------------------ нагрузка на поглотителе

Build buildMsLoad(const Values &v)
{
    const Board bd = board(v);
    const double f0 = v.value(QStringLiteral("f0"));
    const Line l = bd.line(v.value(QStringLiteral("z0")));
    const int cells = int(v.value(QStringLiteral("cells")));

    Builder b(QStringLiteral("msload"));
    b.p.delta = deltaFitting(l.weff, gridFor(l, f0));
    const double d = b.p.delta;
    const double w = l.weff;
    const double yb = 3 * d, yh = yb + w;
    const double X = snap(2.0 * l.lambdaG(f0), d);
    b.p.box = {0.0, X, 0.0, yh + 3 * d};
    b.p.description = QStringLiteral("Нагрузка линии %1 Ом на поглотителе: %2 слоёв поперёк "
                                     "канала у края области, %3.")
                          .arg(num(l.z0)).arg(cells).arg(bd.sub.name);

    b.polyline(QStringLiteral("край полоски, низ"), Material::Magnetic,
               seg(QPointF(0, yb), QPointF(X, yb)));
    b.polyline(QStringLiteral("край полоски, верх"), Material::Magnetic,
               seg(QPointF(0, yh), QPointF(X, yh)));
    fill(b, QStringLiteral("линия"), l, 0.0, X, yb, yh);

    // 🔑 Поглотитель стоит у САМОГО КРАЯ области и растёт внутрь — так же, как рамка
    // поглотителя в авторских заданиях.
    //
    // ⚠️ ABSORBER в языке — КОНТУР, а не заливка: поглощающего объёма не существует.
    // Проверено прогонами (2026-10-01), и обе проверки важны:
    //   * поглотитель посередине области, с каналом за ним, даёт то же самое — часть
    //     волны проходит, отражается от границы коробки (края НЕ поглощают) и
    //     возвращается;
    //   * число слоёв на отражение НЕ влияет: один слой и пять слоёв дали одно и то
    //     же. Стопка контуров не становится толстым поглотителем.
    // Поэтому параметр «слоёв» здесь — повод убедиться в этом самому, а не способ
    // улучшить согласование.
    for (int i = 0; i < cells; ++i) {
        const double x = X - i * d;
        b.polyline(QStringLiteral("поглотитель, слой %1").arg(i + 1), Material::Absorber,
                   seg(QPointF(x, yb), QPointF(x, yh)));
    }

    msPort(b, QStringLiteral("вход 1"), PortSide::Left, yb, yh, true, l);
    b.freqs(std::round(f0 * 0.5 * 100) / 100, std::round(f0 * 1.5 * 100) / 100, 21);
    b.outputs(true);
    b.time(settleTime(2.0 * X * std::sqrt(l.epsEff), f0, 0.0));

    QVector<Row> rows = boardRows(bd);
    rows << lineRow(QStringLiteral("Линия %1 Ом").arg(num(l.z0)), l, f0);
    rows << Row{QStringLiteral("Поглотитель"),
                QStringLiteral("%1 слоёв по %2 у края области (x от %3 до %4)")
                    .arg(cells).arg(mm(d), mm(X - (cells - 1) * d), mm(X))};
    rows << Row{QStringLiteral("Измерено ядром (умолчания)"),
                QStringLiteral("|S11| от -2 до -7 дБ по полосе — столько отражает ABSORBER "
                               "в планарном канале")};
    rows << Row{QStringLiteral("Для сравнения"),
                QStringLiteral("тот же плоский поглотитель в МЕТАЛЛИЧЕСКОМ волноводе (образец "
                               "«Нагрузка волновода») даёт -12...-22 дБ")};
    rows << Row{QStringLiteral("Разомкнутый конец"),
                QStringLiteral("около 0 дБ — отражается всё (образец «Разомкнутый конец линии»)")};
    return b.done(rows, {QStringLiteral("Порт ОДИН: матрица вырождается в одно число S11, и "
                                        "именно оно здесь измеряется."),
                         QStringLiteral("Задание не рекламирует поглотитель, а ИЗМЕРЯЕТ его. "
                                        "Вывод из прогонов: в канале с магнитными стенками "
                                        "ABSORBER отражает заметно больше, чем в металлическом "
                                        "волноводе. Это свойство граничного условия ядра, и его "
                                        "надо знать всем, кто ставит рамку поглотителя вокруг "
                                        "планарной структуры."),
                         QStringLiteral("Число слоёв проверено: один и пять дают одно и то же. "
                                        "ABSORBER — это условие на контуре, а не толщина "
                                        "поглощающей среды, и стопка контуров толстым "
                                        "поглотителем не становится.")});
}

// ------------------------------------------------------------ разомкнутый конец

Build buildOpenEnd(const Values &v)
{
    const Board bd = board(v);
    const double f0 = v.value(QStringLiteral("f0"));
    const Line l = bd.line(v.value(QStringLiteral("z0")));

    Builder b(QStringLiteral("msopen"));
    b.p.delta = deltaFitting(l.weff, gridFor(l, f0));
    const double d = b.p.delta;
    const double w = l.weff;
    const double yb = 3 * d, yh = yb + w;
    const double L = snap(v.value(QStringLiteral("len")) * l.lambdaG(f0), d);
    b.p.box = {0.0, L + 3 * d, 0.0, yh + 3 * d};
    b.p.description = QStringLiteral("Разомкнутый конец линии %1 Ом: магнитная стенка поперёк "
                                     "канала, %2.")
                          .arg(num(l.z0), bd.sub.name);

    b.polyline(QStringLiteral("край полоски, низ"), Material::Magnetic,
               seg(QPointF(0, yb), QPointF(L, yb)));
    b.polyline(QStringLiteral("край полоски, верх"), Material::Magnetic,
               seg(QPointF(0, yh), QPointF(L, yh)));
    b.polyline(QStringLiteral("разомкнутый конец"), Material::Magnetic,
               seg(QPointF(L, yb), QPointF(L, yh)));
    fill(b, QStringLiteral("линия"), l, 0.0, L, yb, yh);
    msPort(b, QStringLiteral("вход 1"), PortSide::Left, yb, yh, true, l);
    b.freqs(std::round(f0 * 0.5 * 100) / 100, std::round(f0 * 1.5 * 100) / 100, 21);
    b.outputs(true);
    b.time(settleTime(2.0 * L * std::sqrt(l.epsEff), f0, 0.0));

    // Удлинение разомкнутого конца по Хаммерстаду: насколько линия «кажется» длиннее
    // из-за краевого поля. Планарная модель этого НЕ содержит, и в том весь смысл.
    const double u = l.w / bd.h;
    const double dl = 0.412 * bd.h * (l.epsEff + 0.3) / (l.epsEff - 0.258) * (u + 0.262)
                      / (u + 0.813);

    QVector<Row> rows = boardRows(bd);
    rows << lineRow(QStringLiteral("Линия %1 Ом").arg(num(l.z0)), l, f0);
    rows << Row{QStringLiteral("Длина до конца"),
                QStringLiteral("%1 (%2 длины волны в линии)").arg(mm(L), num(L / l.lambdaG(f0)))};
    rows << Row{QStringLiteral("Фаза S11 по теории линий"),
                QStringLiteral("%1 град. на f0 (идеальный холостой ход)")
                    .arg(num(std::fmod(-720.0 * L / l.lambdaG(f0), 360.0), 4))};
    rows << Row{QStringLiteral("Удлинение края (Хаммерстад)"),
                QStringLiteral("%1 — столько НЕ учитывает планарная модель").arg(mm(dl))};
    rows << Row{QStringLiteral("Ожидаемо"),
                QStringLiteral("|S11| около 0 дБ на всех частотах: магнитная стенка отражает "
                               "полностью, излучения с края в двумерной модели нет")};
    return b.done(rows, {QStringLiteral("Это образец-ЭТАЛОН. Отражать здесь должно ровно "
                                        "единицу, поэтому всё, что в |S11| не дотягивает до "
                                        "нуля дБ, — остаток самого входа плоской волны."),
                         QStringLiteral("Настоящий разомкнутый конец излучает и «удлиняется» на "
                                        "величину из паспорта. Сравнивая фазу с теорией линий, "
                                        "эту поправку надо добавлять руками.")});
}

// ------------------------------------------------------------ импульсная рефлектометрия

Build buildTdr(const Values &v)
{
    const Board bd = board(v);
    const double f0 = v.value(QStringLiteral("f0"));
    const double z1 = v.value(QStringLiteral("z1")), z2 = v.value(QStringLiteral("z2"));
    const Line l1 = bd.line(z1), l2 = bd.line(z2);

    Builder b(QStringLiteral("mstdr"));
    const Line &narrow = (l1.weff < l2.weff) ? l1 : l2;
    b.p.delta = gridFor(narrow, f0);
    const double d = b.p.delta;
    const double wMax = std::max(l1.weff, l2.weff);
    const double yc = snap(3 * d + wMax / 2.0, d);
    const double L1 = snap(v.value(QStringLiteral("len1")) * l1.lambdaG(f0), d);
    const double L2 = snap(v.value(QStringLiteral("len2")) * l2.lambdaG(f0), d);
    const double X = L1 + L2;
    b.p.box = {0.0, X, 0.0, 2 * yc};
    b.p.description = QStringLiteral("Импульсная рефлектометрия ступеньки импеданса %1 -> %2 Ом: "
                                     "падающая волна из файла, %3.")
                          .arg(num(z1), num(z2), bd.sub.name);

    const auto edge = [&](double s) {
        return QVector<QPointF>{QPointF(0, yc + s * l1.weff / 2), QPointF(L1, yc + s * l1.weff / 2),
                                QPointF(L1, yc + s * l2.weff / 2), QPointF(X, yc + s * l2.weff / 2)};
    };
    b.polyline(QStringLiteral("край полоски, низ"), Material::Magnetic, edge(-1.0));
    b.polyline(QStringLiteral("край полоски, верх"), Material::Magnetic, edge(+1.0));
    fill(b, QStringLiteral("линия %1 Ом").arg(num(z1)), l1, 0.0, L1, yc - l1.weff / 2,
         yc + l1.weff / 2);
    fill(b, QStringLiteral("линия %1 Ом").arg(num(z2)), l2, L1, X, yc - l2.weff / 2,
         yc + l2.weff / 2);

    msPort(b, QStringLiteral("вход 1"), PortSide::Left, yc - l1.weff / 2, yc + l1.weff / 2, true,
           l1);
    msPort(b, QStringLiteral("вход 2"), PortSide::Right, yc - l2.weff / 2, yc + l2.weff / 2, false,
           l2);
    // ⚠️ Сигнал ставим ПОСЛЕ того, как добавлены ВСЕ порты, и по индексу, а не по
    // ссылке: `Builder::port` дописывает в вектор, вектор переселяется, и ссылка,
    // взятая раньше, указывает в освобождённую память. Поймано тем, что файл .sig
    // не появлялся рядом с заданием вовсе.
    Port &in = b.p.ports[0];

    // 🔑 Падающая волна — МОНОЦИКЛ из файла. Формулой её не задать: ядро умеет только
    // несущую под огибающей, а у моноцикла несущей нет вовсе. Нулевое среднее важно:
    // постоянная составляющая в линии не распространяется и только портила бы картину.
    in.signal.source = SignalSource::File;
    in.signal.wave.kind = Waveform::Monocycle;
    in.signal.wave.amplitude = 1.0;
    in.signal.wave.start = 0.0;
    // Длительность берём от времени пробега первого участка: импульс должен быть
    // КОРОЧЕ пробега, иначе отражение от ступеньки сольётся с падающим.
    const double travel1 = L1 * std::sqrt(l1.epsEff) / 299.792458; // нс
    in.signal.wave.width = std::max(2.0 * d / 299.792458, travel1 / 4.0);

    b.p.freqs = {suggestedFreq(in.signal.wave)};
    // SMATRIX не включаем: встроенный пересчёт одночастотный, а смысл задания — во
    // ВРЕМЕННОЙ картине. Результат смотрят по файлу сигналов .t.
    b.outputs(false);
    b.time(6.0 * travel1 + 4.0 * L2 * std::sqrt(l2.epsEff) / 299.792458 + 1.0);

    const double gamma = (z2 - z1) / (z2 + z1);
    QVector<Row> rows = boardRows(bd);
    rows << lineRow(QStringLiteral("Участок 1, %1 Ом").arg(num(z1)), l1, f0);
    rows << lineRow(QStringLiteral("Участок 2, %1 Ом").arg(num(z2)), l2, f0);
    rows << Row{QStringLiteral("Отражение от ступеньки"),
                QStringLiteral("Г = %1 (%2)").arg(num(gamma, 4), db(std::fabs(gamma)))};
    rows << Row{QStringLiteral("Время до ступеньки и обратно"),
                QStringLiteral("%1 нс (участок %2, eps_eff = %3)")
                    .arg(num(2.0 * travel1, 4), mm(L1), num(l1.epsEff))};
    rows << Row{QStringLiteral("Длительность импульса"),
                QStringLiteral("tau = %1 нс, полоса примерно до %2 ГГц")
                    .arg(num(in.signal.wave.width, 4), num(waveBand(in.signal.wave).hi, 4))};
    rows << Row{QStringLiteral("Что смотреть"),
                QStringLiteral("в файле %1.t: падающий импульс, затем отражённый через %2 нс")
                    .arg(b.p.output.signalFile, num(2.0 * travel1, 4))};
    return b.done(rows, {QStringLiteral("Падающую волну пишет сам генератор в файл %1.sig рядом "
                                        "с заданием — править его руками не нужно.")
                             .arg(signalFileName(b.p, b.p.ports.at(0))),
                         QStringLiteral("Матрица рассеяния здесь не снимается: встроенный "
                                        "пересчёт SMATRIX рассчитан на одну частоту, а импульс "
                                        "широкополосный. Полосу по этому же файлу .t строит "
                                        "постобработка."),
                         QStringLiteral("Знак отражения виден прямо на графике: при переходе на "
                                        "БОЛЬШЕЕ сопротивление отражённый импульс той же "
                                        "полярности, на меньшее — обратной.")});
}

// ------------------------------------------------------------ линия задержки

Build buildDelay(const Values &v)
{
    const Board bd = board(v);
    const double f0 = v.value(QStringLiteral("f0"));
    const Line l = bd.line(v.value(QStringLiteral("z0")));
    const int n = int(v.value(QStringLiteral("runs")));

    Builder b(QStringLiteral("msdelay"));
    b.p.delta = deltaFitting(l.weff, gridFor(l, f0));
    const double d = b.p.delta;
    const double w = l.weff;
    const double m0 = 3 * d;
    const double Lr = snap(v.value(QStringLiteral("run")) * l.lambdaG(f0), d);

    // Прогоны идут вдоль X и уложены друг над другом с шагом 2*w: между ними остаётся
    // полоса той же ширины, и колено получается квадратным.
    const auto yOf = [&](int i) { return m0 + i * 2.0 * w; };
    // Колено между прогоном i и i+1 стоит справа при чётном i и слева при нечётном.
    const auto turnRight = [](int i) { return i % 2 == 0; };

    const double x0 = 0.0, x1 = Lr;
    // ⚠️ Координаты колен кладём на УЗЛЫ сетки. Ширина канала кратна шагу по
    // построению (deltaFitting), но «x1 - w» в double даёт число с мусором в
    // последних битах, и при круговороте «открыл -> сохранил» задание отличалось от
    // исходного десятым знаком: эмиттер пишет локальные координаты, а читатель
    // прибавляет анкер, и разряда перестаёт хватать.
    const double xRightIn = snap(x1 - w, d);
    const double xLeftIn = snap(x0 + w, d);
    const double yTop = yOf(n - 1) + w + 3 * d;
    b.p.box = {x0, x1, 0.0, yTop};
    b.p.description = QStringLiteral("Линия задержки «змейкой»: %1 прогонов по %2, линия %3 Ом, "
                                     "%4. Падающая волна — видеоимпульс из файла.")
                          .arg(n).arg(mm(Lr), num(l.z0), bd.sub.name);

    // Заливка: прогоны и колена.
    for (int i = 0; i < n; ++i)
        fill(b, QStringLiteral("прогон %1").arg(i + 1), l, x0, x1, yOf(i), yOf(i) + w);
    for (int i = 0; i + 1 < n; ++i) {
        const double cx1 = turnRight(i) ? x1 : xLeftIn;
        const double cx0 = turnRight(i) ? xRightIn : x0;
        fill(b, QStringLiteral("колено %1").arg(i + 1), l, cx0, cx1, yOf(i) + w, yOf(i + 1));
    }

    // Горизонтальные стенки: каждая с «окном» там, где к прогону подходит колено.
    const auto hWall = [&](const QString &name, double y, bool hole, bool holeRight) {
        if (!hole) {
            b.polyline(name, Material::Magnetic, seg(QPointF(x0, y), QPointF(x1, y)));
        } else if (holeRight) {
            b.polyline(name, Material::Magnetic, seg(QPointF(x0, y), QPointF(xRightIn, y)));
        } else {
            b.polyline(name, Material::Magnetic, seg(QPointF(xLeftIn, y), QPointF(x1, y)));
        }
    };
    for (int i = 0; i < n; ++i) {
        hWall(QStringLiteral("прогон %1: стенка снизу").arg(i + 1), yOf(i), i > 0,
              turnRight(i - 1));
        hWall(QStringLiteral("прогон %1: стенка сверху").arg(i + 1), yOf(i) + w, i + 1 < n,
              turnRight(i));
    }
    // Стенки колен — ОБЕ, и внутренняя, и внешняя.
    //
    // ⚠️ Внешнюю нельзя заменить границей области: края коробки магнитными НЕ
    // являются, и на повороте волна уходила бы
    // в край вместо того, чтобы завернуть. Стенка ставится ровно по краю области —
    // блок на границе законен, так же стоят стенки волноводных образцов.
    for (int i = 0; i + 1 < n; ++i) {
        const double cIn = turnRight(i) ? xRightIn : xLeftIn;
        const double cOut = turnRight(i) ? x1 : x0;
        b.polyline(QStringLiteral("колено %1: внутренняя стенка").arg(i + 1), Material::Magnetic,
                   seg(QPointF(cIn, yOf(i) + w), QPointF(cIn, yOf(i + 1))));
        b.polyline(QStringLiteral("колено %1: внешняя стенка").arg(i + 1), Material::Magnetic,
                   seg(QPointF(cOut, yOf(i) + w), QPointF(cOut, yOf(i + 1))));
    }
    // Закрываем свободные торцы прогонов — все, кроме двух портов.
    const bool outRight = !turnRight(n - 2);
    for (int i = 0; i < n; ++i) {
        const bool leftIsPort = (i == 0) || (i == n - 1 && !outRight);
        const bool rightIsPort = (i == n - 1 && outRight);
        // Торец занят коленом? Колено стоит не в торце, а сверху/снизу, так что торец
        // всегда либо порт, либо стенка.
        if (!leftIsPort) {
            b.polyline(QStringLiteral("прогон %1: торец слева").arg(i + 1), Material::Magnetic,
                       seg(QPointF(x0, yOf(i)), QPointF(x0, yOf(i) + w)));
        }
        if (!rightIsPort) {
            b.polyline(QStringLiteral("прогон %1: торец справа").arg(i + 1), Material::Magnetic,
                       seg(QPointF(x1, yOf(i)), QPointF(x1, yOf(i) + w)));
        }
    }

    msPort(b, QStringLiteral("вход 1"), PortSide::Left, yOf(0), yOf(0) + w, true, l);
    msPort(b, QStringLiteral("вход 2"), outRight ? PortSide::Right : PortSide::Left,
           yOf(n - 1), yOf(n - 1) + w, false, l);
    // Ссылку берём ПОСЛЕ всех портов: см. пояснение в рецепте рефлектометрии.
    Port &in = b.p.ports[0];

    // Видеоимпульс: гауссова шапка без несущей. Формулой её не задать — у ядра всегда
    // есть множитель sin(2*pi*FREQ*t), то есть несущая.
    const double path = n * Lr + (n - 1) * w;
    const double travel = path * std::sqrt(l.epsEff) / 299.792458; // нс
    in.signal.source = SignalSource::File;
    in.signal.wave.kind = Waveform::GaussVideo;
    in.signal.wave.amplitude = 1.0;
    in.signal.wave.start = 0.0;
    in.signal.wave.width = std::max(3.0 * d / 299.792458, travel / 8.0);
    b.p.freqs = {suggestedFreq(in.signal.wave)};
    b.outputs(false);
    b.time(3.0 * travel + 1.0);

    QVector<Row> rows = boardRows(bd);
    rows << lineRow(QStringLiteral("Линия %1 Ом").arg(num(l.z0)), l, f0);
    rows << Row{QStringLiteral("Развёрнутая длина"),
                QStringLiteral("%1 при габарите %2 x %3").arg(mm(path), mm(Lr), mm(yTop))};
    rows << Row{QStringLiteral("Задержка по теории"),
                QStringLiteral("%1 нс (eps_eff = %2)").arg(num(travel, 4), num(l.epsEff))};
    rows << Row{QStringLiteral("Длительность импульса"), QStringLiteral("tau = %1 нс")
                    .arg(num(in.signal.wave.width, 4))};
    rows << Row{QStringLiteral("Что смотреть"),
                QStringLiteral("в файле %1.t импульс на входе 2 приходит на %2 нс позже, чем на "
                               "входе 1").arg(b.p.output.signalFile, num(travel, 4))};
    return b.done(rows, {QStringLiteral("Смысл задания — ВРЕМЕННАЯ картина, поэтому матрица "
                                        "рассеяния не снимается. Результат читают по файлу "
                                        "сигналов .t."),
                         QStringLiteral("Колена прибавляют задержку и чуть портят форму импульса: "
                                        "сравните приход с теоретической задержкой из паспорта."),
                         QStringLiteral("Соседние прогоны друг с другом НЕ связаны: стенки канала "
                                        "магнитные, и поле за них не выходит. У настоящей "
                                        "змейки связь между витками есть, и двумерная модель её "
                                        "не воспроизводит.")});
}

// ------------------------------------------------------------ плавный переход

Build buildTaper(const Values &v)
{
    const Board bd = board(v);
    const double f0 = v.value(QStringLiteral("f0"));
    const double z1 = v.value(QStringLiteral("z1")), z2 = v.value(QStringLiteral("z2"));
    const int steps = int(v.value(QStringLiteral("steps")));
    const Line l1 = bd.line(z1), l2 = bd.line(z2);

    Builder b(QStringLiteral("mstaper"));
    const Line &narrow = (l1.weff < l2.weff) ? l1 : l2;
    b.p.delta = gridFor(narrow, f0);
    const double d = b.p.delta;
    const double wMax = std::max(l1.weff, l2.weff);
    const double yc = snap(3 * d + wMax / 2.0, d);
    const double feed = snap(0.5 * l1.lambdaG(f0), d);
    const double L = snap(v.value(QStringLiteral("len")) * l1.lambdaG(f0), d);
    const double X = feed + L + feed;
    b.p.box = {0.0, X, 0.0, 2 * yc};
    b.p.description = QStringLiteral("Экспоненциальный переход %1 -> %2 Ом длиной %3, %4.")
                          .arg(num(z1), num(z2), mm(L), bd.sub.name);

    // Z(x) = Z1*exp(x/L*ln(Z2/Z1)) — классический экспоненциальный тейпер.
    const double lnr = std::log(z2 / z1);
    QVector<QPointF> lower, upper;
    lower << QPointF(0, yc - l1.weff / 2);
    upper << QPointF(0, yc + l1.weff / 2);
    fill(b, QStringLiteral("питание %1 Ом").arg(num(z1)), l1, 0.0, feed, yc - l1.weff / 2,
         yc + l1.weff / 2);
    lower << QPointF(feed, yc - l1.weff / 2);
    upper << QPointF(feed, yc + l1.weff / 2);

    for (int i = 0; i < steps; ++i) {
        const double u0 = double(i) / steps, u1 = double(i + 1) / steps;
        const double zc = z1 * std::exp(lnr * (u0 + u1) / 2.0); // импеданс середины шага
        const Line ls = bd.line(zc);
        const double xa = snap(feed + L * u0, d), xb = snap(feed + L * u1, d);
        if (xb <= xa)
            continue;
        fill(b, QStringLiteral("шаг %1 (%2 Ом)").arg(i + 1).arg(num(zc)), ls, xa, xb,
             yc - ls.weff / 2, yc + ls.weff / 2);
        lower << QPointF(xa, yc - ls.weff / 2) << QPointF(xb, yc - ls.weff / 2);
        upper << QPointF(xa, yc + ls.weff / 2) << QPointF(xb, yc + ls.weff / 2);
    }

    lower << QPointF(feed + L, yc - l2.weff / 2) << QPointF(X, yc - l2.weff / 2);
    upper << QPointF(feed + L, yc + l2.weff / 2) << QPointF(X, yc + l2.weff / 2);
    fill(b, QStringLiteral("питание %1 Ом").arg(num(z2)), l2, feed + L, X, yc - l2.weff / 2,
         yc + l2.weff / 2);

    b.polyline(QStringLiteral("край полоски, низ"), Material::Magnetic, lower);
    b.polyline(QStringLiteral("край полоски, верх"), Material::Magnetic, upper);
    msPort(b, QStringLiteral("вход 1"), PortSide::Left, yc - l1.weff / 2, yc + l1.weff / 2, true,
           l1);
    msPort(b, QStringLiteral("вход 2"), PortSide::Right, yc - l2.weff / 2, yc + l2.weff / 2, false,
           l2);
    b.freqs(std::round(f0 * 0.3 * 100) / 100, std::round(f0 * 2.0 * 100) / 100, 35);
    b.outputs(true);
    b.time(settleTime(X * std::sqrt(l1.epsEff), f0 * 0.3, 0.0));

    // |Г| экспоненциального тейпера: ln(Z2/Z1)/2 * sin(bL)/(bL).
    const auto gamma = [&](double f) {
        const double bl = 2.0 * kPi * L / (rf::lambda0(f) / std::sqrt(l1.epsEff));
        const double sinc = bl > 1e-9 ? std::sin(bl) / bl : 1.0;
        return std::fabs(lnr / 2.0 * sinc);
    };

    QVector<Row> rows = boardRows(bd);
    rows << lineRow(QStringLiteral("Линия %1 Ом").arg(num(z1)), l1, f0);
    rows << lineRow(QStringLiteral("Линия %1 Ом").arg(num(z2)), l2, f0);
    rows << Row{QStringLiteral("Длина перехода"),
                QStringLiteral("%1 (%2 длины волны), шагов %3")
                    .arg(mm(L), num(L / l1.lambdaG(f0)))
                    .arg(steps)};
    for (double k : {0.5, 1.0, 2.0})
        rows << Row{QStringLiteral("Теория тейпера, %1 ГГц").arg(num(k * f0, 3)),
                    QStringLiteral("|S11| = %1").arg(db(gamma(k * f0)))};
    rows << Row{QStringLiteral("Без перехода"),
                QStringLiteral("|S11| = %1").arg(db(std::fabs(z2 - z1) / (z2 + z1)))};
    return b.done(rows, {QStringLiteral("Экспоненциальный переход широкополоснее "
                                        "четвертьволнового трансформатора: отражение падает как "
                                        "sin(bL)/(bL), то есть монотонно с длиной."),
                         QStringLiteral("Переход набран ступеньками: в планарной модели с ростом "
                                        "Z0 меняется и ширина канала, и eps_eff, поэтому каждый "
                                        "шаг — своя линия со своим заполнением.")});
}

// ------------------------------------------------------------ патч-антенна

Build buildPatch(const Values &v)
{
    const Board bd = board(v);
    const double fr = v.value(QStringLiteral("fr"));
    const Line feedLine = bd.line(v.value(QStringLiteral("z0")));

    // Ширина патча по обычной рекомендации, проницаемость — по этой ширине.
    const double c = 299.792458; // мм*ГГц
    const double Wp = c / (2.0 * fr) * std::sqrt(2.0 / (bd.sub.eps + 1.0));
    const double epsEffPatch = rf::msEpsEff(Wp / bd.h, bd.sub.eps);
    const double Lp = c / (2.0 * fr * std::sqrt(epsEffPatch));

    Builder b(QStringLiteral("mspatch"));
    b.p.delta = chooseDelta(c / fr / std::sqrt(epsEffPatch), std::min(feedLine.weff, Lp), 24.0,
                            12.0);
    const double d = b.p.delta;
    const int frame = int(v.value(QStringLiteral("frame")));
    const double air = snap(v.value(QStringLiteral("air")) * c / fr, d); // воздух вокруг патча
    const double t = frame * d;

    const double feed = snap(0.5 * feedLine.lambdaG(fr), d);
    const double xp = snap(t + air + feed, d);
    const double xq = snap(xp + Lp, d);
    const double X = snap(xq + air + t, d);
    const double yc = snap((t + air + Wp / 2.0 + air + t) / 2.0, d);
    const double Y = snap(2.0 * yc, d);
    const double yp0 = snap(yc - Wp / 2.0, d), yp1 = snap(yc + Wp / 2.0, d);
    const double wf = feedLine.weff;
    const double yf0 = snap(yc - wf / 2.0, d), yf1 = snap(yc + wf / 2.0, d);

    b.p.box = {0.0, X, 0.0, Y};
    // В описании — размеры, которые РЕАЛЬНО легли на сетку, а не расчётные: человек
    // читает описание и сверяет его с холстом, и расхождение в сотых только путает.
    const double LpGrid = xq - xp, WpGrid = yp1 - yp0;
    b.p.description = QStringLiteral("Патч-антенна %1 x %2 на %3 ГГц, модель резонатора с "
                                     "излучающими щелями, %4.")
                          .arg(mm(LpGrid), mm(WpGrid), num(fr, 3), bd.sub.name);

    // Патч: заливка eps_eff и магнитные стенки по двум краям вдоль резонанса.
    fill(b, QStringLiteral("патч"), Line{v.value(QStringLiteral("z0")), 0.0, epsEffPatch, Wp}, xp,
         xq, yp0, yp1);
    b.polyline(QStringLiteral("край патча, низ"), Material::Magnetic,
               seg(QPointF(xp, yp0), QPointF(xq, yp0)));
    b.polyline(QStringLiteral("край патча, верх"), Material::Magnetic,
               seg(QPointF(xp, yp1), QPointF(xq, yp1)));

    // 🔑 Два ЧЕСТНЫХ случая, и выбирает их человек.
    //
    // «Резонатор»: края x тоже магнитные — тогда вдоль x стоит полуволновый резонанс,
    // и его частоту даёт формула. Это и есть классическая модель резонатора патча
    // (Баланис): сперва находят частоту в замкнутой полости, а излучение добавляют
    // потом как поправку.
    //
    // «Излучение»: края x открыты в воздух, и резонатора вдоль x НЕТ ВОВСЕ.
    // Проверено прогоном: при открытых краях провалы |S11| оказались на 2.475 и
    // 3.825 ГГц, тогда как полуволновый резонанс патча — 2.98 ГГц. Провалы ставит
    // линия питания со щелью, а не полость. Паспорт, обещавший бы здесь резонанс,
    // просто врал бы — что и выяснилось числами ядра.
    const bool closed = v.value(QStringLiteral("edges")) < 0.5;
    if (closed) {
        // Левый край — двумя кусками: в середине щель, через неё входит линия.
        b.polyline(QStringLiteral("край патча, слева снизу"), Material::Magnetic,
                   seg(QPointF(xp, yp0), QPointF(xp, yf0)));
        b.polyline(QStringLiteral("край патча, слева сверху"), Material::Magnetic,
                   seg(QPointF(xp, yf1), QPointF(xp, yp1)));
        b.polyline(QStringLiteral("край патча, справа"), Material::Magnetic,
                   seg(QPointF(xq, yp0), QPointF(xq, yp1)));
    }

    // Питающая линия от левой границы до щели патча.
    fill(b, QStringLiteral("питающая линия"), feedLine, 0.0, xp, yf0, yf1);
    b.polyline(QStringLiteral("край линии, низ"), Material::Magnetic,
               seg(QPointF(0, yf0), QPointF(xp, yf0)));
    b.polyline(QStringLiteral("край линии, верх"), Material::Magnetic,
               seg(QPointF(0, yf1), QPointF(xp, yf1)));

    // Рамка поглотителя — это «свободное пространство»: без неё область была бы
    // закрытой коробкой, и излучённое вернулось бы обратно эхом.
    // ⚠️ ABSORBER — контур, а не заливка: слои ставим по одному на шаг сетки.
    for (int i = 0; i < frame; ++i) {
        const double o = i * d;
        b.polyline(QStringLiteral("поглотитель справа, слой %1").arg(i + 1), Material::Absorber,
                   seg(QPointF(X - o, 0.0), QPointF(X - o, Y)));
        b.polyline(QStringLiteral("поглотитель снизу, слой %1").arg(i + 1), Material::Absorber,
                   seg(QPointF(0.0, o), QPointF(X, o)));
        b.polyline(QStringLiteral("поглотитель сверху, слой %1").arg(i + 1), Material::Absorber,
                   seg(QPointF(0.0, Y - o), QPointF(X, Y - o)));
        // Слева — только выше и ниже апертуры питающей линии: сам вход закрывать нечем.
        b.polyline(QStringLiteral("поглотитель слева снизу, слой %1").arg(i + 1),
                   Material::Absorber, seg(QPointF(o, 0.0), QPointF(o, yf0)));
        b.polyline(QStringLiteral("поглотитель слева сверху, слой %1").arg(i + 1),
                   Material::Absorber, seg(QPointF(o, yf1), QPointF(o, Y)));
    }

    msPort(b, QStringLiteral("вход 1"), PortSide::Left, yf0, yf1, true, feedLine);
    b.freqs(std::round(fr * 0.7 * 100) / 100, std::round(fr * 1.3 * 100) / 100, 25);
    b.outputs(true);
    b.time(settleTime(4.0 * X * std::sqrt(epsEffPatch), fr * 0.7, 0.0, 5.0));

    QVector<Row> rows = boardRows(bd);
    rows << Row{QStringLiteral("Ширина патча"),
                QStringLiteral("%1 (по формуле c/(2*fr)*sqrt(2/(eps+1)))").arg(mm(Wp))};
    rows << Row{QStringLiteral("Проницаемость патча"), QStringLiteral("eps_eff = %1 при W/h = %2")
                    .arg(num(epsEffPatch), num(Wp / bd.h))};
    rows << Row{QStringLiteral("Длина патча"),
                QStringLiteral("%1 = c/(2*fr*sqrt(eps_eff)) — БЕЗ поправки на края").arg(mm(Lp))};
    rows << Row{QStringLiteral("Легло на сетку"),
                QStringLiteral("%1 x %2 при шаге %3 — резонанс считается по ЭТИМ размерам")
                    .arg(mm(LpGrid), mm(WpGrid), mm(d))};
    rows << Row{QStringLiteral("Резонанс по размерам сетки"),
                QStringLiteral("%1 ГГц")
                    .arg(num(c / (2.0 * LpGrid * std::sqrt(epsEffPatch)), 4))};
    rows << lineRow(QStringLiteral("Питающая линия %1 Ом").arg(num(feedLine.z0)), feedLine, fr);
    const double frGrid = c / (2.0 * LpGrid * std::sqrt(epsEffPatch));
    rows << Row{QStringLiteral("Края патча"),
                closed ? QStringLiteral("магнитные стенки — замкнутая полость, резонатор")
                       : QStringLiteral("открыты в воздух — излучающие щели")};
    rows << Row{QStringLiteral("Ожидаемо"),
                closed ? QStringLiteral("узкий провал |S11| — полуволновый резонанс полости. "
                                        "Формула по размерам сетки даёт %1 ГГц, а ядро на "
                                        "умолчаниях показало -12.8 дБ на 3.30 ГГц, то есть на "
                                        "11 процентов выше: щель под питание в торцевой стенке "
                                        "замыкает полость не до конца")
                             .arg(num(frGrid, 4))
                       : QStringLiteral("провалы |S11| ставит линия питания со щелью, а НЕ "
                                        "полость: резонатора вдоль патча при открытых краях нет. "
                                        "Проверено прогоном — провалы вышли на 2.475 и 3.825 ГГц "
                                        "при расчётном резонансе %1 ГГц").arg(num(frGrid, 4))};

    QStringList notes;
    if (closed) {
        notes << QStringLiteral("Все четыре края патча — магнитные стенки, в левом оставлена "
                                "щель под линию питания. Так и считают частоту резонатора патча "
                                "(Баланис): излучение добавляют потом поправкой, а двумерная "
                                "модель даёт саму частоту.");
        notes << QStringLiteral("Рамка поглотителя вокруг области здесь страховка: из замкнутой "
                                "полости наружу ничего не идёт. Переключите края на открытые — и "
                                "рамка станет свободным пространством, в которое патч излучает.");
    } else {
        notes << QStringLiteral("Края патча открыты — это излучающие щели, и рамка поглотителя "
                                "вокруг области играет свободное пространство. ⚠ Играет "
                                "неточно: в канале с магнитными стенками ABSORBER отражает "
                                "около -2...-7 дБ (измерено образцом «Поглотитель в канале»), "
                                "так что часть излучённого возвращается эхом.");
        notes << QStringLiteral("Резонанса вдоль патча здесь НЕТ: полость разомкнута. Провалы "
                                "|S11| — это согласование линии питания с излучающей щелью, и "
                                "их положение зависит от длины линии. Чтобы увидеть именно "
                                "резонанс, переключите края на магнитные стенки.");
    }
    notes << QStringLiteral("⚠ Два разных сдвига частоты, и путать их нельзя. У НАСТОЯЩЕГО "
                            "патча края «удлиняют» резонатор (около 2 x 0.4 h), и его резонанс "
                            "ниже формулы идеальной полости — этого двумерная модель не знает "
                            "вовсе. А в самой модели резонанс вышел ВЫШЕ формулы (3.30 против "
                            "2.98 ГГц на умолчаниях): так действует щель под питание. Оба числа "
                            "измерены, а не выведены.");
    notes << QStringLiteral("Питание торцом, а не вставкой: согласование не идеальное, и глубина "
                            "провала тут не показатель.");
    return b.done(rows, notes);
}

// ------------------------------------------------------------ ПЗФ на шлейфах

Build buildBsf(const Values &v)
{
    const Board bd = board(v);
    const double f0 = v.value(QStringLiteral("f0"));
    const Line l = bd.line(v.value(QStringLiteral("z0")));
    const Line st = bd.line(v.value(QStringLiteral("zs")));
    const int n = int(v.value(QStringLiteral("stubs")));
    const bool wideband = v.value(QStringLiteral("mode")) > 0.5;
    const double corr = v.value(QStringLiteral("corr")) / 100.0;

    Builder b(QStringLiteral("msbsf"));
    b.p.delta = deltaFitting(st.weff, gridFor(st, f0));
    const double d = b.p.delta;
    const double lg = l.lambdaG(f0);
    const double w = l.weff, ws = st.weff;
    const double yb = 3 * d, yh = yb + w;
    const double pitch = snap(lg / 4.0 * (1.0 + corr), d); // между осями шлейфов
    const double lead = snap(lg * 0.75, d);
    const double stubLen = snap(lg / 4.0 * (1.0 + corr) - w / 2.0, d);
    const double ytop = yh + stubLen;

    QVector<double> axes;
    for (int i = 0; i < n; ++i)
        axes << snap(lead + i * pitch, d);
    const double X = snap(axes.last() + lead, d);
    b.p.box = {0.0, X, 0.0, ytop + 3 * d};
    b.p.description = QStringLiteral("Полосно-заграждающий фильтр на %1 разомкнутых шлейфах "
                                     "lambda/4, %2 ГГц, линия %3 Ом, шлейфы %4 Ом, %5.")
                          .arg(n).arg(num(f0, 3), num(l.z0), num(st.z0), bd.sub.name);

    b.polyline(QStringLiteral("край полоски, низ"), Material::Magnetic,
               seg(QPointF(0, yb), QPointF(X, yb)));
    // Верхний край с «гребёнкой» шлейфов: одна ломаная на всю линию.
    QVector<QPointF> top{QPointF(0, yh)};
    for (double a : axes) {
        top << QPointF(a - ws / 2.0, yh) << QPointF(a - ws / 2.0, ytop)
            << QPointF(a + ws / 2.0, ytop) << QPointF(a + ws / 2.0, yh);
    }
    top << QPointF(X, yh);
    b.polyline(QStringLiteral("край полоски и шлейфов"), Material::Magnetic, top);

    fill(b, QStringLiteral("линия"), l, 0.0, X, yb, yh);
    for (int i = 0; i < n; ++i) {
        fill(b, QStringLiteral("шлейф %1").arg(i + 1), st, axes.at(i) - ws / 2.0,
             axes.at(i) + ws / 2.0, yh, ytop);
    }

    msPort(b, QStringLiteral("вход 1"), PortSide::Left, yb, yh, true, l);
    msPort(b, QStringLiteral("вход 2"), PortSide::Right, yb, yh, false, l);
    // Ссылку берём ПОСЛЕ всех портов: см. пояснение в рецепте рефлектометрии.
    Port &in = b.p.ports[0];

    const double fLo = std::round(f0 * 0.5 * 100) / 100;
    const double fHi = std::round(f0 * 1.5 * 100) / 100;
    if (wideband) {
        // 🔑 Весь диапазон за ОДИН проход: гребёнка тонов в файле падающей волны.
        // Вместо сорока одного прогона — один, а матрицу по полосе строит
        // постобработка по файлу сигналов .t.
        in.signal.source = SignalSource::File;
        in.signal.wave.kind = Waveform::MultiTone;
        in.signal.wave.f1 = fLo;
        in.signal.wave.f2 = fHi;
        in.signal.wave.tones = 41;
        in.signal.wave.amplitude = 1.0;
        in.signal.wave.start = 0.0;
        b.p.freqs = {suggestedFreq(in.signal.wave)};
        b.outputs(false);
        in.signal.wave.duration = 0.0; // поставим после b.time(), когда известен счёт
    } else {
        b.freqs(fLo, fHi, 41);
        b.outputs(true);
    }
    b.time(settleTime((X + 2 * stubLen) * std::sqrt(l.epsEff), f0 * 0.5, 0.0, 5.0));
    if (wideband) {
        // Посылка тянется на весь счёт: многотон непрерывный, и постобработка
        // раскладывает его по частотам на УСТАНОВИВШЕМСЯ участке.
        b.p.ports[0].signal.wave.duration = b.p.timeMax;
    }

    QVector<Row> rows = boardRows(bd);
    rows << lineRow(QStringLiteral("Линия %1 Ом").arg(num(l.z0)), l, f0);
    rows << lineRow(QStringLiteral("Шлейфы %1 Ом").arg(num(st.z0)), st, f0);
    rows << Row{QStringLiteral("Шлейфов"),
                QStringLiteral("%1, между осями %2, длина от края линии %3")
                    .arg(n).arg(mm(pitch), mm(stubLen))};
    rows << Row{QStringLiteral("Ожидаемо"),
                QStringLiteral("провал |S21| на %1 ГГц тем глубже и шире, чем больше шлейфов; "
                               "у %2 шлейфов полоса заграждения заметно шире, чем у одного")
                    .arg(num(f0, 3)).arg(n)};
    rows << Row{QStringLiteral("Способ расчёта"),
                wideband ? QStringLiteral("ОДИН проход: гребёнка из %1 тонов %2...%3 ГГц в файле "
                                          "падающей волны")
                               .arg(41).arg(num(fLo, 3), num(fHi, 3))
                         : QStringLiteral("свип: 41 шаг, на каждом своя FREQ")};
    // Числа замерены на умолчаниях (2026-10-01, i5-9600KF): свип 393 с, широкополосный
    // проход 13.6 с. Разница — в число точек свипа, как и обещает метод.
    rows << Row{QStringLiteral("Время счёта (замерено)"),
                wideband ? QStringLiteral("13.6 с — один проход; свип по тем же 41 точкам шёл "
                                          "393 с, то есть в 29 раз дольше")
                         : QStringLiteral("393 с на 41 шаг; тем же заданием через гребёнку тонов "
                                          "вышло 13.6 с — в 29 раз быстрее")};
    rows << Row{QStringLiteral("Поправка длин"), QStringLiteral("%1 %").arg(num(corr * 100.0))};

    QStringList notes{QStringLiteral("Шлейфы работают по самому каналу, а не через краевое поле, "
                                     "поэтому планарная модель их выражает. Фильтр на СВЯЗАННЫХ "
                                     "резонаторах в ней не получится: магнитные стенки не "
                                     "пропускают поле между полосками.")};
    if (wideband) {
        notes << QStringLiteral("Широкополосный проход считается столько же, сколько один шаг "
                                "свипа, — выигрыш во столько раз, сколько в свипе точек. "
                                "Матрица рассеяния не снимается: встроенный SMATRIX "
                                "одночастотный, полосу строит постобработка по файлу .t.");
        notes << QStringLiteral("FREQ стоит в ЦЕНТРЕ полосы: по ней ядро согласует порт.");
    } else {
        notes << QStringLiteral("Тот же диапазон можно посчитать за ОДИН проход — переключите "
                                "способ расчёта на гребёнку тонов.");
    }
    return b.done(rows, notes);
}

} // namespace

void addMicrostrip2(QVector<Sample> *out)
{
    const QString sec = QStringLiteral("microstrip");
    const QString ref = QStringLiteral("Хаммерстад-Йенсен; планарная модель Весселя-Кольхоффа");
    {
        Sample s;
        s.id = QStringLiteral("ms_bend");
        s.section = sec;
        s.title = QStringLiteral("Изгиб 90° со скосом");
        s.summary = QStringLiteral("Поворот линии: прямой угол против скошенного");
        s.standard = ref;
        s.help = QStringLiteral(
                     "Прямоугольный поворот микрополосковой линии. В углу собирается лишняя "
                     "медь, и она работает как ёмкость — отражение растёт. Лечат это скосом: "
                     "внешний угол срезают под 45°. Поставьте скос 0 и сравните |S11|.")
                 + helpPlanar();
        s.params = boardParams();
        s.params << z0Param(QStringLiteral("Сопротивление линии"), 50.0) << freqParam(3.0)
                 << Param::number(QStringLiteral("miter"), QStringLiteral("Скос"),
                                  QStringLiteral("% ширины"), 57.0, 0.0, 90.0, 1.0, 0,
                                  QStringLiteral("Доля ширины канала, срезаемая у внешнего угла. "
                                                 "0 — прямой угол; около 57 % считается "
                                                 "оптимумом"));
        s.build = buildBend;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ms_tee");
        s.section = sec;
        s.title = QStringLiteral("Т-образный делитель мощности");
        s.summary = QStringLiteral("Деление пополам на два плеча двойного импеданса");
        s.standard = ref;
        s.help = QStringLiteral(
                     "Простейший делитель: линия Z0 расходится на два плеча по 2·Z0, и в "
                     "параллель они снова дают Z0. Мощность делится поровну, но развязки между "
                     "плечами нет — это не Вилкинсон. Хорошая задача, чтобы увидеть, сколько "
                     "портит само соединение.")
                 + helpPlanar();
        s.params = boardParams();
        s.params << z0Param(QStringLiteral("Сопротивление входа"), 50.0) << freqParam(3.0);
        s.build = buildTee;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ms_load");
        s.section = sec;
        s.title = QStringLiteral("Поглотитель в канале: сколько отражает");
        s.summary = QStringLiteral("Измерение ABSORBER вместо веры в него");
        s.standard = ref;
        s.help = QStringLiteral(
                     "Планарная линия, закрытая поглотителем у края области. ABSORBER ставит "
                     "на узлах контура условие безотражательного прохода — так в двумерной "
                     "модели делают согласованную нагрузку и рамку вокруг излучающей "
                     "структуры.\n\n"
                     "Задание его ИЗМЕРЯЕТ. Прогон на умолчаниях дал |S11| от -2 до "
                     "-7 дБ: в канале с магнитными стенками поглотитель отражает заметно "
                     "больше, чем в металлическом волноводе, где тот же контур даёт "
                     "-12...-22 дБ. Это надо знать всем, кто ставит рамку поглотителя вокруг "
                     "планарной структуры.\n\n"
                     "⚠ ABSORBER — КОНТУР, а не заливка: поглощающего объёма не существует. "
                     "Число слоёв проверено прогоном и на отражение почти не влияет: стопка "
                     "контуров толстым поглотителем не становится.")
                 + helpPlanar();
        s.params = boardParams();
        s.params << z0Param(QStringLiteral("Сопротивление линии"), 50.0) << freqParam(3.0)
                 << Param::integer(QStringLiteral("cells"), QStringLiteral("Слоёв поглотителя"),
                                   1, 1, 20,
                                   QStringLiteral("Проверено прогоном: на отражение почти не "
                                                  "влияет. ABSORBER — одно граничное условие на "
                                                  "контуре, а не слой поглощающей среды"));
        s.build = buildMsLoad;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ms_openend");
        s.section = sec;
        s.title = QStringLiteral("Разомкнутый конец линии");
        s.summary = QStringLiteral("Магнитная стенка как холостой ход; эталон для входа");
        s.standard = ref;
        s.help = QStringLiteral(
                     "Линия, оборванная магнитной стенкой. В планарной модели это и есть "
                     "холостой ход: отражается всё, фаза считается длиной линии. Задание "
                     "полезно дважды — как эталон (|S11| обязан быть 0 дБ, и отклонение "
                     "показывает остаток самого входа) и как напоминание, что настоящий "
                     "открытый конец излучает и «удлиняет» линию.")
                 + helpPlanar();
        s.params = boardParams();
        s.params << z0Param(QStringLiteral("Сопротивление линии"), 50.0) << freqParam(3.0)
                 << Param::number(QStringLiteral("len"), QStringLiteral("Длина"),
                                  QStringLiteral("длин волны"), 1.0, 0.25, 5.0, 0.25, 2);
        s.build = buildOpenEnd;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ms_tdr");
        s.section = sec;
        s.title = QStringLiteral("Импульсная рефлектометрия ступеньки");
        s.summary = QStringLiteral("Моноцикл из файла: отражение видно во времени");
        s.standard = ref;
        s.help = QStringLiteral(
                     "Две линии разного сопротивления встык. На вход подаётся короткий "
                     "импульс-моноцикл, и в файле сигналов видно сначала падающий импульс, "
                     "потом отражённый от ступеньки — через время двойного пробега. Так "
                     "устроена импульсная рефлектометрия: по задержке находят, ГДЕ "
                     "неоднородность, по амплитуде — какая она.\n\n"
                     "Форму такой волны выражением задать нельзя: ядро умеет только несущую "
                     "под огибающей, а у моноцикла несущей нет. Поэтому падающую волну пишет "
                     "генератор — в файл .sig рядом с заданием.")
                 + helpPlanar();
        s.params = boardParams();
        s.params << freqParam(3.0)
                 << Param::number(QStringLiteral("z1"), QStringLiteral("Z1"), QStringLiteral("Ом"),
                                  50.0, 20.0, 120.0, 5.0, 1)
                 << Param::number(QStringLiteral("z2"), QStringLiteral("Z2"), QStringLiteral("Ом"),
                                  100.0, 20.0, 120.0, 5.0, 1)
                 << Param::number(QStringLiteral("len1"), QStringLiteral("Длина участка 1"),
                                  QStringLiteral("длин волны"), 3.0, 1.0, 10.0, 0.5, 1)
                 << Param::number(QStringLiteral("len2"), QStringLiteral("Длина участка 2"),
                                  QStringLiteral("длин волны"), 3.0, 1.0, 10.0, 0.5, 1);
        s.build = buildTdr;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ms_delay");
        s.section = sec;
        s.title = QStringLiteral("Линия задержки «змейкой»");
        s.summary = QStringLiteral("Видеоимпульс приходит позже: задержка во времени");
        s.standard = ref;
        s.help = QStringLiteral(
                     "Линия, свёрнутая змейкой, чтобы уложить большую длину в маленький "
                     "габарит. На вход подаётся видеоимпульс, и на втором порту он приходит "
                     "позже ровно на время пробега. Задание показывает и саму задержку, и то, "
                     "как колена портят форму импульса.\n\n"
                     "Видеоимпульс (гауссова шапка без несущей) выражением тоже не задать — "
                     "в формуле ядра всегда есть множитель sin(2·pi·FREQ·t). Файл пишет "
                     "генератор.")
                 + helpPlanar();
        s.params = boardParams();
        s.params << z0Param(QStringLiteral("Сопротивление линии"), 50.0) << freqParam(3.0)
                 // Нечётное число прогонов выводит сигнал на ПРОТИВОПОЛОЖНУЮ сторону —
                 // так картинка читается сама собой: вошло слева, вышло справа.
                 << Param::integer(QStringLiteral("runs"), QStringLiteral("Прогонов"), 3, 2, 8)
                 << Param::number(QStringLiteral("run"), QStringLiteral("Длина прогона"),
                                  QStringLiteral("длин волны"), 2.0, 0.5, 8.0, 0.5, 1);
        s.build = buildDelay;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ms_taper");
        s.section = sec;
        s.title = QStringLiteral("Плавный переход (тейпер)");
        s.summary = QStringLiteral("Экспоненциальный переход шире по полосе, чем λ/4");
        s.standard = QStringLiteral("Поузар, «Microwave Engineering», гл. 5; ") + ref;
        s.help = QStringLiteral(
                     "Согласование двух сопротивлений плавным переходом: Z(x) растёт "
                     "экспоненциально по длине. У такого перехода отражение падает как "
                     "sin(βL)/(βL) — монотонно с длиной и без резонансов, в отличие от "
                     "четвертьволнового трансформатора. Сравните с ним на тех же Z1 и Z2.")
                 + helpPlanar();
        s.params = boardParams();
        s.params << freqParam(3.0)
                 << Param::number(QStringLiteral("z1"), QStringLiteral("Z1"), QStringLiteral("Ом"),
                                  50.0, 20.0, 120.0, 5.0, 1)
                 << Param::number(QStringLiteral("z2"), QStringLiteral("Z2"), QStringLiteral("Ом"),
                                  100.0, 20.0, 120.0, 5.0, 1)
                 << Param::number(QStringLiteral("len"), QStringLiteral("Длина перехода"),
                                  QStringLiteral("длин волны"), 1.0, 0.25, 4.0, 0.25, 2)
                 << Param::integer(QStringLiteral("steps"), QStringLiteral("Ступеней"), 12, 4, 40);
        s.build = buildTaper;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ms_patch");
        s.section = sec;
        s.title = QStringLiteral("Патч-антенна: полость и излучение");
        s.summary = QStringLiteral("Магнитные стенки или открытые щели; рамка поглотителя");
        s.standard = QStringLiteral("Баланис, модель резонатора патча; ") + ref;
        s.help = QStringLiteral(
                     "Прямоугольная патч-антенна в двумерной модели, питаемая торцом. "
                     "Переключатель краёв даёт ДВЕ разные задачи, и обе честные.\n\n"
                     "«Магнитные стенки» — замкнутая полость: вдоль патча стоит полуволновый "
                     "резонанс, и его частоту даёт формула. Так её и считают (модель "
                     "резонатора патча), а излучение добавляют потом поправкой.\n\n"
                     "«Открытые края» — щели излучают в воздух, и рамка поглотителя вокруг "
                     "области играет свободное пространство. ⚠ Резонатора вдоль патча при "
                     "этом НЕТ: провалы |S11| ставит линия питания со щелью. Проверено "
                     "прогоном — провалы вышли на 2.475 и 3.825 ГГц при расчётном резонансе "
                     "2.98 ГГц.\n\n"
                     "В замкнутой полости резонанс есть: ядро на умолчаниях дало узкий провал "
                     "-12.8 дБ на 3.30 ГГц против расчётных 2.98 — на 11 % ВЫШЕ формулы. "
                     "Разницу вносит щель под питание в торцевой стенке: полость замкнута не "
                     "до конца.\n\n"
                     "⚠ Длина патча взята по формуле БЕЗ поправки на удлинение краёв. У "
                     "настоящего патча эта поправка опускает резонанс, а двумерная модель её "
                     "не содержит: сравнивать её числа с реальной антенной напрямую нельзя.")
                 + helpPlanar();
        s.params = boardParams();
        s.params << Param::number(QStringLiteral("fr"), QStringLiteral("Резонансная частота"),
                                  QStringLiteral("ГГц"), 3.0, 0.5, 15.0, 0.1, 2)
                 << Param::choice(QStringLiteral("edges"), QStringLiteral("Края патча"),
                                  {QStringLiteral("Магнитные стенки (резонатор)"),
                                   QStringLiteral("Открытые (излучение)")},
                                  0)
                 << z0Param(QStringLiteral("Сопротивление питающей линии"), 50.0)
                 << Param::number(QStringLiteral("air"), QStringLiteral("Воздух вокруг"),
                                  QStringLiteral("длин волны"), 0.3, 0.1, 1.0, 0.05, 2)
                 << Param::integer(QStringLiteral("frame"),
                                   QStringLiteral("Слоёв поглотителя в рамке"), 5, 1, 20);
        s.build = buildPatch;
        *out << s;
    }
    {
        Sample s;
        s.id = QStringLiteral("ms_bsf");
        s.section = sec;
        s.title = QStringLiteral("ПЗФ на разомкнутых шлейфах");
        s.summary = QStringLiteral("Полоса заграждения; считается свипом или одним проходом");
        s.standard = ref;
        s.help = QStringLiteral(
                     "Несколько разомкнутых шлейфов λ/4, расставленных через λ/4 по линии. "
                     "Каждый в одиночку даёт узкий провал (см. «Режекторный шлейф»), а вместе "
                     "они образуют полосу заграждения: чем больше шлейфов, тем шире и глубже.\n\n"
                     "Этим же заданием удобно сравнить два способа расчёта. Свип считает 41 "
                     "шаг, по одному на частоту. Гребёнка тонов кладёт все 41 частоту в один "
                     "файл падающей волны и считается ОДИН раз — ровно столько же, сколько "
                     "один шаг свипа.")
                 + helpPlanar();
        s.params = boardParams();
        s.params << z0Param(QStringLiteral("Сопротивление линии"), 50.0) << freqParam(3.0)
                 << Param::integer(QStringLiteral("stubs"), QStringLiteral("Шлейфов"), 3, 1, 7)
                 << Param::number(QStringLiteral("zs"), QStringLiteral("Сопротивление шлейфов"),
                                  QStringLiteral("Ом"), 70.0, 20.0, 120.0, 5.0, 1)
                 << Param::choice(QStringLiteral("mode"), QStringLiteral("Способ расчёта"),
                                  {QStringLiteral("Свип по частотам (41 шаг)"),
                                   QStringLiteral("Один проход: гребёнка тонов")},
                                  0)
                 << corrParam(17.0);
        s.build = buildBsf;
        *out << s;
    }
}

} // namespace tpl::samples
