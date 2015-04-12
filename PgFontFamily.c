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

static PgFontFamily    *Families;
static int              NFamilies;

static void scanFontsPerFile(const wchar_t *filename, void *data) {
    int nfonts = 1;
    for (int index = 0; index < nfonts; index++) {
        PgFont *font = pgLoadFont(data, index, true);
        if (!font) continue;
        nfonts = $(getCount, font);
        
        const wchar_t *family = $(getFamily, font);
        int i, dir = 0;
        
        for (i = 0; i < NFamilies; i++) {
            dir = wcsicmp(family, Families[i].name);
            if (dir <= 0)
                break;
        }
        
        if (dir < 0 || i == NFamilies) {
            Families = realloc(Families, (NFamilies + 1) * sizeof *Families);
            for (int j = NFamilies; j > i; j--)
                Families[j] = Families[j - 1];
            
            memset(&Families[i], 0, sizeof *Families);
            Families[i].name = wcsdup(family);
            NFamilies++;
        }
        
        int weight = $(getWeight, font) / 100;
        if (weight >= 0 && weight < 10) {
            const wchar_t **slot = $(isItalic, font)
                ? &Families[i].italic[weight]
                : &Families[i].roman[weight];
            
            if (*slot)
                free((void*)*slot);
            *slot = wcsdup(filename);
            if ($(isItalic, font))
                Families[i].italicIndex[weight] = index;
            else
                Families[i].romanIndex[weight] = index;
        }
        $(free, font);
    }
}

void pgFreeFontFamily(PgFontFamily *family) {
    if (family) {
        free((void*)family->name);
        for (int i = 0; i < 10; i++) {
            free((void*)family->roman[i]);
            free((void*)family->italic[i]);
        }
    }
}
static PgFontFamily copyFontFamily(PgFontFamily *src) {
    PgFontFamily out;
    
    out.name = wcsdup(src->name);
    for (int i = 0; i < 10; i++) {
        out.roman[i] = src->roman[i];
        out.italic[i] = src->italic[i];
        out.romanIndex[i] = src->romanIndex[i];
        out.italicIndex[i] = src->italicIndex[i];
    }
    return out;
}

PgFontFamily *pgScanFonts(const wchar_t *dir, int *countp) {
//    for (int i = 0; i < NFamilies; i++)
//        pgFreeFontFamily(&Families[i]);
    Families = NULL;
    NFamilies = 0;
    _pgScanDirectory(dir, scanFontsPerFile);
    
    // Copy families
    PgFontFamily *families = malloc(NFamilies * sizeof *families);
    for (int i = 0; i < NFamilies; i++) 
        families[i] = copyFontFamily(&Families[i]);
    
    if (countp) *countp = NFamilies;
    return families;
}
PgFont *pgOpenFont(const wchar_t *family, PgFontWeight weight, bool italic, PgFontStretch stretch) {
    if (weight < 100) weight = 400;
    if (stretch < 1) stretch = 0;
    if (weight < 900 && stretch < 900) {
        if (Families == NULL)
            pgScanFonts(NULL, NULL);
        
        int f;
        for (f = 0; f < NFamilies; f++)
            if (!wcsicmp(Families[f].name, family))
                break;
        
        if (f != NFamilies) {
            uint8_t *index = italic? Families[f].italicIndex: Families[f].romanIndex;
            const wchar_t **filename = italic? Families[f].italic: Families[f].roman;
            
            if (filename[weight/100])
                return pgOpenFontFile(filename[weight/100], index[weight/100], false);
            if (weight/100+1 <= 9 && filename[weight/100+1])
                return pgOpenFontFile(filename[weight/100+1], index[weight/100+1], false);
            if (weight/100-1 >= 0 && filename[weight/100-1])
                return pgOpenFontFile(filename[weight/100-1], index[weight/100-1], false);
        }
    }
    return NULL;
}
PgFont *pgOpenFontFile(const wchar_t *filename, int font_index, bool scan_only) {
    return _pgOpenFontFile(filename, font_index, scan_only);
}

PgFont *pgLoadFont(const void *file, int font_index, bool scan_only) {
    return (void*)pgLoadOpenType((void*)file, font_index, scan_only);
}