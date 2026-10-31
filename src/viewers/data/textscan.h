// Разбор чисел в текстовых файлах результатов без зависимости от локали.
//
// Windows-вьюверы читают текст через sscanf("%d", "%lg"). Здесь — то же поведение
// (пропуск пробелов, затем самое длинное число), но всегда с точкой: sscanf зависит от
// LC_NUMERIC, а на телефоне и в iOS локаль может быть русской (запятая). Перевод строки
// в double — QByteArray::toDouble (правильное округление, как у strtod MSVC/glibc).
#pragma once

#include <QByteArray>
#include <cstdlib>

namespace viewers::scan {

inline bool isBlank(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

inline const char *skipBlanks(const char *p, const char *end)
{
    while (p < end && isBlank(*p))
        ++p;
    return p;
}

// Как "%lg": пропустить пробелы, прочитать число. false — числа нет (p не меняется).
inline bool readDouble(const char *&p, const char *end, double *out)
{
    const char *s = skipBlanks(p, end);
    const char *q = s;
    if (q < end && (*q == '+' || *q == '-'))
        ++q;
    const char *digits = q;
    while (q < end && *q >= '0' && *q <= '9')
        ++q;
    if (q < end && *q == '.') {
        ++q;
        while (q < end && *q >= '0' && *q <= '9')
            ++q;
    }
    if (q == digits || (q == digits + 1 && *digits == '.'))
        return false;
    if (q < end && (*q == 'e' || *q == 'E')) {
        const char *e = q + 1;
        if (e < end && (*e == '+' || *e == '-'))
            ++e;
        if (e < end && *e >= '0' && *e <= '9') {
            while (e < end && *e >= '0' && *e <= '9')
                ++e;
            q = e;
        }
    }
    bool ok = false;
    const double v = QByteArray(s, int(q - s)).toDouble(&ok);
    if (!ok)
        return false;
    *out = v;
    p = q;
    return true;
}

// Как "%d".
inline bool readInt(const char *&p, const char *end, int *out)
{
    const char *s = skipBlanks(p, end);
    const char *q = s;
    if (q < end && (*q == '+' || *q == '-'))
        ++q;
    const char *digits = q;
    while (q < end && *q >= '0' && *q <= '9')
        ++q;
    if (q == digits)
        return false;
    *out = int(std::strtol(QByteArray(s, int(q - s)).constData(), nullptr, 10));
    p = q;
    return true;
}

// Как литерал в формате sscanf: пробел в образце — любое число пробелов (в том числе
// ни одного), прочие символы — точно. false — не совпало.
inline bool expect(const char *&p, const char *end, const char *literal)
{
    const char *q = p;
    for (const char *l = literal; *l; ++l) {
        if (isBlank(*l)) {
            q = skipBlanks(q, end);
            continue;
        }
        if (q >= end || *q != *l)
            return false;
        ++q;
    }
    p = q;
    return true;
}

// Как "%s": слово до пробела.
inline bool readWord(const char *&p, const char *end, QByteArray *out)
{
    const char *s = skipBlanks(p, end);
    const char *q = s;
    while (q < end && !isBlank(*q))
        ++q;
    if (q == s)
        return false;
    *out = QByteArray(s, int(q - s));
    p = q;
    return true;
}

} // namespace viewers::scan
