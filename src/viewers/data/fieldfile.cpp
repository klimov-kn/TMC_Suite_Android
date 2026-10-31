#include "fieldfile.h"

#include "textscan.h"

#include <QCoreApplication>
#include <QtEndian>

#include <cstring>

namespace viewers {

bool FieldFile::parse(const QByteArray &data)
{
    m_error.clear();
    m_values.clear();
    static const QByteArray id("#TMC_GraphicsOutputFieldFile");
    if (!data.startsWith(id)) {
        m_error = QCoreApplication::translate("viewers", "не файл поля (нет #TMC_GraphicsOutputFieldFile)");
        return false;
    }
    // Заголовок: строки до «#TMCGROFLD_nY <число>» включительно.
    int pos = 0;
    bool haveNy = false;
    while (pos < data.size() && !haveNy) {
        int eol = data.indexOf('\n', pos);
        const int nyAt = data.indexOf("#TMCGROFLD_nY ", pos);
        if (nyAt == pos) {
            // После nY перевода строки нет: число, затем CR LF первой строки данных.
            const char *p = data.constData() + pos + 14;
            const char *end = data.constData() + data.size();
            if (!scan::readInt(p, end, &m_ny)) {
                m_error = QCoreApplication::translate("viewers", "не читается nY");
                return false;
            }
            pos = int(p - data.constData());
            haveNy = true;
            break;
        }
        if (eol < 0)
            break;
        const QByteArray line = data.mid(pos, eol - pos).trimmed();
        const char *p = line.constData();
        const char *end = p + line.size();
        auto field = [&](const char *name) {
            const size_t n = std::strlen(name);
            if (size_t(line.size()) > n && std::memcmp(line.constData(), name, n) == 0) {
                p = line.constData() + n;
                return true;
            }
            return false;
        };
        if (field("#TMCGROFLD_nT ")) scan::readInt(p, end, &m_nT);
        else if (field("#TMCGROFLD_Time ")) scan::readDouble(p, end, &m_time);
        else if (field("#TMCGROFLD_Delta ")) scan::readDouble(p, end, &m_delta);
        else if (field("#TMCGROFLD_Xmin ")) scan::readDouble(p, end, &m_xmin);
        else if (field("#TMCGROFLD_Ymin ")) scan::readDouble(p, end, &m_ymin);
        else if (field("#TMCGROFLD_Accuracy ")) scan::readInt(p, end, &m_accuracy);
        else if (field("#TMCGROFLD_nX ")) scan::readInt(p, end, &m_nx);
        else if (field("#TMCGROFLD_LongUnit ")) {
            QByteArray unit;
            if (scan::readWord(p, end, &unit))
                m_longUnit = QString::fromLatin1(unit);
            scan::readDouble(p, end, &m_longUnitValue);
        }
        pos = eol + 1;
    }
    if (!haveNy || m_nx <= 0 || m_ny <= 0) {
        m_error = QCoreApplication::translate("viewers", "в заголовке нет nX/nY");
        return false;
    }
    if (m_accuracy != 8) {
        m_error = QCoreApplication::translate("viewers", "точность %1 байт не поддерживается").arg(m_accuracy);
        return false;
    }
    const qint64 need = qint64(m_ny) * (2 + qint64(m_nx) * 8);
    if (data.size() - pos < need) {
        m_error = QCoreApplication::translate("viewers", "файл обрезан: нужно %1 байт данных, есть %2").arg(need).arg(data.size() - pos);
        return false;
    }
    m_values.resize(size_t(m_nx) * size_t(m_ny));
    const uchar *p = reinterpret_cast<const uchar *>(data.constData()) + pos;
    for (int iy = 0; iy < m_ny; ++iy) {
        p += 2;   // CR LF перед строкой
        for (int ix = 0; ix < m_nx; ++ix, p += 8) {
            const quint64 u = qFromLittleEndian<quint64>(p);
            double v;
            std::memcpy(&v, &u, 8);
            m_values[size_t(iy) * size_t(m_nx) + size_t(ix)] = v;
        }
    }
    return true;
}

} // namespace viewers
