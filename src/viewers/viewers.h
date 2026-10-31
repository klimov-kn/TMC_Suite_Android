/// @file viewers.h
/// @brief Модуль просмотра результатов: S-матрица, ДН, сигналы.
///
/// Чтение файлов — data/ (форматы и обработка — перенос из Windows-вьюверов без изменений
/// математики), рисование — chart/ (QQuickPaintedItem + QPainter, оформление — src/ui/theme.h).
/// QML-модуль Tamic.Viewers: SMatrixChart, PatternChart, SignalChart.
/// @see https://www.tamic.ru/docs/platforms/android.html#charts

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <functional>

namespace viewers {

// Чтение файла задачи: папка (путь или content:// SAF) и имя файла в ней. Оболочка
// подставляет TaskStore::read (main.cpp); по умолчанию — обычный файл с диска.
// Вызывается из рабочего потока.
using FileReader = std::function<QByteArray(const QString &folder, const QString &name)>;
void setFileReader(FileReader reader);
FileReader fileReader();

// Десятичный знак в подписях графиков: запятая (русский интерфейс) или точка
// (английский). Задаёт оболочка при выборе языка (Language); заодно
// графики перестраивают подписи (сигнал languageNotifier()->changed).
void setDecimalComma(bool comma);
bool decimalComma();

class LanguageNotifier : public QObject
{
    Q_OBJECT
signals:
    void changed();
};
LanguageNotifier *languageNotifier();
// Строка числа: «.» -> десятичный знак языка, «-» -> «−».
QString localizeNumber(QString s);

// Вид графика по имени файла: "smatrix" (.s), "pattern" (.DAT), "signal" (.t),
// "field" (.ex, .ex.frames, .AMP, .FAZ, .tt — поле,), пусто — не график.
QString chartKind(const QString &fileName);

} // namespace viewers
