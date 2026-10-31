/// @file signalfile.cpp
/// @brief signalfile.cpp
/// @see https://www.tamic.ru/docs/platforms/android.html#charts

#include "signalfile.h"

#include "textscan.h"

#include <QCoreApplication>

#include <cfloat>
#include <cmath>
#include <cstring>

namespace viewers {

bool SignalFile::parse(const QByteArray &data)
{
    m_error.clear();
    m_time.clear();
    m_ports.clear();
    m_freq = 1.e+10;   // read_RT_output_signal: *dFr = 1.e+10

    static const QByteArray freqId("#TMC_GROTS_Freq ");   // TMC_GROTS_FLOUTFreq_ID
    const char *const base = data.constData();
    const char *const end = base + data.size();
    const char *line = base;
    while (line < end) {
        const char *eol = static_cast<const char *>(memchr(line, '\n', size_t(end - line)));
        const char *lineEnd = eol ? eol : end;
        if (lineEnd - line >= freqId.size() && std::memcmp(line, freqId.constData(), size_t(freqId.size())) == 0) {
            const char *p = line + freqId.size();
            double f;
            if (scan::readDouble(p, lineEnd, &f))
                m_freq = f;
        } else if (line < lineEnd && *line == ' ') {
            // Строка данных: nT, время, затем тройки «блок Up Uo» по портам.
            const char *p = line;
            int nT = 0;
            double t = 0.0;
            if (scan::readInt(p, lineEnd, &nT) && scan::readDouble(p, lineEnd, &t)) {
                m_time.push_back(t);
                const size_t row = m_time.size() - 1;
                size_t port = 0;
                int block;
                double up, uo;
                while (scan::readInt(p, lineEnd, &block)) {
                    // gets_sss: при нехватке чисел значение не определено — берём 0.
                    if (!scan::readDouble(p, lineEnd, &up)) up = 0.0;
                    if (!scan::readDouble(p, lineEnd, &uo)) uo = 0.0;
                    if (port >= m_ports.size()) {
                        Port np;
                        np.block = block;
                        np.up.assign(row, 0.0);
                        np.uo.assign(row, 0.0);
                        m_ports.push_back(std::move(np));
                    }
                    m_ports[port].up.push_back(up);
                    m_ports[port].uo.push_back(uo);
                    ++port;
                }
                for (; port < m_ports.size(); ++port) {
                    m_ports[port].up.push_back(0.0);
                    m_ports[port].uo.push_back(0.0);
                }
            }
        }
        line = eol ? eol + 1 : end;
    }
    if (m_time.empty() || m_ports.empty()) {
        m_error = QCoreApplication::translate("viewers", "в файле нет строк сигнала");
        return false;
    }
    return true;
}

// Перенос CTMCGROUTView::PrepareDoubleGraph (Tmcrtout/TMCGROUTView.cpp, ветки
// TMC_GROTS_TYPE_AMPL / WSVR / LDB / FULL) для ряда без выражения (pcExpr == NULL).
// Амплитуда — модуль последнего экстремума (r14), с задержкой на точку, как в оригинале.
std::vector<double> SignalFile::transform(const std::vector<double> &s, YType type)
{
    const int n = int(s.size());
    std::vector<double> y(size_t(n), 0.0);
    int ii = 0;
    double r11 = 0, r12 = 0, r13 = 0, r14 = 0, s11;

    auto track = [&](int j) {
        if (j == 2) {
            r11 = s[0];
            r12 = s[1];
            r13 = s[2];
        }
        if (j > 2) {
            r11 = r12;
            r12 = r13;
            r13 = s[size_t(j)];
            if ((r12 - r11) * (r13 - r12) < 0.0) {
                r14 = std::fabs(r12);
                ii = 1;
            } else {
                if ((std::fabs(r11) + std::fabs(r12) + std::fabs(r13)) < 3 * DBL_MIN) {
                    r14 = std::fabs(r12);
                    ii = 1;
                }
            }
            if (ii == 0) r14 = std::fabs(r12);
        }
    };

    for (int j = 0; j < n; ++j) {
        switch (type) {
        case Amplitude:
            y[size_t(j)] = r14;
            track(j);
            break;
        case Vswr:
            s11 = r14;
            if (std::fabs(s11 - 1.) < 0.0001)
                s11 = 0.9999;
            y[size_t(j)] = (1. + s11) / (1. - s11);
            track(j);
            break;
        case LossDb:
            if (r14 < DBL_MIN) y[size_t(j)] = 0.;
            else y[size_t(j)] = -20. * std::log10(r14);
            track(j);
            break;
        case Signal:
        default:
            y[size_t(j)] = s[size_t(j)];
            break;
        }
    }
    return y;
}

} // namespace viewers
