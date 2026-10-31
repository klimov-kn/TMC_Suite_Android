/// @file samplepreview.cpp
/// @brief Картинка образца: задание берётся из рецепта и кэшируется на время работы приложения.
/// @see https://www.tamic.ru/docs/code/tpl-designer.html#gui-samples
#include "samplepreview.h"

#include <QHash>

namespace {

/// Собранные образцы: рецепт считает геометрию заново, а плитки при прокрутке создаются вновь.
QHash<QString, tpl::Project> &cache()
{
    static QHash<QString, tpl::Project> c;
    return c;
}

} // namespace

SamplePreview::SamplePreview(QQuickItem *parent)
    : GeometryCanvas(parent)
{
    setAcceptTouchEvents(false);
    setAcceptedMouseButtons(Qt::NoButton);
    setGridVisible(false);
    setModel(&m_own);
}

void SamplePreview::setSampleKey(const QString &key)
{
    if (key == m_key)
        return;
    m_key = key;
    emit sampleKeyChanged();
    reload();
}

void SamplePreview::setSource(TplDesignerModel *m)
{
    if (m == m_source)
        return;
    if (m_source)
        disconnect(m_source, nullptr, this, nullptr);
    m_source = m;
    if (m_source)
        connect(m_source, &TplDesignerModel::sampleCardChanged, this, &SamplePreview::reload);
    emit sourceChanged();
    reload();
}

void SamplePreview::reload()
{
    if (!m_source)
        return;
    if (m_key.isEmpty()) {
        m_own.setViewProject(m_source->cardProject());
    } else if (m_key.startsWith(QLatin1String("b:"))) {
        auto it = cache().find(m_key);
        if (it == cache().end())
            it = cache().insert(m_key, m_source->buildSample(m_key, nullptr).project);
        m_own.setViewProject(it.value());
    } else {
        m_own.setViewProject(m_source->buildSample(m_key, nullptr).project);
    }
    zoomToFit();
}
