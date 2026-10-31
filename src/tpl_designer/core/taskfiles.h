#pragma once

#include "emitter.h" // TaskChunk: конвейер и запись задания — одна и та же работа
#include "project.h"

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

namespace tpl {

/// Итог записи задания на диск.
struct WriteResult {
    bool ok = false;
    QStringList written; ///< имена записанных файлов, первым — .tpl
    QString error;
    bool lossy = false;  ///< часть символов не легла в CP1251 и заменена на '?'
};

/// Записать задание в папку ровно так, как его ждёт ядро: `.tpl` (и `_step.h` у
/// макро-версии) в CP1251 рядом друг с другом, плюс сопутствующие файлы
/// (готовые распределения `.eps`) байт в байт.
///
/// Одно место для мастера, библиотеки образцов и «Моих образцов»: иначе рано или
/// поздно один из путей забыл бы положить рядом `_step.h` или файл распределения.
WriteResult writeTask(const QString &dir, const Project &p,
                      const QMap<QString, QByteArray> &companions = {});

/// Какие файлы появятся в папке: для вопроса «перезаписать?» до записи.
QStringList taskFileNames(const Project &p, const QMap<QString, QByteArray> &companions = {});

/// Строка запуска ядра для подсказки человеку. Ключ `/Time` добавляется, если
/// пользователь попросил замер времени.
///
/// ⚠️ Имя исполняемого файла зависит от системы, поэтому строку составляет GUI
/// (`kernelnames.h`), а здесь — только хвост из ключей: он одинаков везде.
QString runFlags(const Project &p);

/// Пакетный файл, прогоняющий все задания конвейера по очереди.
///
/// Пишется рядом с заданиями, когда их больше одного. Два файла сразу — `.bat` для
/// Windows и `.sh` для Linux/macOS: папку с заданиями носят между машинами, и
/// угадывать, где её будут считать, незачем.
QString runBat(const Project &p, const QVector<TaskChunk> &chunks);
QString runSh(const Project &p, const QVector<TaskChunk> &chunks);

} // namespace tpl
