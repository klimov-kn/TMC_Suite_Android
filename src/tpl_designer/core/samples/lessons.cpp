// Раздел «Учебные задания»: авторские задания лабораторных работ и демо-примеры пакета.
//
// Эти задания писали люди, а не рецепты, — они и ценны как образец того, как задание
// выглядит «от руки». Лежат ресурсом байт в байт и открываются ТЕМ ЖЕ читателем, что
// и «Открыть готовое»: так пользователь видит их в редакторе, а при сохранении
// получает то же задание, записанное генератором.

#include "cp1251.h"
#include "sample.h"
#include "tplreader.h"

#include <QFile>

// Ресурсы статической библиотеки сами не регистрируются — нужен явный вызов, и
// сделать его можно только вне пространства имён.
static void initLessonResources()
{
    Q_INIT_RESOURCE(lessons);
}

namespace tpl::samples {

namespace {

struct Lesson {
    const char *id;
    const char *file;
    Kernel kernel;
    const char *title;
    const char *summary;
    const char *help;
};

Build readLesson(const Lesson &l)
{
    Build b;
    QFile f(QStringLiteral(":/lessons/") + QLatin1String(l.file));
    if (!f.open(QIODevice::ReadOnly)) {
        b.notes << QStringLiteral("Не удалось прочитать встроенное задание %1").arg(QLatin1String(l.file));
        return b;
    }
    const QString text = decodeTplBytes(f.readAll());
    const ReadResult r = readTplText(text);
    b.project = r.project;
    b.project.name = QString::fromLatin1(l.file).section(QLatin1Char('.'), 0, 0);
    // Мода определяется ядром, которым считают, а не текстом файла: в рупоре ЛР2
    // нет плазмы, но считается он X-модой.
    b.project.kernel = l.kernel;

    // Шапка авторского задания — готовое описание: берём комментарии до первого #.
    QStringList head;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString t = line.trimmed();
        if (t.startsWith(QLatin1Char('#')) && !t.startsWith(QLatin1String("#TMC_RT_H")))
            break;
        if (!t.startsWith(QLatin1Char('!')))
            continue;
        QString c = t.mid(1).trimmed();
        if (c.startsWith(QLatin1String("====")) || c.startsWith(QLatin1String("----")) || c.isEmpty())
            continue;
        head << c;
        if (head.size() >= 12)
            break;
    }
    b.project.description = head.join(QLatin1Char('\n'));
    b.theory << Row{QStringLiteral("Шагов расчёта"), QString::number(b.project.stepCount())};
    b.theory << Row{QStringLiteral("Фигур / входов"), QStringLiteral("%1 / %2")
                                                           .arg(b.project.shapes.size())
                                                           .arg(b.project.ports.size())};
    b.notes << QStringLiteral("Авторское задание прочитано так же, как «Открыть готовое». "
                              "Сохранив его, вы получите то же задание, записанное генератором: "
                              "макросы-константы автора раскроются в числа.");
    for (const QString &w : r.warnings)
        b.notes << QStringLiteral("При чтении: ") + w;
    return b;
}

} // namespace

void addLessons(QVector<Sample> *out)
{
    initLessonResources();

    static const Lesson list[] = {
        {"les_lr1_wg", "LR1_wg.tpl", Kernel::H, "ЛР1: волновод с диэлектрической вставкой",
         "Свип по частоте, WR-229, вставка eps = 2",
         "Лабораторная работа 1. Прямоугольный волновод R40 / WR-229 с диэлектрической "
         "вставкой, свип 3.3-4.9 ГГц. Порт 1 активный, порт 2 пассивный; поле и топология "
         "выгружаются только на первом шаге."},
        {"les_lr1_horn", "LR1_rupor.tpl", Kernel::H, "ЛР1: рупор",
         "Рупор в открытом пространстве, H-мода",
         "Лабораторная работа 1. Питающий волновод плавно раскрывается в апертуру; за "
         "раскрывом — открытая область, ограниченная поглотителем. Диаграмма направленности "
         "пишется для TMC_DN."},
        {"les_lr2_plasma", "LR2_plasma.tpl", Kernel::X, "ЛР2: замагниченная плазма",
         "Квадрат плазмы с подмагничиванием в волноводе",
         "Лабораторная работа 2. Небольшой квадрат плазмы в центре волновода вместе с "
         "магнитным полем в той же области — гиротропная среда. Считается ядром X-моды."},
        {"les_lr2_horn", "LR2_rupor.tpl", Kernel::X, "ЛР2: рупор, X-мода",
         "Тот же рупор, посчитанный X-модой",
         "Лабораторная работа 2. Тот же рупор, что в ЛР1, но для ядра X-моды: поле E лежит "
         "в плоскости рисунка. Сравните ДН двух мод."},
        {"les_demo_h", "demo_h.tpl", Kernel::H, "Демо: плоский волновод (H)",
         "Короткий учебный пример из поставки ядра",
         "Самодостаточный учебный пример из поставки TMC Suite: канал из двух металлических "
         "полос, вход слева, поглотитель по краям. Маленькая сетка — считается быстро."},
        {"les_demo_x", "demo_x.tpl", Kernel::X, "Демо: плоский волновод (X)",
         "Тот же пример для ядра X-моды",
         "Учебный пример из поставки TMC Suite для ядра X-моды. Формат файла и заголовок "
         "#TMC_RT_H у обеих мод одинаковые — отдельного #TMC_RT_X не существует."},
        {"les_horn_demo", "horn_demo.tpl", Kernel::H, "Планарный рупор с рамкой поглотителя",
         "Демо-задание пакета: излучающий рупор в открытой области",
         "Задание из демонстрационного набора TMC Suite: питающий канал плавно раскрывается "
         "в апертуру и излучает в пространство, ограниченное рамкой поглотителя. Отрезки стенок "
         "записаны с повтором вершины — программа читает это без замечаний."},
    };

    for (const Lesson &l : list) {
        Sample s;
        s.id = QLatin1String(l.id);
        s.section = QStringLiteral("lesson");
        s.title = QString::fromUtf8(l.title);
        s.summary = QString::fromUtf8(l.summary);
        s.standard = QStringLiteral("TMC Suite, samples/") + QLatin1String(l.file);
        s.help = QString::fromUtf8(l.help);
        s.kernel = l.kernel;
        const Lesson copy = l;
        s.build = [copy](const Values &) { return readLesson(copy); };
        *out << s;
    }
}

} // namespace tpl::samples
