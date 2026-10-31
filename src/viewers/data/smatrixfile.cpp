/// @file smatrixfile.cpp
/// @brief smatrixfile.cpp
/// @see https://www.tamic.ru/docs/platforms/android.html#charts

#include "smatrixfile.h"

#include <QCoreApplication>
#include <QtEndian>

#include <cstdlib>
#include <cstring>

namespace viewers {

namespace {

// Последовательное чтение, как read_S/fseek по FILE* в SFILE95.
class Reader
{
public:
    explicit Reader(const QByteArray &d) : m_d(d) {}
    qint64 pos() const { return m_pos; }
    bool seek(qint64 p)
    {
        if (p < 0 || p > m_d.size())
            return false;
        m_pos = p;
        return true;
    }
    bool skip(qint64 n) { return seek(m_pos + n); }
    bool bytes(void *out, qint64 n)
    {
        if (n < 0 || m_pos + n > m_d.size())
            return false;
        std::memcpy(out, m_d.constData() + m_pos, size_t(n));
        m_pos += n;
        return true;
    }
    bool i16(short *v)
    {
        uchar b[2];
        if (!bytes(b, 2))
            return false;
        *v = qFromLittleEndian<qint16>(b);
        return true;
    }
    bool u16(quint16 *v)
    {
        uchar b[2];
        if (!bytes(b, 2))
            return false;
        *v = qFromLittleEndian<quint16>(b);
        return true;
    }
    bool i32(int *v)
    {
        uchar b[4];
        if (!bytes(b, 4))
            return false;
        *v = qFromLittleEndian<qint32>(b);
        return true;
    }
    bool f64(double *v)
    {
        uchar b[8];
        if (!bytes(b, 8))
            return false;
        const quint64 u = qFromLittleEndian<quint64>(b);
        std::memcpy(v, &u, 8);
        return true;
    }
    // read_CR: разделитель — одно INT_1 (2 байта).
    bool cr() { short s; return i16(&s); }
    // skip_string: длина INT_1, затем строка и разделитель.
    bool skipString()
    {
        short len;
        return i16(&len) && skip(qint64(len) + 2);
    }
    bool readString(QByteArray *out)
    {
        short len;
        if (!i16(&len) || len < 0)
            return false;
        out->resize(len);
        return bytes(out->data(), len) && cr();
    }

private:
    const QByteArray &m_d;
    qint64 m_pos = 0;
};

} // namespace

bool SMatrixFile::parse(const QByteArray &data)
{
    m_points.clear();
    m_error.clear();
    Reader r(data);

    // --- Read_TITLE ---------------------------------------------------------
    char label[4];
    if (!r.bytes(label, 4) || std::strncmp(label, "SMT", 3) != 0) {
        m_error = QCoreApplication::translate("viewers", "не файл S-матрицы (нет метки SMT)");
        return false;
    }
    char ver[4];
    if (!r.bytes(ver, 4)) {
        m_error = QCoreApplication::translate("viewers", "файл обрезан (версия)");
        return false;
    }
    ver[3] = '\0';
    if (std::atoi(ver) < 102) {   // SMT_VERSION (Include/S_file.h)
        m_error = QCoreApplication::translate("viewers", "старая версия файла S-матрицы");
        return false;
    }
    short psNum = 0;
    if (!r.cr() || !r.i16(&psNum) || !r.cr() || !r.skipString() || !r.skipString()) {
        m_error = QCoreApplication::translate("viewers", "файл обрезан (заголовок)");
        return false;
    }

    // --- Read_GRID ------------------------------------------------------------
    short gridLen = 0, extrapol = 0;
    if (!r.i16(&gridLen) || !r.i16(&extrapol) || extrapol < 0) {
        m_error = QCoreApplication::translate("viewers", "файл обрезан (сетки)");
        return false;
    }
    for (int i = 0; i <= extrapol; ++i) {
        double d;
        if (!r.f64(&d) || !r.f64(&d) || !r.f64(&d)) {
            m_error = QCoreApplication::translate("viewers", "файл обрезан (шаги сеток)");
            return false;
        }
    }
    if (!r.cr()) {
        m_error = QCoreApplication::translate("viewers", "файл обрезан (сетки)");
        return false;
    }
    for (int i = 0; i <= extrapol; ++i) {
        QByteArray tpl, sname;
        if (!r.readString(&tpl) || !r.readString(&sname)) {
            m_error = QCoreApplication::translate("viewers", "файл обрезан (имена файлов)");
            return false;
        }
    }

    // --- Таблица частот -----------------------------------------------------
    quint16 tableLen = 0, nFreq = 0;
    if (!r.u16(&tableLen) || !r.u16(&nFreq)) {
        m_error = QCoreApplication::translate("viewers", "файл обрезан (число частот)");
        return false;
    }
    qint64 next = r.pos();
    m_points.reserve(nFreq);
    for (int i = 0; i < int(nFreq); ++i) {
        FreqPoint p;
        int offset = 0;
        if (!r.seek(next) || !r.f64(&p.f) || !r.i32(&offset)) {
            m_error = QCoreApplication::translate("viewers", "файл обрезан (таблица частот, точка %1)").arg(i + 1);
            return false;
        }
        next = r.pos();
        short v = 0;
        if (!r.seek(qint64(offset) + 8) || !r.i16(&v)) {
            m_error = QCoreApplication::translate("viewers", "неверное смещение частоты %1").arg(i + 1);
            return false;
        }
        p.num = v;
        if (p.num < 0 || p.num > 1000) {
            m_error = QCoreApplication::translate("viewers", "неверное число входов на частоте %1").arg(i + 1);
            return false;
        }
        p.modes.resize(size_t(p.num));
        for (int m = 0; m < p.num; ++m) {
            if (!r.i16(&p.modes[size_t(m)])) {
                m_error = QCoreApplication::translate("viewers", "файл обрезан (моды)");
                return false;
            }
        }
        if (!r.i16(&v) || !r.cr()) {
            m_error = QCoreApplication::translate("viewers", "файл обрезан (размер матрицы)");
            return false;
        }
        p.dim = v;
        if (p.dim < 0) {
            m_error = QCoreApplication::translate("viewers", "неверный размер матрицы");
            return false;
        }
        const qint64 len = qint64(p.dim) * p.dim;
        p.s.resize(size_t(len));
        for (qint64 k = 0; k < len; ++k) {
            if (!r.f64(&p.s[size_t(k)].x) || !r.f64(&p.s[size_t(k)].y)) {
                m_error = QCoreApplication::translate("viewers", "файл обрезан (матрица на частоте %1)").arg(i + 1);
                return false;
            }
        }
        r.cr();   // разделитель в конце точки (в последней может отсутствовать)
        m_points.push_back(std::move(p));
    }
    if (m_points.empty()) {
        m_error = QCoreApplication::translate("viewers", "в файле нет частот");
        return false;
    }
    return true;
}

bool SMatrixFile::element(int &in1, int &mod1, int &in2, int &mod2,
                          std::vector<double> *freq, std::vector<Cplx> *values) const
{
    freq->clear();
    values->clear();
    if (m_points.empty())
        return false;
    // S_remt.cpp: приведение номеров (по данным первой частоты, id[0].num).
    if (in1 < 1) in1 = 1;
    if (in2 < 1) in2 = 1;
    if (mod1 < 1) mod1 = 1;
    if (mod2 < 1) mod2 = 1;
    if (in1 > m_points[0].num) in1 = m_points[0].num;
    if (in2 > m_points[0].num) in2 = m_points[0].num;
    if (in1 < 1 || in2 < 1)
        return false;

    for (const FreqPoint &p : m_points) {
        if (in1 > p.num || in2 > p.num)
            continue;
        if (!((mod1 <= p.modes[size_t(in1 - 1)]) && (mod2 <= p.modes[size_t(in2 - 1)])))
            continue;
        // Тот же обход, что в оригинале: вход, мода, вход, мода — счётчик ii.
        Cplx value;
        size_t ii = 0;
        for (int i = 0; i < p.num; ++i) {
            for (int j = 0, kol1 = p.modes[size_t(i)]; j < kol1; ++j) {
                for (int i1 = 0; i1 < p.num; ++i1) {
                    for (int j1 = 0, kol = p.modes[size_t(i1)]; j1 < kol; ++j1) {
                        if ((i == (in1 - 1)) && (j == (mod1 - 1)) && (i1 == (in2 - 1)) && (j1 == (mod2 - 1))
                            && ii < p.s.size())
                            value = p.s[ii];
                        ++ii;
                    }
                }
            }
        }
        freq->push_back(p.f);
        values->push_back(value);
    }
    return !freq->empty();
}

} // namespace viewers
