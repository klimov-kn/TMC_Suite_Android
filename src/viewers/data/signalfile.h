/// @file signalfile.h
/// @brief Файл сигналов (.t, «#TMC_GraphicsOutputSignalFile») — чтение, как в TMCROS.
///
/// Источник — src/win_src/viewers/Tmcrtout/TMCGROUTDoc.cpp (read_RT_output_signal,
/// gets_f1, gets_sss, skip_ntdt_itd): строки данных начинаются с пробела; в строке —
/// номер такта, время (с), затем для каждого порта тройка «блок Up Uo». Частота —
/// строка «#TMC_GROTS_Freq» (по умолчанию 1e10, как в оригинале).
/// Обработка для графика (амплитуда по экстремумам, КСВН, L дБ) — перенос
/// CTMCGROUTView::PrepareDoubleGraph (Tmcrtout/TMCGROUTView.cpp) без изменений.
/// @see https://www.tamic.ru/docs/platforms/android.html#charts

#pragma once

#include <QByteArray>
#include <QString>

#include <vector>

namespace viewers {

class SignalFile
{
public:
    struct Port
    {
        int block = 0;                // номер блока порта (из первой строки данных)
        std::vector<double> up;       // Up (mod1 = 0 в TMCROS)
        std::vector<double> uo;       // Uo (mod1 = 1)
    };

    bool parse(const QByteArray &data);
    QString error() const { return m_error; }

    double freq() const { return m_freq; }                 // #TMC_GROTS_Freq, Гц
    const std::vector<double> &time() const { return m_time; }   // с
    const std::vector<Port> &ports() const { return m_ports; }

    // Виды оси Y — TMC_GROTS_TYPE_* (Include/Tmcgrviw.h).
    enum YType { Signal = 0, Amplitude = 1, Vswr = 4, LossDb = 5 };
    // PrepareDoubleGraph для одного ряда: значения по тактам.
    static std::vector<double> transform(const std::vector<double> &s, YType type);

private:
    QString m_error;
    double m_freq = 1.e+10;
    std::vector<double> m_time;
    std::vector<Port> m_ports;
};

} // namespace viewers
