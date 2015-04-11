// Plain Graphics Library
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
    PgFillRule     fill_rule;
} PgPath;

typedef struct PgFont {
    const void  *file;
    void        *host;
    void        (*host_free)(struct PgFont*);
} PgFont;

typedef struct {
    const wchar_t   *name;
    const wchar_t   *roman[10];
    const wchar_t   *italic[10];
    uint8_t         roman_index[10];
    uint8_t         italic_index[10];
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
    const wchar_t *style_name;
    const wchar_t *name;
        
    uint16_t    cmap[65536];
} PgOpenType;

const static PgMatrix PgIdentityMatrix = { 1, 0, 0, 1, 0, 0 };

static PgPt pgPt(float x, float y) { PgPt p = { x, y }; return p; }
static PgRect pgRect(PgPt a, PgPt b) { PgRect r; r.a=a;r.b=b; return r; }
static uint32_t pgRgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xff000000 | r << 16 | g << 8 | b;
}
uint32_t pgBlend(uint32_t bg, uint32_t fg, uint32_t a);

uint16_t *pgUtf8To16(const uint8_t *input, int len, int *lenp);
uint8_t *pgUtf16To8(const uint16_t *input, int len, int *lenp);

// GRAPHICS STATE MANAGEMENT
Pg *pgNew(void *buf, int width, int height);
void pgFree(Pg *gs);
void pgLoadIdentity(Pg *gs);
void pgTranslate(Pg *gs, float x, float y);
void pgScale(Pg *gs, float x, float y);
void pgShear(Pg *gs, float x, float y);
void pgRotate(Pg *gs, float rad);
PgPt pgTransformPoint(const PgMatrix *ctm, PgPt p);
void pgMultiply(Pg *gs, const PgMatrix * __restrict mat);

// MATRIX
void pgIdentityMatrix(PgMatrix *mat);
void pgTranslateMatrix(PgMatrix *mat, float x, float y);
void pgScaleMatrix(PgMatrix *mat, float x, float y);
void pgShearMatrix(PgMatrix *mat, float x, float y);
void pgRotateMatrix(PgMatrix *mat, float rad);
void pgMultiplyMatrix(PgMatrix * __restrict a, const PgMatrix * __restrict b);

// IMMEDIATE MODE DRAWING
void pgClear(
    const Pg *gs,
    uint32_t color);
void pgStrokeLine(
    const Pg *gs,
    PgPt a,
    PgPt b,
    uint32_t color);
void pgStrokeRectangle(
    const Pg *gs,
    PgPt nw,
    PgPt se,
    uint32_t color);
void pgFillRectangle(
    const Pg *gs,
    PgPt nw,
    PgPt se,
    uint32_t color);
void pgStrokeCircle(
    const Pg *gs,
    PgPt at,
    float radius,
    uint32_t color);
void pgFillCircle(
    const Pg *gs,
    PgPt centre,
    float radius,
    uint32_t color);
void psStrokeQuadratic(
    const Pg *gs,
    PgPt a,
    PgPt b,
    PgPt c,
    uint32_t color);
void psStrokeCubic(
    const Pg *gs,
    PgPt a,
    PgPt b,
    PgPt c,
    PgPt d,
    uint32_t color);

// PATHS
PgPath *pgNew_path(void);
void pgFree_path(PgPath *path);
PgRect pgGetBoundingBox(PgPath *path);
void pgSubpath(
    PgPath *path,
    const PgMatrix *ctm,
    PgPt p);
void pgClosePath(PgPath *path);
void pgLine(
    PgPath *path,
    const PgMatrix *ctm,
    PgPt b);
void pgQuadratic(
    PgPath *path,
    const PgMatrix *ctm,
    PgPt b,
    PgPt c);
void pgCubic(
    PgPath *path,
    const PgMatrix *ctm,
    PgPt b,
    PgPt c,
    PgPt d);
void pgStroke(
    const Pg *gs,
    const PgPath *path,
    uint32_t color);
void pgFill(
    const Pg *gs,
    const PgPath *path,
    uint32_t color);


// FONTS & TEXT
    PgFont *pgOpenFontFile(const wchar_t *filename, int font_index, bool scan_only);
    PgFont *pgOpenFont(const wchar_t *family, PgFontWeight weight, bool italic, PgFontStretch stretch);
    PgFontFamily *pgScanFonts(const wchar_t *dir, int *countp);
    void pgFree_font_family(PgFontFamily *family);
    int pgGetFontCount(PgFont *font);
    wchar_t **pgListFonts(int *countp);
    PgFont *pgLoadFont(const void *file, int font_index, bool scan_only);
    void pgFree_font(PgFont *font);
    void pgScale_font(PgFont *font, float height, float width);
    float pgGetFontAscender(const PgFont *font);
    float pgGetFontDescender(const PgFont *font);
    float pgGetFontLeading(const PgFont *font);
    float pgGetFontEm(const PgFont *font);
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
    void pgSetFontFeatures(PgFont *font, const uint8_t *features);
    
    // CHARACTER BASED
        float pgGetCharLsb(const PgFont *font, unsigned c);
        float pgGetCharWidth(const PgFont *font, unsigned c);
        float pgGetCharsWidthUtf8(const PgFont *font, const char chars[], int len);
        float pgGetCharsWidth(const PgFont *font, const wchar_t chars[], int len);
        float pgFillChar(
            Pg *gs,
            const PgFont *font,
            PgPt at,
            unsigned g,
            uint32_t color);
        float pgFillString_utf8(
            Pg *gs,
            const PgFont *font,
            PgPt at,
            const uint8_t chars[],
            int len,
            uint32_t color);
        float pgFillString(
            Pg *gs,
            const PgFont *font,
            PgPt at,
            const uint16_t chars[],
            int len,
            uint32_t color);
        PgPath *pgGetCharPath(
            const PgFont *font,
            const PgMatrix *ctm,
            unsigned c);
    // GLYPH BASED
        unsigned pgGetGlyph(const PgFont *font, unsigned c);
        float pgGetGlyph_lsb(const PgFont *font, unsigned g);
        float pgGetGlyph_width(const PgFont *font, unsigned g);
        float pgFillGlyph(
            Pg *gs,
            const PgFont *font,
            PgPt at,
            unsigned g,
            uint32_t color);
        PgPath *pgGetGlyph_path(
            const PgFont *font,
            const PgMatrix *ctm,
            unsigned g);

// EXTERNAL FORMATS

// UNICODE
    uint16_t *pgUtf8To16(const uint8_t *input, int len, int *lenp);
    uint8_t *pgUtf16To8(const uint16_t *input, int len, int *lenp);

// OPENTYPE FORMAT (OpenType) / TRUE TYPE FORMAT (TTF) FONTS
    PgOpenType *pgLoadOpenType(const void *file, int font_index, bool scan_only);
    int pgGetOpenTypeFontCount(PgOpenType*font);
    void pgFree_otf(PgOpenType *font);
    void pgScale_otf(PgOpenType *font, float height, float width);
    float getOpenTypeAscender(const PgOpenType *font);
    float getOpenTypeDescender(const PgOpenType *font);
    float getOpenTypeLeading(const PgOpenType *font);
    float getOpenTypeEm(const PgOpenType *font);
    PgFontWeight getOpenTypeWeight(const PgOpenType *font);
    PgFontStretch getOpenTypeStretch(const PgOpenType *font);
    PgRect getOpenTypeSubscript(const PgOpenType *font);
    PgRect getOpenTypeSuperscript(const PgOpenType *font);
    bool isOpenTypeMonospaced(const PgOpenType *font);
    bool isOpenTypeItalic(const PgOpenType *font);
    const wchar_t *getOpenTypeFamily(const PgOpenType *font);
    const wchar_t *getOpenTypeName(const PgOpenType *font);
    const wchar_t *getOpenTypeStyleName(const PgOpenType *font);
    

    // CHARACTER-BASED
    float getOpenTypeCharLsb(const PgOpenType *font, unsigned c);
    float getOpenTypeCharWidth(const PgOpenType *font, unsigned c);
    float pgFillChar(
        Pg *gs,
        const PgFont *font,
        PgPt at,
        unsigned c,
        uint32_t color);
    float fillOpenTypeString_utf8(
        Pg *gs,
        const PgOpenType *font,
        PgPt at,
        const char chars[],
        int len,
        uint32_t color);
    float fillOpenTypeString(
        Pg *gs,
        const PgOpenType *font,
        PgPt at,
        const wchar_t chars[],
        int len,
        uint32_t color);
    PgPath *getOpenTypeGlyphPath(
        const PgOpenType *font,
        const PgMatrix *ctm,
        unsigned c);

    // GLYPH-BASED
    unsigned getOpenTypeGlyph(const PgOpenType *font, unsigned c);
    void pgSubstituteOpenTypeGlyph(PgOpenType *font, uint16_t in, uint16_t out);
    float getOpenTypeGlyph_lsb(const PgOpenType *font, unsigned g);
    float getOpenTypeGlyph_width(const PgOpenType *font, unsigned g);
    float pgFillOpenTypeGlyph(
        Pg *gs,
        const PgOpenType *font,
        PgPt at,
        unsigned g,
        uint32_t color);
    PgPath *getOpenTypeGlyphPath(
        const PgOpenType *font,
        const PgMatrix *ctm,
        unsigned g);



// SVG
    PgPath *pgInterpretSvgPath(
        const char *svg,
        const PgMatrix *initial_ctm);