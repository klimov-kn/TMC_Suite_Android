// Три вида графиков результатов (этап О3):
//   SMatrixChart — элементы S-матрицы по частоте (как TMCGROUT, файл .s);
//   PatternChart — диаграмма направленности и КНД (как TMC_DN, файл .DAT);
//   SignalChart  — сигналы на портах во времени (как TMCROS, файл .t).
// Формулы осей Y — перенос из Windows-вьюверов (PrepareDoubleGraph), см. .cpp.
#pragma once

#include "chartbase.h"

namespace viewers {

class SMatrixChart : public ChartBase
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit SMatrixChart(QQuickItem *parent = nullptr);
    QString kindTitle() const override { return tr("S-матрица"); }
    void retranslate() override;

protected:
    Loader loader() const override;
    void rebuild() override;
    QStringList pointText(const Series &s, size_t i) const override;
};

class PatternChart : public ChartBase
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit PatternChart(QQuickItem *parent = nullptr);
    QString kindTitle() const override { return tr("ДН"); }
    void retranslate() override;
    bool polarAvailable() const override { return true; }

protected:
    Loader loader() const override;
    void rebuild() override;
    double yAutoFloorSpan() const override;
    double polarRadius(double y) const override;
    QString polarRingLabel(double r) const override;
    QStringList pointText(const Series &s, size_t i) const override;
};

class SignalChart : public ChartBase
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit SignalChart(QQuickItem *parent = nullptr);
    QString kindTitle() const override { return tr("Сигналы"); }
    void retranslate() override;

protected:
    Loader loader() const override;
    void rebuild() override;
    QStringList pointText(const Series &s, size_t i) const override;
};

// Помощник для экранов: вид графика по имени файла, сводка файла поля.
class ViewerTools : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    using QObject::QObject;
    Q_INVOKABLE QString chartKind(const QString &fileName) const;
    // Короткая сводка файла поля (.AMP/.FAZ): размер сетки и шаг; пусто — ошибка.
    Q_INVOKABLE QString fieldSummary(const QString &folder, const QString &fileName) const;
};

} // namespace viewers
