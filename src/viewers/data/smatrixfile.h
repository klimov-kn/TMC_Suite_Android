/// @file smatrixfile.h
/// @brief Файл S-матрицы (.s, «SMT 102») — чтение, как в TMCGROUT.
///
/// Формат и порядок чтения — src/win_src/libs/SFILE95/Read_s.cpp (read_S_matrix) и
/// S_FILE.CPP (read_S, skip_string); выбор элемента — S_remt.cpp
/// (read_S_matrix_element_TMC). Двоичные поля little-endian: WORD и INT_1 (= short) — 2 байта,
/// int — 4, double — 8. Файлы HFSS/CST (запасные ветки read_S_matrix_element) — не
/// поддерживаются: их ядро не пишет.
/// @see https://www.tamic.ru/docs/platforms/android.html#charts

#pragma once

#include "tmccomplex.h"

#include <QByteArray>
#include <QString>

#include <vector>

namespace viewers {

class SMatrixFile
{
public:
    struct FreqPoint
    {
        double f = 0.0;               // частота, Гц (как в файле)
        int num = 0;                  // число входов (INPUT_DATA.num)
        std::vector<short> modes;     // число мод на каждом входе (INPUT_DATA.array)
        int dim = 0;                  // размер матрицы (INPUT_DATA.dim)
        std::vector<Cplx> s;          // dim*dim, по строкам
    };

    // Разбор файла целиком. false — ошибка (текст — error()).
    bool parse(const QByteArray &data);
    QString error() const { return m_error; }

    const std::vector<FreqPoint> &points() const { return m_points; }
    int inputs() const { return m_points.empty() ? 0 : m_points.front().num; }

    // Перенос read_S_matrix_element_TMC (S_remt.cpp): значения элемента
    // S(in1,mod1; in2,mod2) по частотам. Номера входов и мод — с 1, приводятся к
    // допустимым так же, как в оригинале (in1/in2 — не больше числа входов, моды — не
    // меньше 1). Частоты, где у входа меньше мод, пропускаются. false — нет точек.
    bool element(int &in1, int &mod1, int &in2, int &mod2,
                 std::vector<double> *freq, std::vector<Cplx> *values) const;

private:
    QString m_error;
    std::vector<FreqPoint> m_points;
};

} // namespace viewers
