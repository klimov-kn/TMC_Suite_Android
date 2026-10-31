/// @file viewers.cpp
/// @brief viewers.cpp
/// @see https://www.tamic.ru/docs/platforms/android.html#charts

#include "viewers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>

#include <atomic>

namespace viewers {

namespace {
QMutex g_mutex;
FileReader g_reader;
std::atomic<bool> g_decimalComma{ true };
}

void setDecimalComma(bool comma)
{
    g_decimalComma = comma;
    emit languageNotifier()->changed();
}

LanguageNotifier *languageNotifier()
{
    static LanguageNotifier *n = new LanguageNotifier;
    return n;
}
bool decimalComma() { return g_decimalComma; }

QString localizeNumber(QString s)
{
    if (g_decimalComma)
        s.replace(QLatin1Char('.'), QLatin1Char(','));
    s.replace(QLatin1Char('-'), QChar(0x2212));
    return s;
}

void setFileReader(FileReader reader)
{
    QMutexLocker lock(&g_mutex);
    g_reader = std::move(reader);
}

FileReader fileReader()
{
    QMutexLocker lock(&g_mutex);
    if (g_reader)
        return g_reader;
    return [](const QString &folder, const QString &name) {
        QFile f(QDir(folder).filePath(name));
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    };
}

QString chartKind(const QString &fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    // Поле: мгновенное (.ex), серия тактов (.ex.frames), распределение (.AMP/.FAZ),
    // топология (.tt) — всё открывает экран «Поле».
    if (ext == QLatin1String("ex") || ext == QLatin1String("tt")
        || fileName.endsWith(QLatin1String(".ex.frames"), Qt::CaseInsensitive))
        return QStringLiteral("field");
    if (ext == QLatin1String("s"))
        return QStringLiteral("smatrix");
    if (ext == QLatin1String("dat"))
        return QStringLiteral("pattern");
    if (ext == QLatin1String("t"))
        return QStringLiteral("signal");
    if (ext == QLatin1String("amp") || ext == QLatin1String("faz"))
        return QStringLiteral("field");
    return {};
}

} // namespace viewers
