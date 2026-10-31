// Файл распределения поля (.AMP, .FAZ — «#TMC_GraphicsOutputFieldFile»).
//
// Формат — как пишет ядро (src/win_src/libs/TMCIndan/FieldIntegrated.cpp): текстовый
// заголовок из строк «#TMCGROFLD_<имя> <значение>», разделённых CR LF (после nY перевода
// строки нет), затем nY строк: CR LF и nX чисел double (8 байт, little-endian).
// Здесь только чтение (для этапа О4 «Поле» и сводки на экране «Результаты»).
#pragma once

#include <QByteArray>
#include <QString>

#include <vector>

namespace viewers {

class FieldFile
{
public:
    bool parse(const QByteArray &data);
    QString error() const { return m_error; }

    int nx() const { return m_nx; }
    int ny() const { return m_ny; }
    int nT() const { return m_nT; }
    double time() const { return m_time; }
    double delta() const { return m_delta; }
    double xmin() const { return m_xmin; }
    double ymin() const { return m_ymin; }
    QString longUnit() const { return m_longUnit; }
    double longUnitValue() const { return m_longUnitValue; }
    // Значение в узле (ix, iy), строка iy — как в файле.
    double at(int ix, int iy) const { return m_values[size_t(iy) * size_t(m_nx) + size_t(ix)]; }
    const std::vector<double> &values() const { return m_values; }

private:
    QString m_error;
    int m_nx = 0, m_ny = 0, m_nT = 0, m_accuracy = 8;
    double m_time = 0, m_delta = 0, m_xmin = 0, m_ymin = 0, m_longUnitValue = 1;
    QString m_longUnit;
    std::vector<double> m_values;
};

} // namespace viewers
