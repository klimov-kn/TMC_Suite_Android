#include "sample.h"

// Числа, посчитанные ядром для образцов с параметрами по умолчанию.
// Файл создан автоматически — руками не править.

namespace tpl::samples {

const QVector<Baked> &bakedTable()
{
    static const QVector<Baked> table = {
        {QStringLiteral("ant_horn"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 203x168, шаг 1.42875 мм, TIME 20 нс, шагов 1"), {
            {QStringLiteral("10 ГГц"), QStringLiteral("|S11| -31.5 дБ")},
        }},
        {QStringLiteral("ant_lens_horn"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 209x189, шаг 1.42875 мм, TIME 20 нс, шагов 2"), {
            {QStringLiteral("10 ГГц (без линзы)"), QStringLiteral("|S11| -28.9 дБ")},
            {QStringLiteral("10 ГГц (с линзой)"), QStringLiteral("|S11| -14.7 дБ")},
        }},
        {QStringLiteral("ant_open_wg"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 147x168, шаг 1.42875 мм, TIME 20 нс, шагов 1"), {
            {QStringLiteral("10 ГГц"), QStringLiteral("|S11| -24.4 дБ")},
        }},
        {QStringLiteral("ant_reflector"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 210x294, шаг 1.42875 мм, TIME 25 нс, шагов 1"), {
            {QStringLiteral("10 ГГц"), QStringLiteral("|S11| -7.6 дБ")},
        }},
        {QStringLiteral("les_horn_demo"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 240x200, шаг 1 мм, TIME 3 нс, шагов 1"), {
            {QStringLiteral("10 ГГц"), QStringLiteral("|S11| -22.3 дБ")},
        }},
        {QStringLiteral("les_lr1_horn"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 240x200, шаг 1 мм, TIME 60 нс, шагов 1"), {
            {QStringLiteral("6 ГГц"), QStringLiteral("|S11| -30.4 дБ")},
        }},
        {QStringLiteral("les_lr1_wg"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 155x78, шаг 1 мм, TIME 115 нс, шагов 17"), {
            {QStringLiteral("3.3 ГГц"), QStringLiteral("|S11| -32.4 дБ, |S21| -0.0 дБ")},
            {QStringLiteral("4.1 ГГц"), QStringLiteral("|S11| -7.7 дБ, |S21| -0.8 дБ")},
            {QStringLiteral("4.9 ГГц"), QStringLiteral("|S11| -8.9 дБ, |S21| -0.6 дБ")},
        }},
        {QStringLiteral("les_lr2_horn"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 240x170, шаг 1 мм, TIME 60 нс, шагов 1"), {
            {QStringLiteral("8 ГГц"), QStringLiteral("|S11| -34.9 дБ")},
        }},
        {QStringLiteral("les_lr2_plasma"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 240x80, шаг 1 мм, TIME 115 нс, шагов 31"), {
            {QStringLiteral("3 ГГц"), QStringLiteral("|S11| -0.0 дБ, |S21| -30.7 дБ")},
            {QStringLiteral("4.5 ГГц"), QStringLiteral("|S11| -0.1 дБ, |S21| -16.3 дБ")},
            {QStringLiteral("6 ГГц"), QStringLiteral("|S11| -5.1 дБ, |S21| -1.6 дБ")},
        }},
        {QStringLiteral("med_eps_file"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 299x262, шаг 0.8 мм, TIME 20 нс, шагов 1"), {
            {QStringLiteral("10 ГГц"), QStringLiteral("|S11| -32.4 дБ")},
        }},
        {QStringLiteral("med_grad"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 180x36, шаг 1 мм, TIME 30 нс, шагов 32"), {
            {QStringLiteral("2.5 ГГц (резкая граница)"), QStringLiteral("|S11| -7.2 дБ, |S21| -1.0 дБ")},
            {QStringLiteral("6.5 ГГц (резкая граница)"), QStringLiteral("|S11| -8.2 дБ, |S21| -1.1 дБ")},
            {QStringLiteral("9 ГГц (резкая граница)"), QStringLiteral("|S11| -8.3 дБ, |S21| -1.1 дБ")},
            {QStringLiteral("10 ГГц (резкая граница)"), QStringLiteral("|S11| -7.6 дБ, |S21| -1.2 дБ")},
            {QStringLiteral("2.5 ГГц (плавный профиль)"), QStringLiteral("|S11| -16.0 дБ, |S21| -0.3 дБ")},
            {QStringLiteral("6.5 ГГц (плавный профиль)"), QStringLiteral("|S11| -24.3 дБ, |S21| -0.2 дБ")},
            {QStringLiteral("8.5 ГГц (плавный профиль)"), QStringLiteral("|S11| -30.7 дБ, |S21| -0.3 дБ")},
            {QStringLiteral("10 ГГц (плавный профиль)"), QStringLiteral("|S11| -23.0 дБ, |S21| -0.3 дБ")},
        }},
        {QStringLiteral("med_luneburg"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 300x270, шаг 1 мм, TIME 25 нс, шагов 1"), {
            {QStringLiteral("10 ГГц"), QStringLiteral("|S11| -33.3 дБ")},
        }},
        {QStringLiteral("med_slab"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 517x27, шаг 0.6 мм, TIME 20 нс, шагов 33"), {
            {QStringLiteral("2 ГГц"), QStringLiteral("|S11| -8.0 дБ, |S21| -0.9 дБ")},
            {QStringLiteral("7 ГГц"), QStringLiteral("|S11| -14.9 дБ, |S21| -0.3 дБ")},
            {QStringLiteral("7.313 ГГц"), QStringLiteral("|S11| -33.7 дБ, |S21| -0.2 дБ")},
            {QStringLiteral("12 ГГц"), QStringLiteral("|S11| -4.8 дБ, |S21| -1.9 дБ")},
        }},
        {QStringLiteral("ms_bend"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 111x111, шаг 0.56284 мм, TIME 15 нс, шагов 21"), {
            {QStringLiteral("1.8 ГГц"), QStringLiteral("|S11| -28.6 дБ, |S21| -0.3 дБ")},
            {QStringLiteral("3.3 ГГц"), QStringLiteral("|S11| -19.6 дБ, |S21| -0.3 дБ")},
            {QStringLiteral("4.2 ГГц"), QStringLiteral("|S11| -34.6 дБ, |S21| -0.3 дБ")},
            {QStringLiteral("4.8 ГГц"), QStringLiteral("|S11| -16.0 дБ, |S21| -0.4 дБ")},
        }},
        {QStringLiteral("ms_branchline"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 128x47, шаг 0.6 мм, TIME 20 нс, шагов 21"), {
            {QStringLiteral("1.8 ГГц"), QStringLiteral("|S11| -5.9 дБ, |S21| -8.1 дБ, |S31| -4.7 дБ, |S41| -8.4 дБ")},
            {QStringLiteral("3 ГГц"), QStringLiteral("|S11| -21.4 дБ, |S21| -3.3 дБ, |S31| -3.6 дБ, |S41| -20.7 дБ")},
            {QStringLiteral("4.2 ГГц"), QStringLiteral("|S11| -7.9 дБ, |S21| -5.6 дБ, |S31| -5.5 дБ, |S41| -7.6 дБ")},
        }},
        {QStringLiteral("ms_bsf"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 302x56, шаг 0.377427 мм, TIME 25 нс, шагов 41"), {
            {QStringLiteral("1.5 ГГц"), QStringLiteral("|S11| -10.4 дБ, |S21| -0.8 дБ")},
            {QStringLiteral("3 ГГц"), QStringLiteral("|S11| -0.5 дБ, |S21| -59.2 дБ")},
            {QStringLiteral("4.275 ГГц"), QStringLiteral("|S11| -33.6 дБ, |S21| -0.3 дБ")},
            {QStringLiteral("4.5 ГГц"), QStringLiteral("|S11| -8.0 дБ, |S21| -1.0 дБ")},
        }},
        {QStringLiteral("ms_line"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 195x17, шаг 0.56284 мм, TIME 15 нс, шагов 21"), {
            {QStringLiteral("1.8 ГГц"), QStringLiteral("|S11| -19.0 дБ, |S21| -0.4 дБ")},
            {QStringLiteral("3.3 ГГц"), QStringLiteral("|S11| -24.6 дБ, |S21| -0.4 дБ")},
            {QStringLiteral("4.2 ГГц"), QStringLiteral("|S11| -27.7 дБ, |S21| -0.3 дБ")},
            {QStringLiteral("4.8 ГГц"), QStringLiteral("|S11| -26.6 дБ, |S21| -0.4 дБ")},
        }},
        {QStringLiteral("ms_load"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 195x17, шаг 0.56284 мм, TIME 20 нс, шагов 21"), {
            {QStringLiteral("1.5 ГГц"), QStringLiteral("|S11| -1.7 дБ")},
            {QStringLiteral("3 ГГц"), QStringLiteral("|S11| -4.3 дБ")},
            {QStringLiteral("4.05 ГГц"), QStringLiteral("|S11| -8.2 дБ")},
            {QStringLiteral("4.5 ГГц"), QStringLiteral("|S11| -6.3 дБ")},
        }},
        {QStringLiteral("ms_lpf"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 281x42, шаг 0.4 мм, TIME 20 нс, шагов 37"), {
            {QStringLiteral("0.4 ГГц"), QStringLiteral("|S11| -10.9 дБ, |S21| -0.7 дБ")},
            {QStringLiteral("1.5 ГГц"), QStringLiteral("|S11| -28.2 дБ, |S21| -0.3 дБ")},
            {QStringLiteral("2.2 ГГц"), QStringLiteral("|S11| -2.8 дБ, |S21| -4.4 дБ")},
            {QStringLiteral("4 ГГц"), QStringLiteral("|S11| -0.4 дБ, |S21| -19.0 дБ")},
        }},
        {QStringLiteral("ms_openend"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 100x17, шаг 0.56284 мм, TIME 15 нс, шагов 21"), {
            {QStringLiteral("1.5 ГГц"), QStringLiteral("|S11| -0.1 дБ")},
            {QStringLiteral("2.1 ГГц"), QStringLiteral("|S11| -0.8 дБ")},
            {QStringLiteral("3 ГГц"), QStringLiteral("|S11| -0.3 дБ")},
            {QStringLiteral("4.5 ГГц"), QStringLiteral("|S11| -0.3 дБ")},
        }},
        {QStringLiteral("ms_patch"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 235x160, шаг 0.5 мм, TIME 35 нс, шагов 25"), {
            {QStringLiteral("2.1 ГГц"), QStringLiteral("|S11| -0.5 дБ")},
            {QStringLiteral("3 ГГц"), QStringLiteral("|S11| -0.6 дБ")},
            {QStringLiteral("3.3 ГГц"), QStringLiteral("|S11| -12.8 дБ")},
            {QStringLiteral("3.9 ГГц"), QStringLiteral("|S11| -1.6 дБ")},
        }},
        {QStringLiteral("ms_qwt"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 420x26, шаг 0.3 мм, TIME 15 нс, шагов 25"), {
            {QStringLiteral("1.2 ГГц"), QStringLiteral("|S11| -9.3 дБ, |S21| -0.5 дБ")},
            {QStringLiteral("2.4 ГГц"), QStringLiteral("|S11| -27.3 дБ, |S21| 0.0 дБ")},
            {QStringLiteral("3 ГГц"), QStringLiteral("|S11| -22.7 дБ, |S21| -0.0 дБ")},
            {QStringLiteral("4.8 ГГц"), QStringLiteral("|S11| -9.7 дБ, |S21| -0.6 дБ")},
        }},
        {QStringLiteral("ms_stub"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 206x40, шаг 0.56284 мм, TIME 20 нс, шагов 41"), {
            {QStringLiteral("1.5 ГГц"), QStringLiteral("|S11| -12.0 дБ, |S21| -0.6 дБ")},
            {QStringLiteral("3 ГГц"), QStringLiteral("|S11| -0.0 дБ, |S21| -21.0 дБ")},
            {QStringLiteral("4.5 ГГц"), QStringLiteral("|S11| -5.8 дБ, |S21| -1.8 дБ")},
        }},
        {QStringLiteral("ms_taper"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 364x26, шаг 0.3 мм, TIME 15 нс, шагов 35"), {
            {QStringLiteral("0.9 ГГц"), QStringLiteral("|S11| -3.0 дБ, |S21| -3.1 дБ")},
            {QStringLiteral("2.7 ГГц"), QStringLiteral("|S11| -22.8 дБ, |S21| -0.0 дБ")},
            {QStringLiteral("3.45 ГГц"), QStringLiteral("|S11| -22.5 дБ, |S21| -0.0 дБ")},
            {QStringLiteral("6 ГГц"), QStringLiteral("|S11| -15.6 дБ, |S21| -0.1 дБ")},
        }},
        {QStringLiteral("ms_tee"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 381x219, шаг 0.29459 мм, TIME 25 нс, шагов 21"), {
            {QStringLiteral("1.8 ГГц"), QStringLiteral("|S11| -14.2 дБ, |S21| -3.4 дБ, |S31| -3.1 дБ")},
            {QStringLiteral("3 ГГц"), QStringLiteral("|S11| -14.9 дБ, |S21| -3.2 дБ, |S31| -3.1 дБ")},
            {QStringLiteral("4.2 ГГц"), QStringLiteral("|S11| -29.8 дБ, |S21| -2.9 дБ, |S31| -3.2 дБ")},
        }},
        {QStringLiteral("pl_column"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 428x46, шаг 1.8035 мм, TIME 45 нс, шагов 21"), {
            {QStringLiteral("2.6 ГГц"), QStringLiteral("|S11| -3.3 дБ, |S21| -2.7 дБ")},
            {QStringLiteral("3.275 ГГц"), QStringLiteral("|S11| -7.0 дБ, |S21| -1.0 дБ")},
            {QStringLiteral("3.95 ГГц"), QStringLiteral("|S11| -10.4 дБ, |S21| -0.4 дБ")},
        }},
        {QStringLiteral("pl_magnetized"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 437x46, шаг 1.8035 мм, TIME 45 нс, шагов 21"), {
            {QStringLiteral("2.6 ГГц"), QStringLiteral("|S11| -2.6 дБ, |S21| -3.5 дБ")},
            {QStringLiteral("3.275 ГГц"), QStringLiteral("|S11| -5.7 дБ, |S21| -1.4 дБ")},
            {QStringLiteral("3.95 ГГц"), QStringLiteral("|S11| -8.7 дБ, |S21| -0.6 дБ")},
        }},
        {QStringLiteral("pl_slab"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 437x46, шаг 1.8035 мм, TIME 40 нс, шагов 21"), {
            {QStringLiteral("2.6 ГГц"), QStringLiteral("|S11| -1.7 дБ, |S21| -4.9 дБ")},
            {QStringLiteral("3.275 ГГц"), QStringLiteral("|S11| -4.1 дБ, |S21| -2.1 дБ")},
            {QStringLiteral("3.95 ГГц"), QStringLiteral("|S11| -6.7 дБ, |S21| -1.0 дБ")},
        }},
        {QStringLiteral("rcs_airfoil"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 280x280, шаг 1.5 мм, TIME 30 нс, шагов 38"), {
            {QStringLiteral("угол 5°"), QStringLiteral("|S11| -28.9 дБ")},
            {QStringLiteral("угол 95°"), QStringLiteral("|S11| -10.1 дБ")},
            {QStringLiteral("угол 110°"), QStringLiteral("|S11| -37.9 дБ")},
            {QStringLiteral("угол 0°"), QStringLiteral("|S11| -29.2 дБ")},
            {QStringLiteral("5 ГГц (без объекта)"), QStringLiteral("|S11| -38.6 дБ")},
        }},
        {QStringLiteral("rcs_coated"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 340x340, шаг 1.5 мм, TIME 35 нс, шагов 27"), {
            {QStringLiteral("2 ГГц (без объекта)"), QStringLiteral("|S11| -32.6 дБ")},
            {QStringLiteral("5 ГГц (без объекта)"), QStringLiteral("|S11| -38.1 дБ")},
            {QStringLiteral("5.75 ГГц (без объекта)"), QStringLiteral("|S11| -40.7 дБ")},
            {QStringLiteral("8 ГГц (без объекта)"), QStringLiteral("|S11| -40.3 дБ")},
            {QStringLiteral("2 ГГц (металл)"), QStringLiteral("|S11| -19.5 дБ")},
            {QStringLiteral("5 ГГц (металл)"), QStringLiteral("|S11| -27.4 дБ")},
            {QStringLiteral("8 ГГц (металл)"), QStringLiteral("|S11| -29.8 дБ")},
            {QStringLiteral("2 ГГц (металл + плазма)"), QStringLiteral("|S11| -19.2 дБ")},
            {QStringLiteral("5 ГГц (металл + плазма)"), QStringLiteral("|S11| -28.0 дБ")},
            {QStringLiteral("8 ГГц (металл + плазма)"), QStringLiteral("|S11| -29.0 дБ")},
        }},
        {QStringLiteral("rcs_cylinder"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 326x326, шаг 1.5 мм, TIME 30 нс, шагов 26"), {
            {QStringLiteral("2 ГГц (без объекта)"), QStringLiteral("|S11| -30.8 дБ")},
            {QStringLiteral("5 ГГц (без объекта)"), QStringLiteral("|S11| -38.7 дБ")},
            {QStringLiteral("8 ГГц (без объекта)"), QStringLiteral("|S11| -43.4 дБ")},
            {QStringLiteral("2 ГГц (цилиндр)"), QStringLiteral("|S11| -18.2 дБ")},
            {QStringLiteral("2.5 ГГц (цилиндр)"), QStringLiteral("|S11| -27.6 дБ")},
            {QStringLiteral("5 ГГц (цилиндр)"), QStringLiteral("|S11| -26.2 дБ")},
            {QStringLiteral("8 ГГц (цилиндр)"), QStringLiteral("|S11| -25.9 дБ")},
        }},
        {QStringLiteral("rcs_strip"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 240x240, шаг 1.5 мм, TIME 25 нс, шагов 38"), {
            {QStringLiteral("угол 5°"), QStringLiteral("|S11| -11.2 дБ")},
            {QStringLiteral("угол 95°"), QStringLiteral("|S11| -25.3 дБ")},
            {QStringLiteral("угол 150°"), QStringLiteral("|S11| -42.5 дБ")},
            {QStringLiteral("угол 0°"), QStringLiteral("|S11| -10.0 дБ")},
            {QStringLiteral("5 ГГц (без объекта)"), QStringLiteral("|S11| -36.3 дБ")},
        }},
        {QStringLiteral("wg_bend"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 175x175, шаг 0.5715 мм, TIME 20 нс, шагов 21"), {
            {QStringLiteral("8.2 ГГц"), QStringLiteral("|S11| -18.5 дБ, |S21| -0.1 дБ")},
            {QStringLiteral("10.3 ГГц"), QStringLiteral("|S11| -39.2 дБ, |S21| -0.0 дБ")},
            {QStringLiteral("10.51 ГГц"), QStringLiteral("|S11| -52.8 дБ, |S21| 0.0 дБ")},
            {QStringLiteral("12.4 ГГц"), QStringLiteral("|S11| -24.0 дБ, |S21| -0.0 дБ")},
        }},
        {QStringLiteral("wg_filter"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 359x54, шаг 0.47625 мм, TIME 25 нс, шагов 31"), {
            {QStringLiteral("9 ГГц"), QStringLiteral("|S11| 0.0 дБ, |S21| -54.1 дБ")},
            {QStringLiteral("10 ГГц"), QStringLiteral("|S11| -0.8 дБ, |S21| -7.6 дБ")},
            {QStringLiteral("10.13 ГГц"), QStringLiteral("|S11| -8.6 дБ, |S21| -0.6 дБ")},
            {QStringLiteral("11 ГГц"), QStringLiteral("|S11| -0.0 дБ, |S21| -22.9 дБ")},
        }},
        {QStringLiteral("wg_iris"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 264x46, шаг 0.5715 мм, TIME 20 нс, шагов 21"), {
            {QStringLiteral("8.2 ГГц"), QStringLiteral("|S11| -1.0 дБ, |S21| -6.9 дБ")},
            {QStringLiteral("10.3 ГГц"), QStringLiteral("|S11| -2.5 дБ, |S21| -3.6 дБ")},
            {QStringLiteral("12.4 ГГц"), QStringLiteral("|S11| -4.4 дБ, |S21| -2.0 дБ")},
        }},
        {QStringLiteral("wg_load"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 333x46, шаг 0.5715 мм, TIME 20 нс, шагов 33"), {
            {QStringLiteral("8.2 ГГц (короткое замыкание)"), QStringLiteral("|S11| -0.0 дБ")},
            {QStringLiteral("10.3 ГГц (короткое замыкание)"), QStringLiteral("|S11| -0.0 дБ")},
            {QStringLiteral("12.4 ГГц (короткое замыкание)"), QStringLiteral("|S11| -0.0 дБ")},
            {QStringLiteral("8.2 ГГц (плоский поглотитель)"), QStringLiteral("|S11| -12.1 дБ")},
            {QStringLiteral("10.3 ГГц (плоский поглотитель)"), QStringLiteral("|S11| -17.8 дБ")},
            {QStringLiteral("12.4 ГГц (плоский поглотитель)"), QStringLiteral("|S11| -21.8 дБ")},
            {QStringLiteral("8.2 ГГц (клин)"), QStringLiteral("|S11| -7.1 дБ")},
            {QStringLiteral("10.3 ГГц (клин)"), QStringLiteral("|S11| -11.0 дБ")},
            {QStringLiteral("12.4 ГГц (клин)"), QStringLiteral("|S11| -14.5 дБ")},
        }},
        {QStringLiteral("wg_post"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 396x66, шаг 0.381 мм, TIME 20 нс, шагов 21"), {
            {QStringLiteral("8.2 ГГц"), QStringLiteral("|S11| -0.3 дБ, |S21| -12.4 дБ")},
            {QStringLiteral("10.3 ГГц"), QStringLiteral("|S11| -0.8 дБ, |S21| -7.9 дБ")},
            {QStringLiteral("12.4 ГГц"), QStringLiteral("|S11| -1.6 дБ, |S21| -5.1 дБ")},
        }},
        {QStringLiteral("wg_section"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 198x46, шаг 0.5715 мм, TIME 15 нс, шагов 21"), {
            {QStringLiteral("8.2 ГГц"), QStringLiteral("|S11| -91.1 дБ, |S21| 0.0 дБ")},
            {QStringLiteral("10.3 ГГц"), QStringLiteral("|S11| -100.3 дБ, |S21| 0.0 дБ")},
            {QStringLiteral("11.77 ГГц"), QStringLiteral("|S11| -111.5 дБ, |S21| 0.0 дБ")},
            {QStringLiteral("12.4 ГГц"), QStringLiteral("|S11| -107.1 дБ, |S21| 0.0 дБ")},
        }},
        {QStringLiteral("wg_step"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 258x46, шаг 0.5715 мм, TIME 20 нс, шагов 42"), {
            {QStringLiteral("8.49 ГГц"), QStringLiteral("|S11| -13.7 дБ, |S21| -0.2 дБ")},
            {QStringLiteral("10.44 ГГц"), QStringLiteral("|S11| -22.9 дБ, |S21| -0.0 дБ")},
            {QStringLiteral("12.4 ГГц"), QStringLiteral("|S11| -27.9 дБ, |S21| -0.0 дБ")},
        }},
        {QStringLiteral("wg_tee"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 304x175, шаг 0.5715 мм, TIME 20 нс, шагов 33"), {
            {QStringLiteral("8.2 ГГц"), QStringLiteral("|S11| -9.1 дБ, |S21| -2.6 дБ, |S31| -4.8 дБ")},
            {QStringLiteral("10.3 ГГц"), QStringLiteral("|S11| -13.6 дБ, |S21| -1.9 дБ, |S31| -5.0 дБ")},
            {QStringLiteral("12.4 ГГц"), QStringLiteral("|S11| -11.0 дБ, |S21| -0.5 дБ, |S31| -14.1 дБ")},
        }},
        {QStringLiteral("wg_window"), QStringLiteral("2026-10-02"), QStringLiteral("сетка 284x46, шаг 0.5715 мм, TIME 20 нс, шагов 21"), {
            {QStringLiteral("8.2 ГГц"), QStringLiteral("|S11| -6.7 дБ, |S21| -1.1 дБ")},
            {QStringLiteral("10.09 ГГц"), QStringLiteral("|S11| -46.4 дБ, |S21| -0.0 дБ")},
            {QStringLiteral("10.3 ГГц"), QStringLiteral("|S11| -28.0 дБ, |S21| -0.0 дБ")},
            {QStringLiteral("12.4 ГГц"), QStringLiteral("|S11| -9.3 дБ, |S21| -0.5 дБ")},
        }},
    };
    return table;
}

} // namespace tpl::samples
