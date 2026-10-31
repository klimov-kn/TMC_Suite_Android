// Сборка без OpenCASCADE: STEP не читается, остальное работает.
#include "stepio.h"

namespace tpl {

MeshReadResult readStepFile(const QString &, double)
{
    MeshReadResult r;
    r.error = QStringLiteral("эта сборка программы не умеет STEP: она собрана без библиотеки "
                             "OpenCASCADE. Сохраните модель из CAD в STL или OBJ");
    return r;
}

bool stepSupported() { return false; }
bool stepWriterAvailable() { return false; }

} // namespace tpl
