// Language — язык интерфейса. См. language.h.

#include "language.h"

#include "platform.h"
#include "viewers.h"

#include <QCoreApplication>
#include <QDir>
#include <QJSEngine>
#include <QLocale>
#include <QQmlEngine>
#include <QSettings>
#include <QTranslator>

namespace {

const char *const kKey = "ui/language";

QSettings &settings()
{
    static QSettings s(QDir(Platform::dataDir()).filePath(QStringLiteral("settings.ini")),
                       QSettings::IniFormat);
    return s;
}

} // namespace

Language *Language::instance()
{
    static Language *s = new Language;
    return s;
}

Language *Language::create(QQmlEngine *, QJSEngine *)
{
    Language *s = instance();
    QJSEngine::setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

Language::Language() = default;
Language::~Language() = default;

QString Language::systemCode()
{
    return QLocale::system().language() == QLocale::Russian ? QStringLiteral("ru") : QStringLiteral("en");
}

void Language::init(const QString &forced)
{
    if (forced == QLatin1String("ru") || forced == QLatin1String("en")) {
        apply(forced);
        return;
    }
    QString code = settings().value(QLatin1String(kKey)).toString();
    if (code != QLatin1String("ru") && code != QLatin1String("en")) {
        // Первый запуск: по языку системы, и сразу запомнить.
        code = systemCode();
        settings().setValue(QLatin1String(kKey), code);
        settings().sync();
    }
    apply(code);
    Platform::log(QStringLiteral("language=%1 system=%2").arg(code, QLocale::system().name()));
}

void Language::setCode(const QString &code)
{
    const QString c = code == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("ru");
    settings().setValue(QLatin1String(kKey), c);
    settings().sync();
    if (c == m_code)
        return;
    apply(c);
    if (m_engine)
        m_engine->retranslate();
}

void Language::apply(const QString &code)
{
    if (m_translator) {
        QCoreApplication::removeTranslator(m_translator.get());
        m_translator.reset();
    }
    if (code == QLatin1String("en")) {
        auto t = std::make_unique<QTranslator>();
        if (t->load(QStringLiteral(":/i18n/tamic_en.qm"))) {
            QCoreApplication::installTranslator(t.get());
            m_translator = std::move(t);
        } else {
            Platform::log(QStringLiteral("language: tamic_en.qm not found"));
        }
    }
    m_code = code;
    QLocale::setDefault(QLocale(code == QLatin1String("ru") ? QLocale::Russian : QLocale::English));
    viewers::setDecimalComma(code == QLatin1String("ru"));
    Platform::setLanguage(code);
    emit changed();
}

QString Language::number(double value, int decimals) const
{
    QString s = QString::number(value, 'f', decimals);
    s.replace(QLatin1Char('-'), QChar(0x2212));
    return dec(s);
}

QString Language::dec(const QString &numberText) const
{
    QString s = numberText;
    if (m_code == QLatin1String("ru"))
        s.replace(QLatin1Char('.'), QLatin1Char(','));
    return s;
}
