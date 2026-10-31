/// @file appinfo.cpp
/// @brief AppInfo — сведения для «Справки». См. appinfo.h.
/// @see https://www.tamic.ru/docs/platforms/android.html#app

#include "appinfo.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtGlobal>

namespace {

QString readText(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

} // namespace

QString AppInfo::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString AppInfo::fontLicenses() const
{
    return QStringLiteral("Inter\n\n") + readText(QStringLiteral(":/fonts/Inter-OFL.txt"))
           + QStringLiteral("\n\nJetBrains Mono\n\n") + readText(QStringLiteral(":/fonts/JetBrainsMono-OFL.txt"));
}

QString AppInfo::license() const
{
    QString text = readText(QStringLiteral(":/legal/LICENSE"));
    if (text.startsWith(QChar(0xFEFF)))   // метка UTF-8 в начале файла
        text.remove(0, 1);
    return text;
}

QVariantList AppInfo::manual(const QString &lang) const
{
    QFile f(QStringLiteral(":/manual/%1.json").arg(lang == QLatin1String("en") ? QStringLiteral("en")
                                                                              : QStringLiteral("ru")));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).array().toVariantList();
}
