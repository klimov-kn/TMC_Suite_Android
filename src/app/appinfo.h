/// @file appinfo.h
/// @brief AppInfo — сведения для «Справки»: версия Qt, тексты лицензий шрифтов, руководство
///
/// пользователя на языке интерфейса (ресурсы :/manual/<язык>.json, снимки экрана —
/// :/manual/<язык>/*.png; источник — src/app/manual).
/// @see https://www.tamic.ru/docs/platforms/android.html#app

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class AppInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString fontLicenses READ fontLicenses CONSTANT)

public:
    using QObject::QObject;

    QString qtVersion() const;
    QString fontLicenses() const;

    // Разделы руководства на языке lang ("ru" / "en"):
    // [{ id, title, blocks: [{ type: "p"|"h"|"img"|"list"|"note", text, src, items }] }].
    Q_INVOKABLE QVariantList manual(const QString &lang) const;

    // Лицензия TMC Suite без изменений (файл LICENSE в корне проекта, ресурс :/legal/LICENSE).
    Q_INVOKABLE QString license() const;
};
