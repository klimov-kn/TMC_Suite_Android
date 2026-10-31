/// @file textscan.h
/// @brief Разбор текстовых строк заголовков так же, как в Windows-версии (fgets + sscanf),
///
/// но без зависимости от локали: числа читаются как в локали "C".
/// @see https://www.tamic.ru/docs/platforms/android.html#field

#pragma once

#include <QByteArray>
#include <QString>

namespace fieldview {

/// fgets по буферу: строка до '\n' включительно, не длиннее TMC_GROUT_MAXSTRING_BUF-1.
class LineReader
{
public:
    LineReader(const char *data, qint64 size) : m_data(data), m_size(size) {}
    bool next(QByteArray *line)
    {
        if (m_pos >= m_size)
            return false;
        constexpr qint64 kMax = 10000 - 1;   // TMC_GROUT_MAXSTRING_BUF - 1
        qint64 end = m_pos;
        while (end < m_size && end - m_pos < kMax) {
            if (m_data[end++] == '\n')
                break;
        }
        *line = QByteArray(m_data + m_pos, int(end - m_pos));
        m_pos = end;
        return true;
    }
    qint64 position() const { return m_pos; }

private:
    const char *m_data;
    qint64 m_size;
    qint64 m_pos = 0;
};

inline bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

/// Число в начале строки с позиции from, как sscanf("%lg"): при неудаче — fallback
/// (sscanf в этом случае переменную не меняет). *next — позиция после числа.
inline double scanDouble(const QByteArray &s, int from, double fallback, int *next = nullptr)
{
    int i = from;
    while (i < s.size() && isSpace(s[i]))
        ++i;
    const int start = i;
    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
        ++i;
    bool digits = false;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        ++i;
        digits = true;
    }
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            ++i;
            digits = true;
        }
    }
    if (digits && i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        int k = i + 1;
        if (k < s.size() && (s[k] == '+' || s[k] == '-'))
            ++k;
        if (k < s.size() && s[k] >= '0' && s[k] <= '9') {
            while (k < s.size() && s[k] >= '0' && s[k] <= '9')
                ++k;
            i = k;
        }
    }
    if (next)
        *next = i;
    if (!digits) {
        // inf / nan, как у strtod
        const QByteArray w = s.mid(start, 4).toLower();
        bool ok = false;
        if (w.startsWith("inf") || w.startsWith("-inf") || w.startsWith("nan")) {
            const double v = s.mid(start).trimmed().toDouble(&ok);
            if (ok)
                return v;
        }
        return fallback;
    }
    bool ok = false;
    const double v = s.mid(start, i - start).toDouble(&ok);
    return ok ? v : fallback;
}

/// Целое как sscanf("%d").
inline int scanInt(const QByteArray &s, int from, int fallback)
{
    int i = from;
    while (i < s.size() && isSpace(s[i]))
        ++i;
    const int start = i;
    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
        ++i;
    const int digitsFrom = i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9')
        ++i;
    if (i == digitsFrom)
        return fallback;
    bool ok = false;
    const int v = s.mid(start, i - start).toInt(&ok);
    return ok ? v : fallback;
}

/// "%s %lg": слово до пробела и число за ним.
inline void scanWordDouble(const QByteArray &s, int from, QString *word, double *value)
{
    int i = from;
    while (i < s.size() && isSpace(s[i]))
        ++i;
    const int start = i;
    while (i < s.size() && !isSpace(s[i]))
        ++i;
    if (i > start)
        *word = QString::fromLatin1(s.mid(start, i - start));
    *value = scanDouble(s, i, *value);
}

} // namespace fieldview
