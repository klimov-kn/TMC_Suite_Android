// Диаграмма направленности (ДН) решётки излучателей по файлу .DAT — как в TMC_DN.
//
// Перенос без изменений математики классов c2DArray и cIzl
// (src/win_src/viewers/DiaNapr/c2DArray.cpp, cIzl.cpp) и того, как их вызывает документ
// TMC_DN (DiaNapGr/TMCGROUTDoc.cpp, read_RT_output_signal): ReadData → CalculateDiagrNapr →
// NormirovkaDiagrNapr (КНД, КИП/излучённая мощность) → WriteData → ResizeAngle.
// Сохранены: константы (3.141592653589, 299792458.), число углов (400; 1000 при N > 400),
// порядок суммирования, накопление угла dAng += dStep, «приведение» углов сектора
// излучателя (dAnglePrived — поле класса меняется при каждом вызове, как в оригинале),
// FLT_MIN вне сектора, круговой путь частоты dFreq/1e9*1e9 при записи (WriteData).
//
// Не перенесено (и не влияет на числа первого открытия файла): временные файлы
// «_Temp1»/«~~tempDia» (кэш TMC_DN на диске; при повторном открытии Windows-версия берёт
// из него числа, округлённые до %lg), девиации и оптимизация. Выражения ДН излучателя:
// поддержаны "1.", "sin(f)", "cos(f)" (их пишет ядро) и числовая константа в строке 3;
// произвольная формула требует интерпретатора exprint — тогда error().
#pragma once

#include <QByteArray>
#include <QString>

#include <vector>

namespace viewers {

class DnPattern
{
public:
    // mod1 документа TMC_DN: 1 (по умолчанию) — сектор излучателя учитывается
    // (bIsReduceElMagnAnalysis = TRUE); 0 — сектор −180…180°.
    bool compute(const QByteArray &data, bool reduceElMagnAnalysis = true);
    QString error() const { return m_error; }

    int emitters() const { return nIzl; }
    double freqGHz() const { return dFreq / 1000000000.; }   // GetdFreqGHz
    double knd() const { return dKnd; }
    double kndDb() const;                                     // GetKnd_dB: 10·lg КНД
    double kip() const { return dKip; }                       // КИП или излучённая мощность
    double kipDb() const;
    bool kipIsEmittedPower() const { return nIzl > nMaxTemporary; }   // IsCalculateKip
    double dmax() const { return dMax; }                      // GetdMax

    // Углы (рад) и нормированная ДН (pdRealDN после нормировки).
    const std::vector<double> &angles() const { return pdAngle; }
    const std::vector<double> &pattern() const { return pdRealDN; }

    // Виды оси Y TMC_DN (DiaNapGr/TMCGROUTView.cpp, PrepareDoubleGraph):
    //   нормированная — y = pdRealDN, в дБ — 20·lg(pdRealDN);
    //   абсолютная    — y = pdRealDN·dMax, в дБ — 20·lg(pdRealDN·dMax).
    double value(size_t i, bool absolute, bool db) const;

private:
    struct Izl   // cIzl
    {
        double dAmplituda = 0.0;
        double dFaza = 0.0;
        double dXcoordinata = 0.0;
        double dYcoordinata = 0.0;
        QByteArray csEmitterDirectPat;
        int nType = 0;
        double dFimax = 0.0;
        double dFimin = 2 * 3.141592653589;

        void setPattern(const QByteArray &s);
        bool bIsAngleInDiapazon(double dAngle);
        double dGetDirectionalPatternEmitter(double dAngle);
        double GetdAmplituda(double dAngle);
    };

    bool readData(const QByteArray &data);
    void initAngle();
    void calculateDiagrNapr();
    void normirovkaDiagrNapr();
    void writeData();
    void resizeAngle(double dAngleMin, double dAngleMax);
    double calculateRealDN(double dAngle);
    double calculateImageDN(double dAngle);
    double calculateRealDN_ForKip(double dAngle);
    double calculateImageDN_ForKip(double dAngle);

    QString m_error;
    bool bIsError = false;
    bool bIsReduceElMagnAnalysis = true;
    bool bIsInitialData = false;
    bool IsNormirovka = false;
    int nMaxTemporary = 400;
    int nPointForNormir = 1000;
    int nAngle = 400;
    int nIzl = 0;
    double dFreq = 0.0;
    double dMax = -1.;
    double dKnd = 0.0;
    double dKip = 0.0;
    double oneExpression = 1.0;   // значение строки 3 (szOneExpression)
    std::vector<double> pdAngle, pdRealDN, pdImageDN;
    std::vector<Izl> pcIzl;
    // Содержимое временного файла TMC_DN (WriteTemporaryFile) — для случая N = 400.
    std::vector<double> m_tempAngleDeg, m_tempAbs, m_tempPhaseDeg;
};

} // namespace viewers
