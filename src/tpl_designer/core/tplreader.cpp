#include "tplreader.h"

#include "sigfile.h"

#include "cp1251.h"
#include "emitter.h"
#include "expr.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QPointF>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace tpl {
namespace {

// ===========================================================================
//  Препроцессор
//
//  Повторяет `src/libs/PREPR` из TMC Suite. Правила брались из кода, а не из
//  справочника: ядро правее справочника, и половины этих тонкостей в нём нет.
// ===========================================================================

/// Символ имени макроса. ⚠️ Именно ASCII-диапазоны, как в `def_gets`:
/// QChar::isLetterOrNumber() принял бы кириллицу и разъехался бы с ядром.
bool isIdentChar(QChar c)
{
    const ushort u = c.unicode();
    return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == '_';
}

/// Буфер ядра — цепочка строк, оканчивающихся '\0'. После склейки по `\\`
/// внутри одной записи появляются настоящие переводы строк.
using Records = QStringList;

/// `DeleteComment()` из `PREPR/Prep_mn.cpp`: от первого '!' и до конца строки
/// текст заменяется на " \n".
///
/// ⚠️ Порядок важен: комментарии режутся ДО склейки по `\\`, поэтому '!' перед
/// `\\` убивает перенос строки. Ядро ведёт себя именно так.
QString deleteComment(QString line)
{
    const int i = line.indexOf(QLatin1Char('!'));
    if (i >= 0)
        return line.left(i) + QLatin1String(" \n");
    if (!line.endsWith(QLatin1Char('\n')))
        line += QLatin1Char('\n');
    return line;
}

Records loadRecords(const QString &text)
{
    Records recs;
    int start = 0;
    while (start < text.size()) {
        const int nl = text.indexOf(QLatin1Char('\n'), start);
        if (nl < 0) {
            recs << deleteComment(text.mid(start));
            break;
        }
        recs << deleteComment(text.mid(start, nl - start + 1));
        start = nl + 1;
    }
    return recs;
}

/// `def_sl()`: найденные `\\` и весь текст после них до конца строки выбрасываются,
/// а строка склеивается со следующей через " \n".
///
/// ⚠️ Ровно ДВА бэкслеша. С одним ядро макрос не соберёт — тело вывалится текстом.
void joinBackslash(Records *recs)
{
    for (int i = 0; i < recs->size(); ++i) {
        for (;;) {
            const int pos = recs->at(i).indexOf(QLatin1String("\\\\"));
            if (pos < 0)
                break;
            QString head = recs->at(i).left(pos) + QLatin1String(" \n");
            if (i + 1 < recs->size()) {
                head += recs->at(i + 1);
                recs->removeAt(i + 1);
            }
            recs->replace(i, head);
        }
    }
}

/// Позиция директивы `#define` / `#include` в записи. Ядро сравнивает 8 (9)
/// символов вместе с разделителем, поэтому `#defineX` директивой не считается.
int findDirective(const QString &rec, QLatin1String name, bool last = false)
{
    int found = -1;
    int from = 0;
    for (;;) {
        const int p = rec.indexOf(name, from);
        if (p < 0)
            return found;
        const int after = p + name.size();
        if (after < rec.size()
            && (rec.at(after) == QLatin1Char(' ') || rec.at(after) == QLatin1Char('\t'))) {
            if (!last)
                return p;
            found = p;
        }
        from = p + 1;
    }
}

QString firstLineOf(const QString &s)
{
    const int nl = s.indexOf(QLatin1Char('\n'));
    return (nl < 0 ? s : s.left(nl)).trimmed();
}

struct Macro {
    QString name;
    QStringList params;
    QString body;
};

/// Разбирает `#define ИМЯ [пар1; пар2;] @ тело` (см. `def_separ_mac_name`).
bool parseMacro(const QString &text, Macro *m, QString *why)
{
    const int at = text.indexOf(QLatin1Char('@'));
    if (at < 0) {
        *why = QStringLiteral("нет разделителя '@'");
        return false;
    }
    if (at < 8) {
        *why = QStringLiteral("пустое имя");
        return false;
    }

    const QString head = text.mid(8, at - 8);
    // Пробел после '@' — оформление, а не часть тела. Если его не срезать, каждая
    // подстановка добавляет лишний пробел, и текст расходится с тем, что пишет ядро.
    m->body = text.mid(at + 1).trimmed();

    const QStringList parts = head.split(QLatin1Char(';'));
    static const QRegularExpression space(QStringLiteral("\\s+"));
    const QStringList firstWords = parts.value(0).split(space, Qt::SkipEmptyParts);
    if (firstWords.isEmpty()) {
        *why = QStringLiteral("нет имени макроса");
        return false;
    }
    m->name = firstWords.at(0);
    for (int i = 1; i < firstWords.size(); ++i)
        m->params << firstWords.at(i);
    for (int i = 1; i < parts.size(); ++i) {
        const QString p = parts.at(i).trimmed();
        if (!p.isEmpty())
            m->params << p;
    }

    // ⚠️ Длину имени и число параметров НЕ проверяем. Справочник обещает «32», но
    // код препроцессора не проверяет длину вообще, а параметров держит до 59
    // (BUF_PAR_MAX = 60 в PREPR/Prepr1.h). Отвергать по документу чужой файл,
    // который ядро прекрасно считает, — ровно та ошибка, ради которой мы тут.
    for (QChar c : m->name) {
        if (!isIdentChar(c)) {
            *why = QStringLiteral("в имени недопустимый символ");
            return false;
        }
    }
    return true;
}

/// `def_buf_gets_param`: первая и последняя скобки фактического параметра
/// заменяются пробелами (tpl-guide 9.2 — та самая ловушка со `Stm (Stp 10; 2;);`).
QString stripOuterParens(const QString &arg)
{
    if (arg.size() >= 2 && arg.startsWith(QLatin1Char('(')) && arg.endsWith(QLatin1Char(')')))
        return QLatin1Char(' ') + arg.mid(1, arg.size() - 2) + QLatin1Char(' ');
    return arg;
}

/// Подставляет фактические параметры в тело макроса — по границам идентификаторов.
QString expandBody(const Macro &m, const QStringList &args)
{
    QString out;
    out.reserve(m.body.size() + 64);
    int i = 0;
    while (i < m.body.size()) {
        if (!isIdentChar(m.body.at(i))) {
            out += m.body.at(i);
            ++i;
            continue;
        }
        int j = i;
        while (j < m.body.size() && isIdentChar(m.body.at(j)))
            ++j;
        const QString tok = m.body.mid(i, j - i);
        const int idx = m.params.indexOf(tok);
        out += (idx >= 0) ? args.value(idx) : tok;
        i = j;
    }
    return out;
}

/// Один проход подстановки макроса по тексту.
///
/// ⚠️ Совпадение — по ЦЕЛОМУ идентификатору, а не по подстроке: иначе макрос `a`
/// изуродовал бы слово `radian`. Ядро (`def_buf_gets_mac`) читает максимальный
/// кусок из [A-Za-z0-9_] и сравнивает целиком — делаем так же.
QString substitute(const QString &text, const Macro &m, int *calls, QStringList *warnings)
{
    QString out;
    out.reserve(text.size());
    int i = 0;
    while (i < text.size()) {
        if (!isIdentChar(text.at(i))) {
            out += text.at(i);
            ++i;
            continue;
        }
        int j = i;
        while (j < text.size() && isIdentChar(text.at(j)))
            ++j;
        const QString tok = text.mid(i, j - i);
        if (tok != m.name) {
            out += tok;
            i = j;
            continue;
        }

        if (m.params.isEmpty()) {
            out += m.body;
            i = j;
            continue; // константы вызовами не считаем — макро-версию делают параметры
        }

        // Фактические параметры: каждый до ';' на нулевой глубине скобок.
        int k = j;
        if (k < text.size())
            ++k; // ядро съедает разделитель, стоящий сразу за именем
        QStringList args;
        bool bad = false;
        for (int a = 0; a < m.params.size() && !bad; ++a) {
            int depth = 0;
            QString arg;
            bool closed = false;
            while (k < text.size()) {
                const QChar c = text.at(k);
                if (c == QLatin1Char('\n')) { // параметры вызова не переносятся
                    bad = true;
                    break;
                }
                if (c == QLatin1Char('(')) {
                    ++depth;
                } else if (c == QLatin1Char(')')) {
                    --depth;
                } else if (c == QLatin1Char(';') && depth == 0) {
                    ++k;
                    closed = true;
                    break;
                }
                arg += c;
                ++k;
            }
            if (!closed)
                bad = true;
            else
                args << stripOuterParens(arg.trimmed()); // пробелы вокруг аргумента — оформление
        }

        if (bad) {
            *warnings << QStringLiteral("вызов макроса «%1» не разобран: не хватает "
                                        "фактических параметров (ожидалось %2)")
                             .arg(m.name).arg(m.params.size());
            out += tok;
            i = j;
            continue;
        }

        out += expandBody(m, args);
        ++*calls;
        i = k;
    }
    return out;
}

/// Ищет файл в папке задания, НЕ различая регистр имени.
///
/// ⚠️ Нужно именно на macOS и Linux. Задания пишут на Windows, где регистр в именах
/// файлов не важен, и `#include lr2_step.h` там открывает `LR2_step.h` без разговоров.
/// На чужой системе такое задание молча теряло бы все макросы: файл «не найден»,
/// шаги не раскрылись. Ядро TMC на этих системах ищет так же (см. версию для Linux,
/// «поиск имени файла без учёта регистра»), поэтому и мы обязаны.
///
/// Точное совпадение проверяется первым: если файл есть, каталог не перебираем.
/// Возвращает пустую строку, если ничего похожего нет.
QString findFileIgnoringCase(const QString &dir, const QString &name)
{
    const QString exact = QDir(dir).filePath(name);
    if (QFileInfo::exists(exact))
        return exact;

    const QDir d(dir);
    const QStringList entries = d.entryList(QDir::Files);
    for (const QString &e : entries) {
        if (e.compare(name, Qt::CaseInsensitive) == 0)
            return d.filePath(e);
    }
    return {};
}

struct IncludeCtx {
    QString baseDir;
    QStringList *included = nullptr;
    QStringList *warnings = nullptr;
    QStringList *errors = nullptr;
    int budget = 64;
};

void processIncludes(Records *recs, IncludeCtx *ctx)
{
    int i = 0;
    while (i < recs->size()) {
        const int pos = findDirective(recs->at(i), QLatin1String("#include"));
        if (pos < 0) {
            ++i;
            continue;
        }
        if (--ctx->budget < 0) {
            *ctx->errors << QStringLiteral("слишком много #include — похоже на кольцевую вставку");
            return;
        }

        const QString rec = recs->at(i);
        const QString prefix = rec.left(pos);
        const QString tail = rec.mid(pos + 9); // после "#include "
        const int nl = tail.indexOf(QLatin1Char('\n'));
        const QString name = (nl < 0 ? tail : tail.left(nl)).trimmed();

        recs->removeAt(i);

        if (name.isEmpty()) {
            *ctx->warnings << QStringLiteral("строка #include без имени файла — пропущена");
            continue;
        }

        // ⚠️ Ядро ищет включаемый файл РОВНО в папке задания и больше нигде:
        // имя собирается как GetCurrentPath() + "\" + имя (PREPR/DEFMIS.CPP:125),
        // а ветка с путями поиска целиком закомментирована.
        //
        // Регистр не сверяем: на Windows им занимается файловая система, а на macOS
        // и Linux задание, написанное на Windows, иначе молча теряло бы все макросы.
        QString content;
        QString err;
        bool loaded = false;
        if (!ctx->baseDir.isEmpty()) {
            const QString full = findFileIgnoringCase(ctx->baseDir, name);
            if (!full.isEmpty())
                loaded = readTplFileText(full, &content, &err);
        }
        if (!loaded) {
            *ctx->warnings << QStringLiteral("включаемый файл «%1» не найден рядом с заданием — "
                                             "макросы из него не раскрыты, задание прочитано "
                                             "неполно").arg(name);
            continue;
        }

        *ctx->included << name;

        Records sub = loadRecords(content);
        if (!prefix.trimmed().isEmpty())
            sub.prepend(prefix + QLatin1Char('\n'));
        for (int k = sub.size() - 1; k >= 0; --k)
            recs->insert(i, sub.at(k));

        joinBackslash(recs); // ядро повторяет def_sl после каждой вставки
        // i не двигаем: вложенные #include внутри вставленного текста тоже наши
    }
}

void expandMacros(Records *recs, PreprocessResult *res)
{
    for (int guard = 0; guard < 1024; ++guard) {
        int idx = -1;
        int pos = -1;
        for (int i = recs->size() - 1; i >= 0 && idx < 0; --i) {
            const int p = findDirective(recs->at(i), QLatin1String("#define"), /*last=*/true);
            if (p >= 0) {
                idx = i;
                pos = p;
            }
        }
        if (idx < 0)
            return;

        QString text = recs->at(idx).mid(pos);
        if (text.endsWith(QLatin1Char('\n')))
            text.chop(1); // ядро затирает последний символ логической строки

        // Определение удаляется из буфера ДО подстановки — как в def_main_macdef.
        //
        // ⚠️ Перевод строки при этом СОХРАНЯЕМ, иначе строка исчезает целиком и весь
        // текст ниже съезжает вверх. Съезжали и номера строк в предупреждениях: у
        // авторского `horn.tpl` с одиннадцатью `#define` замечание к блоку на строке 56
        // приходило как «строка 45», а там лежит `#END_PARM`. Человек идёт смотреть и
        // не находит ничего. Пустая строка на месте определения ничего не стоит:
        // разбору она безразлична, а номера остаются настоящими.
        const QString wasRec = recs->at(idx);
        QString head = wasRec.left(pos);
        if (wasRec.endsWith(QLatin1Char('\n')) && !head.endsWith(QLatin1Char('\n')))
            head += QLatin1Char('\n');
        recs->replace(idx, head);

        Macro m;
        QString why;
        if (!parseMacro(text, &m, &why)) {
            res->warnings << QStringLiteral("макроопределение не разобрано (%1): %2")
                                 .arg(why, firstLineOf(text));
            continue;
        }

        // ⚠️ Подстановка идёт только ВНИЗ от места определения — как в ядре.
        for (int i = idx; i < recs->size(); ++i)
            recs->replace(i, substitute(recs->at(i), m, &res->macroCalls, &res->warnings));
    }
    res->errors << QStringLiteral("слишком много макроопределений — разбор остановлен");
}

// ===========================================================================
//  Разбор плоского текста
// ===========================================================================

/// Строка начинается с ключевого слова, за которым стоит разделитель.
/// ⚠️ Разделитель обязателен: иначе `TIME_UNIT` совпал бы с `TIME`, а `FREQ_UNIT`
/// с `FREQ`. В ядре ровно поэтому у всех констант хвостовой пробел.
bool matchKey(const QString &line, QLatin1String key, QString *rest)
{
    if (!line.startsWith(key))
        return false;
    const int n = key.size();
    if (n < line.size()) {
        const QChar c = line.at(n);
        if (isIdentChar(c))
            return false;
    }
    if (rest)
        *rest = line.mid(n);
    return true;
}

/// Делит хвост примитива на параметры по ';'. Пустые хвосты отбрасываются,
/// пробелы и переводы строк внутри параметра снимаются (`expr_del_Blanks2`).
QStringList splitParams(const QString &text)
{
    QStringList out = text.split(QLatin1Char(';'));
    for (QString &s : out)
        s = s.trimmed();
    while (!out.isEmpty() && out.constLast().isEmpty())
        out.removeLast();
    return out;
}

enum class Prim { Rect, Circle, Polygon, InputX, InputY, FileEps, Unknown };

struct PrimSpec {
    QLatin1String word;
    Prim prim;
    const char *suffix;
    bool moving;
};

// Порядок — от длинного к короткому: «RECT_STAT» не должен съесть «RECT_STAT_N».
const PrimSpec kPrims[] = {
    {QLatin1String("RECT_STAT_N"),    Prim::Rect,    "_N", false},
    {QLatin1String("RECT_STAT_B"),    Prim::Rect,    "_B", false},
    {QLatin1String("RECT_STAT_Y"),    Prim::Rect,    "_Y", false},
    {QLatin1String("RECT_STAT"),      Prim::Rect,    "",   false},
    {QLatin1String("RECT_MOVE"),      Prim::Rect,    "",   true},
    {QLatin1String("CIRCLE_STAT_N"),  Prim::Circle,  "_N", false},
    {QLatin1String("CIRCLE_STAT_B"),  Prim::Circle,  "_B", false},
    {QLatin1String("CIRCLE_STAT_Y"),  Prim::Circle,  "_Y", false},
    {QLatin1String("CIRCLE_STAT"),    Prim::Circle,  "",   false},
    {QLatin1String("CIRCLE_MOVE"),    Prim::Circle,  "",   true},
    {QLatin1String("POLYGON_STAT_N"), Prim::Polygon, "_N", false},
    {QLatin1String("POLYGON_STAT_B"), Prim::Polygon, "_B", false},
    {QLatin1String("POLYGON_STAT_Y"), Prim::Polygon, "_Y", false},
    {QLatin1String("POLYGON_STAT"),   Prim::Polygon, "",   false},
    {QLatin1String("POLYGON_MOVE"),   Prim::Polygon, "",   true},
    {QLatin1String("INPUT_X"),        Prim::InputX,  "",   false},
    {QLatin1String("INPUT_Y"),        Prim::InputY,  "",   false},
    {QLatin1String("FILE_N"),         Prim::FileEps, "_N", false},
    {QLatin1String("FILE_B"),         Prim::FileEps, "_B", false},
    {QLatin1String("FILE_Y"),         Prim::FileEps, "_Y", false},
    {QLatin1String("FILE"),           Prim::FileEps, "",   false},
};

struct RawBlock {
    int id = 0;
    int line = 0;
    Prim prim = Prim::Unknown;
    QString word;      ///< как написано в файле — для сообщений
    QString suffix;    ///< "", "_N", "_B", "_Y"
    bool moving = false;
    QStringList params;                       ///< для RECT/CIRCLE/INPUT/FILE, первый — материал
    QVector<QPair<QString, QString>> points;  ///< для POLYGON
    QStringList extra;                        ///< хвост POLYGON_MOVE (скорости)
    QString raw;
};

struct RawParams {
    AngleUnit angle = AngleUnit::Radian;
    double longToMm = 1.0;
    double timeToNs = 1.0;
    double freqToGHz = 1.0;
    LengthUnit lengthUnit = LengthUnit::Millimeter;
    double delta = 1.0;
    double timeMin = 0.0;
    double timeMax = 0.0;
    double xMin = 0.0, xMax = 0.0, yMin = 0.0, yMax = 0.0;
    double freq = 0.0;
    bool haveTimeUnit = false;
    bool haveAny = false;
};

struct RawOutput {
    QString file;
    bool topology = false;
    bool fields = false;
    bool sMatrix = false;
    QString sFile;
    double sTMin = 0.0, sTMax = 0.0;
    bool pattern = false;
    bool patternInMemory = true;
    QString pFile;
    double pTMin = 0.0, pTMax = 0.0;
};

struct RawStep {
    int line = 0;
    RawParams parm;
    QVector<RawBlock> blocks;
    QHash<int, QPointF> links; ///< анкеры, уже в мм
    QSet<int> linkIds;
    RawOutput out;
};

/// Общий контекст разбора: складывать предупреждения удобнее в одно место.
struct Ctx {
    QStringList *warnings = nullptr;
    QStringList *errors = nullptr;
    /// Папка задания: рядом с `.tpl` лежат и профили `.eps`, и файлы падающей
    /// волны `.sig`. Без неё сигнал из файла прочитать нечем.
    QString baseDir;

    void warn(int line, const QString &text) const
    {
        *warnings << QStringLiteral("строка %1: %2").arg(line).arg(text);
    }
};

/// Вычисляет числовой параметр. Не вычислилось — предупреждаем и отдаём запасное
/// значение: файл, написанный руками, не обязан состоять из одних констант.
double evalOr(const Ctx &ctx, int line, const QString &what, const QString &text,
              AngleUnit angle, double fallback, bool *ok = nullptr)
{
    const auto v = evalExpression(text, angle);
    if (ok)
        *ok = v.has_value();
    if (v.has_value())
        return *v;
    ctx.warn(line, QStringLiteral("не удалось вычислить %1: «%2» — взято %3")
                       .arg(what, text.trimmed(), QString::number(fallback)));
    return fallback;
}

double lengthUnitFactor(const QString &name, LengthUnit *unit)
{
    if (name == QLatin1String("cm")) { *unit = LengthUnit::Centimeter; return 10.0; }
    if (name == QLatin1String("m"))  { *unit = LengthUnit::Meter;      return 1000.0; }
    *unit = LengthUnit::Millimeter;
    return 1.0; // mm
}

double timeUnitFactorNs(const QString &name)
{
    if (name == QLatin1String("s"))   return 1.0e9;
    if (name == QLatin1String("ms"))  return 1.0e6;
    if (name == QLatin1String("mks")) return 1.0e3;
    if (name == QLatin1String("ps"))  return 1.0e-3;
    return 1.0; // ns
}

double freqUnitFactorGHz(const QString &name)
{
    if (name == QLatin1String("Hz"))  return 1.0e-9;
    if (name == QLatin1String("kHz")) return 1.0e-6;
    if (name == QLatin1String("MHz")) return 1.0e-3;
    return 1.0; // GHz
}

/// Разбирает один `BLOCK n; ... END_B`.
RawBlock parseBlock(const Ctx &ctx, int id, int line, const QStringList &bodyLines)
{
    RawBlock b;
    b.id = id;
    b.line = line;
    b.raw = bodyLines.join(QLatin1Char('\n')).trimmed();

    QString text = bodyLines.join(QLatin1Char('\n'));
    // Ведущие пробелы и переводы строк до ключевого слова примитива.
    int start = 0;
    while (start < text.size() && text.at(start).isSpace())
        ++start;
    text = text.mid(start);

    for (const PrimSpec &spec : kPrims) {
        QString rest;
        if (matchKey(text, spec.word, &rest)) {
            b.prim = spec.prim;
            b.word = spec.word;
            b.suffix = QLatin1String(spec.suffix);
            b.moving = spec.moving;
            text = rest;
            break;
        }
    }

    if (b.prim == Prim::Unknown) {
        ctx.warn(line, QStringLiteral("BLOCK %1: неизвестный примитив — блок пропущен: %2")
                           .arg(id).arg(firstLineOf(b.raw)));
        return b;
    }

    if (b.prim != Prim::Polygon) {
        b.params = splitParams(text);
        return b;
    }

    // Полигон: материал на первой строке, дальше — по строке на вершину `L x; y;`.
    // Строчная структура здесь значима: ядро считает вершины именно по строкам
    // (`CalcEOL` в TmcRTH_BolckList.cpp), и меньше трёх не примет.
    const int semi = text.indexOf(QLatin1Char(';'));
    if (semi < 0) {
        ctx.warn(line, QStringLiteral("BLOCK %1: у полигона нет материала").arg(id));
        return b;
    }
    b.params << text.left(semi).trimmed();

    const QStringList rest = text.mid(semi + 1).split(QLatin1Char('\n'));
    for (const QString &raw : rest) {
        const QString t = raw.trimmed();
        if (t.isEmpty())
            continue;
        if (t.startsWith(QLatin1Char('L')) && (t.size() == 1 || !isIdentChar(t.at(1)))) {
            const QStringList xy = splitParams(t.mid(1));
            if (xy.size() >= 2)
                b.points << qMakePair(xy.at(0), xy.at(1));
            else
                ctx.warn(line, QStringLiteral("BLOCK %1: строка «%2» не похожа на вершину L x; y;")
                                   .arg(id).arg(t));
        } else {
            b.extra += splitParams(t);
        }
    }
    return b;
}

/// Разбирает `#PARM ... #END_PARM`. Отсутствующие поля наследуются от предыдущего
/// шага — так же, как это делает ядро (tpl-guide 4).
void parseParm(const Ctx &ctx, const QStringList &lines, int firstLine, RawParams *p)
{
    for (int i = 0; i < lines.size(); ++i) {
        const QString t = lines.at(i).trimmed();
        const int ln = firstLine + i;
        if (t.isEmpty())
            continue;
        QString rest;

        if (matchKey(t, QLatin1String("ANGLE_UNIT"), &rest)) {
            p->angle = splitParams(rest).value(0) == QLatin1String("gradus") ? AngleUnit::Gradus
                                                                            : AngleUnit::Radian;
        } else if (matchKey(t, QLatin1String("LONG_UNIT"), &rest)) {
            p->longToMm = lengthUnitFactor(splitParams(rest).value(0), &p->lengthUnit);
        } else if (matchKey(t, QLatin1String("TIME_UNIT"), &rest)) {
            p->timeToNs = timeUnitFactorNs(splitParams(rest).value(0));
            p->haveTimeUnit = true;
        } else if (matchKey(t, QLatin1String("FREQ_UNIT"), &rest)) {
            p->freqToGHz = freqUnitFactorGHz(splitParams(rest).value(0));
        } else if (matchKey(t, QLatin1String("DELTA"), &rest)) {
            p->delta = evalOr(ctx, ln, QStringLiteral("DELTA"), splitParams(rest).value(0),
                              p->angle, p->delta / p->longToMm) * p->longToMm;
        } else if (matchKey(t, QLatin1String("TIME"), &rest)) {
            const QStringList v = splitParams(rest);
            p->timeMin = evalOr(ctx, ln, QStringLiteral("начало TIME"), v.value(0), p->angle, 0.0)
                         * p->timeToNs;
            p->timeMax = evalOr(ctx, ln, QStringLiteral("конец TIME"), v.value(1), p->angle, 0.0)
                         * p->timeToNs;
        } else if (matchKey(t, QLatin1String("X_MIN"), &rest)) {
            p->xMin = evalOr(ctx, ln, QStringLiteral("X_MIN"), splitParams(rest).value(0),
                             p->angle, 0.0) * p->longToMm;
        } else if (matchKey(t, QLatin1String("X_MAX"), &rest)) {
            p->xMax = evalOr(ctx, ln, QStringLiteral("X_MAX"), splitParams(rest).value(0),
                             p->angle, 0.0) * p->longToMm;
        } else if (matchKey(t, QLatin1String("Y_MIN"), &rest)) {
            p->yMin = evalOr(ctx, ln, QStringLiteral("Y_MIN"), splitParams(rest).value(0),
                             p->angle, 0.0) * p->longToMm;
        } else if (matchKey(t, QLatin1String("Y_MAX"), &rest)) {
            p->yMax = evalOr(ctx, ln, QStringLiteral("Y_MAX"), splitParams(rest).value(0),
                             p->angle, 0.0) * p->longToMm;
        } else if (matchKey(t, QLatin1String("FREQ"), &rest)) {
            p->freq = evalOr(ctx, ln, QStringLiteral("FREQ"), splitParams(rest).value(0),
                             p->angle, 0.0) * p->freqToGHz;
        } else if (matchKey(t, QLatin1String("N_UNIT"), &rest)
                   || matchKey(t, QLatin1String("B_UNIT"), &rest)) {
            // Мёртвые параметры (Баг #16): разбираются, но на расчёт не влияют.
        } else if (matchKey(t, QLatin1String("HEIGHT"), &rest)
                   || matchKey(t, QLatin1String("DELTA_T_MECHANICAL"), &rest)) {
            ctx.warn(ln, QStringLiteral("параметр «%1» на расчёт не влияет и в модель "
                                        "не переносится").arg(firstLineOf(t)));
        } else {
            ctx.warn(ln, QStringLiteral("непонятная строка в #PARM: «%1»").arg(t));
        }
        p->haveAny = true;
    }
}

void parseOutput(const Ctx &ctx, const QStringList &lines, int firstLine, const RawParams &parm,
                 RawOutput *o)
{
    for (int i = 0; i < lines.size(); ++i) {
        const QString t = lines.at(i).trimmed();
        const int ln = firstLine + i;
        if (t.isEmpty())
            continue;
        QString rest;

        if (matchKey(t, QLatin1String("FILE"), &rest)) {
            o->file = splitParams(rest).value(0);
        } else if (matchKey(t, QLatin1String("FIELD_DISTRIBUTION_M"), &rest)
                   || matchKey(t, QLatin1String("FIELD_DISTRIBUTION"), &rest)) {
            o->patternInMemory = t.startsWith(QLatin1String("FIELD_DISTRIBUTION_M"));
            const QStringList v = splitParams(rest);
            o->pattern = true;
            o->pFile = v.value(0);
            o->pTMin = evalOr(ctx, ln, QStringLiteral("T_min диаграммы"), v.value(2), parm.angle,
                              0.0) * parm.timeToNs;
            o->pTMax = evalOr(ctx, ln, QStringLiteral("T_max диаграммы"), v.value(3), parm.angle,
                              0.0) * parm.timeToNs;
            if (!o->patternInMemory) {
                ctx.warn(ln, QStringLiteral("FIELD_DISTRIBUTION без _M дёргает диск на каждом "
                                            "такте; при записи будет использован вариант _M"));
            }
        } else if (matchKey(t, QLatin1String("FIELDS"), &rest)) {
            o->fields = true;
        } else if (matchKey(t, QLatin1String("TOPOLOGY"), &rest)) {
            o->topology = true;
        } else if (matchKey(t, QLatin1String("SMATRIX"), &rest)) {
            const QStringList v = splitParams(rest);
            o->sMatrix = true;
            o->sFile = v.value(0);
            o->sTMin = evalOr(ctx, ln, QStringLiteral("T_min S-матрицы"), v.value(2), parm.angle,
                              0.0) * parm.timeToNs;
            o->sTMax = evalOr(ctx, ln, QStringLiteral("T_max S-матрицы"), v.value(3), parm.angle,
                              0.0) * parm.timeToNs;
        } else {
            ctx.warn(ln, QStringLiteral("непонятная строка в #OUTPUT: «%1»").arg(t));
        }
    }
}

/// Режет плоский текст на шаги. Собственно физика — уже следующим этапом.
QVector<RawStep> parseSteps(const Ctx &ctx, const QString &flat, bool *sawHeader, bool *sawEof)
{
    const QStringList lines = flat.split(QLatin1Char('\n'));
    QVector<RawStep> steps;
    RawParams carried; ///< параметры «как в предыдущем шаге»

    *sawHeader = false;
    *sawEof = false;

    int i = 0;
    while (i < lines.size()) {
        const QString t = lines.at(i).trimmed();
        if (t.startsWith(QLatin1String("#TMC_RT_H"))) {
            *sawHeader = true;
            ++i;
            continue;
        }
        if (t.startsWith(QLatin1String("#EOF"))) {
            *sawEof = true;
            break;
        }
        if (!t.startsWith(QLatin1String("#STEP"))) {
            if (!t.isEmpty() && t.startsWith(QLatin1Char('#')))
                ctx.warn(i + 1, QStringLiteral("секция вне шага пропущена: «%1»").arg(t));
            ++i;
            continue;
        }

        RawStep step;
        step.line = i + 1;
        step.parm = carried;
        ++i;

        while (i < lines.size()) {
            const QString s = lines.at(i).trimmed();
            if (s.startsWith(QLatin1String("#END_STEP"))) {
                ++i;
                break;
            }
            if (s.startsWith(QLatin1String("#EOF"))) {
                *sawEof = true;
                break;
            }

            // --- #PARM ---
            if (s.startsWith(QLatin1String("#PARM"))) {
                const int first = i + 2;
                QStringList body;
                ++i;
                while (i < lines.size() && !lines.at(i).trimmed().startsWith(QLatin1String("#END_PARM"))
                       && !lines.at(i).trimmed().startsWith(QLatin1String("#END_STEP"))) {
                    body << lines.at(i);
                    ++i;
                }
                if (i < lines.size() && lines.at(i).trimmed().startsWith(QLatin1String("#END_PARM")))
                    ++i;
                parseParm(ctx, body, first, &step.parm);
                continue;
            }

            // --- #TOPOLOGY ---
            if (s.startsWith(QLatin1String("#TOPOLOGY"))) {
                ++i;
                while (i < lines.size()) {
                    const QString b = lines.at(i).trimmed();
                    if (b.startsWith(QLatin1String("#END_TOPOLOGY")) || b.startsWith(QLatin1Char('#')))
                        break;
                    QString rest;
                    if (matchKey(b, QLatin1String("BLOCK"), &rest)) {
                        const int blockLine = i + 1;
                        const int semi = rest.indexOf(QLatin1Char(';'));
                        const int id = (semi < 0 ? rest : rest.left(semi)).trimmed().toInt();
                        QStringList body;
                        if (semi >= 0)
                            body << rest.mid(semi + 1);
                        ++i;
                        while (i < lines.size()
                               && !lines.at(i).trimmed().startsWith(QLatin1String("END_B"))
                               && !lines.at(i).trimmed().startsWith(QLatin1Char('#'))) {
                            body << lines.at(i);
                            ++i;
                        }
                        if (i < lines.size() && lines.at(i).trimmed().startsWith(QLatin1String("END_B")))
                            ++i;
                        if (id < 1) {
                            ctx.warn(blockLine, QStringLiteral("номер блока должен быть >= 1 — "
                                                               "блок пропущен"));
                            continue;
                        }
                        step.blocks << parseBlock(ctx, id, blockLine, body);
                        continue;
                    }
                    if (!b.isEmpty())
                        ctx.warn(i + 1, QStringLiteral("строка в #TOPOLOGY вне блока: «%1»").arg(b));
                    ++i;
                }
                if (i < lines.size() && lines.at(i).trimmed().startsWith(QLatin1String("#END_TOPOLOGY")))
                    ++i;
                continue;
            }

            // --- #LINK_LIST ---
            if (s.startsWith(QLatin1String("#LINK_LIST"))) {
                ++i;
                while (i < lines.size()) {
                    const QString b = lines.at(i).trimmed();
                    if (b.startsWith(QLatin1Char('#')))
                        break;
                    QString rest;
                    if (matchKey(b, QLatin1String("T"), &rest)) {
                        QStringList v = splitParams(rest);
                        v.removeAll(QString()); // терпим лишний ';' — он встречается в эталонах
                        if (v.size() >= 3) {
                            const int id = v.at(0).toInt();
                            const double x = evalOr(ctx, i + 1, QStringLiteral("X0 анкера"),
                                                    v.at(1), step.parm.angle, 0.0)
                                             * step.parm.longToMm;
                            const double y = evalOr(ctx, i + 1, QStringLiteral("Y0 анкера"),
                                                    v.at(2), step.parm.angle, 0.0)
                                             * step.parm.longToMm;
                            step.links.insert(id, QPointF(x, y));
                            step.linkIds.insert(id);
                        } else if (!b.isEmpty()) {
                            ctx.warn(i + 1, QStringLiteral("строка #LINK_LIST не разобрана: «%1»")
                                                .arg(b));
                        }
                    } else if (!b.isEmpty()) {
                        ctx.warn(i + 1, QStringLiteral("строка в #LINK_LIST не похожа на T n; x; y;: "
                                                       "«%1»").arg(b));
                    }
                    ++i;
                }
                if (i < lines.size() && lines.at(i).trimmed().startsWith(QLatin1String("#END_LINK")))
                    ++i;
                continue;
            }

            // --- #OUTPUT ---
            if (s.startsWith(QLatin1String("#OUTPUT"))) {
                const int first = i + 2;
                QStringList body;
                ++i;
                while (i < lines.size() && !lines.at(i).trimmed().startsWith(QLatin1Char('#'))) {
                    body << lines.at(i);
                    ++i;
                }
                if (i < lines.size() && lines.at(i).trimmed().startsWith(QLatin1String("#END_OUTPUT")))
                    ++i;
                parseOutput(ctx, body, first, step.parm, &step.out);
                continue;
            }

            if (!s.isEmpty() && s.startsWith(QLatin1Char('#')))
                ctx.warn(i + 1, QStringLiteral("неизвестная секция «%1» пропущена").arg(s));
            ++i;
        }

        carried = step.parm;
        steps << step;
    }

    return steps;
}

// ===========================================================================
//  Плоские шаги -> модель
// ===========================================================================

/// Плазменная добавка блока: суффикс примитива -> поле модели.
///
/// ⚠️ Вся связь «суффикс языка — поле модели» живёт ТОЛЬКО здесь. В языке N, B и Y
/// это три разных примитива с одинаковой геометрией, в модели — одна фигура с
/// тремя величинами; склейкой занимается mergePlasmaInto().
/// Непустая `formula` означает, что величина задана выражением от координат; тогда
/// `value` не используется, а в файл при сохранении уйдёт сама формула.
void applyPlasma(Shape &s, QStringView suffix, double value, const QString &formula = QString())
{
    if (suffix == QLatin1String("_N")) {
        s.plasma.hasN = true;
        s.plasma.n = value;
        s.plasma.nFormula = formula;
    } else if (suffix == QLatin1String("_B")) {
        s.plasma.hasB = true;
        s.plasma.b = value;
        s.plasma.bFormula = formula;
    } else if (suffix == QLatin1String("_Y")) {
        s.plasma.hasY = true;
        s.plasma.y = value;
        s.plasma.yFormula = formula;
    }
}

bool samePoint(const QPointF &a, const QPointF &b)
{
    // Допуск — одна миллионная миллиметра, а не машинный ноль. Координаты приходят
    // из ТЕКСТА, где число уже округлено при записи, и точная середина отрезка
    // 1.0…10.16384259 читается обратно с расхождением в последнем знаке. Строгое
    // сравнение из-за этого не узнавало собственную добивку отрезка.
    //
    // Ниже нанометра геометрии в задании не бывает: сетка DELTA идёт долями миллиметра,
    // так что слить две действительно разные вершины этот допуск не может.
    constexpr double kEps = 1e-6;
    return std::fabs(a.x() - b.x()) < kEps && std::fabs(a.y() - b.y()) < kEps;
}

/// Убирает вершины, которых в геометрии нет, а в файле они есть только потому,
/// что ядро отвергает `POLYGON_STAT` меньше чем с ТРЕМЯ строками `L`
/// (`if( n1 < 3 )` в TmcRTH_BolckList.cpp стоит до разбора материала).
///
/// Отрезок добивают до трёх точек двумя способами:
///  * автор в старых заданиях ДУБЛИРУЕТ последнюю точку (RT_HPLANAR);
///  * наш эмиттер вставляет СЕРЕДИНУ отрезка.
/// И то и другое при чтении надо схлопнуть: иначе прогон emit -> read вернёт
/// ломаную из трёх точек там, где пользователь рисовал две.
/// Убирает вершины, которые в задании стоят ТОЛЬКО ради требования ядра.
///
/// `POLYGON_STAT` не грузится меньше чем с тремя строками `L` (проверка `n1 < 3` в
/// `TmcRTH_BolckList.cpp` стоит до разбора материала), поэтому отрезок из двух точек
/// приходится добивать до трёх. Способов два, и оба встречаются:
///   * повторить вершину — так написаны авторские задания (`samples/DEMO/.../horn.tpl`,
///     блоки 3 и 4: последняя точка продублирована);
///   * поставить точку посередине — так пишет наш эмиттер.
/// Геометрия у обоих одна и та же: звено нулевой длины и точка на той же прямой
/// поля не меняют.
///
/// ⚠️ Молчим намеренно (решение 2026-09-06). Раньше на каждую такую вершину шло
/// предупреждение, и открытие авторского `horn.tpl` встречало человека окном
/// «часть задания модель выражает не так, как в файле». Формально правда — при
/// сохранении повтор вершины сменится точкой посередине, — но ТЕРЯТЬ тут нечего:
/// это одна и та же обходка одного и того же ограничения ядра, записанная двумя
/// способами. Предупреждать надо о потерях, а не о смене записи.
void dropPaddingPoints(const Ctx &ctx, int line, int id, QVector<QPointF> *pts)
{
    Q_UNUSED(ctx);
    Q_UNUSED(line);
    Q_UNUSED(id);

    // Подряд идущие совпадающие вершины — всегда лишние.
    for (int i = pts->size() - 1; i > 0; --i) {
        if (samePoint(pts->at(i), pts->at(i - 1)))
            pts->removeAt(i);
    }

    // Ровно три точки, средняя точно посередине — это наша «добивка» отрезка.
    if (pts->size() == 3) {
        const QPointF mid = (pts->at(0) + pts->at(2)) / 2.0;
        if (samePoint(pts->at(1), mid))
            pts->removeAt(1);
    }
}

/// Первый параметр примитива: материал или выражение добавки eps.
/// ⚠️ В файле лежит ДОБАВКА eps-1, в модели — сама eps.
///
/// `longToMm` нужен формулам: ядро подставляет в них x и y в единицах LONG_UNIT
/// того файла, где формула написана, а модель редактора и выдаваемый им файл всегда
/// в миллиметрах. Формулу из файла в сантиметрах надо пересчитать, иначе профиль
/// растянется вдесятеро.
void applyMaterial(const Ctx & /*ctx*/, int /*line*/, const QString &first, const QString &suffix,
                   AngleUnit angle, double longToMm, Shape *s)
{
    QString expr = first;
    const double toModel = (longToMm != 0.0) ? 1.0 / longToMm : 1.0;

    // RECT_STAT [name]<выражение> — ядру велено сохранить посчитанный профиль в файл.
    // Имя запоминаем: это часть задания, и потерять его значило бы при сохранении
    // отобрать у пользователя файл, на который он, возможно, уже ссылается.
    QString profileFile;
    if (expr.startsWith(QLatin1Char('['))) {
        const int close = expr.indexOf(QLatin1Char(']'));
        if (close > 0) {
            profileFile = expr.mid(1, close - 1).trimmed();
            expr = expr.mid(close + 1);
        }
    }

    if (!suffix.isEmpty()) {
        // Плазменный параметр бывает и профилем: ядро считает первый параметр блока
        // тем же вычислителем, что и добавку eps, поэтому `RECT_STAT_N <выражение
        // от x и y>` — законный блок. Раньше такое выражение не вычислялось (оно и
        // не может быть вычислено: у него нет ОДНОГО значения), фигура получала ноль,
        // и сохранение поверх стирало профиль плотности из задания.
        const auto v = evalExpression(expr, angle);
        if (v.has_value() && profileFile.isEmpty()) {
            applyPlasma(*s, suffix, *v);
        } else {
            // Имя файла возвращаем в текст: `[имя]` — часть первого параметра, и
            // потерять его значило бы отобрать у пользователя файл профиля.
            const QString body = scaleCoordinates(expr.trimmed(), toModel);
            const QString kept = profileFile.isEmpty()
                                     ? body
                                     : QStringLiteral("[%1]%2").arg(profileFile, body);
            applyPlasma(*s, suffix, 0.0, kept);
        }
        return;
    }

    if (expr == QLatin1String("METAL"))         { s->material = Material::Metal;    return; }
    if (expr == QLatin1String("MAGNETIC"))      { s->material = Material::Magnetic; return; }
    if (expr == QLatin1String("ABSORBER"))      { s->material = Material::Absorber; return; }

    s->material = Material::Dielectric;
    const auto add = evalExpression(expr, angle);
    if (add.has_value()) {
        s->eps = *add + 1.0; // в файле лежит ДОБАВКА eps-1, в модели — сама eps
        if (!profileFile.isEmpty()) {
            // Постоянное выражение с именем файла: профиль вырожденный, но имя всё
            // равно осмысленно — ядро запишет по нему файл.
            s->epsProfile.enabled = true;
            s->epsProfile.file = profileFile;
            s->epsProfile.formula = QString::number(s->eps, 'g', 12);
        }
        return;
    }

    // Выражение зависит от координат — это и есть неоднородная eps.
    //
    // В файле лежит ДОБАВКА, в модели — сама eps, поэтому единицу надо вернуть.
    // Наш собственный вид `(X)-1` разбираем обратно в `X`: иначе круговорот
    // «открыл -> сохранил» обрастал бы `((X)-1)+1` на каждом заходе.
    QString formula = expr.trimmed();
    if (formula.startsWith(QLatin1Char('(')) && formula.endsWith(QLatin1String(")-1"))) {
        const QString inner = formula.mid(1, formula.size() - 4);
        // Проверяем, что снятая скобка была парной, а не двумя разными: у `(a)+(b)-1`
        // такое «снятие» изуродовало бы выражение.
        int depth = 0;
        bool balanced = true;
        for (QChar c : inner) {
            if (c == QLatin1Char('('))
                ++depth;
            else if (c == QLatin1Char(')') && --depth < 0)
                balanced = false;
        }
        if (balanced && depth == 0)
            formula = inner;
        else
            formula = QStringLiteral("(%1)+1").arg(formula);
    } else {
        formula = QStringLiteral("(%1)+1").arg(formula);
    }

    s->epsProfile.enabled = true;
    // Координаты в формуле — в единицах LONG_UNIT исходного файла, а модель и
    // выдаваемый файл всегда в миллиметрах.
    s->epsProfile.formula = scaleCoordinates(formula, toModel);
    s->epsProfile.file = profileFile;
    s->eps = 1.0; // числом такую среду не описать; поле остаётся при выключенном профиле

    // Форма здесь ни при чём: выражение ядро считает и в круге, и в полигоне, и
    // записываем мы его обратно той же фигурой. Прямоугольник нужен лишь для `[имя]`,
    // и об этом скажет валидатор — ровно тому, кто такое имя задал.
}

QString num(double v)
{
    return QString::number(v, 'g', 12);
}

/// Ключ ГЕОМЕТРИИ — без материала и плазмы. Нужен, чтобы узнать три плазменных
/// блока одной и той же фигуры.
QString shapeGeometryKey(const Shape &s)
{
    QStringList p;
    p << QString::number(int(s.kind)) << num(s.vx) << num(s.vy);
    switch (s.kind) {
    case ShapeKind::Rectangle:
        p << num(s.xMin) << num(s.xMax) << num(s.yMin) << num(s.yMax);
        break;
    case ShapeKind::Circle:
        p << num(s.center.x()) << num(s.center.y()) << num(s.rMin) << num(s.rMax)
          << num(s.fiMin) << num(s.fiMax);
        break;
    case ShapeKind::Polygon:
    case ShapeKind::Polyline:
        for (const QPointF &pt : s.points)
            p << num(pt.x()) << num(pt.y());
        break;
    }
    return p.join(QLatin1Char('|'));
}

QString shapeKey(const Shape &s)
{
    // Профиль входит в ключ наравне с материалом: две одинаковые по форме области с
    // РАЗНЫМИ формулами — это две разные среды, и слить их в одну фигуру значило бы
    // потерять одну из формул.
    //
    // Имя файла — в ключе только у профиля ИЗ файла (там это сама среда). У профиля
    // формулой имя файла — пометка «сохранить посчитанное», и эмиттер ставит её лишь
    // на первом шаге: с ней в ключе одна фигура свипа рассыпалась бы на две.
    return QStringList{shapeGeometryKey(s), QString::number(int(s.material)), num(s.eps),
                       s.epsProfile.formula,
                       s.epsProfile.fromFile ? s.epsProfile.file : QString(),
                       s.epsProfile.customOrigin ? num(s.epsProfile.origin.x()) : QString(),
                       s.epsProfile.customOrigin ? num(s.epsProfile.origin.y()) : QString(),
                       QString::number(int(s.plasma.hasN)), num(s.plasma.n), s.plasma.nFile,
                       QString::number(int(s.plasma.hasB)), num(s.plasma.b),
                       QString::number(int(s.plasma.hasY)), num(s.plasma.y)}
        .join(QLatin1Char('|'));
}

/// Собирает `_N`, `_B` и `_Y` одной области обратно в ОДНУ фигуру модели.
///
/// ⚠️ Эмиттер пишет B и Y всегда ПРЯМОУГОЛЬНИКОМ по габариту фигуры: круглым и
/// полигональным вариантам этих примитивов ядро не верит (в диспетчере
/// `TmcRTH_BolckList.cpp:609,627` теги `CIRCLE_STAT_B`/`POLYGON_STAT_B`
/// сравниваются по длине `CIRCLE_STAT `, и подмагничивание молча становится
/// концентрацией). Поэтому рамку по габариту тоже узнаём как «та же фигура».
bool mergePlasmaInto(QVector<Shape> *shapes, const Shape &add)
{
    for (int i = shapes->size() - 1; i >= 0; --i) {
        Shape &t = (*shapes)[i];
        if (!t.plasma.any())
            continue;

        bool same = (shapeGeometryKey(t) == shapeGeometryKey(add));
        if (!same && add.kind == ShapeKind::Rectangle && !add.plasma.hasN) {
            double xLo = 0, xHi = 0, yLo = 0, yHi = 0;
            t.bounds(&xLo, &xHi, &yLo, &yHi);
            same = std::fabs(add.xMin - xLo) < 1e-9 && std::fabs(add.xMax - xHi) < 1e-9
                   && std::fabs(add.yMin - yLo) < 1e-9 && std::fabs(add.yMax - yHi) < 1e-9;
        }
        if (!same)
            continue;

        if ((add.plasma.hasN && t.plasma.hasN) || (add.plasma.hasB && t.plasma.hasB)
            || (add.plasma.hasY && t.plasma.hasY))
            continue; // величина уже занята — это другая фигура, а не та же

        if (add.plasma.hasN) {
            t.plasma.hasN = true;
            t.plasma.n = add.plasma.n;
            t.plasma.nFile = add.plasma.nFile;
            t.plasma.nFormula = add.plasma.nFormula;
        }
        if (add.plasma.hasB) {
            t.plasma.hasB = true;
            t.plasma.b = add.plasma.b;
            t.plasma.bFormula = add.plasma.bFormula;
        }
        if (add.plasma.hasY) {
            t.plasma.hasY = true;
            t.plasma.y = add.plasma.y;
            t.plasma.yFormula = add.plasma.yFormula;
        }
        // Анкер формулы приходит вместе с той величиной, которая формулой и задана.
        if (add.plasma.customOrigin && !t.plasma.customOrigin) {
            t.plasma.customOrigin = true;
            t.plasma.origin = add.plasma.origin;
        }
        return true;
    }
    return false;
}

QString portKey(const Port &p)
{
    // Сторона, а не только ось: у волновода оба входа — INPUT_X с одной и той же
    // апертурой, и различает их ровно то, что один левый, а другой правый. По оси
    // они склеивались в один порт, и половина задания пропадала при чтении.
    return QStringList{QString::number(int(p.side)), num(p.from), num(p.to),
                       num(p.plane.value_or(std::numeric_limits<double>::quiet_NaN())),
                       QString::number(int(p.mode))}
        .join(QLatin1Char('|'));
}

/// Падающая волна из файла: на месте Amp стоит `[имя]`.
///
/// Форму восстанавливаем по служебной строке `@TPLD-SIG` в шапке самого файла. Без
/// неё (файл написан руками или другой программой) форма неизвестна, и честный ответ
/// один — взять точки как есть, таблицей: так ничего не потеряется и ничего не будет
/// выдумано.
///
/// ⚠️ Имя файла ищем БЕЗ учёта регистра, как и `#include`: задания пишут на Windows,
/// где регистр не значим, а читать их могут на Linux и macOS, где значим.
bool restoreFileSignal(const Ctx &ctx, int line, const QString &ampExpr, Port *port)
{
    const QString s = ampExpr.trimmed();
    if (!s.startsWith(QLatin1Char('[')))
        return false;
    const int close = s.indexOf(QLatin1Char(']'));
    if (close < 0) {
        // Ядро на этом тоже останавливается: «missing ] or bad [ in signal expression».
        ctx.warn(line, QStringLiteral("в имени файла сигнала «%1» нет закрывающей скобки — "
                                      "ядро такой файл не примет; вход оставлен без сигнала")
                           .arg(s));
        return false;
    }
    const QString name = s.mid(1, close - 1).trimmed();
    if (name.isEmpty()) {
        ctx.warn(line, QStringLiteral("имя файла сигнала пусто"));
        return false;
    }

    // Расширение ядро дописывает само, но только если точки в имени нет.
    const QString fileName = name.contains(QLatin1Char('.')) ? name
                                                            : name + QStringLiteral(".sig");
    const QString path = ctx.baseDir.isEmpty()
                             ? QString()
                             : findFileIgnoringCase(ctx.baseDir, fileName);
    QString text;
    if (path.isEmpty() || !readTplFileText(path, &text)) {
        // Потеря, а не смена записи: предупреждаем прямо. Порт переводим на
        // непрерывный сигнал, иначе задание нельзя было бы сохранить вовсе.
        ctx.warn(line, QStringLiteral("файл падающей волны «%1» рядом с заданием не найден — "
                                      "форма сигнала потеряна, вход переведён на непрерывный "
                                      "сигнал. Положите файл рядом и откройте задание заново")
                           .arg(fileName));
        return false;
    }

    port->signal.source = SignalSource::File;
    // Имя храним БЕЗ расширения — таким оно и уйдёт обратно в квадратные скобки.
    port->signal.file = name.contains(QLatin1Char('.'))
                            ? name.left(name.lastIndexOf(QLatin1Char('.')))
                            : name;

    Waveshape w;
    bool restored = false;
    for (const QString &l : text.split(QLatin1Char('\n'))) {
        if (parseSigMarker(l, &w)) {
            restored = true;
            break;
        }
    }
    if (restored) {
        port->signal.wave = w;
        // У таблицы сами точки живут только в файле — их надо взять оттуда.
        if (w.kind == Waveform::Table) {
            port->signal.wave.points = parseSigPoints(text);
            port->signal.wave.tableTones = parseSigTones(text);
        }
        return true;
    }

    Waveshape t;
    t.kind = Waveform::Table;
    t.points = parseSigPoints(text);
    t.tableTones = parseSigTones(text);
    port->signal.wave = t;
    if (t.tableTones.size() > kMaxFreqs) {
        ctx.warn(line, QStringLiteral("в файле «%1» строка #FREQ называет %2, а пакет берёт "
                                      "не больше %3: обработаются первые %3, остальные %4 "
                                      "будут отброшены")
                           .arg(fileName,
                                plural(int(t.tableTones.size()), QStringLiteral("тон"),
                                       QStringLiteral("тона"), QStringLiteral("тонов")))
                           .arg(kMaxFreqs)
                           .arg(t.tableTones.size() - kMaxFreqs));
    }
    if (t.points.size() < 2) {
        ctx.warn(line, QStringLiteral("в файле «%1» меньше двух точек — ядро его не примет")
                           .arg(fileName));
    } else {
        ctx.warn(line, QStringLiteral("файл «%1» сделан не этой программой: форма взята "
                                      "таблицей из %2 точек — так ничего не потеряется")
                           .arg(fileName).arg(t.points.size()));
    }
    return true;
}

/// Восстанавливает форму сигнала из выражения Amp и временного окна.
///
/// Огибающая живёт в Amp — окно ядро режет резко, поэтому «гауссов импульс»
/// узнаётся именно по виду выражения, а не по узкому окну.
void restoreSignal(const Ctx &ctx, int line, const QString &ampExpr, const QString &fazaExpr,
                   double tMin, double tMax, const RawParams &parm, Port *port)
{
    static const QRegularExpression gauss(
        QStringLiteral("^\\s*(.+?)\\*exp\\(\\s*-\\s*\\(\\(t-(.+?)\\)/(.+?)\\)\\*"
                       "\\(\\(t-(.+?)\\)/(.+?)\\)\\s*\\)\\s*$"));

    port->signal.phase = fazaExpr.trimmed().isEmpty()
                             ? 0.0
                             : evalOr(ctx, line, QStringLiteral("фазу входа"), fazaExpr,
                                      parm.angle, 0.0);
    if (parm.angle == AngleUnit::Gradus)
        port->signal.phase *= 3.14159265358979323846 / 180.0; // модель хранит радианы

    // Сигнал из файла проверяем ПЕРВЫМ: `[имя]` не выражение, и вычислять его нельзя —
    // ядро на разборе такого Amp падало с `abnormal function`, пока в него не добавили
    // ту же проверку по первому символу.
    if (restoreFileSignal(ctx, line, ampExpr, port))
        return;
    if (ampExpr.trimmed().startsWith(QLatin1Char('['))) {
        // Файл не нашёлся, и об этом уже сказано выше. Пытаться после этого ВЫЧИСЛИТЬ
        // «[имя]» как выражение значит выдать второе предупреждение о том же самом —
        // человек получал подряд «форма потеряна» и «амплитуду вычислить не удалось».
        port->signal.shape = SignalShape::Continuous;
        port->signal.amplitude = 1.0;
        return;
    }

    const QRegularExpressionMatch m = gauss.match(ampExpr.trimmed());
    if (m.hasMatch()) {
        port->signal.shape = SignalShape::Gaussian;
        port->signal.amplitude = evalOr(ctx, line, QStringLiteral("амплитуду"), m.captured(1),
                                        parm.angle, 1.0);
        port->signal.center = evalOr(ctx, line, QStringLiteral("центр импульса"), m.captured(2),
                                     parm.angle, 0.0) * parm.timeToNs;
        port->signal.duration = evalOr(ctx, line, QStringLiteral("длительность импульса"),
                                       m.captured(3), parm.angle, 1.0) * parm.timeToNs;
        return;
    }

    bool ok = false;
    const double amp = ampExpr.trimmed().isEmpty()
                           ? 1.0
                           : evalOr(ctx, line, QStringLiteral("амплитуду входа"), ampExpr,
                                    parm.angle, 1.0, &ok);
    if (!ampExpr.trimmed().isEmpty() && !ok) {
        ctx.warn(line, QStringLiteral("выражение амплитуды «%1» модель не выражает — взята "
                                      "постоянная амплитуда 1").arg(ampExpr.trimmed()));
        port->signal.shape = SignalShape::Continuous;
        port->signal.amplitude = 1.0;
        return;
    }

    // Окно, накрывающее весь счёт, — это несущая на всё время; иначе резкий импульс.
    if (tMin <= parm.timeMin + 1e-9 && tMax >= parm.timeMax - 1e-9) {
        port->signal.shape = SignalShape::Continuous;
        port->signal.amplitude = amp;
    } else {
        port->signal.shape = SignalShape::Rectangular;
        port->signal.amplitude = amp;
        port->signal.center = (tMin + tMax) / 2.0;
        port->signal.duration = tMax - tMin;
    }
}

/// Модель одного `#STEP` — промежуточное представление перед склейкой.
struct StepModel {
    double freq = 0.0;
    QVector<Shape> shapes;
    QVector<Port> ports;
    RawOutput out;
    bool anyPlasma = false;
};

/// Имя файла распределения так, как его держит модель: без расширения.
///
/// Ядро дописывает `.eps` само (`AddFileEpsExtention`), поэтому в задании имя обычно
/// написано голым. Но файл могли написать руками и с расширением — тогда, сохранив имя
/// как есть, мы получили бы при следующей записи `имя.eps.eps`.
QString stripEpsExtension(const QString &name)
{
    if (name.endsWith(QLatin1String(".eps"), Qt::CaseInsensitive))
        return name.left(name.size() - 4);
    return name;
}

StepModel buildStep(const Ctx &ctx, const RawStep &step)
{
    StepModel sm;
    sm.freq = step.parm.freq;
    sm.out = step.out;

    const Box box{step.parm.xMin, step.parm.xMax, step.parm.yMin, step.parm.yMax};
    const AngleUnit angle = step.parm.angle;
    const double k = step.parm.longToMm;

    for (const RawBlock &b : step.blocks) {
        if (b.prim == Prim::Unknown)
            continue;

        QPointF anchor(0.0, 0.0);
        if (step.links.contains(b.id)) {
            anchor = step.links.value(b.id);
        } else {
            ctx.warn(b.line, QStringLiteral("BLOCK %1 не упомянут в #LINK_LIST — ядро такой файл "
                                            "не примет; принято положение (0;0)").arg(b.id));
        }

        if (b.prim == Prim::FileEps) {
            // Распределение eps из готового файла. В модели это фигура с профилем в
            // режиме «читать из файла» — тогда она и рисуется, и возвращается в
            // задание дословно.
            //
            // ⚠️ Все варианты FILE* ядро разбирает ОДНИМ обработчиком: ветка выбирается
            // сравнением по пяти символам «FILE_» (TmcRTH_BolckList.cpp:566), а сам
            // ReadFileStat_1 (там же:2903-2929) читает ровно один параметр — имя файла —
            // и ставит nType = CTMCRTH_BLCKNTYPE_EPS, перезаписав тег на «FILE ».
            // Значит FILE_N — это НЕ концентрация, а та же проницаемость; о подмене
            // говорим предупреждением, но блок не теряем.
            Shape s;
            s.id = b.id;
            s.kind = ShapeKind::Rectangle;
            s.material = Material::Dielectric;
            s.epsProfile.enabled = true;
            s.epsProfile.fromFile = true;
            s.epsProfile.file = stripEpsExtension(b.params.value(0).trimmed());

            if (s.epsProfile.file.isEmpty()) {
                ctx.warn(b.line, QStringLiteral("BLOCK %1: у блока FILE не указано имя файла "
                                                "распределения").arg(b.id));
            }
            if (!b.suffix.isEmpty()) {
                ctx.warn(b.line,
                         QStringLiteral("BLOCK %1: написано FILE%2, но ядро читает любой FILE_* "
                                        "как распределение диэлектрической проницаемости, а не "
                                        "%3 — блок прочитан как профиль eps из «%4»")
                             .arg(b.id).arg(b.suffix,
                                            b.suffix == QLatin1String("_N")
                                                ? QStringLiteral("концентрации")
                                                : QStringLiteral("параметра плазмы"),
                                            s.epsProfile.file));
            }

            // Координаты ядро у этого блока не читает — сетка лежит в самом файле, а
            // положение задаёт анкер. Но в авторских заданиях они написаны, и по ним
            // видно, где профиль лежит: берём их для показа в редакторе. Нет их —
            // остаётся точка анкера, и фигура просто не имеет площади.
            const bool haveBox = b.params.size() >= 5;
            if (haveBox) {
                s.xMin = evalOr(ctx, b.line, QStringLiteral("X_min блока FILE"),
                                b.params.value(1), angle, 0.0) * k + anchor.x();
                s.xMax = evalOr(ctx, b.line, QStringLiteral("X_max блока FILE"),
                                b.params.value(2), angle, 0.0) * k + anchor.x();
                s.yMin = evalOr(ctx, b.line, QStringLiteral("Y_min блока FILE"),
                                b.params.value(3), angle, 0.0) * k + anchor.y();
                s.yMax = evalOr(ctx, b.line, QStringLiteral("Y_max блока FILE"),
                                b.params.value(4), angle, 0.0) * k + anchor.y();
            } else {
                s.xMin = s.xMax = anchor.x();
                s.yMin = s.yMax = anchor.y();
            }

            // Анкер — это НЕ украшение: от него ядро отсчитывает `#Xmin`/`#Ymin` из
            // файла, то есть он и ставит распределение на место. Теряем его —
            // распределение уезжает. Держим там же, где начало отсчёта формулы.
            const QPointF centre = s.epsOrigin();
            if (std::hypot(centre.x() - anchor.x(), centre.y() - anchor.y()) > 1e-6) {
                s.epsProfile.customOrigin = true;
                s.epsProfile.origin = anchor;
            }

            sm.shapes << s;
            continue;
        }

        if (b.prim == Prim::InputX || b.prim == Prim::InputY) {
            const bool alongY = (b.prim == Prim::InputX);
            Port p;
            p.id = b.id;
            p.mode = (b.params.value(0) == QLatin1String("MAGNETIC")) ? PortMode::Plane
                                                                     : PortMode::H10;
            // ⚠️ В старых заданиях первым параметром входа встречается произвольный
            // профиль поля («INPUT_X sin(y*3.141592653589);»). Нынешнее ядро такое
            // отвергает (TmcRTH_BolckList.cpp:2331-2342 — только METAL, MAGNETIC
            // или пусто), поэтому молча разбирать это нельзя.
            if (!b.params.value(0).isEmpty() && b.params.value(0) != QLatin1String("MAGNETIC")
                && b.params.value(0) != QLatin1String("METAL")) {
                ctx.warn(b.line, QStringLiteral("BLOCK %1: профиль входа выражением («%2») "
                                                "нынешнее ядро не принимает — оно ждёт METAL, "
                                                "MAGNETIC или пусто; принята волна H10")
                                     .arg(b.id).arg(b.params.value(0)));
            }

            // Апертура ЛОКАЛЬНАЯ: к ней прибавляется анкер поперёк плоскости входа.
            const double along = alongY ? anchor.y() : anchor.x();
            p.from = evalOr(ctx, b.line, QStringLiteral("нижнюю границу входа"), b.params.value(1),
                            angle, 0.0) * k + along;
            p.to = evalOr(ctx, b.line, QStringLiteral("верхнюю границу входа"), b.params.value(2),
                          angle, 0.0) * k + along;

            const double tMin = evalOr(ctx, b.line, QStringLiteral("T_min входа"),
                                       b.params.value(3), angle, 0.0) * step.parm.timeToNs;
            const double tMax = evalOr(ctx, b.line, QStringLiteral("T_max входа"),
                                       b.params.value(4), angle, 0.0) * step.parm.timeToNs;

            // Плоскость входа ядро берёт буквально из анкера, а «левый/правый»
            // выводит само — по половине коробки.
            const double plane = alongY ? anchor.x() : anchor.y();
            p.side = directionForPlane(alongY, plane, box);
            const double def = portPlane(p, box); // p.plane ещё пуст -> граница по side
            if (std::fabs(plane - def) > 1e-9)
                p.plane = plane;

            restoreSignal(ctx, b.line, b.params.value(5), b.params.value(6), tMin, tMax,
                          step.parm, &p);

            // Пассивный вход — это окно, начинающееся позже конца счёта (tpl-guide 5.5).
            const bool late = tMin > step.parm.timeMax + 1e-9;
            // У сигнала из файла амплитуда живёт в форме, а не в Signal::amplitude,
            // и «ноль вместо Amp» там невозможен: на месте Amp стоит имя файла.
            const bool silent = p.signal.source != SignalSource::File
                                && (p.signal.shape != SignalShape::Gaussian)
                                && std::fabs(p.signal.amplitude) < 1e-12;
            p.active = !late && !silent;

            if (!p.active) {
                // У пассивного входа сигнала нет вовсе: и окно, и амплитуда у него —
                // заглушки, которыми его глушили при записи. Восстанавливать по ним
                // «прямоугольный импульс длиной 115 нс» значит выдумать то, чего в
                // задании не было. Возвращаем умолчание — форму зададут, когда порт
                // сделают активным.
                p.signal = Signal{};
            }

            // Восьмой параметр — среда в сечении входа, записанная ДОБАВКОЙ (eps-1).
            // Модель хранит саму eps, поэтому единицу возвращаем здесь.
            if (b.params.size() > 7 && !b.params.value(7).trimmed().isEmpty()) {
                bool okEps = false;
                const double add = evalOr(ctx, b.line, QStringLiteral("eps в сечении входа"),
                                          b.params.value(7), angle, 0.0, &okEps);
                if (okEps) {
                    p.eps = add + 1.0;
                    if (p.eps <= 0.0) {
                        ctx.warn(b.line, QStringLiteral("BLOCK %1: eps в сечении входа = %2 — "
                                                        "ядро такой файл не примет")
                                             .arg(b.id).arg(p.eps));
                    }
                } else {
                    ctx.warn(b.line, QStringLiteral("BLOCK %1: добавку eps «%2» вычислить не "
                                                    "удалось — вход оставлен в вакууме")
                                         .arg(b.id).arg(b.params.value(7).trimmed()));
                }
            }
            if (b.params.size() > 8) {
                ctx.warn(b.line, QStringLiteral("BLOCK %1: у входа больше восьми параметров — "
                                                "лишние отброшены").arg(b.id));
            }
            sm.ports << p;
            continue;
        }

        Shape s;
        s.id = b.id;
        applyMaterial(ctx, b.line, b.params.value(0), b.suffix, angle, k, &s);
        if (!b.suffix.isEmpty())
            sm.anyPlasma = true;

        switch (b.prim) {
        case Prim::Rect: {
            s.kind = ShapeKind::Rectangle;
            s.xMin = evalOr(ctx, b.line, QStringLiteral("X_min блока"), b.params.value(1), angle,
                            0.0) * k + anchor.x();
            s.xMax = evalOr(ctx, b.line, QStringLiteral("X_max блока"), b.params.value(2), angle,
                            0.0) * k + anchor.x();
            s.yMin = evalOr(ctx, b.line, QStringLiteral("Y_min блока"), b.params.value(3), angle,
                            0.0) * k + anchor.y();
            s.yMax = evalOr(ctx, b.line, QStringLiteral("Y_max блока"), b.params.value(4), angle,
                            0.0) * k + anchor.y();
            if (b.moving) {
                s.vx = evalOr(ctx, b.line, QStringLiteral("Vx"), b.params.value(5), angle, 0.0);
                s.vy = evalOr(ctx, b.line, QStringLiteral("Vy"), b.params.value(6), angle, 0.0);
            }
            break;
        }
        case Prim::Circle: {
            s.kind = ShapeKind::Circle;
            s.center = anchor; // центра в блоке нет — им служит анкер
            s.rMin = evalOr(ctx, b.line, QStringLiteral("R_min"), b.params.value(1), angle, 0.0) * k;
            s.rMax = evalOr(ctx, b.line, QStringLiteral("R_max"), b.params.value(2), angle, 0.0) * k;
            const double a2r = (angle == AngleUnit::Gradus) ? 3.14159265358979323846 / 180.0 : 1.0;
            s.fiMin = evalOr(ctx, b.line, QStringLiteral("F_min"), b.params.value(3), angle, 0.0) * a2r;
            s.fiMax = evalOr(ctx, b.line, QStringLiteral("F_max"), b.params.value(4), angle, 0.0) * a2r;
            if (b.moving) {
                s.vx = evalOr(ctx, b.line, QStringLiteral("Vx"), b.params.value(5), angle, 0.0);
                s.vy = evalOr(ctx, b.line, QStringLiteral("Vy"), b.params.value(6), angle, 0.0);
            }
            break;
        }
        case Prim::Polygon: {
            for (const auto &pt : b.points) {
                const double x = evalOr(ctx, b.line, QStringLiteral("X вершины"), pt.first, angle,
                                        0.0) * k + anchor.x();
                const double y = evalOr(ctx, b.line, QStringLiteral("Y вершины"), pt.second, angle,
                                        0.0) * k + anchor.y();
                s.points << QPointF(x, y);
            }
            if (b.moving && b.extra.size() >= 2) {
                s.vx = evalOr(ctx, b.line, QStringLiteral("Vx"), b.extra.at(0), angle, 0.0);
                s.vy = evalOr(ctx, b.line, QStringLiteral("Vy"), b.extra.at(1), angle, 0.0);
            }
            dropPaddingPoints(ctx, b.line, b.id, &s.points);
            // У КОНТУРНОГО материала ядро последнюю точку с первой не соединяет:
            // замкнутый контур в файле записан повтором первой вершины.
            const bool closedByHand = s.points.size() >= 4
                                      && s.points.first() == s.points.last();
            if (s.isContour() && closedByHand) {
                s.points.removeLast();
                s.kind = ShapeKind::Polygon;
            } else if (s.isContour()) {
                s.kind = ShapeKind::Polyline; // ломаная, как её понимает ядро
            } else {
                s.kind = ShapeKind::Polygon;  // выражение ядро замыкает само
            }
            if (s.points.size() < 2)
                ctx.warn(b.line, QStringLiteral("BLOCK %1: у полигона меньше двух вершин").arg(b.id));
            break;
        }
        default:
            break;
        }

        // Начало отсчёта координат в формуле eps — это анкер блока, и другого способа
        // его задать в языке нет. Запоминаем его как есть: в авторских заданиях он
        // сплошь и рядом стоит НЕ в центре фигуры (`data/cir_quas.h`, блок 28:
        // прямоугольник по x 140...412, а формула отсчитана от 550 — от центра сосуда
        // снаружи фигуры). Подставить сюда центр значило бы молча сдвинуть среду на
        // 274 мм при первом же сохранении.
        if (s.hasEpsProfile() && s.kind != ShapeKind::Circle) {
            const QPointF centre = s.epsOrigin();
            if (std::hypot(centre.x() - anchor.x(), centre.y() - anchor.y()) > 1e-6) {
                s.epsProfile.customOrigin = true;
                s.epsProfile.origin = anchor;
            }
        }

        // То же и для профиля плазмы: у токамака анкер блока с плотностью стоит в
        // (0; 160) мм, и без него формула отсчитывалась бы от угла коробки, а плазма
        // при сохранении уехала бы на полкамеры. Эмиттер по умолчанию ставит плазме
        // анкер (0; 0) — вот с ним и сравниваем.
        if (s.plasma.hasFormula() && s.kind != ShapeKind::Circle
            && std::hypot(anchor.x(), anchor.y()) > 1e-6) {
            s.plasma.customOrigin = true;
            s.plasma.origin = anchor;
        }

        // Три плазменных примитива одной области — одна фигура модели.
        if (s.plasma.any() && mergePlasmaInto(&sm.shapes, s))
            continue;

        sm.shapes << s;
    }

    return sm;
}

/// Убирает хвост шага углового свипа: `_ref` у опорного шага, `_a30`/`_a12p5`/`_am15`
/// у шагов с повёрнутым объектом (см. `sweepFileTag`).
QString stripSweepSuffix(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("_(ref|am?[0-9p]+)$"));
    QString s = name;
    return s.remove(re);
}

/// Убирает хвост `_f<частота>`, которым эмиттер разводит `.t` шагов свипа.
///
/// Срезаем ТОЛЬКО точное совпадение с меткой частоты ЭТОГО шага. Регулярным
/// выражением по виду «подчёркивание, f, цифры» тут обойтись нельзя: авторский файл
/// вполне может называться `wg_f30`, и тогда при первом же сохранении программа
/// молча переименовала бы человеку выходные данные.
QString stripFreqSuffix(const QString &name, double freqGhz)
{
    if (freqGhz <= 0.0)
        return name;
    const QString tail = QLatin1Char('_') + freqFileTag(freqGhz);
    return name.endsWith(tail) ? name.left(name.size() - tail.size()) : name;
}

/// Убирает хвост `_p<N>` — так эмиттер помечает файлы разных возбуждений.
QString stripExciterSuffix(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("_p\\d+$"));
    QString s = name;
    return s.remove(re);
}

QString commonPrefix(const QStringList &list)
{
    if (list.isEmpty())
        return {};
    QString p = list.first();
    for (const QString &s : list) {
        int n = 0;
        while (n < p.size() && n < s.size() && p.at(n) == s.at(n))
            ++n;
        p.truncate(n);
    }
    return p;
}

} // namespace

// ===========================================================================

PreprocessResult preprocess(const QString &text, const QString &baseDir)
{
    PreprocessResult res;

    Records recs = loadRecords(text);
    joinBackslash(&recs);

    IncludeCtx ictx;
    ictx.baseDir = baseDir;
    ictx.included = &res.includedFiles;
    ictx.warnings = &res.warnings;
    ictx.errors = &res.errors;
    processIncludes(&recs, &ictx);

    expandMacros(&recs, &res);

    res.text = recs.join(QString());
    return res;
}

/// Подписи блоков из комментариев ИСХОДНОГО текста: «BLOCK 20; ! плазменный столб».
///
/// Препроцессор режет `!` первым делом, поэтому к разбору подписи уже не доживают.
/// А между тем это единственное человеческое имя фигуры во всём файле — и наш
/// эмиттер сам его туда и пишет. Собираем до раскрытия макросов, по номеру блока.
///
/// Номер, записанный выражением (в макро-теле он бывает параметром), пропускаем:
/// сопоставить такую подпись фигуре нечем, а гадать нельзя.
QHash<int, QString> collectBlockLabels(const QString &text)
{
    // Пробелы ищем БЕЗ перевода строки ([^\S\n]), иначе `\s*` перепрыгивает на
    // следующую строку и подписью становится закомментированная строка кода:
    //     BLOCK 1;
    //     !  INPUT_X ; -a1/2; a1/2; 0; 11 ;
    // В legacy-заданиях такие выключенные строки встречаются постоянно.
    static const QRegularExpression re(
        QStringLiteral("^[^\\S\\n]*BLOCK[^\\S\\n]+(\\d+)[^\\S\\n]*;[^\\S\\n]*"
                       "(?:\\\\\\\\)?[^\\S\\n]*![^\\S\\n]*(.+?)[^\\S\\n]*$"),
        QRegularExpression::MultilineOption);

    QHash<int, QString> out;
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        QString label = m.captured(2);
        // Хвост «\\» — это перенос строки макроса, а не часть подписи.
        while (label.endsWith(QLatin1Char('\\')))
            label.chop(1);

        // Эмиттер пишет комментарий как «имя — пояснение» (что за материал, какая
        // плазма, активен ли вход). Забрать нужно ТОЛЬКО имя: иначе пояснение
        // прилипнет к имени, а при следующем сохранении к нему допишется новое —
        // и подпись будет разрастаться с каждым циклом «открыл-сохранил».
        const int dash = label.indexOf(QStringLiteral(" — "));
        if (dash > 0)
            label.truncate(dash);
        label = label.trimmed();
        if (!label.isEmpty())
            out.insert(m.captured(1).toInt(), label);
    }
    return out;
}

namespace {

/// Имена вариантов из комментариев эмиттера, по порядку и без повторов:
///     ! ===== вариант: пустой, FREQ = 3.0 ГГц =====        (явный шаг)
///     ! вариант: пустой, возбуждён вход 1                   (перед #define макроса)
QStringList collectVariantTitles(const QString &text)
{
    static const QRegularExpression re(
        QStringLiteral("^[^\\S\\n]*!.*?вариант: (.+?)(?:, возбуждён.*|, FREQ.*|[^\\S\\n]*=====.*)?[^\\S\\n]*$"),
        QRegularExpression::MultilineOption);
    QStringList out;
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QString name = it.next().captured(1).trimmed();
        if (!name.isEmpty() && !out.contains(name))
            out << name;
    }
    return out;
}

/// Описание задания из шапки, которую пишет наш эмиттер:
///
///     ! Задание сгенерировано TPL Designer.
///     ! <описание, строка за строкой>
///     ! Ядро: tmc_rth (H-мода)
///
/// Чужие файлы такой шапки не имеют, и описания у них нет — это нормально.
QString readOwnDescription(const QString &src)
{
    const QStringList lines = src.split(QLatin1Char('\n'));
    int i = 0;
    while (i < lines.size() && !lines.at(i).startsWith(QStringLiteral("! Задание сгенерировано TPL Designer")))
        ++i;
    if (i >= lines.size() || i > 5)
        return {};
    QStringList out;
    for (++i; i < lines.size(); ++i) {
        const QString &l = lines.at(i);
        if (!l.startsWith(QLatin1Char('!')) || l.startsWith(QStringLiteral("! Ядро:")))
            break;
        QString t = l.mid(1);
        if (t.startsWith(QLatin1Char(' ')))
            t = t.mid(1);
        out << t;
    }
    return out.join(QLatin1Char('\n'));
}

/// Служебная строка углового свипа (см. `sweepHeader` в emitter.cpp).
struct SweepMarker {
    bool valid = false;
    QSet<int> blocks;
    QPointF pivot;
    QVector<double> angles;
    bool ref = false;
};

SweepMarker readSweepMarker(const QString &src)
{
    SweepMarker m;
    static const QRegularExpression re(
        QStringLiteral("^!\\s*%1\\s+blocks=([0-9,]*);\\s*pivot=([-0-9.eE+]+),([-0-9.eE+]+);"
                       "\\s*angles=([-0-9.eE+,]*);\\s*ref=([01])")
            .arg(QRegularExpression::escape(QLatin1String(kSweepMarker))),
        QRegularExpression::MultilineOption);
    const QRegularExpressionMatch hit = re.match(src);
    if (!hit.hasMatch())
        return m;
    for (const QString &b : hit.captured(1).split(QLatin1Char(','), Qt::SkipEmptyParts))
        m.blocks << b.toInt();
    m.pivot = QPointF(hit.captured(2).toDouble(), hit.captured(3).toDouble());
    for (const QString &a : hit.captured(4).split(QLatin1Char(','), Qt::SkipEmptyParts))
        m.angles << a.toDouble();
    m.ref = hit.captured(5) == QLatin1String("1");
    m.valid = !m.blocks.isEmpty() && !m.angles.isEmpty();
    return m;
}

/// Округление до 1e-11 мм: убирает шум `cos`/`sin`, оставшийся после обратного
/// поворота, чтобы одна и та же фигура со всех шагов свипа склеилась в одну.
///
/// Мельче нанометра — намеренно: эмиттер пишет смещение от оси десятью значащими
/// цифрами (0.9711089277), и грубое округление съедало бы десятую цифру — повторная
/// запись отличалась бы от исходной (найдено тестом на профиле крыла).
double snapNm(double v)
{
    const double r = std::round(v * 1e11) / 1e11;
    return r == 0.0 ? 0.0 : r; // без «-0»
}

/// Вернуть повёрнутую фигуру в исходное положение.
///
/// Повёрнутый прямоугольник эмиттер пишет полигоном из четырёх углов; вернувшись
/// в исходное положение, он снова лежит по осям — и снова становится прямоугольником,
/// иначе круговорот «записал — прочитал» превращал бы каждый прямоугольник в контур.
Shape unrotateShape(const Shape &s, const QPointF &pivot, double angleDeg)
{
    Shape r = rotatedShape(s, pivot, -angleDeg);
    r.center = QPointF(snapNm(r.center.x()), snapNm(r.center.y()));
    r.fiMin = snapNm(r.fiMin);
    r.fiMax = snapNm(r.fiMax);
    for (QPointF &pt : r.points)
        pt = QPointF(snapNm(pt.x()), snapNm(pt.y()));

    if (r.kind == ShapeKind::Polygon && r.points.size() == 4) {
        const QVector<QPointF> &q = r.points;
        double xLo = 0, xHi = 0, yLo = 0, yHi = 0;
        r.bounds(&xLo, &xHi, &yLo, &yHi);
        // Сравнение с допуском: после обратного поворота остаётся шум порядка 1e-12 мм.
        const auto near = [](double a, double b) { return std::fabs(a - b) < 1e-8; };
        bool axis = xHi - xLo > 1e-6 && yHi - yLo > 1e-6;
        QSet<int> seen; // какой угол занят: 0 — левый нижний, 1 — правый, 2 — верхний...
        for (const QPointF &pt : q) {
            const bool cx = near(pt.x(), xLo) || near(pt.x(), xHi);
            const bool cy = near(pt.y(), yLo) || near(pt.y(), yHi);
            axis &= cx && cy;
            seen << ((near(pt.x(), xLo) ? 0 : 1) | (near(pt.y(), yLo) ? 0 : 2));
        }
        // Все четыре угла различны — иначе это не прямоугольник, а вырожденный контур.
        axis &= (seen.size() == 4);
        if (axis) {
            // Углы — по 1e-9 мм: у настоящего прямоугольника они и так круглые.
            const auto nm = [](double v) {
                const double x = std::round(v * 1e9) / 1e9;
                return x == 0.0 ? 0.0 : x;
            };
            r.kind = ShapeKind::Rectangle;
            r.xMin = nm(xLo); r.xMax = nm(xHi); r.yMin = nm(yLo); r.yMax = nm(yHi);
            r.points.clear();
        }
    }
    return r;
}

} // namespace

ReadResult readTplText(const QString &text, const QString &baseDir)
{
    ReadResult r;

    QString src = text;
    src.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    src.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    const PreprocessResult pp = preprocess(src, baseDir);
    r.warnings += pp.warnings;
    r.errors += pp.errors;
    r.includedFiles = pp.includedFiles;
    r.expandedText = pp.text;
    r.wasMacro = (pp.macroCalls > 0);
    if (!r.ok())
        return r;

    // Подписи собираем и из самого файла, и из подключённых: у макро-версии вся
    // топология (а значит и подписи) лежит именно в .h.
    QHash<int, QString> labels = collectBlockLabels(src);
    // Имена вариантов — из комментариев эмиттера «! вариант: имя». Без них вариант
    // получал вместо имени метку файлов («short» вместо «короткое замыкание»).
    QStringList variantTitles = collectVariantTitles(src);
    for (const QString &inc : std::as_const(pp.includedFiles)) {
        QString incText;
        QString incErr;
        // Регистр здесь так же не важен, как и при вставке: файл тот же самый.
        const QString incPath = findFileIgnoringCase(baseDir, inc);
        if (incPath.isEmpty() || !readTplFileText(incPath, &incText, &incErr))
            continue; // сам факт пропажи include препроцессор уже отметил

        const QHash<int, QString> more = collectBlockLabels(incText);
        for (auto i = more.constBegin(); i != more.constEnd(); ++i)
            labels.insert(i.key(), i.value());
        for (const QString &t : collectVariantTitles(incText)) {
            if (!variantTitles.contains(t))
                variantTitles << t; // те же имена стоят и в .tpl перед вызовами
        }
    }

    Ctx ctx;
    ctx.warnings = &r.warnings;
    ctx.errors = &r.errors;
    ctx.baseDir = baseDir;

    bool sawHeader = false;
    bool sawEof = false;
    const QVector<RawStep> raw = parseSteps(ctx, pp.text, &sawHeader, &sawEof);

    if (!sawHeader) {
        r.errors << QStringLiteral("это не файл задания .tpl: нет заголовка #TMC_RT_H");
        return r;
    }
    if (!sawEof)
        r.warnings << QStringLiteral("в файле нет #EOF — ядро такой файл не примет");
    if (raw.isEmpty()) {
        r.errors << QStringLiteral("в задании нет ни одного #STEP — считать нечего");
        return r;
    }
    if (!raw.first().parm.haveTimeUnit) {
        r.warnings << QStringLiteral("TIME_UNIT не задан. Ядро по умолчанию понимает секунды, "
                                     "редактор принял наносекунды — проверьте время счёта");
    }

    // --- параметры задания берём из первого шага ---
    const RawParams &p0 = raw.first().parm;
    Project &prj = r.project;
    prj.box = Box{p0.xMin, p0.xMax, p0.yMin, p0.yMax};
    prj.delta = p0.delta;
    prj.timeMax = p0.timeMax;
    prj.unit = p0.lengthUnit;
    if (std::fabs(p0.timeMin) > 1e-9) {
        r.warnings << QStringLiteral("начало счёта TIME = %1 нс не равно нулю — модель хранит "
                                     "только конец счёта").arg(num(p0.timeMin));
    }

    // --- разбираем шаги ---
    QVector<StepModel> steps;
    steps.reserve(raw.size());
    bool anyPlasma = false;
    for (const RawStep &s : raw) {
        StepModel sm = buildStep(ctx, s);
        anyPlasma |= sm.anyPlasma;
        steps << sm;

        const Box b{s.parm.xMin, s.parm.xMax, s.parm.yMin, s.parm.yMax};
        if (std::fabs(b.xMin - prj.box.xMin) > 1e-9 || std::fabs(b.xMax - prj.box.xMax) > 1e-9
            || std::fabs(b.yMin - prj.box.yMin) > 1e-9 || std::fabs(b.yMax - prj.box.yMax) > 1e-9
            || std::fabs(s.parm.delta - prj.delta) > 1e-9
            || std::fabs(s.parm.timeMax - prj.timeMax) > 1e-9) {
            r.warnings << QStringLiteral("шаг в строке %1 считает другую область или другой шаг "
                                         "сетки — модель хранит одну коробку на всё задание, "
                                         "взяты параметры первого шага").arg(s.line);
        }
    }
    prj.kernel = anyPlasma ? Kernel::X : Kernel::H;
    prj.description = readOwnDescription(src);

    // --- угловой свип: развернуть повёрнутый объект обратно ---
    //
    // На каждом шаге свипа объект повёрнут на свой угол, и без этого разворота каждый
    // шаг дал бы свою копию фигуры. Какие блоки поворачивались и на какие углы — знает
    // служебная строка в шапке; шаги идут в том же порядке, в каком их пишет эмиттер:
    // опорный (без объекта), затем углы по списку.
    const SweepMarker sweep = readSweepMarker(src);
    bool sweepOk = false;
    if (sweep.valid) {
        const int first = sweep.ref ? 1 : 0;
        if (steps.size() == sweep.angles.size() + first) {
            sweepOk = true;
            for (int si = first; si < steps.size(); ++si) {
                const double a = sweep.angles.at(si - first);
                for (Shape &s : steps[si].shapes) {
                    if (sweep.blocks.contains(s.id))
                        s = unrotateShape(s, sweep.pivot, a);
                }
            }
        } else {
            r.warnings << QStringLiteral("в шапке записан поворот объекта на %1 углов, а шагов в "
                                         "файле %2 — поворот не восстановлен, шаги прочитаны "
                                         "как есть")
                              .arg(sweep.angles.size()).arg(steps.size());
        }
    }
    QVector<int> sweepShapeIds;

    // --- объединяем фигуры и порты: одинаковая геометрия в разных шагах = ОДНА фигура ---
    QHash<QString, int> shapeByKey;
    QHash<QString, int> portByKey;
    QSet<int> usedIds;
    QVector<QVector<int>> stepShapeIds(steps.size());
    QVector<QVector<int>> stepActiveIds(steps.size());

    int freeId = 0;
    const auto reserve = [&](int id) {
        usedIds.insert(id);
        freeId = std::max(freeId, id);
    };

    // Перенумерацию копим и докладываем ОДНОЙ строкой на каждый исходный номер.
    //
    // Иначе задание вроде авторского `TEST.TPL` — там макрос разворачивается в 51 шаг,
    // и в каждом свой блок 20 — давало полсотни одинаковых предупреждений. Пользователю
    // важен факт «блок 20 встретился 51 раз, номера выданы новые», а не список.
    QHash<int, int> renamedShapes;
    QHash<int, int> renamedPorts;

    // Порядок обхода шагов. Обычно — как в файле. Но у свипа первым идёт опорный шаг
    // БЕЗ объекта, и фигуры из него встали бы впереди объекта, перепутав порядок
    // блоков при следующей записи. Поэтому опорный шаг разбираем последним.
    QVector<int> order;
    for (int si = 0; si < steps.size(); ++si)
        order << si;
    if (sweepOk && sweep.ref && order.size() > 1)
        std::rotate(order.begin(), order.begin() + 1, order.end());

    for (int si : std::as_const(order)) {
        // Новая фигура встаёт СРАЗУ ЗА той, что шла перед ней в этом шаге, а не в конец
        // списка. Иначе фигура, выключенная в первом варианте, при чтении уезжала бы в
        // конец — и сохранённое заново задание меняло бы порядок блоков.
        int after = -1; // позиция предыдущей фигуры этого шага в prj.shapes
        for (const Shape &s : steps.at(si).shapes) {
            const QString key = shapeKey(s);
            if (!shapeByKey.contains(key)) {
                Shape copy = s;
                if (usedIds.contains(copy.id)) {
                    ++renamedShapes[copy.id];
                    copy.id = ++freeId;
                }
                reserve(copy.id);
                // Подпись ищем по номеру блока ИЗ ФАЙЛА (s.id): copy.id к этому
                // моменту мог смениться из-за столкновения номеров.
                copy.name = labels.value(s.id);
                if (copy.name.isEmpty())
                    copy.name = QStringLiteral("фигура %1").arg(s.id);
                const int pos = after + 1;
                for (auto it = shapeByKey.begin(); it != shapeByKey.end(); ++it) {
                    if (it.value() >= pos)
                        ++it.value();
                }
                shapeByKey.insert(key, pos);
                prj.shapes.insert(pos, copy);
            }
            after = shapeByKey.value(key);
            // Файл профиля записан только на одном шаге — переносим его имя на фигуру.
            Shape &merged = prj.shapes[shapeByKey.value(key)];
            if (!s.epsProfile.fromFile && merged.epsProfile.file.isEmpty()
                && !s.epsProfile.file.isEmpty())
                merged.epsProfile.file = s.epsProfile.file;
            const int mergedId = merged.id;
            stepShapeIds[si] << mergedId;
            if (sweepOk && sweep.blocks.contains(s.id) && !sweepShapeIds.contains(mergedId))
                sweepShapeIds << mergedId;
        }

        for (const Port &p : steps.at(si).ports) {
            const QString key = portKey(p);
            if (!portByKey.contains(key)) {
                Port copy = p;
                if (usedIds.contains(copy.id)) {
                    ++renamedPorts[copy.id];
                    copy.id = ++freeId;
                }
                reserve(copy.id);
                copy.name = labels.value(p.id);
                if (copy.name.isEmpty())
                    copy.name = QStringLiteral("вход %1").arg(prj.ports.size() + 1);
                portByKey.insert(key, prj.ports.size());
                prj.ports << copy;
            }
            if (p.active)
                stepActiveIds[si] << prj.ports.at(portByKey.value(key)).id;
        }
    }

    // Итог перенумерации: по строке на исходный номер, а не на каждое столкновение.
    {
        QList<int> ids = renamedShapes.keys();
        std::sort(ids.begin(), ids.end());
        for (int id : ids) {
            r.warnings << QStringLiteral("номер блока %1 занят %2 разными фигурами — всем, кроме "
                                         "первой, выданы свободные номера. На расчёт это не "
                                         "влияет: номер блока произвольный, важна лишь "
                                         "уникальность")
                              .arg(id).arg(renamedShapes.value(id) + 1);
        }
        ids = renamedPorts.keys();
        std::sort(ids.begin(), ids.end());
        for (int id : ids) {
            r.warnings << QStringLiteral("номер блока %1 занят и фигурой, и входом (%2 раза) — "
                                         "входу выдан свободный номер")
                              .arg(id).arg(renamedPorts.value(id));
        }
    }

    // --- частоты: различные, в порядке появления ---
    for (const StepModel &s : steps) {
        bool seen = false;
        for (double f : std::as_const(prj.freqs))
            seen |= (std::fabs(f - s.freq) < 1e-9);
        if (!seen)
            prj.freqs << s.freq;
    }

    // --- возбуждения: шаги, отличающиеся активным портом ---
    QVector<QVector<int>> activeSets;
    QVector<int> stepActiveSet(steps.size(), 0);
    for (int si = 0; si < steps.size(); ++si) {
        QVector<int> set = stepActiveIds.at(si);
        std::sort(set.begin(), set.end());
        int idx = activeSets.indexOf(set);
        if (idx < 0) {
            idx = activeSets.size();
            activeSets << set;
        }
        stepActiveSet[si] = idx;
    }

    bool useExciters = false;
    if (activeSets.size() > 1) {
        useExciters = true;
        for (const QVector<int> &set : activeSets)
            useExciters &= (set.size() == 1);
        if (useExciters) {
            for (const QVector<int> &set : activeSets)
                prj.exciters << set.first();
        } else {
            r.warnings << QStringLiteral("шаги отличаются набором активных входов, но не по одному "
                                         "входу на шаг — очередь возбуждений не восстановлена, "
                                         "взят первый шаг");
        }
    }

    // Активность показываем по ПЕРВОМУ шагу, а не по всей очереди возбуждений.
    //
    // Когда в задании два шага и в каждом свой активный вход, порты по очереди
    // возбуждаются, но ОДНОВРЕМЕННО активными не бывают никогда. Пометить активными
    // оба (раз оба входят в очередь) значит показать пользователю картину, которой
    // в задании нет: он увидит два излучающих входа вместо «сначала первый, потом
    // второй». Очередь при этом никуда не девается — она в Project::exciters и
    // показана на странице «Что считать»; эмиттер при непустой очереди этот флаг
    // не смотрит вовсе, так что записи он не меняет.
    for (Port &p : prj.ports)
        p.active = activeSets.value(0).contains(p.id);

    // --- варианты геометрии: шаги, отличающиеся набором фигур ---
    QVector<QVector<int>> shapeSets;
    QVector<int> stepShapeSet(steps.size(), 0);
    for (int si = 0; si < steps.size(); ++si) {
        QVector<int> set = stepShapeIds.at(si);
        std::sort(set.begin(), set.end());
        int idx = shapeSets.indexOf(set);
        if (idx < 0) {
            idx = shapeSets.size();
            shapeSets << set;
        }
        stepShapeSet[si] = idx;
    }

    // Имя файла сигналов первого шага каждого варианта — из него достаём метку.
    //
    // Суффикс «_p1» отрезаем ТОЛЬКО когда возбуждения действительно идут по очереди:
    // тогда его дописал эмиттер, разводя сигналы по портам. При единственном
    // возбуждении такой суффикс — часть авторского имени файла (задание так и
    // называется, wg_p1), и срезать его значит молча переименовать пользователю
    // выходные данные при первом же сохранении.
    const bool suffixIsOurs = prj.exciters.size() > 1;
    QStringList variantCores(shapeSets.size());
    for (int si = steps.size() - 1; si >= 0; --si) {
        // Порядок обратный записи: эмиттер дописывает вариант, потом возбуждение,
        // потом частоту — снимаем с конца.
        const QString file = stripFreqSuffix(steps.at(si).out.file, steps.at(si).freq);
        variantCores[stepShapeSet.at(si)] = suffixIsOurs ? stripExciterSuffix(file) : file;
    }

    QString baseSignal = variantCores.value(0);
    if (sweepOk) {
        // Свип — это не варианты: опорный шаг без объекта и шаги с объектом — одна
        // задача. Имена файлов у шагов свои (`_ref`, `_a30`), общее у них — основа.
        QStringList files;
        for (const StepModel &s : std::as_const(steps))
            files << stripSweepSuffix(stripFreqSuffix(s.out.file, s.freq));
        QString base = commonPrefix(files);
        while (base.endsWith(QLatin1Char('_')))
            base.chop(1);
        if (!base.isEmpty())
            baseSignal = base;

        prj.sweep.enabled = true;
        prj.sweep.shapeIds = sweepShapeIds;
        prj.sweep.pivot = sweep.pivot;
        // Ось в центре объекта — значит и дальше она едет вместе с ним.
        prj.sweep.pivotAtCenter = true;
        const QPointF centre = sweepPivot(prj);
        prj.sweep.pivotAtCenter =
            std::hypot(centre.x() - sweep.pivot.x(), centre.y() - sweep.pivot.y()) < 1e-6;
        prj.sweep.angles = sweep.angles;
        prj.sweep.referenceStep = sweep.ref;
    } else if (shapeSets.size() > 1) {
        QString base = commonPrefix(variantCores);
        while (base.endsWith(QLatin1Char('_')))
            base.chop(1);

        QStringList tags;
        bool goodTags = !base.isEmpty();
        for (const QString &core : std::as_const(variantCores)) {
            QString tag = core.mid(base.size());
            while (tag.startsWith(QLatin1Char('_')))
                tag = tag.mid(1);
            goodTags &= (!tag.isEmpty() && !tags.contains(tag));
            tags << tag;
        }
        if (!goodTags) {
            base = variantCores.value(0);
            tags.clear();
            for (int i = 0; i < shapeSets.size(); ++i)
                tags << QStringLiteral("v%1").arg(i + 1);
            r.warnings << QStringLiteral("шаги считают разные наборы фигур; имена файлов не дают "
                                         "понять, как называются варианты — им выданы метки v1, "
                                         "v2, …");
        }
        baseSignal = base;

        for (int vi = 0; vi < shapeSets.size(); ++vi) {
            Variant v;
            v.tag = tags.at(vi);
            v.name = tags.at(vi);
            // Имя из комментария — если их ровно столько, сколько вариантов: тогда
            // порядок у них общий (эмиттер пишет варианты внешним циклом).
            if (variantTitles.size() == shapeSets.size())
                v.name = variantTitles.at(vi);
            for (const Shape &s : std::as_const(prj.shapes)) {
                if (!shapeSets.at(vi).contains(s.id))
                    v.disabledShapes << s.id;
            }
            prj.variants << v;
        }
    }

    // --- выходные данные ---
    const RawOutput &o0 = steps.first().out;
    prj.output.signalFile = baseSignal;

    int fieldsCount = 0;
    bool anyTopology = false;
    bool anySMatrix = false;
    bool anyPattern = false;
    for (const StepModel &s : steps) {
        fieldsCount += s.out.fields ? 1 : 0;
        anyTopology |= s.out.topology;
        anySMatrix |= s.out.sMatrix;
        anyPattern |= s.out.pattern;
    }
    prj.output.topology = anyTopology;
    if (fieldsCount == 0)
        prj.output.fields = OutputSettings::Fields::Off;
    else if (fieldsCount == steps.size())
        prj.output.fields = OutputSettings::Fields::AllSteps;
    else if (fieldsCount == 1 && steps.first().out.fields)
        prj.output.fields = OutputSettings::Fields::FirstStep;
    else if (sweepOk && fieldsCount == 1 && sweep.ref && steps.size() > 1
             && steps.at(1).out.fields)
        prj.output.fields = OutputSettings::Fields::FirstStep; // первый шаг С ОБЪЕКТОМ
    else if (sweepOk && sweep.ref && fieldsCount == steps.size() - 1 && !steps.first().out.fields)
        prj.output.fields = OutputSettings::Fields::AllSteps;  // опорный шаг полей не пишет
    else {
        prj.output.fields = OutputSettings::Fields::AllSteps;
        r.warnings << QStringLiteral("поля выгружаются на %1 шагах из %2 — модель знает только "
                                     "«нигде», «на первом» и «на всех», выбрано «на всех»")
                          .arg(fieldsCount).arg(steps.size());
    }

    prj.output.sMatrix = anySMatrix;
    if (anySMatrix) {
        QStringList sCores;
        for (const StepModel &s : steps) {
            const QString core = sweepOk ? stripSweepSuffix(s.out.sFile) : s.out.sFile;
            if (s.out.sMatrix && !sCores.contains(core))
                sCores << core;
        }
        if (sCores.size() > 1) {
            QString base = commonPrefix(sCores);
            while (base.endsWith(QLatin1Char('_')))
                base.chop(1);
            prj.output.sMatrixFile = base.isEmpty() ? sCores.first() : base;
        } else {
            prj.output.sMatrixFile = sCores.value(0);
        }
        for (const StepModel &s : steps) {
            if (s.out.sMatrix) {
                prj.output.sMatrixTMin = s.out.sTMin;
                prj.output.sMatrixTMax = s.out.sTMax;
                break;
            }
        }
    }

    prj.output.directionalPattern = anyPattern;
    if (anyPattern) {
        QStringList pCores;
        for (const StepModel &s : steps) {
            if (!s.out.pattern)
                continue;
            const QString c = sweepOk ? stripSweepSuffix(s.out.pFile)
                                      : stripExciterSuffix(s.out.pFile);
            if (!pCores.contains(c))
                pCores << c;
        }
        QString base = commonPrefix(pCores);
        while (base.endsWith(QLatin1Char('_')))
            base.chop(1);
        prj.output.patternFile = base.isEmpty() ? pCores.value(0) : base;
        for (const StepModel &s : steps) {
            if (s.out.pattern) {
                prj.output.patternTMin = s.out.pTMin;
                prj.output.patternTMax = s.out.pTMax;
                break;
            }
        }
    }

    if (prj.output.signalFile.isEmpty())
        r.warnings << QStringLiteral("в #OUTPUT не нашлось строки FILE — имя файла сигналов пусто");

    prj.name = o0.file.isEmpty() ? QStringLiteral("task") : baseSignal;

    // --- сверка: сколько шагов должно получиться при обратной записи ---
    if (prj.stepCount() != steps.size()) {
        r.warnings << QStringLiteral("в файле %1 шагов, а из разобранной модели получится %2 — "
                                     "часть различий между шагами модель не выражает")
                          .arg(steps.size()).arg(prj.stepCount());
    }

    return r;
}

ReadResult readTplFile(const QString &path)
{
    ReadResult r;
    const QFileInfo fi(path);

    QString text;
    QString err;
    if (!readTplFileText(path, &text, &err)) {
        r.errors << err;
        return r;
    }

    r = readTplText(text, fi.absolutePath());
    if (r.ok() || !r.project.name.isEmpty())
        r.project.name = fi.completeBaseName();
    return r;
}

} // namespace tpl
