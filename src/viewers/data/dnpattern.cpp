/// @file dnpattern.cpp
/// @brief Перенос c2DArray / cIzl (src/win_src/viewers/DiaNapr) — см. dnpattern.h.
///
/// Выражения и порядок операций оставлены как в оригинале; комментарии «// c2DArray::…»
/// указывают, откуда взят фрагмент.
/// @see https://www.tamic.ru/docs/platforms/android.html#charts

#include "dnpattern.h"

#include "textscan.h"

#include <QCoreApplication>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace viewers {

namespace {

constexpr int kTypeAll = 0;   // TMC_IZL_DIRPATTYPE_ALL
constexpr int kTypeSin = 1;   // TMC_IZL_DIRPATTYPE_SIN
constexpr int kTypeCos = 2;   // TMC_IZL_DIRPATTYPE_COS
constexpr int kTypeArb = -1;  // TMC_IZL_DIRPATTYPE_ARB

// cIzl::dAnglePrived
void dAnglePrived(double &dAngle)
{
    double x, y, f;
    x = std::cos(dAngle);
    y = std::sin(dAngle);
    f = std::atan2(y, x);
    dAngle = f;
}

// cIzl::dAnglePrived1
void dAnglePrived1(double &dAngle)
{
    double x, y, f;
    x = std::cos(dAngle);
    y = std::sin(dAngle);
    f = std::atan2(y, x);
    if (f <= 0.0) f += 2 * 3.141592653589;
    dAngle = f;
}

// Число после fprintf("%lg") и fscanf("%lg") — для временного файла TMC_DN (случай N = 400).
double roundTripLg(double v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", v);
    return std::strtod(buf, nullptr);
}

QByteArray lineAt(const QByteArray &data, int *pos)
{
    if (*pos >= data.size())
        return {};
    int eol = data.indexOf('\n', *pos);
    if (eol < 0)
        eol = int(data.size());
    QByteArray line = data.mid(*pos, eol - *pos + 1);   // как fgets — с переводом строки
    *pos = eol + 1;
    return line;
}

} // namespace

// --- cIzl ---------------------------------------------------------------------

void DnPattern::Izl::setPattern(const QByteArray &s)   // cIzl::SetcsEmitterDirectPat
{
    csEmitterDirectPat = s;
    if (s == "1." || s.isEmpty()) { nType = kTypeAll; return; }
    if (s == "sin(f)") { nType = kTypeSin; return; }
    if (s == "cos(f)") { nType = kTypeCos; return; }
    nType = kTypeArb;
}

bool DnPattern::Izl::bIsAngleInDiapazon(double dAngle)   // cIzl::bIsAngleInDiapazon
{
    if (csEmitterDirectPat.isEmpty()) return true;

    dAnglePrived(dFimin);
    dAnglePrived(dFimax);
    dAnglePrived(dAngle);
    if ((dAngle >= dFimin) && (dAngle <= dFimax)) return true;

    dAnglePrived1(dFimin);
    dAnglePrived1(dFimax);
    dAnglePrived1(dAngle);
    if ((dAngle >= dFimin) && (dAngle <= dFimax)) return true;

    return false;
}

double DnPattern::Izl::dGetDirectionalPatternEmitter(double dAngle)   // cIzl::…
{
    if (csEmitterDirectPat.isEmpty()) return 1.0;
    if (!bIsAngleInDiapazon(dAngle)) return FLT_MIN;
    switch (nType) {
    case kTypeAll: return 1.;
    case kTypeSin: return std::sin(dAngle);
    case kTypeCos: return std::cos(dAngle);
    default: break;
    }
    return 0.0;   // произвольная формула: не поддержана (DnPattern::readData отказывает раньше)
}

double DnPattern::Izl::GetdAmplituda(double dAngle)   // cIzl::GetdAmplituda(double)
{
    if (std::fabs(dAmplituda) < FLT_MIN) return 0.0;
    return dGetDirectionalPatternEmitter(dAngle) * dAmplituda;
}

// --- c2DArray -----------------------------------------------------------------

bool DnPattern::compute(const QByteArray &data, bool reduceElMagnAnalysis)
{
    // c2DArray::c2DArray
    *this = DnPattern();
    bIsReduceElMagnAnalysis = reduceElMagnAnalysis;
    nAngle = 400;
    initAngle();
    dKnd = FLT_MIN;
    dKip = FLT_MIN;

    // CTMCGROUTDoc::read_RT_output_signal (DiaNapGr): CalculateDiagrNapr(файл, вывод) —
    // ReadData, CalculateDiagrNapr, WriteData; затем ResizeAngle(Xmin, Xmax).
    if (!readData(data))
        return false;
    calculateDiagrNapr();
    writeData();
    // Диапазон углов: полный круг −π…π (тот же, что ReadData задаёт при N > 400; при
    // N > 400 ResizeAngle ничего не меняет — числа как у Windows при любых Xmin/Xmax).
    resizeAngle(-3.141592653589, 3.141592653589);
    if (bIsError)
        return false;
    return true;
}

bool DnPattern::readData(const QByteArray &data)   // c2DArray::ReadData()
{
    int pos = 0;
    QByteArray buf = lineAt(data, &pos);
    {
        const char *p = buf.constData();
        const char *end = p + buf.size();
        double f;
        if (!(scan::expect(p, end, "Frequence =") && scan::readDouble(p, end, &f))) {
            bIsError = true;
            m_error = QCoreApplication::translate("viewers", "нет строки «Frequence = … Ghz;»");
            return false;
        }
        dFreq = f;
    }
    dFreq *= 1000000000;

    buf = lineAt(data, &pos);
    {
        const char *p = buf.constData();
        const char *end = p + buf.size();
        if (!(scan::expect(p, end, "N =") && scan::readInt(p, end, &nIzl))) {
            bIsError = true;
            m_error = QCoreApplication::translate("viewers", "нет строки «N = …;»");
            return false;
        }
    }

    if (nIzl > nMaxTemporary) {
        nAngle = 1000;
        initAngle();
        double dAngleMax, dAngleMin, dStep;
        dAngleMax = 3.141592653589;
        dAngleMin = -3.141592653589;
        int i;
        for (i = 0, dStep = (dAngleMax - dAngleMin) / (nAngle - 1); i < nAngle; i++)
            pdAngle[size_t(i)] = dAngleMin + i * dStep;
    }

    // Строка 3 — выражение ДН одного излучателя (szOneExpression). Ядро пишет «1.».
    buf = lineAt(data, &pos);
    if (buf.isEmpty()) {
        bIsError = true;
        m_error = QCoreApplication::translate("viewers", "нет строки с ДН одного излучателя");
        return false;
    }
    {
        const QByteArray expr = buf.trimmed();
        bool ok = false;
        const double v = expr.toDouble(&ok);
        if (!ok) {
            bIsError = true;
            m_error = QCoreApplication::translate("viewers", "формула ДН излучателя «%1» не поддерживается (только число)")
                          .arg(QString::fromLatin1(expr));
            return false;
        }
        oneExpression = v;
    }

    if (nIzl < 1) {
        bIsError = true;
        m_error = QCoreApplication::translate("viewers", "число излучателей %1 < 1").arg(nIzl);
        return false;
    }
    pcIzl.assign(size_t(nIzl), Izl());   // InitData
    std::fill(pdRealDN.begin(), pdRealDN.end(), 0.0);
    std::fill(pdImageDN.begin(), pdImageDN.end(), 0.0);

    for (int i = 0; i < nIzl; i++) {
        buf = lineAt(data, &pos);
        if (buf.isEmpty()) {
            bIsError = true;
            m_error = QCoreApplication::translate("viewers", "файл обрезан: излучатель %1").arg(i + 1);
            return false;
        }
        const char *const b = buf.constData();
        const char *const end = b + buf.size();
        int j;
        double x = 0, y = 0, a = 0, f = 0, dFiMax, dFiMin;
        QByteArray szBuf1;
        // sscanf( szBuf, "%d   X = %lg mm; Y = %lg mm; Amplitude = %lg; Faza = %lg degree;
        //         FiMin = %lg degree; FiMax = %lg degree; %s", … ) != 8
        const char *p = b;
        const bool full = scan::readInt(p, end, &j)
            && scan::expect(p, end, " X =") && scan::readDouble(p, end, &x)
            && scan::expect(p, end, " mm; Y =") && scan::readDouble(p, end, &y)
            && scan::expect(p, end, " mm; Amplitude =") && scan::readDouble(p, end, &a)
            && scan::expect(p, end, "; Faza =") && scan::readDouble(p, end, &f)
            && scan::expect(p, end, " degree; FiMin =") && scan::readDouble(p, end, &dFiMin)
            && scan::expect(p, end, " degree; FiMax =") && scan::readDouble(p, end, &dFiMax)
            && scan::expect(p, end, " degree;") && scan::readWord(p, end, &szBuf1);
        if (!full) {
            dFiMin = -180.;
            dFiMax = 180.;
            szBuf1.clear();
            p = b;
            const bool five = scan::readInt(p, end, &j)
                && scan::expect(p, end, " X =") && scan::readDouble(p, end, &x)
                && scan::expect(p, end, " mm; Y =") && scan::readDouble(p, end, &y)
                && scan::expect(p, end, " mm; Amplitude =") && scan::readDouble(p, end, &a)
                && scan::expect(p, end, "; Faza =") && scan::readDouble(p, end, &f);
            if (!five) {
                bIsError = true;
                m_error = QCoreApplication::translate("viewers", "не читается излучатель %1").arg(i + 1);
                return false;
            }
        }
        Izl &e = pcIzl[size_t(i)];
        e.dXcoordinata = x / 1000.;
        e.dYcoordinata = y / 1000.;
        e.dAmplituda = a;
        e.dFaza = f * 3.141592653589 / 180.;
        if (!bIsReduceElMagnAnalysis) {
            dFiMin = -180.;
            dFiMax = 180.;
        }
        e.dFimin = dFiMin * 3.141592653589 / 180.;
        e.dFimax = dFiMax * 3.141592653589 / 180.;
        e.setPattern(szBuf1);
        if (e.nType == kTypeArb) {
            bIsError = true;
            m_error = QCoreApplication::translate("viewers", "ДН излучателя %1 «%2» — формула, не поддерживается")
                          .arg(i + 1).arg(QString::fromLatin1(szBuf1));
            return false;
        }
    }
    return true;
}

void DnPattern::initAngle()   // c2DArray::InitAngle
{
    pdRealDN.assign(size_t(nAngle), 0.0);
    pdImageDN.assign(size_t(nAngle), 0.0);
    pdAngle.assign(size_t(nAngle), 0.0);
    int i;
    double dStep;
    for (i = 0, dStep = 3.141592653589 / (nAngle - 1); i < nAngle; i++)
        pdAngle[size_t(i)] = i * dStep;
}

void DnPattern::calculateDiagrNapr()   // c2DArray::CalculateDiagrNapr()
{
    if (bIsError) return;
    if (nAngle < 2) return;
    if ((nIzl > nMaxTemporary) && bIsInitialData) return;

    double dReal, dImage, r1;

    // IsDirectPatSaveInFile: при N >= 400 Windows-версия берёт ДН из временного файла,
    // если он новее .DAT. При первом открытии файла его нет; затем он есть всегда,
    // но при N > 400 сюда больше не попадаем (проверка выше) — остаётся N = 400:
    // ResizeAngle читает только что записанный файл (числа и углы округлены %lg).
    if (nIzl == nMaxTemporary && bIsInitialData) {
        for (int iN = 0; iN < nAngle; iN++) {   // ReadDataFromTempFile
            const double ugol = roundTripLg(m_tempAngleDeg[size_t(iN)]);
            pdAngle[size_t(iN)] = ugol * 3.141592653589 / 180.;
            pdRealDN[size_t(iN)] = roundTripLg(m_tempAbs[size_t(iN)]);
            pdImageDN[size_t(iN)] = roundTripLg(m_tempPhaseDeg[size_t(iN)]) * 3.141592653589 / 180.;
        }
    } else {
        // szOneExpression прочитан fgets с переводом строки, поэтому сравнение с "1."
        // в оригинале не проходит и всегда работает ветка с выражением: r1 — его значение.
        for (int iN = 0; iN < nAngle; iN++) {
            r1 = oneExpression;
            dReal = r1 * calculateRealDN(pdAngle[size_t(iN)]);
            dImage = r1 * calculateImageDN(pdAngle[size_t(iN)]);
            pdRealDN[size_t(iN)] = std::sqrt(dReal * dReal + dImage * dImage);
            pdImageDN[size_t(iN)] = std::atan2(dImage, dReal);
        }
    }
    normirovkaDiagrNapr();
}

double DnPattern::calculateRealDN(double dAngle)   // c2DArray::CalculateRealDN
{
    if (bIsError) return 0.;
    if (nIzl < 1 || pcIzl.empty()) return 0.;
    int i;
    double d, a, f, x, y, c, alf1;
    for (i = 0, d = 0.0; i < nIzl; i++) {
        Izl &e = pcIzl[size_t(i)];
        a = e.GetdAmplituda(dAngle);
        f = e.dFaza;
        x = e.dXcoordinata;
        y = e.dYcoordinata;
        c = std::sqrt(x * x + y * y);
        alf1 = std::atan2(y, x);
        d += a * std::cos(f + 2 * 3.141592653589 * dFreq * c * std::cos(dAngle - alf1) / 299792458.);
    }
    return d;
}

double DnPattern::calculateImageDN(double dAngle)   // c2DArray::CalculateImageDN
{
    if (bIsError) return 0.;
    if (nIzl < 1 || pcIzl.empty()) return 0.;
    int i;
    double d, a, f, x, y, c, alf1;
    for (i = 0, d = 0.0; i < nIzl; i++) {
        Izl &e = pcIzl[size_t(i)];
        a = e.GetdAmplituda(dAngle);
        f = e.dFaza;
        x = e.dXcoordinata;
        y = e.dYcoordinata;
        c = std::sqrt(x * x + y * y);
        alf1 = std::atan2(y, x);
        d += a * std::sin(f + 2 * 3.141592653589 * dFreq * c * std::cos(dAngle - alf1) / 299792458.);
    }
    return d;
}

double DnPattern::calculateRealDN_ForKip(double dAngle)   // c2DArray::CalculateRealDN_ForKip
{
    if (bIsError) return 0.;
    if (nIzl < 1 || pcIzl.empty()) return 0.;
    int i;
    double d, a, f, x, y, c, alf1;
    for (i = 0, d = 0.0; i < nIzl; i++) {
        a = 1.;
        f = 0.;
        x = pcIzl[size_t(i)].dXcoordinata;
        y = pcIzl[size_t(i)].dYcoordinata;
        c = std::sqrt(x * x + y * y);
        alf1 = std::atan2(y, x);
        d += a * std::cos(f + 2 * 3.141592653589 * dFreq * c * std::cos(dAngle - alf1) / 299792458.);
    }
    return d;
}

double DnPattern::calculateImageDN_ForKip(double dAngle)   // c2DArray::CalculateImageDN_ForKip
{
    if (bIsError) return 0.;
    if (nIzl < 1 || pcIzl.empty()) return 0.;
    int i;
    double d, a, f, x, y, c, alf1;
    for (i = 0, d = 0.0; i < nIzl; i++) {
        a = 1.;
        f = 0.;
        x = pcIzl[size_t(i)].dXcoordinata;
        y = pcIzl[size_t(i)].dYcoordinata;
        c = std::sqrt(x * x + y * y);
        alf1 = std::atan2(y, x);
        d += a * std::sin(f + 2 * 3.141592653589 * dFreq * c * std::cos(dAngle - alf1) / 299792458.);
    }
    return d;
}

void DnPattern::normirovkaDiagrNapr()   // c2DArray::NormirovkaDiagrNapr, Б-12 Windows 5.0
{
    if (bIsError) return;
    if (nAngle < 2) return;

    int i, nFine;
    double x, y, r, r1, dAng, dStep, dRmax, dPow, dMaxKip, dIntKip;

    if (!IsNormirovka) {
        IsNormirovka = true;
        // Электрический размер решётки задаёт густоту сетки: 64 точки на k*Rmax,
        // чтобы не пропустить узкий главный лепесток.
        for (i = 0, dRmax = 0.0; i < nIzl; i++) {
            x = pcIzl[size_t(i)].dXcoordinata;
            y = pcIzl[size_t(i)].dYcoordinata;
            r = std::sqrt(x * x + y * y);
            if (r > dRmax) dRmax = r;
        }
        nFine = nPointForNormir;
        r = 64. * 2 * 3.141592653589 * dFreq * dRmax / 299792458.;
        if (r + 1. > double(nFine)) nFine = (r + 1. > 4000000.) ? 4000000 : int(r + 1.);
        if (nFine < 16) nFine = 16;
        dStep = 2 * 3.141592653589 / nFine;
        const bool big = nIzl > nMaxTemporary;
        for (i = 0, dMax = 0.0, dKnd = 0.0, dMaxKip = 0.0, dIntKip = 0.0; i < nFine; i++) {
            dAng = i * dStep;
            r1 = oneExpression;
            x = r1 * calculateRealDN(dAng);
            y = r1 * calculateImageDN(dAng);
            dPow = x * x + y * y;
            dKnd += dPow * dStep;
            if (dPow > dMax) dMax = dPow;
            if (!big) {     // КИП: равномерная синфазная решётка той же геометрии
                x = calculateRealDN_ForKip(dAng);
                y = calculateImageDN_ForKip(dAng);
                dPow = x * x + y * y;
                dIntKip += dPow * dStep;
                if (dPow > dMaxKip) dMaxKip = dPow;
            }
        }
        if (!(dMax > DBL_MIN) || !std::isfinite(dMax) || !(dKnd > 0.) || !std::isfinite(dKnd)) {
            bIsError = true;
            m_error = QStringLiteral("Directional pattern is identically zero or not a finite number");
            return;
        }
        if (big) {
            // Большая решётка: «КИП» — излучённая мощность, график нормируется своим максимумом.
            dKip = dKnd;
            dKnd = 2 * 3.141592653589 * dMax / dKnd;
            for (i = 0, r = 0.0; i < nAngle; i++) {
                if (!std::isfinite(pdRealDN[size_t(i)])) r = -1.;
                if (r >= 0. && pdRealDN[size_t(i)] > r) r = pdRealDN[size_t(i)];
            }
            if (!(r > DBL_MIN)) {
                bIsError = true;
                m_error = QStringLiteral("Directional pattern is identically zero or not a finite number");
                return;
            }
            dMax = r;
            for (i = 0; i < nAngle; i++) pdRealDN[size_t(i)] /= dMax;
            return;
        }
        dKnd = 2 * 3.141592653589 * dMax / dKnd;
        if (dIntKip > 0. && dMaxKip > 0.) dKip = dKnd / (2 * 3.141592653589 * dMaxKip / dIntKip);
        else dKip = FLT_MIN;
        dMax = std::sqrt(dMax);
        dMax *= 1.001;
    }

    for (i = 0; i < nAngle; i++)
        pdRealDN[size_t(i)] /= dMax;
}

void DnPattern::writeData()   // c2DArray::WriteData() — только то, что меняет состояние
{
    if (bIsError) return;
    if ((nIzl > nMaxTemporary) && bIsInitialData) return;
    // fprintf( fp, "Frequence = %lg Ghz;\n", dFreq/1e9 ) — частота делится и умножается
    // обратно на 1e9 (в последнем бите может измениться).
    dFreq /= 1000000000;
    dFreq *= 1000000000;
    // WriteTemporaryFile: «угол° | a·dMax | a | фаза°» — нужен только при N = 400.
    m_tempAngleDeg.resize(size_t(nAngle));
    m_tempAbs.resize(size_t(nAngle));
    m_tempPhaseDeg.resize(size_t(nAngle));
    for (int i = 0; i < nAngle; i++) {
        const double a = pdRealDN[size_t(i)];
        m_tempAngleDeg[size_t(i)] = pdAngle[size_t(i)] / 3.141592653589 * 180.;
        m_tempAbs[size_t(i)] = a * dMax;
        m_tempPhaseDeg[size_t(i)] = pdImageDN[size_t(i)] * 180. / 3.141592653589;
    }
    bIsInitialData = true;
}

void DnPattern::resizeAngle(double dAngleMin, double dAngleMax)   // c2DArray::ResizeAngle
{
    if (bIsError) return;
    if (nAngle < 2) return;
    if ((nIzl > nMaxTemporary) && bIsInitialData) return;
    int i;
    double dStep;
    for (i = 0, dStep = (dAngleMax - dAngleMin) / (nAngle - 1); i < nAngle; i++)
        pdAngle[size_t(i)] = dAngleMin + i * dStep;
    calculateDiagrNapr();
    writeData();
}

double DnPattern::kndDb() const { return 10. * std::log10(dKnd); }
double DnPattern::kipDb() const { return 10. * std::log10(dKip); }

double DnPattern::value(size_t i, bool absolute, bool db) const
{
    // DiaNapGr/TMCGROUTView.cpp, PrepareDoubleGraph: FULL / AMPL (·dMax), PHAS_R / PHAS_G.
    const double r11 = absolute ? pdRealDN[i] * dMax : pdRealDN[i];
    return db ? 20. * std::log10(r11) : r11;
}

} // namespace viewers
