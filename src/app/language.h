// Language (в QML — Lang) — язык интерфейса: русский или английский (решение M28).
//
// Первый запуск: язык системы русский — русский, любой другой — английский; выбор
// сразу запоминается. Дальше язык меняет только пользователь (переключатель в
// «О программе»), и его выбор хранится всегда — язык системы больше не смотрится.
//
// Тексты в коде — по-русски (qsTr / tr), английский — перевод Qt Linguist
// (i18n/tamic_en.ts → tamic_en.qm в ресурсах). Смена языка на ходу — retranslate()
// движка QML, экраны не пересоздаются.
#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QJSEngine;
class QQmlEngine;
class QTranslator;

class Language : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Lang)
    QML_SINGLETON

    // "ru" или "en".
    Q_PROPERTY(QString code READ code NOTIFY changed)
    // Десятичный знак для чисел на экране: «,» по-русски, «.» по-английски.
    Q_PROPERTY(QString decimal READ decimal NOTIFY changed)

public:
    static Language *instance();
    static Language *create(QQmlEngine *, QJSEngine *);

    // Выбрать язык при запуске (до загрузки QML): сохранённый или по системе.
    // forced ("ru"/"en") — только на этот запуск, без записи (снимки руководства).
    void init(const QString &forced = QString());
    // Движок, которому сказать retranslate() при смене языка.
    void setEngine(QQmlEngine *engine) { m_engine = engine; }

    QString code() const { return m_code; }
    QString decimal() const { return m_code == QLatin1String("ru") ? QStringLiteral(",") : QStringLiteral("."); }

    // Выбор пользователя: запоминается и действует до следующей смены.
    Q_INVOKABLE void setCode(const QString &code);
    // Число строкой с десятичным знаком языка и знаком «−» (как на графиках).
    Q_INVOKABLE QString number(double value, int decimals) const;
    // Заменить «.» на десятичный знак языка в готовой строке числа.
    Q_INVOKABLE QString dec(const QString &numberText) const;

    // Язык системы: "ru" для русского, иначе "en".
    static QString systemCode();

signals:
    void changed();

private:
    Language();
    ~Language() override;
    void apply(const QString &code);

    QString m_code = QStringLiteral("ru");
    std::unique_ptr<QTranslator> m_translator;
    QQmlEngine *m_engine = nullptr;
};
