#define BEZIER_RECURSION_LIMIT 10
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

typedef struct {
    PgPt    a;
    PgPt    b;
    float   m;
    float   dir;
} Segment;

typedef struct {
    int     n;
    int     cap;
    Segment *segs;
} SegList;

static void addSeg(SegList *list, PgPt a, PgPt b) {
    if (list->n + 1 >= list->cap) {
        list->cap = list->cap? list->cap * 2: 128;
        list->segs = realloc(list->segs, list->cap * sizeof *list->segs);
    }
    list->segs[list->n].a = a.y < b.y? a: b;
    list->segs[list->n].b = a.y < b.y? b: a;
    list->segs[list->n].dir = a.y < b.y? -1: 1;
    list->n++;
}
static void decompQuad(SegList *list, PgPt a, PgPt b, PgPt c, float flatness, int n) {    
    if (!n) {
        addSeg(list, a, c);
        return;
    }
    PgPt m = { (a.x + 2 * b.x + c.x) / 4, (a.y + 2 * b.y + c.y) / 4 };
    PgPt d = { (a.x + c.x) / 2 - m.x, (a.y + c.y) / 2 - m.y };
    if (d.x * d.x + d.y * d.y > .05f) {
        decompQuad(list, a, pgPt((a.x + b.x) / 2, (a.y + b.y) / 2), m, flatness, n - 1);
        decompQuad(list, m, pgPt((b.x + c.x) / 2, (b.y + c.y) / 2), c, flatness, n - 1);
    } else
        addSeg(list, a, c);
}
static void decompCubic(SegList *list, PgPt a, PgPt b, PgPt c, PgPt d, float flatness, int n) {    
    if (!n) {
        addSeg(list, a, d);
        return;
    }
    float d1 = dist(a.x - b.x, a.y - b.y);
    float d2 = dist(b.x - c.x, b.y - c.y);
    float d3 = dist(c.x - d.x, c.y - d.y);
    float d4 = dist(a.x - d.x, a.y - d.y);
    if (d1 + d2 + d3 >= flatness * d4) {
        PgPt mab = mid(a, b);
        PgPt mbc = mid(b, c);
        PgPt mcd = mid(c, d);
        PgPt mabc = mid(mab, mbc);
        PgPt mbcd = mid(mbc, mcd);
        PgPt mabcd = mid(mabc, mbcd);
        decompCubic(list, a, mab, mabc, mabcd, flatness, n - 1);
        decompCubic(list, mabcd, mbcd, mcd, d, flatness, n - 1);
    } else
        addSeg(list, a, d);
}
static int sortTops(const void *ap, const void *bp) {
    const Segment * __restrict a = ap;
    const Segment * __restrict b = bp;
    return  a->a.y < b->a.y? -1:
            a->a.y > b->a.y? 1:
            a->a.x < b->a.x? -1:
            a->a.x > b->a.x? 1:
            0;
}
static void fillSegments(const Pg *g, const Segment *segs, int nsegs, uint32_t color) {
    typedef struct {
        float y0;
        float y1;
        float x;
        float m;
    } Edge;
    int                     max_y = INT_MIN;
    int                     min_y = INT_MAX; // calculated later
    int                     min_x = 0; // per-line minimum used x
    int                     max_x = g->width - 1; // per-line maximum used x
    for (int i = 0; i < nsegs; i++) {
        if (segs[i].a.y < min_y) min_y = segs[i].a.y;
        if (segs[i].b.y > max_y) max_y = segs[i].b.y;
    }
    min_y = clamp(0, min_y / g->subsamples + 1, g->height - 1);
    max_y = clamp(0, max_y / g->subsamples + 1, g->height - 1);
    
    uint8_t * __restrict    buffer = NEW_ARRAY(uint8_t, g->width);
    Edge * __restrict       edges = calloc(1, nsegs * sizeof *edges);
        
    // Rasterise each line
    int nedges = 0;
    float max_alpha = (color >> 24) / g->subsamples;
    for (int scan_y = min_y, min_seg = 0; scan_y <= max_y; scan_y++) {
        // Clear line buffer
        if (min_x <= max_x)
            memset(buffer + min_x, 0, max_x - min_x + 1);
        min_x = g->width;
        max_x = 0;
        
        for (int s = -g->subsamples / 2; s < g->subsamples / 2; s++) {
            float y = g->subsamples * scan_y + s + .5;
            
            int old_nedges = nedges;
            nedges = 0;
            for (int i = 0; i < old_nedges; i++)
                if (y < edges[i].y1) {
                    if (i != nedges)
                        edges[nedges] = edges[i];
                    edges[nedges].x += edges[nedges].m;
                    nedges++;
                }
            for ( ; min_seg < nsegs; min_seg++)
                if (segs[min_seg].a.y <= y) { // starts on or just before this scanline
                    if (y < segs[min_seg].b.y) { // ends after this scanline
                        edges[nedges].y0 = segs[min_seg].a.y;
                        edges[nedges].y1 = segs[min_seg].b.y;
                        edges[nedges].m = segs[min_seg].m;
                        edges[nedges].x = segs[min_seg].a.x +
                            segs[min_seg].m * (y - segs[min_seg].a.y);
                        nedges++;
                    } // starts and ends before this scanline
                } else // starts after this scanline
                    break;
            
            // Sort edges from left to right
            for (int i = 1; i < nedges; i++)
                for (int j = i; j > 0 && edges[j - 1].x > edges[j].x; j--) {
                    Edge tmp = edges[j];
                    edges[j] = edges[j - 1];
                    edges[j - 1] = tmp;
                }
        
            // Render edges
            for (int i = 1; i < nedges; i += 2) {
                int start = clamp(0, edges[i - 1].x, g->width - 1);
                int end = clamp(0, edges[i].x, g->width - 1);
                if (start < min_x) min_x = start;
                if (end > max_x) max_x = end;
                
                if (start == end)
                    if (start == (int)edges[i-1].x)
                        buffer[start] += (edges[i].x - edges[i - 1].x) * max_alpha;
                    else;
                else {
                    if (start == (int)edges[i-1].x)
                        buffer[start] += (1 - fraction(edges[i-1].x)) * max_alpha;
                    for (int i = start + 1; i < end; i++)
                        buffer[i] += max_alpha;
                    if (end == (int)edges[i].x)
                        buffer[end] += fraction(edges[i].x) * max_alpha;
                }
            }
        }

        // Copy buffer to screen
//        if ((g->width & 3) == 0) {
//            if (min_x & 3)
//                min_x &= ~3;
//            if (max_x & 3)
//                max_x = clamp(0, max_x + 3 & ~3, g->width - 1);
//            else
//                max_x = clamp(0, max_x + 4, g->width - 1);
//                
//            uint32_t * __restrict   screen = ((PgBitmapCanvas*)g)->data + scan_y * g->width;
//            __m128i fg = _mm_set_epi32(color, color, color, color);
//            for (int i = min_x; i < max_x; i += 4) {
//                __m128i a = _mm_setr_epi32(buffer[i], buffer[i+1], buffer[i+2], buffer[i+3]);
//                __m128i na = _mm_sub_epi32(_mm_set1_epi32(255), a);
//                __m128i bg = _mm_load_si128((__m128i*)(screen + i));
//                
//                __m128i rb_fg = _mm_and_si128(fg, _mm_set1_epi32(0x00ff00ff));
//                __m128i rb_bg = _mm_and_si128(bg, _mm_set1_epi32(0x00ff00ff));
//                __m128i  g_fg = _mm_and_si128(fg, _mm_set1_epi32(0x0000ff00));
//                __m128i  g_bg = _mm_and_si128(bg, _mm_set1_epi32(0x0000ff00));
//                
//                rb_fg = _mm_or_si128(
//                            _mm_mul_epu32(rb_fg, a),
//                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(rb_fg, 4), _mm_srli_si128(a, 4)), 4));
//                rb_bg = _mm_or_si128(
//                            _mm_mul_epu32(rb_bg, na),
//                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(rb_bg, 4), _mm_srli_si128(na, 4)), 4));
//                __m128i rb = _mm_and_si128(_mm_add_epi32(rb_bg, rb_fg), _mm_set1_epi32(0xff00ff00));
//                
//                g_fg = _mm_or_si128(
//                            _mm_mul_epu32(g_fg, a),
//                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(g_fg, 4), _mm_srli_si128(a, 4)), 4));
//                g_bg = _mm_or_si128(
//                            _mm_mul_epu32(g_bg, na),
//                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(g_bg, 4), _mm_srli_si128(na, 4)), 4));
//                __m128i g = _mm_and_si128(_mm_add_epi32(g_bg, g_fg), _mm_set1_epi32(0x00ff0000));
//                
//                *(__m128i*)(screen + i) = _mm_srli_epi32(_mm_or_si128(rb, g), 8);
//            }
//        } else
        {
            uint32_t * __restrict   screen = ((PgBitmapCanvas*)g)->data + scan_y * g->width;
            for (int i = min_x; i <= max_x; i++)
                if (buffer[i]) screen[i] = pgBlend(screen[i], color, buffer[i]);
        }
    }
    free(buffer);
    free(edges);
}
static void _fill(const Pg *g, const PgPath *path, uint32_t color) {
    if (path->nparts == 0) return;
    
    SegList list = { 0 };
    
    // Decompose curves into a list of lines
    PgPt a = {0, 0};
    for (int i = 0, ip = 0; i < path->nparts; ip += pgPathPartTypeArgs(path->types[i]), i++)
        switch (path->types[i]) {
        case PG_PATH_MOVE:
            a = path->points[ip];
            break;
        case PG_PATH_LINE:
            addSeg(&list, a, path->points[ip]);
            a = path->points[ip];
            break;
        case PG_PATH_QUADRATIC:
            decompQuad(&list, a, path->points[ip], path->points[ip+1], g->flatness, BEZIER_RECURSION_LIMIT);
            a = path->points[ip+1];
            break;
        case PG_PATH_CUBIC:
            decompCubic(&list, a, path->points[ip], path->points[ip+1], path->points[ip+2], g->flatness, BEZIER_RECURSION_LIMIT);
            a = path->points[ip+2];
            break;
        }
        
    // Subsample in Y direction
    for (int i = 0; i < list.n; i++) {
        list.segs[i].a.y *= g->subsamples;
        list.segs[i].b.y *= g->subsamples;
        list.segs[i].m = list.segs[i].a.y == list.segs[i].b.y
            ? 0
            : (list.segs[i].b.x - list.segs[i].a.x) / (list.segs[i].b.y - list.segs[i].a.y);
    }
    
    // Sort line segments by their tops
    const Segment *const __restrict segs = list.segs;
    const int nsegs = list.n;
    qsort(list.segs, nsegs, sizeof *segs, sortTops);
    
    fillSegments(g, segs, nsegs, color);
    free(list.segs);
}
static float _fillGlyph(Pg *gs, const PgFont *font, PgPt at, unsigned g, uint32_t color) {
    float width = $(getGlyphWidth, font, g);
    float em = $(getEm, font);
    if (!within(-em, at.x, gs->width+em) || !within(-em, at.y, gs->height+em))
        return width;
    
    PgMatrix ctm = gs->ctm;
    pgTranslateMatrix(&ctm, at.x, at.y);
    PgPath *path = $(getGlyphPath, font, &ctm, g);
    if (path) {
        $(fill, gs, path, color);
        $(free, path);
    }
    return width;
}
static float _fillChar(Pg *gs, const PgFont *font, PgPt at, unsigned c, uint32_t color) {
    return $(fillGlyph, gs, font, at, $(getGlyph, font, c), color);
}
static float _fillUtf8(Pg *gs, const PgFont *font, PgPt at, const uint8_t chars[], int len, uint32_t color) {
    wchar_t *wchars = pgUtf8To16(chars, len, &len);
    float width = $(fillString, gs, font, at, wchars, len, color);
    free(wchars);
    return width;
}
static float _fillString( Pg *gs, const PgFont *font, PgPt at, const wchar_t chars[], int len, uint32_t color) {
    float org = at.x;
    if (len < 0) len = wcslen(chars);
    for (int i = 0; i < len; i++)
        at.x += $(fillGlyph, gs, font, at, $(getGlyph, font, chars[i]), color);
    return at.x - org;
}

static void _resize(Pg *g, int width, int height) {
    g->width = width;
    g->height = height;
    REALLOC(((PgBitmapCanvas*)g)->data, uint32_t, width * height);
}
static void _free(Pg *g) {
    if (g) {
        free(((PgBitmapCanvas*)g)->data);
        free(g);
    }
}
static void _clear(const Pg *g, uint32_t color) {
    uint32_t *p = ((PgBitmapCanvas*)g)->data;
    uint32_t *end = ((PgBitmapCanvas*)g)->data + g->width * g->height;
    while (p < end) *p++ = color;
}

PgBitmapCanvas pgDefaultBitmapCanvas() {
    PgBitmapCanvas g;
    g._ = pgDefaultCanvas();
    g._.free = _free;
    g._.resize = _resize;
    g._.clear = _clear;
    g._.fill = _fill;
    g._.fillChar = _fillChar;
    g._.fillGlyph = _fillGlyph;
    g._.fillString = _fillString;
    g._.fillUtf8 = _fillUtf8;
    g.data = NULL;
    return g;
}
Pg *pgNewBitmapCanvas(int width, int height) {
    Pg *g = NEW(PgBitmapCanvas);
    *(PgBitmapCanvas*)g = pgDefaultBitmapCanvas();
    $(resize, g, width, height);
    return g;
}