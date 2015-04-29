// TODO: decode Mac encodings for CJK names
#define _USE_MATH_DEFINES
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <emmintrin.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <pg/pg.h>
#include <pg/platform.h>
#include "common.h"

static void unpack(const void **datap, const char *fmt, ...) {
    const uint8_t *data = *datap;
    va_list ap;
    va_start(ap, fmt);
    for ( ; *fmt; fmt++)
        switch (*fmt) {
        case 'b': (uint8_t*)data; data++; break;
        case 's': (uint16_t*)data; data += 2; break;
        case 'l': (uint32_t*)data; data += 4; break;
        case 'B': *va_arg(ap, uint8_t*) = *(uint8_t*)data; data++; break;
        case 'S': *va_arg(ap, uint16_t*) = be16(*(uint16_t*)data); data += 2; break;
        case 'L': *va_arg(ap, uint32_t*) = be32(*(uint32_t*)data); data += 4; break;
        }
    va_end(ap);
    *datap = data;
}

static void _scale(PgFont *font, float height, float width) {
    PgOpenType *otf = (PgOpenType*)font;
    if (width <= 0)
        width = height;
    otf->scale_x = width / otf->em;
    otf->scale_y = height / otf->em;
}
static PgPath *_getCharPath(const PgFont *font, const PgMatrix *ctm, unsigned c) {
    return $(getGlyphPath, font, ctm, $(getGlyph, font, c));
}
static void glyphPath(PgPath *path, const PgOpenType *font, const PgMatrix *ctm, unsigned g) {
    if (g >= font->nglyphs)
        g = 0;
    const void          *data;
    {
        const uint32_t      *loca32 = font->loca;
        const uint16_t      *loca16 = font->loca;
        g &= 0xffff;
        data = font->long_loca?
                loca32[g] == loca32[g + 1]? NULL:
                (char*)font->glyf + be32(loca32[g]):
            loca16[g] == loca16[g + 1]? NULL:
                (char*)font->glyf + be16(loca16[g]) * 2;
        if (!data)
            return;
    }
    
    int16_t ncontours;
    unpack(&data, "Sssss", &ncontours);
    
    if (ncontours < 0) {
        uint16_t flags, glyph;
        do {
            unpack(&data, "SS", &flags, &glyph);
            float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
            if (flags & 3 == 3) { // x & y words
                int16_t ee, ff;
                unpack(&data, "SS", &ee, &ff);
                e = ee, f = ff;
            } else if (flags & 2) { // x & y bytes
                int8_t ee, ff;
                unpack(&data, "BB", &ee, &ff);
                e = ee, f = ff;
            } else if (flags & 1) // matching points
                unpack(&data, "s"), e = f = 0;
            else // matching points
                unpack(&data, "b"), e = f = 0;
            
            if (flags & 8) { // single scale
                int16_t ss;
                unpack(&data, "S", &ss);
                a = d = ss;
            } else if (flags & 64) { // x & y scales
                int16_t aa, dd;
                unpack(&data, "SS", &aa, &dd);
                a = aa, d = dd;
            } else if (flags & 128) { // 2x2
                int16_t aa, bb, cc, dd;
                unpack(&data, "SSSS", &aa, &bb, &cc, &dd);
                a = aa, b = bb, c = cc, d = dd;
            }
            PgMatrix new_ctm = {a, b, c, d, e, f};
            pgMultiplyMatrix(&new_ctm, ctm);
            glyphPath(path, font, &new_ctm, glyph);
        } while (flags & 32);
    } else {
        const uint16_t  *ends = data;
        int             npoints = be16(ends[ncontours - 1]) + 1;
        int             ninstr = be16(ends[ncontours]);
        const uint8_t   *flags = (const uint8_t*)(ends + ncontours + 1) + ninstr;
        const uint8_t   *f = flags;
        int             xsize = 0;
        
        // Get length of flags & x coordinates
        for (int i = 0; i < npoints; i++) {
            int dx =    *f & 2? 1:
                        *f & 16? 0:
                        2;
            if (*f & 8) {
                i += f[1];
                dx *= f[1] + 1;
                f += 2;
            } else
                f++;
            xsize += dx;
        }
        const uint8_t   *xs = f;
        const uint8_t   *ys = xs + xsize;
        PgPt a = {0, 0};
        PgPt b;
        bool in_curve = false;
        
        int vx = 0;
        int vy = 0;
        PgPt start;
        for (int i = 0, rep = 0, end = 0; i < npoints; i++) {
            int dx =    *flags & 2? *flags & 16? *xs++: -*xs++:
                        *flags & 16? 0:
                        (int16_t)be16(((int16_t*)(xs+=2))[-1]);
            int dy =    *flags & 4? *flags & 32? *ys++: -*ys++:
                        *flags & 32? 0:
                        (int16_t)be16(((int16_t*)(ys+=2))[-1]);
            vx += dx;
            vy += dy;
            PgPt p = {vx, vy};
    
            
            if (i == end) {
                end = be16(*ends++) + 1;
                if (in_curve)
                    $(quadratic, path, ctm, b, start);
                else
                    $(close, path);
                if (i != npoints - 1)
                    $(move, path, ctm, p);
                start = a = p;
                in_curve = false;
            } else if (~*flags & 1) // curve
                if (in_curve) {
                    PgPt q = mid(b, p);
                    $(quadratic, path, ctm, b, q);
                    a = q;
                    b = p;
                } else {
                    b = p;
                    in_curve = true;
                }
            else if (in_curve) { // curve to line
                $(quadratic, path, ctm, b, p);
                a = p;
                in_curve = false;
            } else { // line to line
                $(line, path, ctm, p);
                a = p;
            }
            
            if (rep) {
                if (--rep == 0)
                    flags += 2;
            } else if (*flags & 8) {
                rep = flags[1];
                if (!rep) flags += 2;
            } else
                flags++;
        }
        if (in_curve)
            $(quadratic, path, ctm, b, start);
        else
            $(close, path);
    }
}
static PgPath *_getGlyphPath(const PgFont *font, const PgMatrix *ctm, unsigned g) {
    PgOpenType *otf = (PgOpenType*)font;
    PgPath *path = pgNewPath();
    PgMatrix new_ctm = {1,0,0, 1,0,0};
    pgTranslateMatrix(&new_ctm, 0, -otf->ascender);
    pgScaleMatrix(&new_ctm, otf->scale_x, -otf->scale_y);
    pgMultiplyMatrix(&new_ctm, ctm);
    glyphPath(path, (PgOpenType*)font, &new_ctm, g);
    return path;
}
static unsigned _getGlyph(const PgFont *font, unsigned c) {
    PgOpenType *otf = (PgOpenType*)font;
    unsigned g = otf->cmap[c & 0xffff];
    for (int i = 0; i < otf->nsubst; i++)
        if (g == otf->subst[i][0])
            g = otf->subst[i][1];
    return g;
}
static float _getUtf8Width(const PgFont *font, const char chars[], int len) {
    float width = 0;
    if (len < 0) len = strlen(chars);
    for (int i = 0; i < len; i++) width += $(getCharWidth, font, chars[i]);
    return width;
}
static float _getStringWidth(const PgFont *font, const wchar_t chars[], int len) {
    float width = 0;
    if (len < 0) len = wcslen(chars);
    for (int i = 0; i < len; i++) width += $(getCharWidth, font, chars[i]);
    return width;
}
static float _getAscender(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->ascender * otf->scale_y;
}
static float _getDescender(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->descender * otf->scale_y;
}
static float _getLeading(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->leading * otf->scale_y;
}
static float _getXHeight(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->x_height * otf->scale_y;
}
static float _getCapHeight(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->cap_height * otf->scale_y;
}
static float _getEm(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->em * otf->scale_y;
}
static float _getGlyphLsb(const PgFont *font, unsigned g) {
    PgOpenType *otf = (PgOpenType*)font;
    return  g < otf->nhmtx?    be16(otf->hmtx[g * 2 + 1]) * otf->scale_x:
            g < otf->nglyphs?  be16(otf->hmtx[(otf->nhmtx - 1) * 2 + 1]) * otf->scale_x:
            0;
}
static float _getGlyphWidth(const PgFont *font, unsigned g) {
    PgOpenType *otf = (PgOpenType*)font;
    return  g < otf->nhmtx?    be16(otf->hmtx[g * 2]) * otf->scale_x:
            g < otf->nglyphs?  be16(otf->hmtx[(otf->nhmtx - 1) * 2]) * otf->scale_x:
            0;
}
static float _getCharLsb(const PgFont *font, unsigned c) {
    return $(getGlyphLsb, font, $(getGlyph, font, c));
}
static float _getCharWidth(const PgFont *font, unsigned c) {
    return $(getGlyphWidth, font, $(getGlyph, font, c));
}
static PgRect _getSubscriptBox(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return (PgRect) {
        pgPt(otf->superscript_box.a.x * otf->scale_x,
            otf->superscript_box.a.y * otf->scale_y),
        pgPt(otf->superscript_box.b.x * otf->scale_x,
            otf->superscript_box.b.y * otf->scale_y) };
}
static PgRect _getSuperscriptBox(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return (PgRect) {
        pgPt(otf->subscript_box.a.x * otf->scale_x,
            otf->subscript_box.a.y * otf->scale_y),
        pgPt(otf->subscript_box.b.x * otf->scale_x,
            otf->subscript_box.b.y * otf->scale_y) };
}
static char *lookupFeatures(
    PgOpenType *font,
    const uint8_t *table,
    uint32_t script,
    uint32_t lang,
    const char *feature_tags,
    void subtable_handler(PgOpenType *font, uint32_t tag, const uint8_t *subtable, int lookup_type))
{
    if (!table)
        return NULL;
    
    const uint8_t *script_list;
    const uint8_t *feature_list;
    const uint8_t *lookup_list;
    
    // GSUB/GPOS header
    {
        const uint8_t *header = table;
        uint16_t script_off, feature_off, lookup_off;
        unpack(&header, "lSSS", &script_off, &feature_off, &lookup_off);
        
        script_list = table + script_off;
        feature_list = table + feature_off;
        lookup_list = table + lookup_off;
    }
    
    // ScriptList -> ScriptRecord
    const char *script_table = NULL;
    {
        uint16_t nscripts;
        const uint8_t *header = script_list;
        unpack(&header, "S", &nscripts);
        for (int i = 0; i < nscripts; i++) {
            uint32_t tag;
            uint16_t offset;
            unpack(&header, "LS", &tag, &offset);
            if (tag == 'DFLT')
                script_table = script_list + offset;
            else if (tag == script && !script_table) {
                script_table = script_list + offset;
                break;
            }
        }
    }
    
    // Script -> LangSys
    const char *langsys = NULL;
    if (script_table) {
        uint16_t def_lang, nlangs;
        const uint8_t *header = script_table;
        unpack(&header, "SS", &def_lang, &nlangs);
        if (def_lang)
            langsys = script_table + def_lang;
        for (int i = 0; i < nlangs; i++) {
            uint32_t tag;
            uint16_t offset;
            unpack(&header, "LS", &tag, &offset);
            if (tag == lang) {
                langsys = script_table + offset;
                break;
            }
        }
    }
    
    // LangSys -> Feature List
    char *all_features = NULL;
    if (langsys) {
        const uint8_t *header = langsys;
        uint16_t nfeatures;
        unpack(&header, "ssS", &nfeatures);
        
        if (!*feature_tags) {
            all_features = malloc(nfeatures * 4 + 1);
            all_features[nfeatures * 4] = 0;
        }
        
        for (int i = 0; i < nfeatures; i++) {
            uint32_t tag;
            const uint8_t *feature;
            {
                uint16_t index;
                unpack(&header, "S", &index);
                
                feature = feature_list + 2 + index * 6;
                uint16_t offset;
                unpack(&feature, "LS", &tag, &offset);
                feature = feature_list + offset;
            }
            
            if (!*feature_tags) {
                all_features[i * 4 + 0] = ((uint8_t*)&tag)[3];
                all_features[i * 4 + 1] = ((uint8_t*)&tag)[2];
                all_features[i * 4 + 2] = ((uint8_t*)&tag)[1];
                all_features[i * 4 + 3] = ((uint8_t*)&tag)[0];
            }
            
            for (int i = 0; feature_tags[i]; i += 4)
                if (feature_tags[i] == ',' || feature_tags[i] == ' ')
                    i -= 3;
                else if (tag == be32(*(uint32_t*)(feature_tags + i))) {
                    uint16_t nlookups;
                    unpack(&feature, "sS", &nlookups);
                    
                    for (int i = 0; i < nlookups; i++) {
                        uint16_t index;
                        unpack(&feature, "S", &index);
                        
                        // Lookup table -> GSUB/GPOS specific subtable
                        const uint8_t *lookup = lookup_list + be16(*(uint16_t*)(lookup_list + 2 + index * 2));
                        const uint8_t *lookup_base = lookup;
                        uint16_t lookup_type, nsubtables;
                        unpack(&lookup, "SsS", &lookup_type, &nsubtables);
                        
                        for (int i = 0; i < nsubtables; i++) {
                            uint16_t subtable_offset;
                            unpack(&lookup, "S", &subtable_offset);
                            const uint8_t *subtable = lookup_base + subtable_offset;
                            if (subtable_handler != NULL)
                                subtable_handler(font, tag, subtable, lookup_type);
                        }
                    }
                    break;
                }
        }
    }
    return all_features;
}
static void gsub_handler(PgOpenType *font, uint32_t tag, const uint8_t *subtable_base, int lookup_type) {
    const uint8_t *subtable = subtable_base;
    uint16_t subst_format, coverage_offset;

redo_subtable:
    unpack(&subtable, "SS", &subst_format, &coverage_offset);
    
    // Get glyph coverage
    const uint8_t *coverage = subtable_base + coverage_offset;
    uint16_t coverage_format, count;
    if (lookup_type == 7) {
        subtable = subtable_base;
        uint32_t offset;
        unpack(&subtable, "sSL", &lookup_type, &offset);
        subtable = subtable_base += offset;
        goto redo_subtable;
    } else if (lookup_type != 5 && lookup_type != 6 && lookup_type != 8)
        unpack(&coverage, "SS", &coverage_format, &count);
    
    if (lookup_type == 1) { // Single Substitution
        if (subst_format == 1) { 
            uint16_t delta;
            unpack(&subtable, "S", &delta);
            
            if (coverage_format == 1)
                for (int i = 0; i < count; i++) {
                    uint16_t input;
                    unpack(&coverage, "S", &input);
                    uint16_t output = input + delta;
                    $$(substituteGlyph, font, input, output);
                }
            else if (coverage_format == 2)
                for (int i = 0; i < count; i++) {
                    uint16_t start, end, start_index;
                    unpack(&coverage, "SSS", &start, &end, &start_index);
                    for (int glyph = start; glyph <= end; glyph++) {
                        uint16_t input = glyph;
                        uint16_t output = input + delta;
                        $$(substituteGlyph, font, input, output);
                    }
                }
        }
        else if (subst_format == 2) {
            uint16_t nglyphs;
            unpack(&subtable, "S", &nglyphs);
    
            if (coverage_format == 1)
                for (int i = 0; i < count; i++) {
                    uint16_t input;
                    uint16_t output;
                    unpack(&coverage, "S", &input);
                    unpack(&subtable, "S", &output);
                    $$(substituteGlyph, font, input, output);
                }
            else if (coverage_format == 2)
                for (int i = 0; i < count; i++) {
                    uint16_t start, end, start_index;
                    unpack(&coverage, "SSS", &start, &end, &start_index);
                    for (int glyph = start; glyph <= end; glyph++) {
                        uint16_t output;
                        uint16_t input = glyph;
                        unpack(&subtable, "S", &output);
                        $$(substituteGlyph, font, input, output);
                    }
                }
        }
    }
}
static void _useFeatures(PgFont *font, const uint8_t *features) {
    PgOpenType *otf = (PgOpenType*)font;
    free(otf->features);
    free(otf->subst);
    otf->nsubst = 0;
    otf->subst = NULL;
    otf->features = (void*)strdup(features);
    if (*features)
        lookupFeatures(otf, otf->gsub, otf->script, otf->lang, features, gsub_handler);
}
static char *_getFeatures(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return lookupFeatures(otf, otf->gsub, otf->script, otf->lang, "", NULL);
}
static void _substituteGlyph(PgFont *font, uint16_t in, uint16_t out) {
    PgOpenType *otf = (PgOpenType*)font;
    otf->subst = realloc(otf->subst, (otf->nsubst + 1) * 2 * sizeof *otf->subst);
    otf->subst[otf->nsubst][0] = in;
    otf->subst[otf->nsubst][1] = out;
    otf->nsubst++;
}
static const wchar_t *_getFamily(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->family;
}
static const wchar_t *_getName(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->name;
}
static const wchar_t *_getStyleName(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->styleName;
}
static PgFontWeight _getWeight(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->weight;
}
static bool _isMonospaced(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->panose[3] == 9; // PANOSE porportion = 9 (monospaced)
}
static bool _isItalic(const PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->is_italic;
}
static int _getCount(PgFont *font) {
    PgOpenType *otf = (PgOpenType*)font;
    return otf->nfonts;
}
static void _free(PgFont *font) {
    if (font) {
        PgOpenType *otf = (PgOpenType*)font;
        _$(_freeHost, font);
        free((void*)otf->family);
        free((void*)otf->styleName);
        free((void*)otf->name);
        free(otf->features);
        free(otf->subst);
        free(font);
    }
}
PgOpenType pgDefaultOpenType() {
    return (PgOpenType) {
        ._ = {
            .free = _free,
            .scale = _scale,
            .getCharPath = _getCharPath,
            .getGlyphPath = _getGlyphPath,
            .getGlyph = _getGlyph,
            .getUtf8Width = _getUtf8Width,
            .getStringWidth = _getStringWidth,
            .getAscender = _getAscender,
            .getDescender = _getDescender,
            .getLeading = _getLeading,
            .getXHeight = _getXHeight,
            .getCapHeight = _getCapHeight,
            .getEm = _getEm,
            .getGlyphLsb = _getGlyphLsb,
            .getGlyphWidth = _getGlyphWidth,
            .getCharLsb = _getCharLsb,
            .getCharWidth = _getCharWidth,
            .getSubscriptBox = _getSubscriptBox,
            .getSuperscriptBox = _getSuperscriptBox,
            .useFeatures = _useFeatures,
            .getFeatures = _getFeatures,
            .substituteGlyph = _substituteGlyph,
            .getFamily = _getFamily,
            .getName = _getName,
            .getStyleName = _getStyleName,
            .getWeight = _getWeight,
            .isMonospaced = _isMonospaced,
            .isItalic = _isItalic,
            .getCount = _getCount,
        }
    };
}
PgOpenType *pgLoadOpenType(const void *file, int font_index, bool scan_only) {
    if (!file) return NULL;
    uint32_t nfonts = 1;
    const void *head = NULL;
    const void *hhea = NULL;
    const void *maxp = NULL;
    const void *cmap = NULL;
    const void *hmtx = NULL;
    const void *glyf = NULL;
    const void *loca = NULL;
    const void *os2  = NULL;
    const void *name  = NULL;
    const void *gsub  = NULL;
    
    // Check that this is an OpenType
    {
        uint16_t ntab;
        uint32_t ver;
        const void *header = file;
collection_item:
        unpack(&header, "LSsss", &ver, &ntab);
        
        if (ver == 0x00010000) ;
        else if (ver == 'ttcf') { // TrueType Collection
            header = file;
            unpack(&header, "lLL", &ver, &nfonts);
            if (ver != 0x00010000 && ver != 0x00020000)
                return NULL;
            if (font_index >= nfonts)
                return NULL;
            header = (const char*)file + be32(((uint32_t*)header)[font_index]);
            goto collection_item;
        } else
            return NULL;
        
        // Find the tables we need
        for (unsigned i = 0; i < ntab; i++) {
            uint32_t tag, offset;
            unpack(&header, "LlLl", &tag, &offset);
            const void *address = (char*)file + offset;
            switch (tag) {
            case 'head': head = address; break;
            case 'hhea': hhea = address; break;
            case 'maxp': maxp = address; break;
            case 'hmtx': hmtx = address; break;
            case 'glyf': glyf = address; break;
            case 'loca': loca = address; break;
            case 'cmap': cmap = address; break;
            case 'name': name  = address; break;
            case 'OS/2': os2  = address; break;
            case 'GSUB': gsub = address; break;
            }
        }
    }
    
    PgOpenType *font = NEW(PgOpenType);
    *font = pgDefaultOpenType();
    font->hmtx = hmtx;
    font->glyf = glyf;
    font->loca = loca;
    font->gsub = gsub;
    font->lang = 'eng ';
    font->script = 'latn';
    
    // 'head' table
    {
        uint16_t em, long_loca;
        unpack(&head, "llllsSllllsssssssSs", &em, &long_loca);
        font->em = em;
        font->long_loca = long_loca;
    }
    
    // 'hhea' table
    {
        uint16_t nhmtx;
        unpack(&hhea, "lsssssssssssssssS", &nhmtx);
        font->nhmtx = nhmtx;
    }
    
    // 'os/2' table
    {
        int16_t ascender, descender, leading;
        int16_t subsx, subsy, subx, suby;
        int16_t supsx, supsy, supx, supy;
        int16_t x_height, cap_height;
        uint16_t style, weight, stretch;
        unpack(&os2, "ssSSsSSSSSSSSsssBBBBBBBBBBllllbbbbSssSSSssllSSsss",
            &weight,
            &stretch,
            &subsx, &subsy, &subx, &suby,
            &supsx, &supsy, &supx, &supy,
            &font->panose[0],&font->panose[1],&font->panose[2],&font->panose[3],&font->panose[4],
            &font->panose[5],&font->panose[6],&font->panose[7],&font->panose[8],&font->panose[9],
            &style,
            &ascender, &descender, &leading,
            &x_height, &cap_height);
        font->ascender = ascender;
        font->descender = descender;
        font->leading = leading
            ? leading
            : (font->em - (font->ascender - font->descender)) + font->em * .1f;
        font->x_height = x_height;
        font->cap_height = cap_height;
        font->subscript_box = pgRect(pgPt(subx,suby), pgPt(subx+subsx, suby+subsy));
        font->superscript_box = pgRect(pgPt(supx,supy), pgPt(supx+supsx, supy+supsy));
        font->weight = weight;
        font->stretch = stretch;
        font->is_italic = style & 0x101; // includes italics and oblique
    }
        
    // 'maxp' table
    {
        uint16_t nglyphs;
        unpack(&maxp, "lSsssssssssssss", &nglyphs);
        font->nglyphs = nglyphs;
    }
    
    // 'name' table
    {
        uint16_t nnames, name_string_offset;
        unpack(&name, "sSS", &nnames, &name_string_offset);
        const char *name_strings = (char*)name - 6 + name_string_offset;
        
        for (int i = 0; i < nnames; i++) {
            uint16_t platform, encoding, lang, id, len, off;
            unpack(&name, "SSSSSS", &platform, &encoding, &lang, &id, &len, &off);
            
            // Unicode
            if (platform == 0 || (platform == 3 && (encoding == 0 || encoding == 1) && lang == 0x0409)) {
                if (id == 1 || id == 2 || id == 4 || id == 16) {
                    const uint16_t *source = (uint16_t*)(name_strings + off);
                    uint16_t *output = malloc((len/2 + 1) * sizeof *output);
                    len /= 2; // length was in bytes
                    for (int i = 0; i < len; i++)
                        output[i] = be16(source[i]);
                    output[len] = 0;
                    if (id == 1)
                        font->family = output;
                    else if (id == 2)
                        font->styleName = output;
                    else if (id == 4)
                        font->name = output;
                    else if (id == 16) { // Preferred font family
                        free((void*)font->family);
                        font->family = output;
                    }
                }
            }
            // Mac
            else if (platform == 1 && encoding == 0)
                if (id == 1 || id == 2 || id == 4 || id == 16) {
                    const uint8_t *source = name_strings + off;
                    uint16_t *output = malloc((len + 1) * sizeof *output);
                    for (int i = 0; i < len; i++)
                        output[i] = source[i];
                    output[len] = 0;
                    
                    if (id == 1)
                        font->family = output;
                    else if (id == 2)
                        font->styleName = output;
                    else if (id == 4)
                        font->name = output;
                    else if (id == 16) { // Preferred font family
                        free((void*)font->family);
                        font->family = output;
                    }
                }
        }
        if (!font->family) font->family = wcsdup(L"");
        if (!font->styleName) font->styleName = wcsdup(L"");
        if (!font->name) font->name = wcsdup(L"");
    }
    
    // cmap table
    if (!scan_only) {
        uint16_t nencodings;
        const void *encodings = cmap;
        const void *encoding_table = NULL;
        unpack(&encodings, "sS", &nencodings);
        for (int i = 0; i < nencodings; i++) {
            uint16_t platform, encoding;
            uint32_t offset;
            unpack(&encodings, "SSL", &platform, &encoding, &offset);
            
            if ((platform == 3 || platform == 0) && encoding == 1) // Windows UCS (preferred)
                encoding_table = (char*)cmap + offset;
            else if (!encoding_table && platform == 1 && encoding == 0) // Symbol
                encoding_table = (char*)cmap + offset;
        }
        
        if (encoding_table)
            switch (be16(*(uint16_t*)encoding_table)) {
            case 0: {
                    unpack(&encoding_table, "sssssss");
                    for (int i = 0; i < 256; i++)
                        font->cmap[i] = ((uint8_t*)encoding_table)[i];
                    break;
                }
            case 4: {
                    uint16_t nseg;
                    unpack(&encoding_table, "sssSsss", &nseg);
                    nseg /= 2;
                    
                    const uint16_t *endp    = encoding_table;
                    const uint16_t *startp  = endp + nseg + 1;
                    const uint16_t *deltap  = startp + nseg;
                    const uint16_t *offsetp = deltap + nseg;
                    for (int i = 0; i < nseg; i++) {
                        int end     = be16(endp[i]);
                        int start   = be16(startp[i]);
                        int delta   = be16(deltap[i]);
                        int offset  = be16(offsetp[i]);
                        if (offset)
                            for (int c = start; c <= end; c++) {
                                int16_t index = offset/2 + (c - start) + i; // TODO: why must this be 16-bit maths (faults on monofur)
                                int g = be16(offsetp[index]);
                                font->cmap[c] = g? g + delta: 0;
                            }
                        else
                            for (int c = start; c <= end; c++)
                                font->cmap[c] = c + delta;
                    }
                }
                break;
            }
    }
    
    font->_.file = file;
    font->scale_x = font->scale_y = 1;
    font->nfonts = nfonts;
    return font;
}