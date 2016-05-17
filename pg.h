// Plain Graphics Library
typedef struct { float x, y; } PgPt;
typedef struct { PgPt a, b; } PgRect;
typedef struct { float a, b, c, d, e, f; } PgMatrix;
typedef enum { PG_NONZERO_WINDING, PG_EVENODD_WINDING } PgFillRule;
typedef struct Pg Pg;
typedef struct PgPath PgPath;
typedef struct PgFont PgFont;
struct Pg {
    int         width;
    int         height;
    float       flatness;
    float       subsamples;
    PgMatrix    ctm;
    void        (*free)(Pg *g);
    void        (*resize)(Pg *g, int width, int height);
    void        (*clear)(const Pg *g, uint32_t color);
    void        (*clearSection)(const Pg *g, PgRect rect, uint32_t color);
    void        (*fill)(const Pg *g, const PgPath *path, uint32_t color);
    float       (*fillChar)(Pg *g, const PgFont *font, PgPt at, unsigned c, uint32_t color);
    float       (*fillUtf8)(Pg *g, const PgFont *font, PgPt at, const uint8_t chars[], int len, uint32_t color);
    float       (*fillString)(Pg *g, const PgFont *font, PgPt at, const wchar_t chars[], int len, uint32_t color);
    float       (*fillGlyph)(Pg *g, const PgFont *font, PgPt at, unsigned glyph, uint32_t color);
    void        (*identity)(Pg *g);
    void        (*translate)(Pg *g, float x, float y);
    void        (*scale)(Pg *g, float x, float y);
    void        (*shear)(Pg *g, float x, float y);
    void        (*rotate)(Pg *g, float rad);
    void        (*multiply)(Pg *g, const PgMatrix * __restrict mat);
};

typedef struct {
    Pg          _;
    uint32_t    *data;
} PgBitmapCanvas;

typedef enum {
    PG_PATH_MOVE       = 0,
    PG_PATH_LINE       = 1,
    PG_PATH_QUADRATIC  = 2,
    PG_PATH_CUBIC      = 3,
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

struct PgPath {
    int             nparts;
    int             npoints;
    int             cap;
    PgPathPartType *types;
    PgPt            *points;
    PgPt            start;
    PgFillRule      fillRule;
    
    void            (*free)(PgPath *path);
    void            (*move)(PgPath *path, const PgMatrix *ctm, PgPt p);
    void            (*close)(PgPath *path);
    void            (*line)(PgPath *path, const PgMatrix *ctm, PgPt b);
    void            (*quadratic)(PgPath *path, const PgMatrix *ctm, PgPt b, PgPt c);
    void            (*cubic)(PgPath *path, const PgMatrix *ctm, PgPt b, PgPt c, PgPt d);
    PgRect          (*box)(PgPath *path);
};

struct PgFont {
    const void  *file;
    void        *host;
    void        (*_freeHost)(PgFont *font);
    
    void        (*free)(PgFont *font);
    void        (*scale)(PgFont *font, float height, float width);
    PgPath      *(*getCharPath)(const PgFont *font, const PgMatrix *ctm, unsigned c);
    PgPath      *(*getGlyphPath)(const PgFont *font, const PgMatrix *ctm, unsigned g);
    unsigned    (*getGlyph)(const PgFont *font, unsigned c);
    
    // Metrics
    float       (*getUtf8Width)(const PgFont *font, const char chars[], int len);
    float       (*getStringWidth)(const PgFont *font, const wchar_t chars[], int len);
    float       (*getAscender)(const PgFont *font);
    float       (*getDescender)(const PgFont *font);
    float       (*getLeading)(const PgFont *font);
    float       (*getXHeight)(const PgFont *font);
    float       (*getCapHeight)(const PgFont *font);
    float       (*getEm)(const PgFont *font);
    float       (*getGlyphLsb)(const PgFont *font, unsigned g);
    float       (*getGlyphWidth)(const PgFont *font, unsigned g);
    float       (*getCharLsb)(const PgFont *font, unsigned c);
    float       (*getCharWidth)(const PgFont *font, unsigned c);
    PgRect      (*getSubscriptBox)(const PgFont *font);
    PgRect      (*getSuperscriptBox)(const PgFont *font);
    
    // Features
    void        (*useFeatures)(PgFont *font, const uint8_t *features);
    char        *(*getFeatures)(const PgFont *font);
    void        (*substituteGlyph)(PgFont *font, uint16_t in, uint16_t out);
    
    // Font information
    const wchar_t *(*getFamily)(const PgFont *font);
    const wchar_t *(*getName)(const PgFont *font);
    const wchar_t *(*getStyleName)(const PgFont *font);
    PgFontWeight (*getWeight)(const PgFont *font);
    bool        (*isMonospaced)(const PgFont *font);
    bool        (*isItalic)(const PgFont *font);
    int         (*getCount)(PgFont *font);
};

typedef struct {
    const wchar_t   *name;
    const wchar_t   *roman[10];
    const wchar_t   *italic[10];
    uint8_t         romanIndex[10];
    uint8_t         italicIndex[10];
} PgFontFamily;

typedef struct {
    PgFont      _;
    
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
float PgGamma;

// MISCELLANEOUS
    void pgSetGamma(float gamma);
    static PgPt pgPt(float x, float y) { PgPt p = { x, y }; return p; }
    static PgRect pgRect(PgPt a, PgPt b) { PgRect r; r.a=a;r.b=b; return r; }
    static uint32_t pgRgb(uint8_t r, uint8_t g, uint8_t b) {
        return 0xff000000 | r << 16 | g << 8 | b;
    }
    uint32_t pgBlend(uint32_t bg, uint32_t fg, uint32_t a);
    uint16_t *pgUtf8To16(const uint8_t *input, int len, int *lenp);
    uint8_t *pgUtf16To8(const uint16_t *input, int len, int *lenp);

Pg pgDefaultCanvas();
PgBitmapCanvas pgDefaultBitmapCanvas();
Pg *pgNewBitmapCanvas(int width, int height);

PgPt pgTransformPoint(const PgMatrix *ctm, PgPt p);
void pgIdentityMatrix(PgMatrix *mat);
void pgTranslateMatrix(PgMatrix *mat, float x, float y);
void pgScaleMatrix(PgMatrix *mat, float x, float y);
void pgShearMatrix(PgMatrix *mat, float x, float y);
void pgRotateMatrix(PgMatrix *mat, float rad);
void pgMultiplyMatrix(PgMatrix * __restrict a, const PgMatrix * __restrict b);
PgPath *pgNewPath(void);
PgPath pgDefaultPath();

void pgFreeFontFamily(PgFontFamily *family);
PgFontFamily *pgScanFonts(const wchar_t *dir, int *countp);
int pgGetOpenTypeFontCount(PgOpenType *font);

PgFont *pgOpenFont(const wchar_t *family, PgFontWeight weight, bool italic, PgFontStretch stretch);
PgFont *pgOpenFontFile(const wchar_t *filename, int font_index, bool scan_only);
PgFont *pgLoadFont(const void *file, int font_index, bool scan_only);
PgOpenType *pgLoadOpenType(const void *file, int font_index, bool scan_only);
PgPath *pgInterpretSvgPath(const char *svg, const PgMatrix *initial_ctm);
