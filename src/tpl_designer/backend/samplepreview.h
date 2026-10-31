/// @file samplepreview.h
/// @brief Картинка образца: задание, нарисованное тем же холстом, что и в редакторе.
///
/// На настольной версии картинки образцов рисует `GeometryView::render`. Здесь рисует
/// `GeometryCanvas`, только без касаний и со своей копией задания. Плитка каталога собирает образец
/// по умолчанию, карточка показывает образец с текущими параметрами.
/// @see https://www.tamic.ru/docs/code/tpl-designer.html#gui-samples
#pragma once

#include "designermodel.h"
#include "geometrycanvas.h"

#include <QtQml/qqmlregistration.h>

/// @brief Плитка или картинка карточки образца. Потоки: только поток интерфейса.
class SamplePreview : public GeometryCanvas
{
    Q_OBJECT
    QML_ELEMENT

    /// Ключ образца из `DesignerModel.sampleSections`; пусто — показывать карточку `source`.
    Q_PROPERTY(QString sampleKey READ sampleKey WRITE setSampleKey NOTIFY sampleKeyChanged)
    /// Модель редактора: из неё берутся свои образцы и задание карточки.
    Q_PROPERTY(TplDesignerModel *source READ source WRITE setSource NOTIFY sourceChanged)

public:
    explicit SamplePreview(QQuickItem *parent = nullptr);

    QString sampleKey() const { return m_key; }
    void setSampleKey(const QString &key);
    TplDesignerModel *source() const { return m_source; }
    void setSource(TplDesignerModel *m);

signals:
    void sampleKeyChanged();
    void sourceChanged();

private:
    void reload();

    QString m_key;
    QPointer<TplDesignerModel> m_source;
    TplDesignerModel m_own;   ///< своя копия задания: редактор не затрагивается
};
