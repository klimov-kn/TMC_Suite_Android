#include "materialmap.h"

#include <QHash>
#include <QRegularExpression>

namespace tpl {

namespace {

struct Entry {
    const char *pattern; ///< регулярное выражение по имени в нижнем регистре
    const char *what;
    Material material;
    double eps;
    bool skip;
};

// Порядок важен: первое совпадение побеждает. Поэтому «поглотитель» раньше «abs»
// (ABS-пластик), «магнитная стенка» раньше «металл», а короткие имена — словами
// целиком (\b): «pla» не должно ловить «plasma», «cu» — «vacuum».
//
// Проницаемости — паспортные значения на СВЧ; человек правит их в таблице, если у его
// марки другое.
const Entry kTable[] = {
    // Фон: вставлять нечего.
    {"\\b(air|vacuum|void)\\b|воздух|вакуум", "воздух / вакуум", Material::Dielectric, 1.0, true},

    // Поглотитель и магнитная стенка.
    {"absorb|поглот|eccosorb|\\bram\\b|радиопоглощ", "поглотитель", Material::Absorber, 1.0, false},
    {"\\bpmc\\b|magnetic\\s*wall|магнитн\\w*\\s*стен", "магнитная стенка", Material::Magnetic, 1.0, false},

    // Диэлектрики — до металлов: «стеклотекстолит» не должен стать «сталью».
    {"\\bfr-?4\\b|стеклотекстолит", "FR-4 (eps 4.4)", Material::Dielectric, 4.4, false},
    {"ro-?4003|rogers\\s*4003", "Rogers RO4003C (eps 3.55)", Material::Dielectric, 3.55, false},
    {"ro-?4350|rogers\\s*4350", "Rogers RO4350B (eps 3.66)", Material::Dielectric, 3.66, false},
    {"5880|duroid", "RT/duroid 5880 (eps 2.2)", Material::Dielectric, 2.2, false},
    {"ptfe|teflon|тефлон|фторопласт", "фторопласт / PTFE (eps 2.1)", Material::Dielectric, 2.1, false},
    {"alumina|al2o3|поликор|корунд|керамик|ceramic", "алюмооксидная керамика (eps 9.8)", Material::Dielectric, 9.8, false},
    {"quartz|fused\\s*silica|кварц", "кварц (eps 3.8)", Material::Dielectric, 3.8, false},
    {"pmma|plexi|acryl|оргстекл", "оргстекло (eps 2.6)", Material::Dielectric, 2.6, false},
    {"glass|стекл", "стекло (eps 6)", Material::Dielectric, 6.0, false},
    {"polyethylene|\\bhdpe\\b|\\bldpe\\b|\\bpe\\b|полиэтилен", "полиэтилен (eps 2.25)", Material::Dielectric, 2.25, false},
    {"polystyrene|полистирол|rexolite|рексолит", "полистирол (eps 2.55)", Material::Dielectric, 2.55, false},
    {"polypropylene|полипропилен", "полипропилен (eps 2.2)", Material::Dielectric, 2.2, false},
    {"\\babs\\b", "ABS-пластик (eps 2.8)", Material::Dielectric, 2.8, false},
    {"\\bpla\\b", "PLA-пластик (eps 2.7)", Material::Dielectric, 2.7, false},
    {"nylon|polyamide|капрон|полиамид", "полиамид (eps 3.5)", Material::Dielectric, 3.5, false},
    {"silicon\\b|кремний", "кремний (eps 11.7)", Material::Dielectric, 11.7, false},
    {"\\bwater\\b|\\bвода\\b", "вода (eps 80)", Material::Dielectric, 80.0, false},
    {"rubber|резин", "резина (eps 3)", Material::Dielectric, 3.0, false},
    {"dielectric|диэлектрик|substrate|подложк", "диэлектрик (eps уточните)", Material::Dielectric, 2.0, false},

    // Проводники.
    {"\\bpec\\b|perfect\\s*conductor|идеальн\\w*\\s*проводн", "идеальный проводник", Material::Metal, 1.0, false},
    {"copper|\\bcu\\b|медь|медн|\\bм[0-3]\\b", "медь", Material::Metal, 1.0, false},
    {"alumin|алюмин|\\bамг|\\bд16|\\b6061\\b|\\b7075\\b", "алюминий", Material::Metal, 1.0, false},
    {"steel|stainless|\\baisi\\b|\\b12х18н10т\\b|стал|нерж|\\bст\\s?[0-9]", "сталь", Material::Metal, 1.0, false},
    {"brass|латун|\\bл6[0-9]\\b", "латунь", Material::Metal, 1.0, false},
    {"bronze|бронз", "бронза", Material::Metal, 1.0, false},
    {"silver|серебр|\\bag\\b", "серебро", Material::Metal, 1.0, false},
    {"gold|золот|\\bau\\b", "золото", Material::Metal, 1.0, false},
    {"titan|титан|\\bвт[0-9]", "титан", Material::Metal, 1.0, false},
    {"tungsten|вольфрам", "вольфрам", Material::Metal, 1.0, false},
    {"nickel|никел|inconel|инконел", "никель", Material::Metal, 1.0, false},
    {"iron|желез|чугун", "железо", Material::Metal, 1.0, false},
    {"zinc|цинк|\\btin\\b|олов|\\blead\\b|свин", "металл", Material::Metal, 1.0, false},
    {"metal|металл|conductor|проводник", "металл", Material::Metal, 1.0, false},
};

} // namespace

MaterialGuess guessMaterial(const QString &name)
{
    MaterialGuess g;
    const QString n = name.toLower();
    if (n.trimmed().isEmpty())
        return g;
    for (const Entry &e : kTable) {
        static QHash<const char *, QRegularExpression> cache;
        auto it = cache.find(e.pattern);
        if (it == cache.end())
            it = cache.insert(e.pattern,
                              QRegularExpression(QString::fromUtf8(e.pattern),
                                                 QRegularExpression::UseUnicodePropertiesOption));
        if (it->match(n).hasMatch()) {
            g.known = true;
            g.skip = e.skip;
            g.material = e.material;
            g.eps = e.eps;
            g.what = QString::fromUtf8(e.what);
            return g;
        }
    }
    return g;
}

} // namespace tpl
