#pragma once

// Чтение 3D-моделей: STL (текстовый и двоичный), OBJ (+MTL), STEP (через OpenCASCADE).

#include "mesh3d.h"

#include <QStringList>

namespace tpl {

struct MeshReadResult {
    Mesh mesh;
    QString error;        ///< пусто — прочитано
    QStringList warnings; ///< прочитано, но не всё (пропущенные грани и т. п.)
    /// Единицы, в которых записан файл, если формат их хранит (STEP — да, STL/OBJ — нет).
    /// Миллиметров на единицу файла; 0 — неизвестно, спросить человека.
    double fileUnitMm = 0.0;
    bool ok() const { return error.isEmpty(); }
};

/// Прочитать модель по расширению: .stl, .obj, .step/.stp. Вершины не склеены —
/// это делает вызывающий (`Mesh::weld`) уже после масштаба.
MeshReadResult readMeshFile(const QString &path);

MeshReadResult readStl(const QByteArray &bytes, const QString &fallbackName);
MeshReadResult readObj(const QByteArray &bytes, const QString &fallbackName,
                       const QString &baseDir = QString());

/// Умеет ли эта сборка читать STEP (собрана ли с OpenCASCADE).
bool stepSupported();

/// Фильтр для окна выбора файла — с STEP или без, смотря по сборке.
QString meshFileFilter();

} // namespace tpl
