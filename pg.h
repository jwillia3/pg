// Plain Graphics Library
#include <stdbool.h>
#include <stdint.h>

typedef struct { float x, y; } PgPt;
typedef struct { PgPt a, b; } PgRect;
typedef struct { float a, b, c, d, e, f; } PgMatrix;
typedef enum { PG_NONZERO_WINDING, PG_EVENODD_WINDING } PgFillRule;
typedef struct {
    uint32_t    *buf;
    int         width;
    int         height;
    float       flatness;
    float       subsamples;
    PgMatrix   ctm;
} Pg;

typedef enum {
    PG_PATH_SUBPATH    = 0,
    PG_PATH_LINE       = 1,
    PG_PATH_BEZIER3    = 2,
    PG_PATH_BEZIER4    = 3,
} PgPathPartType;
static int pgPathPartTypeArgs(PgPathPartType type) {
    static int args[] = { 1, 1, 2, 3 };
    return args[type & 3];
}

typedef enum {
    PG_FONT_WEIGHT_THIN = 100,
    PG_FONT_WEIGHT_EXTRA_LIGHT = 200,
    PG_FONT_WEIGHT_LIGHT = 300,
    PG_FONT_WEIGHT_REGULAR = 400,
    PG_FONT_WEIGHT_MEDIUM = 500,
    PG_FONT_WEIGHT_SEMIBOLD = 600,
    PG_FONT_WEIGHT_BOLD = 700,
    PG_FONT_WEIGHT_EXTRABOLD = 800,
    PG_FONT_WEIGHT_BLACK = 900,
} PgFontWeight;
typedef enum {
    PG_FONT_STRETCH_ULTRA_CONDENSED = 1,
    PG_FONT_STRETCH_EXTRA_CONDENSED = 2,
    PG_FONT_STRETCH_CONDENSED = 3,
    PG_FONT_STRETCH_SEMI_CONDENSED = 4,
    PG_FONT_STRETCH_NORMAL = 5,
    PG_FONT_STRETCH_SEMI_EXPANDED = 6,
    PG_FONT_STRETCH_EXPANDED = 7,
    PG_FONT_STRETCH_EXTRA_EXPANDED = 8,
    PG_FONT_STRETCH_ULTRA_EXPANDED = 9,
} PgFontStretch;

typedef struct {
    int             nparts;
    int             npoints;
    int             cap;
    PgPathPartType *types;
    PgPt        *points;
    PgPt        start;
    PgFillRule     fillRule;
} PgPath;

typedef struct PgFont {
    const void  *file;
    void        *host;
    void        (*freeHost)(struct PgFont*);
} PgFont;

typedef struct {
    const wchar_t   *name;
    const wchar_t   *roman[10];
    const wchar_t   *italic[10];
    uint8_t         romanIndex[10];
    uint8_t         italicIndex[10];
} PgFontFamily;

typedef struct {
    PgFont     base;
    
    float       scale_x;
    float       scale_y;
    
    int         nglyphs;
    int         nhmtx;
    int         long_loca;
    int         nfonts;
    
    // Table pointers
    
    const void  *glyf;
    const void  *loca;
    const uint16_t *hmtx;
    const uint8_t *gsub;
    
    
    uint8_t     (*features)[4];
    uint32_t    script;
    uint32_t    lang;
    uint16_t    (*subst)[2];
    int         nsubst;
    
    // Metrics
    float       em;
    float       ascender;
    float       descender;
    float       leading;
    float       x_height;
    float       cap_height;
    PgRect     subscript_box;
    PgRect     superscript_box;
    
    // Style
    int         weight;
    int         stretch;
    uint8_t     panose[10];
    bool        is_italic;
    const wchar_t *family;
    const wchar_t *styleName;
    const wchar_t *name;
        
    uint16_t    cmap[65536];
} PgOpenType;

const static PgMatrix PgIdentityMatrix = { 1, 0, 0, 1, 0, 0 };

// MISCELLANEOU
    static PgPt pgPt(float x, float y) { PgPt p = { x, y }; return p; }
    static PgRect pgRect(PgPt a, PgPt b) { PgRect r; r.a=a;r.b=b; return r; }
    static uint32_t pgRgb(uint8_t r, uint8_t g, uint8_t b) {
        return 0xff000000 | r << 16 | g << 8 | b;
    }
    uint32_t pgBlend(uint32_t bg, uint32_t fg, uint32_t a);
    uint16_t *pgUtf8To16(const uint8_t *input, int len, int *lenp);
    uint8_t *pgUtf16To8(const uint16_t *input, int len, int *lenp);

Pg *pgNew(void *buf, int width, int height);
void pgFree(Pg *gs);
void pgClear(const Pg *gs, uint32_t color);
PgPt pgTransformPoint(const PgMatrix *ctm, PgPt p);
void pgLoadIdentity(Pg *gs);
void pgTranslate(Pg *gs, float x, float y);
void pgScale(Pg *gs, float x, float y);
void pgShear(Pg *gs, float x, float y);
void pgRotate(Pg *gs, float rad);
void pgMultiply(Pg *gs, const PgMatrix * __restrict mat);
void pgIdentityMatrix(PgMatrix *mat);
void pgTranslateMatrix(PgMatrix *mat, float x, float y);
void pgScaleMatrix(PgMatrix *mat, float x, float y);
void pgShearMatrix(PgMatrix *mat, float x, float y);
void pgRotateMatrix(PgMatrix *mat, float rad);
void pgMultiplyMatrix(PgMatrix * __restrict a, const PgMatrix * __restrict b);
PgPath *pgNewPath(void);
void pgFreePath(PgPath *path);
void pgSubpath(PgPath *path, const PgMatrix *ctm, PgPt p);
void pgClosePath(PgPath *path);
void pgLine(PgPath *path, const PgMatrix *ctm, PgPt b);
void pgQuadratic(PgPath *path, const PgMatrix *ctm, PgPt b, PgPt c);
void pgCubic(PgPath *path, const PgMatrix *ctm, PgPt b, PgPt c, PgPt d);
PgRect pgGetBoundingBox(PgPath *path);
void pgFill(const Pg *gs, const PgPath *path, uint32_t color);

void pgFreeFontFamily(PgFontFamily *family);
PgFontFamily *pgScanFonts(const wchar_t *dir, int *countp);
int pgGetOpenTypeFontCount(PgOpenType *font);

PgFont *pgOpenFont(const wchar_t *family, PgFontWeight weight, bool italic, PgFontStretch stretch);
PgFont *pgOpenFontFile(const wchar_t *filename, int font_index, bool scan_only);
PgFont *pgLoadFont(const void *file, int font_index, bool scan_only);
void pgFreeFont(PgFont *font);
int pgGetFontCount(PgFont *font);
void pgScaleFont(PgFont *font, float height, float width);
void pgSetFontFeatures(PgFont *font, const uint8_t *features);
void pgSubstituteGlyph(PgFont *font, uint16_t in, uint16_t out);

float pgGetFontDescender(const PgFont *font);
float pgGetFontLeading(const PgFont *font);
float pgGetFontEm(const PgFont *font);
float pgGetFontXHeight(const PgFont *font);
float pgGetFontCapHeight(const PgFont *font);
PgFontWeight pgGetFontWeight(const PgFont *font);
PgFontStretch pgGetFontStretch(const PgFont *font);
PgRect pgGetFontSubscript(const PgFont *font);
PgRect pgGetFontSuperscript(const PgFont *font);
bool pgIsFontMonospaced(const PgFont *font);
bool pgIsFontItalic(const PgFont *font);
const wchar_t *pgGetFontFamily(const PgFont *font);
const wchar_t *pgGetFontName(const PgFont *font);
const wchar_t *pgGetFontStyleName(const PgFont *font);
char *pgGetFontFeatures(const PgFont *font);

float pgGetCharLsb(const PgFont *font, unsigned c);
float pgGetCharWidth(const PgFont *font, unsigned c);
float pgGetStringWidthUtf8(const PgFont *font, const char chars[], int len);
float pgGetStringWidth(const PgFont *font, const wchar_t chars[], int len);
float pgFillChar(Pg *gs, const PgFont *font, PgPt at, unsigned c, uint32_t color);
float pgFillStringUtf8(Pg *gs, const PgFont *font, PgPt at, const uint8_t chars[], int len, uint32_t color);
float pgFillString(Pg *gs, const PgFont *font, PgPt at, const wchar_t chars[], int len, uint32_t color);
PgPath *pgGetCharPath(const PgFont *font, const PgMatrix *ctm, unsigned c);
unsigned pgGetGlyph(const PgFont *font, unsigned c);
float pgGetGlyphLsb(const PgFont *font, unsigned g);
float pgGetGlyphWidth(const PgFont *font, unsigned g);
float pgFillGlyph(Pg *gs, const PgFont *font, PgPt at, unsigned g, uint32_t color);
PgPath *pgGetGlyphPath(const PgFont *font, const PgMatrix *ctm, unsigned g);

PgOpenType *pgLoadOpenType(const void *file, int font_index, bool scan_only);

PgPath *pgInterpretSvgPath(const char *svg, const PgMatrix *initial_ctm);
