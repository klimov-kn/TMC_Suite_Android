/// @file sysmemory.cpp
/// @brief Чтение `/proc/meminfo`: Android и настольный Linux дают одни и те же поля.
/// @see https://www.tamic.ru/docs/code/tpl-designer.html#gui-memory
#include "sysmemory.h"

#include <QFile>
#include <QRegularExpression>
#include <QString>

namespace sysmem {

Info query()
{
    Info info;
    QFile f(QStringLiteral("/proc/meminfo"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return info;
    const QString text = QString::fromLatin1(f.readAll());
    const auto kb = [&text](const char *name) {
        const QRegularExpression re(QStringLiteral("(?m)^%1:\\s*(\\d+)\\s*kB").arg(QLatin1String(name)));
        const QRegularExpressionMatch m = re.match(text);
        return m.hasMatch() ? m.captured(1).toDouble() * 1024.0 : -1.0;
    };
    const double total = kb("MemTotal");
    const double avail = kb("MemAvailable");
    if (total <= 0.0 || avail < 0.0)
        return info;
    info.ok = true;
    info.total = total;
    info.available = avail < total ? avail : total;
    return info;
}

} // namespace sysmem
