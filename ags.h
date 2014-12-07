// Amazing Graphics System
#include <stdbool.h>
#include <stdint.h>

typedef struct { float x, y; } AgsPoint;
typedef struct { AgsPoint a, b; } AgsRect;
typedef struct { float a, b, c, d, e, f; } AgsMatrix;
typedef enum { AGS_NONZERO_WINDING, AGS_EVENODD_WINDING } AgsFillRule;
typedef struct {
    uint32_t    *buf;
    int         width;
    int         height;
    float       flatness;
    float       subsamples;
    AgsMatrix   ctm;
} Ags;

typedef enum {
    AGS_PATH_SUBPATH    = 0,
    AGS_PATH_LINE       = 1,
    AGS_PATH_BEZIER3    = 2,
    AGS_PATH_BEZIER4    = 3,
} AgsPathPartType;
static int ags_path_part_type_args(AgsPathPartType type) {
    static int args[] = { 1, 1, 2, 3 };
    return args[type & 3];
}

typedef enum {
    AGS_FONT_WEIGHT_THIN = 100,
    AGS_FONT_WEIGHT_EXTRA_LIGHT = 200,
    AGS_FONT_WEIGHT_LIGHT = 300,
    AGS_FONT_WEIGHT_REGULAR = 400,
    AGS_FONT_WEIGHT_MEDIUM = 500,
    AGS_FONT_WEIGHT_SEMIBOLD = 600,
    AGS_FONT_WEIGHT_BOLD = 700,
    AGS_FONT_WEIGHT_EXTRABOLD = 800,
    AGS_FONT_WEIGHT_BLACK = 900,
} AgsFontWeight;
typedef enum {
    AGS_FONT_STRETCH_ULTRA_CONDENSED = 1,
    AGS_FONT_STRETCH_EXTRA_CONDENSED = 2,
    AGS_FONT_STRETCH_CONDENSED = 3,
    AGS_FONT_STRETCH_SEMI_CONDENSED = 4,
    AGS_FONT_STRETCH_NORMAL = 5,
    AGS_FONT_STRETCH_SEMI_EXPANDED = 6,
    AGS_FONT_STRETCH_EXPANDED = 7,
    AGS_FONT_STRETCH_EXTRA_EXPANDED = 8,
    AGS_FONT_STRETCH_ULTRA_EXPANDED = 9,
} AgsFontStretch;

typedef struct {
    int             nparts;
    int             npoints;
    int             cap;
    AgsPathPartType *types;
    AgsPoint        *points;
    AgsPoint        start;
    AgsFillRule     fill_rule;
} AgsPath;

typedef struct AgsFont {
    const void  *file;
    void        *host;
    void        (*host_free)(struct AgsFont*);
} AgsFont;

typedef struct {
    const wchar_t   *name;
    const wchar_t   *roman[10];
    const wchar_t   *italic[10];
    uint8_t         roman_index[10];
    uint8_t         italic_index[10];
} AgsFontFamily;

typedef struct {
    AgsFont     base;
    
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
    AgsRect     subscript_box;
    AgsRect     superscript_box;
    
    // Style
    int         weight;
    int         stretch;
    uint8_t     panose[10];
    bool        is_italic;
    const wchar_t *family;
    const wchar_t *style_name;
    const wchar_t *name;
        
    uint16_t    cmap[65536];
} AgsOTF;

const static AgsMatrix AgsIdentityMatrix = { 1, 0, 0, 1, 0, 0 };

static AgsPoint ags_pt(float x, float y) { AgsPoint p = { x, y }; return p; }
static AgsRect ags_rect(AgsPoint a, AgsPoint b) { AgsRect r; r.a=a;r.b=b; return r; }
static uint32_t ags_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xff000000 | r << 16 | g << 8 | b;
}
uint32_t ags_blend(uint32_t bg, uint32_t fg, uint32_t a);

// GRAPHICS STATE MANAGEMENT
Ags *ags_new(void *buf, int width, int height);
void ags_free(Ags *gs);
void ags_load_identity(Ags *gs);
void ags_translate(Ags *gs, float x, float y);
void ags_scale(Ags *gs, float x, float y);
void ags_shear(Ags *gs, float x, float y);
void ags_rotate(Ags *gs, float rad);
AgsPoint ags_transform_point(const AgsMatrix *ctm, AgsPoint p);
void ags_multiply_matrix(Ags *gs, const AgsMatrix * __restrict mat);

// MATRIX
void ags_matrix_identity(AgsMatrix *mat);
void ags_matrix_translate(AgsMatrix *mat, float x, float y);
void ags_matrix_scale(AgsMatrix *mat, float x, float y);
void ags_matrix_shear(AgsMatrix *mat, float x, float y);
void ags_matrix_rotate(AgsMatrix *mat, float rad);
void ags_matrix_multiply(AgsMatrix * __restrict a, const AgsMatrix * __restrict b);

// IMMEDIATE MODE DRAWING
void ags_clear(
    const Ags *gs,
    uint32_t color);
void ags_stroke_line(
    const Ags *gs,
    AgsPoint a,
    AgsPoint b,
    uint32_t color);
void ags_stroke_rectangle(
    const Ags *gs,
    AgsPoint nw,
    AgsPoint se,
    uint32_t color);
void ags_fill_rectangle(
    const Ags *gs,
    AgsPoint nw,
    AgsPoint se,
    uint32_t color);
void ags_stroke_circle(
    const Ags *gs,
    AgsPoint at,
    float radius,
    uint32_t color);
void ags_fill_circle(
    const Ags *gs,
    AgsPoint centre,
    float radius,
    uint32_t color);
void ags_stroke_bezier3(
    const Ags *gs,
    AgsPoint a,
    AgsPoint b,
    AgsPoint c,
    uint32_t color);
void ags_stroke_bezier4(
    const Ags *gs,
    AgsPoint a,
    AgsPoint b,
    AgsPoint c,
    AgsPoint d,
    uint32_t color);

// PATHS
AgsPath *ags_new_path(void);
void ags_free_path(AgsPath *path);
AgsRect ags_get_bounding_box(AgsPath *path);
void ags_add_subpath(
    AgsPath *path,
    const AgsMatrix *ctm,
    AgsPoint p);
void ags_close_subpath(AgsPath *path);
void ags_add_line(
    AgsPath *path,
    const AgsMatrix *ctm,
    AgsPoint b);
void ags_add_bezier3(
    AgsPath *path,
    const AgsMatrix *ctm,
    AgsPoint b,
    AgsPoint c);
void ags_add_bezier4(
    AgsPath *path,
    const AgsMatrix *ctm,
    AgsPoint b,
    AgsPoint c,
    AgsPoint d);
void ags_stroke_path(
    const Ags *gs,
    const AgsPath *path,
    uint32_t color);
void ags_fill_path(
    const Ags *gs,
    const AgsPath *path,
    uint32_t color);


// FONTS & TEXT
    AgsFont *ags_open_font_file(const wchar_t *filename, int font_index, bool scan_only);
    AgsFont *ags_open_font_variant(const wchar_t *family, AgsFontWeight weight, bool italic, AgsFontStretch stretch);
    AgsFontFamily *ags_scan_fonts(const wchar_t *dir, int *countp);
    void ags_free_font_family(AgsFontFamily *family);
    int ags_get_font_font_count(AgsFont *font);
    wchar_t **ags_list_fonts(int *countp);
    AgsFont *ags_load_font(const void *file, int font_index, bool scan_only);
    void ags_free_font(AgsFont *font);
    void ags_scale_font(AgsFont *font, float height, float width);
    float ags_get_font_ascender(const AgsFont *font);
    float ags_get_font_descender(const AgsFont *font);
    float ags_get_font_leading(const AgsFont *font);
    float ags_get_font_em(const AgsFont *font);
    AgsFontWeight ags_get_font_weight(const AgsFont *font);
    AgsFontStretch ags_get_font_stretch(const AgsFont *font);
    AgsRect ags_get_font_subscript(const AgsFont *font);
    AgsRect ags_get_font_superscript(const AgsFont *font);
    bool ags_is_font_monospaced(const AgsFont *font);
    bool ags_is_font_italic(const AgsFont *font);
    const wchar_t *ags_get_font_family(const AgsFont *font);
    const wchar_t *ags_get_font_name(const AgsFont *font);
    const wchar_t *ags_get_font_style_name(const AgsFont *font);
    char *ags_get_font_features(const AgsFont *font);
    void ags_set_font_features(AgsFont *font, const uint8_t *features);
    
    // CHARACTER BASED
        float ags_get_char_lsb(const AgsFont *font, unsigned c);
        float ags_get_char_width(const AgsFont *font, unsigned c);
        float ags_get_chars_width(const AgsFont *font, const char chars[], int len);
        float ags_get_wchars_width(const AgsFont *font, const wchar_t chars[], int len);
        float ags_fill_char(
            Ags *gs,
            const AgsFont *font,
            AgsPoint at,
            unsigned g,
            uint32_t color);
        float ags_fill_string_utf8(
            Ags *gs,
            const AgsFont *font,
            AgsPoint at,
            const uint8_t chars[],
            int len,
            uint32_t color);
        float ags_fill_string(
            Ags *gs,
            const AgsFont *font,
            AgsPoint at,
            const uint16_t chars[],
            int len,
            uint32_t color);
        AgsPath *ags_get_char_path(
            const AgsFont *font,
            const AgsMatrix *ctm,
            unsigned c);
    // GLYPH BASED
        unsigned ags_get_glyph(const AgsFont *font, unsigned c);
        float ags_get_glyph_lsb(const AgsFont *font, unsigned g);
        float ags_get_glyph_width(const AgsFont *font, unsigned g);
        float ags_fill_glyph(
            Ags *gs,
            const AgsFont *font,
            AgsPoint at,
            unsigned g,
            uint32_t color);
        AgsPath *ags_get_glyph_path(
            const AgsFont *font,
            const AgsMatrix *ctm,
            unsigned g);

// EXTERNAL FORMATS

// UNICODE
    uint16_t *ags_utf8_to_utf16(const uint8_t *input, int len, int *lenp);
    uint8_t *ags_utf16_to_utf8(const uint16_t *input, int len, int *lenp);

// OPENTYPE FORMAT (OTF) / TRUE TYPE FORMAT (TTF) FONTS
    AgsOTF *ags_load_otf(const void *file, int font_index, bool scan_only);
    int ags_get_otf_font_count(AgsOTF*font);
    void ags_free_otf(AgsOTF *font);
    void ags_scale_otf(AgsOTF *font, float height, float width);
    float ags_get_otf_ascender(const AgsOTF *font);
    float ags_get_otf_descender(const AgsOTF *font);
    float ags_get_otf_leading(const AgsOTF *font);
    float ags_get_otf_em(const AgsOTF *font);
    AgsFontWeight ags_get_otf_weight(const AgsOTF *font);
    AgsFontStretch ags_get_otf_stretch(const AgsOTF *font);
    AgsRect ags_get_otf_subscript(const AgsOTF *font);
    AgsRect ags_get_otf_superscript(const AgsOTF *font);
    bool ags_otf_is_monospaced(const AgsOTF *font);
    bool ags_otf_is_italic(const AgsOTF *font);
    const wchar_t *ags_get_otf_family(const AgsOTF *font);
    const wchar_t *ags_get_otf_name(const AgsOTF *font);
    const wchar_t *ags_get_otf_style_name(const AgsOTF *font);
    

    // CHARACTER-BASED
    float ags_get_otf_char_lsb(const AgsOTF *font, unsigned c);
    float ags_get_otf_char_width(const AgsOTF *font, unsigned c);
    float ags_otf_draw_char(
        Ags *gs,
        const AgsOTF *font,
        AgsPoint at,
        unsigned c,
        uint32_t color);
    float ags_otf_fill_string_utf8(
        Ags *gs,
        const AgsOTF *font,
        AgsPoint at,
        const char chars[],
        int len,
        uint32_t color);
    float ags_otf_fill_string(
        Ags *gs,
        const AgsOTF *font,
        AgsPoint at,
        const wchar_t chars[],
        int len,
        uint32_t color);
    AgsPath *ags_get_otf_glyph_path(
        const AgsOTF *font,
        const AgsMatrix *ctm,
        unsigned c);

    // GLYPH-BASED
    unsigned ags_get_otf_glyph(const AgsOTF *font, unsigned c);
    void ags_substitute_otf_glyph(AgsOTF *font, uint16_t in, uint16_t out);
    float ags_get_otf_glyph_lsb(const AgsOTF *font, unsigned g);
    float ags_get_otf_glyph_width(const AgsOTF *font, unsigned g);
    float ags_otf_draw_glyph(
        Ags *gs,
        const AgsOTF *font,
        AgsPoint at,
        unsigned g,
        uint32_t color);
    AgsPath *ags_get_otf_glyph_path(
        const AgsOTF *font,
        const AgsMatrix *ctm,
        unsigned g);



// SVG
    AgsPath *ags_get_svg_path(
        const char *svg,
        const AgsMatrix *initial_ctm);