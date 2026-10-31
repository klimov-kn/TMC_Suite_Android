#pragma once

#include "sample.h"
#include "taskfiles.h"

#include <QDateTime>
#include <QString>
#include <QVector>

/// «Мои образцы»: задания пользователя, положенные в библиотеку.
///
/// Каждый образец — своя папка в `Документы/TPL Designer/Мои образцы/`: там лежит
/// задание ровно так, как его запишет программа (`.tpl`, `_step.h`, `.eps`), и
/// `sample.json` с названием и описанием. Задание — обычный файл: его можно
/// открыть ядром напрямую, переслать коллеге, положить в чужую папку «Мои образцы».
namespace tpl::samples {

struct UserSample {
    QString dir;      ///< папка образца
    QString tplPath;  ///< задание внутри неё
    QString title;
    QString summary;
    Kernel kernel = Kernel::H;
    QDateTime created;
};

/// Корень «Моих образцов». Переменная окружения TPLD_USER_SAMPLES подменяет его —
/// для проверок, чтобы не трогать настоящие Документы.
QString userSamplesRoot();

/// Все образцы из корня, новые сверху.
QVector<UserSample> userSamples();

/// Положить задание в «Мои образцы». \p dirOut — куда легло.
WriteResult saveUserSample(const QString &title, const QString &summary, const Project &p,
                           const QMap<QString, QByteArray> &companions, QString *dirOut = nullptr);

/// Прочитать образец: задание, его сопутствующие файлы и паспорт.
Build loadUserSample(const UserSample &s);

/// Удалить образец вместе с папкой.
bool removeUserSample(const UserSample &s);

} // namespace tpl::samples
