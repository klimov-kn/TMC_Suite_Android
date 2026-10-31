// Шкалы поля в палитре проекта — описание в fieldscales.h.
#include "fieldscales.h"
#include "theme.h"
#include "topology.h"

#include <algorithm>
#include <cmath>

namespace fieldview {

namespace {

QColor hex(const char *s) { return QColor(QString::fromLatin1(s)); }

struct Lab { double L, a, b; };

double toLinear(double c)
{
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
double toSrgb(double c)
{
    c = std::clamp(c, 0.0, 1.0);
    return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}

Lab toOklab(const QColor &c)
{
    const double r = toLinear(c.redF()), g = toLinear(c.greenF()), b = toLinear(c.blueF());
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

QColor fromOklab(const Lab &x)
{
    const double l = std::pow(x.L + 0.3963377774 * x.a + 0.2158037573 * x.b, 3);
    const double m = std::pow(x.L - 0.1055613458 * x.a - 0.0638541728 * x.b, 3);
    const double s = std::pow(x.L - 0.0894841775 * x.a - 1.2914855480 * x.b, 3);
    const double r = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    const double g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    const double b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
    return QColor::fromRgbF(float(toSrgb(r)), float(toSrgb(g)), float(toSrgb(b)));
}

} // namespace

QVector<QColor> scaleStops(Scale s)
{
    if (s == Scale::Wave) {
        // style-guide: −max … 0 … +max
        return {hex("#f1e8ff"), hex("#d7bdff"), hex("#9b6cf0"), hex("#4a3590"), theme::surface(),
                hex("#0f4f45"), hex("#1a9a83"), theme::accent(), hex("#c4f7ee")};
    }
    // style-guide: 0 … max
    return {theme::bg(), hex("#1b2550"), hex("#3a3384"), hex("#7a55d6"), theme::accent2(),
            theme::accent(), theme::accent3(), hex("#fff3d6")};
}

QColor scaleColor(Scale s, double t)
{
    const QVector<QColor> st = scaleStops(s);
    t = std::clamp(t, 0.0, 1.0);
    const double pos = t * (st.size() - 1);
    const int i = std::min(int(pos), int(st.size()) - 2);
    const double f = pos - i;
    const Lab a = toOklab(st[i]), b = toOklab(st[i + 1]);
    return fromOklab({a.L + (b.L - a.L) * f, a.a + (b.a - a.a) * f, a.b + (b.b - a.b) * f});
}

QImage scaleImage(Scale s, int width)
{
    QImage img(width, 1, QImage::Format_RGBA8888);
    for (int x = 0; x < width; ++x)
        img.setPixelColor(x, 0, scaleColor(s, double(x) / (width - 1)));
    return img;
}

QColor topologyColor(int t)
{
    QColor c;
    switch (t) {
    case kTypeMetal:                     // металл — text-dim, заливка
        c = theme::textDim();
        c.setAlphaF(1.0f);
        break;
    case kTypeAbsorber:                  // поглотитель — line
        c = theme::line();
        c.setAlphaF(0.9f);
        break;
    case kTypeMagnetic:                  // магнитная стенка — как металл, но вторичный текст
        c = theme::textMute();
        c.setAlphaF(1.0f);
        break;
    case kTypeInpXLeft:
    case kTypeInpXRight:
    case kTypeInpYTop:
    case kTypeInpYBot:                   // порты — accent
        c = theme::accent();
        c.setAlphaF(1.0f);
        break;
    case kTypePlasma:                    // плазма — accent-3, полупрозрачно
        c = theme::accent3();
        c.setAlphaF(0.75f);
        break;
    default:                             // диэлектрик (ε) — accent-2, полупрозрачно
        c = theme::accent2();
        c.setAlphaF(0.75f);
        break;
    }
    return c;
}

} // namespace fieldview
