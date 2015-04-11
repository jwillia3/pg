// TODO: Handle transformed coordinates in immediate mode drawing
// TODO: provide interface for paths
// TODO: decode Mac encodings for CJK names
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

static PgFontFamily    *Families;
static int              NFamilies;

#define MIN(X,Y) ((X) < (Y)? (X): (Y))
#define MAX(X,Y) ((X) > (Y)? (X): (Y))

static bool within(int a, int b, int c) {
    return a <= b && b < c;
}
static int clamp(int a, int b, int c) {
    return b <= a? a: b >= c? c: b;
}
static float dist(float x, float y) {
    return sqrt(x*x + y*y);
}
static PgPt mid(PgPt a, PgPt b) {
    return pgPt((a.x + b.x) / 2, (a.y + b.y) / 2);
}
static float fraction(float x) {
    return x - floor(x);
}
uint32_t pgBlend(uint32_t bg, uint32_t fg, uint32_t a) {
    if (a == 0xff)
        return fg;
    else if (a == 0)
        return bg;
    unsigned na = 255 - a;
    unsigned rb = ((( fg & 0x00ff00ff) * a) +
                    ((bg & 0x00ff00ff) * na)) &
                    0xff00ff00;
    unsigned g = (((  fg & 0x0000ff00) * a) +
                    ((bg & 0x0000ff00) * na)) &
                    0x00ff0000;
    return (rb | g) >> 8;
}
Pg *pgNew(void *buf, int width, int height) {
    Pg *gs = malloc(sizeof *gs);
    gs->width = width;
    gs->height = height;
    if (buf)
        gs->buf = buf;
    else
        gs->buf = malloc(width * height * sizeof *gs->buf);
    gs->flatness = 1.001f;
    gs->subsamples = 3;
    pgLoadIdentity(gs);
    return gs;
}
void pgFree(Pg *gs) {
    if (gs) {
        free(gs->buf);
        free(gs);
    }
}
void pgIdentityMatrix(PgMatrix *mat) {
    mat->a = 1;
    mat->b = 0;
    mat->c = 0;
    mat->d = 1;
    mat->e = 0;
    mat->f = 0;
}
void pgTranslateMatrix(PgMatrix *mat, float x, float y) {
    mat->e += x;
    mat->f += y;
}
void pgScaleMatrix(PgMatrix *mat, float x, float y) {
    mat->a *= x;
    mat->c *= x;
    mat->e *= x;
    mat->b *= y;
    mat->d *= y;
    mat->f *= y;
}
void pgShearMatrix(PgMatrix *mat, float x, float y) {
    mat->a = mat->a + mat->b * y;
    mat->c = mat->c + mat->d * y;
    mat->e = mat->e + mat->f * y;
    mat->b = mat->a * x + mat->b;
    mat->d = mat->c * x + mat->d;
    mat->f = mat->e * x + mat->f;
}
void pgRotateMatrix(PgMatrix *mat, float rad) {
    PgMatrix old = *mat;
    float m = cos(rad);
    float n = sin(rad);
    mat->a = old.a * m - old.b * n;
    mat->b = old.a * n + old.b * m;
    mat->c = old.c * m - old.d * n;
    mat->d = old.c * n + old.d * m;
    mat->e = old.e * m - old.f * n;
    mat->f = old.e * n + old.f * m;
}
void pgMultiplyMatrix(PgMatrix * __restrict a, const PgMatrix * __restrict b) {
    PgMatrix old = *a;
    
    a->a = old.a * b->a + old.b * b->c;
    a->c = old.c * b->a + old.d * b->c;
    a->e = old.e * b->a + old.f * b->c + b->e;
    
    a->b = old.a * b->b + old.b * b->d;
    a->d = old.c * b->b + old.d * b->d;
    a->f = old.e * b->b + old.f * b->d + b->f;
}
void pgLoadIdentity(Pg *gs) {
    pgIdentityMatrix(&gs->ctm);
}
void pgTranslate(Pg *gs, float x, float y) {
    pgTranslateMatrix(&gs->ctm, x, y);
}
void pgScale(Pg *gs, float x, float y) {
    pgScaleMatrix(&gs->ctm, x, y);
}
void pgShear(Pg *gs, float x, float y) {
    pgShearMatrix(&gs->ctm, x, y);
}
void pgRotate(Pg *gs, float rad) {
    pgRotateMatrix(&gs->ctm, rad);
}
void pgMultiply(Pg *gs, const PgMatrix * __restrict mat) {
    pgMultiplyMatrix(&gs->ctm, mat);
}
PgPt pgTransformPoint(const PgMatrix *ctm, PgPt p) {
    PgPt out = {
        ctm->a * p.x + ctm->c * p.y + ctm->e,
        ctm->b * p.x + ctm->d * p.y + ctm->f
    };
    return out;
}
void pgClear(
    const Pg *gs,
    uint32_t color)
{
    uint32_t *p = gs->buf;
    uint32_t *end = gs->buf + gs->width * gs->height;
    while (p < end) *p++ = color;
}
void pgStrokeLine(
    const Pg *gs,
    PgPt a,
    PgPt b,
    uint32_t color)
{
   int x0 = a.x;
   int y0 = a.y;
   int x1 = b.x;
   int y1 = b.y;
   
   int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
   int dy = abs(y1-y0), sy = y0<y1 ? 1 : -1; 
   int err = dx-dy, e2, x2;                       /* error value e_xy */
   int ed = dx+dy == 0 ? 1 : sqrt((float)dx*dx+(float)dy*dy);

   uint32_t *scr = gs->buf + y0 * gs->width + x0;
   for ( ; ; ) {                                         /* pixel loop */
      if (within(0, x0, gs->width) && within(0, y0, gs->height))
        *scr = pgBlend(color, *scr, 255*abs(err-dx+dy)/ed);
      e2 = err; x2 = x0;
      if (2*e2 >= -dx) {                                    /* x step */
         if (x0 == x1) break;
         if (e2+dy < ed)
            if (within(0, x0, gs->width) && within(0, y0+sy, gs->height)) {
                uint32_t *tmp = scr + (sy > 0? gs->width: -gs->width);
                *tmp = pgBlend(color, *tmp, 255*(e2+dy)/ed);
            }
         err -= dy; x0 += sx;
         scr += sx;
      } 
      if (2*e2 <= dy) {                                     /* y step */
         if (y0 == y1) break;
         if (dx-e2 < ed)
            if (within(0, x2+sx, gs->width) && within(0, y0, gs->height)) {
                uint32_t *tmp = scr + (x2 - x0) + sx;
                *tmp = pgBlend(color, *tmp, 255*(dx-e2)/ed);
            }
         err += dx; y0 += sy; 
         scr += sy > 0? gs->width: -gs->width;
      }
    }
}
void pgStrokeRectangle(
    const Pg *gs,
    PgPt nw,
    PgPt se,
    uint32_t color)
{
    int x0 = clamp(0, nw.x, gs->width - 1);
    int x1 = clamp(0, se.x, gs->width - 1);
    int y0 = clamp(0, nw.y, gs->height - 1);
    int y1 = clamp(0, se.y, gs->height - 1);
    
    int a = color >> 24;
    uint32_t *screen = gs->buf + y0 * gs->width;
    
    if (within(0, nw.y, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = pgBlend(screen[x], color, a * (1 - fraction(nw.y)));
    screen += gs->width;
    if (within(0, nw.y + 1, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = pgBlend(screen[x], color, a * fraction(nw.y));
    
    
    for (int y = y0; y <= y1; y++, screen += gs->width) {
        if (within(0, nw.x, gs->width))
            screen[x0] = pgBlend(screen[x0], color, a * (1 - fraction(nw.x)));
        if (within(0, nw.x+1, gs->width))
            screen[x0+1] = pgBlend(screen[x0+1], color, a * fraction(nw.x));
        if (within(0, se.x, gs->width))
            screen[x1] = pgBlend(screen[x1], color, a * (1 - fraction(se.x)));
        if (within(0, se.x-1, gs->width))
            screen[x1-1] = pgBlend(screen[x1-1], color, a * fraction(se.x));
    }
    
    screen -= gs->width;
    if (within(0, se.y, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = pgBlend(screen[x], color, a * (1 - fraction(se.y)));
    screen += gs->width;
    if (within(0, se.y + 1, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = pgBlend(screen[x], color, a * fraction(se.y));
}

void pgFillRectangle(
    const Pg *gs,
    PgPt nw,
    PgPt se,
    uint32_t color)
{
    int x0 = clamp(0, nw.x, gs->width - 1);
    int x1 = clamp(0, se.x, gs->width - 1);
    int y0 = clamp(0, nw.y, gs->height - 1);
    int y1 = clamp(0, se.y, gs->height - 1);
    
    int a = color >> 24;
    uint32_t *screen = gs->buf + y0 * gs->width;
    
    if (within(0, nw.y, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = pgBlend(screen[x], color, a * (1 - fraction(nw.y)));
    screen += gs->width;
    for (int y = y0; y < y1; y++, screen += gs->width) {
        if (within(0, nw.x, gs->width))
            screen[x0] = pgBlend(screen[x0], color, a * (1 - fraction(nw.x)));
        for (int x = x0 + 1; x < x1; x++)
            screen[x] = pgBlend(screen[x], color, a);
        if (within(0, se.x-1, gs->width))
            screen[x1-1] = pgBlend(screen[x1-1], color, a * fraction(se.x));
    }
    if (within(0, se.y, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = pgBlend(screen[x], color, a * fraction(se.y));
}

void pgStrokeCircle(
    const Pg *gs,
    PgPt centre,
    float radius,
    uint32_t color)
{
    int x = -radius, y = 0, e = 2 - 2 * radius;
    int cx = centre.x;
    int cy = centre.y;
    uint32_t *screen = gs->buf + cy * gs->width + cx;
    do {
        if (within(0, cx - x, gs->width) && within(0, cy + y, gs->height))
            screen[-x + y*gs->width] = color;
        if (within(0, cx - y, gs->width) && within(0, cy - x, gs->height))
            screen[-y - x*gs->width] = color;
        if (within(0, cx + x, gs->width) && within(0, cy - y, gs->height))
            screen[x - y*gs->width] = color;
        if (within(0, cx + y, gs->width) && within(0, cy + x, gs->height))
            screen[y + x*gs->width] = color;
        radius = e;
        if (radius <= y) {
            y++;
            e += y * 2 + 1;
        }
        if (radius > x || e > y) {
            x++;
            e += x * 2 + 1;
        }
    } while (x < 0);
}
void pgFillCircle(
    const Pg *gs,
    PgPt centre,
    float radius,
    uint32_t color)
{
    // TODO Use a real circle algorithm
    int cx = centre.x;
    int cy = centre.y;
    int min_y = clamp(0, centre.y - radius, gs->height - 1);
    int max_y = clamp(0, centre.y + radius, gs->height - 1);
    uint32_t *screen = gs->buf + min_y * gs->width;
    for (int y = min_y; y < max_y; y++) {
        int min_x = clamp(0, centre.x - radius, gs->width - 1);
        int max_x = clamp(0, centre.x + radius, gs->width - 1);
        bool first = false;
        
        for (int x = min_x; x < max_x; x++) {
            float dx = (cx - x);
            float dy = (cy - y);
            float d = sqrt(dx*dx + dy*dy);
            if (d < radius)
                if (within(0,x,gs->width) && within(0,y,gs->height)) {
                    if (!first) {
                        screen[x] = pgBlend(screen[x],
                            color,
                            (color >> 24) * (1 - fraction(centre.x - radius)));
                        first = true;
                    } else
                        screen[x] = pgBlend(screen[x], color, color >> 24);
                }
        }
        if (max_x == centre.x + radius)
            screen[max_x] = pgBlend(screen[max_x],
                color,
                (color >> 24) * fraction(centre.x + radius));
        screen += gs->width;
    }
}

static void stroke_bezier3(
    const Pg *gs,
    PgPt a,
    PgPt b,
    PgPt c,
    uint32_t color,
    int n)
{
    // TODO Make quadratic and cubic agree on what flatness is
    if (!n) {
        pgStrokeLine(gs, a, c, color);
        return;
    }
    PgPt m = { (a.x + 2 * b.x + c.x) / 4, (a.y + 2 * b.y + c.y) / 4 };
    PgPt d = { (a.x + c.x) / 2 - m.x, (a.y + c.y) / 2 - m.y };
    if (d.x * d.x + d.y * d.y > .05) {
        stroke_bezier3(gs, a, pgPt((a.x + b.x) / 2, (a.y + b.y) / 2), m, color, n - 1);
        stroke_bezier3(gs, m, pgPt((b.x + c.x) / 2, (b.y + c.y) / 2), c, color, n - 1);
    } else
        pgStrokeLine(gs, a, c, color);
}

void psStrokeQuadratic(
    const Pg *gs,
    PgPt a,
    PgPt b,
    PgPt c,
    uint32_t color)
{
    stroke_bezier3(gs, a, b, c, color, BEZIER_RECURSION_LIMIT);
}
static void stroke_bezier4(
    const Pg *gs,
    PgPt a,
    PgPt b,
    PgPt c,
    PgPt d,
    uint32_t color,
    int n)
{
    if (!n) {
        pgStrokeLine(gs, a, d, color);
        return;
    }
    float d1 = dist(a.x - b.x, a.y - b.y);
    float d2 = dist(b.x - c.x, b.y - c.y);
    float d3 = dist(c.x - d.x, c.y - d.y);
    float d4 = dist(a.x - d.x, a.y - d.y);
    if (d1 + d2 + d3 < gs->flatness * d4)
        pgStrokeLine(gs, a, d, color);
    else {
        PgPt mab = mid(a, b);
        PgPt mbc = mid(b, c);
        PgPt mcd = mid(c, d);
        PgPt mabc = mid(mab, mbc);
        PgPt mbcd = mid(mbc, mcd);
        PgPt mabcd = mid(mabc, mbcd);
        stroke_bezier4(gs, a, mab, mabc, mabcd, color, n - 1);
        stroke_bezier4(gs, mabcd, mbcd, mcd, d, color, n - 1);
    }
}
void psStrokeCubic(
    const Pg *gs,
    PgPt a,
    PgPt b,
    PgPt c,
    PgPt d,
    uint32_t color)
{
    stroke_bezier4(gs, a, b, c, d, color, BEZIER_RECURSION_LIMIT);
}

PgPath *pgNew_path(void) {
    PgPath *path = calloc(1, sizeof *path);
    return path;
}

void pgFree_path(PgPath *path) {
    if (path) {
        free(path->types);
        free(path->points);
        free(path);
    }
}

static void add_part(
    PgPath *path,
    const PgMatrix *ctm,
    PgPathPartType type,
    ...)
{
    if (path->nparts + 1 >= path->cap) {
        path->cap = path->cap? path->cap * 2: 16;
        path->types = realloc(path->types, path->cap * sizeof *path->types);
        path->points = realloc(path->points, path->cap * 4 * sizeof *path->points);
    }
    
    va_list ap;
    va_start(ap, type);
    path->types[path->nparts] = type;
    for (int i = 0; i < pgPathPartTypeArgs(type); i++)
        path->points[path->npoints + i] = pgTransformPoint(ctm, *va_arg(ap, PgPt*));
    va_end(ap);
    path->nparts++;
    path->npoints += pgPathPartTypeArgs(type);
}

void pgSubpath(
    PgPath *path,
    const PgMatrix *ctm,
    PgPt p)
{
    path->start = pgTransformPoint(ctm, p);
    add_part(path, ctm, PG_PATH_SUBPATH, &p);
}

void pgClosePath(PgPath *path) {
    add_part(path, &PgIdentityMatrix, PG_PATH_LINE, &path->start);
}
void pgLine(
    PgPath *path,
    const PgMatrix *ctm,
    PgPt b)
{
    add_part(path, ctm, PG_PATH_LINE, &b);
}
void pgQuadratic(
    PgPath *path,
    const PgMatrix *ctm,
    PgPt b,
    PgPt c)
{
    add_part(path, ctm, PG_PATH_BEZIER3, &b, &c);
}
void pgCubic(
    PgPath *path,
    const PgMatrix *ctm,
    PgPt b,
    PgPt c,
    PgPt d)
{
    add_part(path, ctm, PG_PATH_BEZIER4, &b, &c, &d);
}

void pgStroke(
    const Pg *gs,
    const PgPath *path,
    uint32_t color)
{    
    PgPt a = {0, 0};
    for (int i = 0, ip = 0; i < path->nparts; ip += pgPathPartTypeArgs(path->types[i]), i++)
        switch (path->types[i]) {
        case PG_PATH_SUBPATH:
            a = path->points[ip];
            break;
        case PG_PATH_LINE:
            pgStrokeLine(gs, a, path->points[ip], color);
            a = path->points[ip];
            break;
        case PG_PATH_BEZIER3:
            psStrokeQuadratic(gs, a, path->points[ip], path->points[ip+1], color);
            a = path->points[ip+1];
            break;
        case PG_PATH_BEZIER4:
            psStrokeCubic(gs, a, path->points[ip], path->points[ip+1], path->points[ip+2], color);
            a = path->points[ip+2];
            break;
        }
}

PgRect pgGetBoundingBox(PgPath *path) {
    PgRect r = { {FLT_MAX,FLT_MAX}, {FLT_MIN,FLT_MIN} };
    // TODO box is not tight since it just goes by curve control points
    for (int i = 0; i < path->npoints; i++) {
        r.a.x = MIN(r.a.x, path->points[i].x);
        r.a.y = MIN(r.a.y, path->points[i].y);
        r.b.x = MAX(r.b.x, path->points[i].x);
        r.b.y = MAX(r.b.y, path->points[i].y);
    }
    return r;
}

typedef struct {
    PgPt    a;
    PgPt    b;
    float       m;
    float       dir;
} Segment;
typedef struct {
    int     n;
    int     cap;
    Segment *segs;
} SegList;


static void add_seg(
    SegList *list,
    PgPt a,
    PgPt b)
{
    if (list->n + 1 >= list->cap) {
        list->cap = list->cap? list->cap * 2: 128;
        list->segs = realloc(list->segs, list->cap * sizeof *list->segs);
    }
    list->segs[list->n].a = a.y < b.y? a: b;
    list->segs[list->n].b = a.y < b.y? b: a;
    list->segs[list->n].dir = a.y < b.y? -1: 1;
    list->n++;
}
static void decomp_bezier3(
    SegList *list,
    PgPt a,
    PgPt b,
    PgPt c,
    float flatness,
    int n)
{    
    if (!n) {
        add_seg(list, a, c);
        return;
    }
    PgPt m = { (a.x + 2 * b.x + c.x) / 4, (a.y + 2 * b.y + c.y) / 4 };
    PgPt d = { (a.x + c.x) / 2 - m.x, (a.y + c.y) / 2 - m.y };
    if (d.x * d.x + d.y * d.y > .05) {
        decomp_bezier3(list, a, pgPt((a.x + b.x) / 2, (a.y + b.y) / 2), m, flatness, n - 1);
        decomp_bezier3(list, m, pgPt((b.x + c.x) / 2, (b.y + c.y) / 2), c, flatness, n - 1);
    } else
        add_seg(list, a, c);
}
static void decomp_bezier4(
    SegList *list,
    PgPt a,
    PgPt b,
    PgPt c,
    PgPt d,
    float flatness,
    int n)
{    
    if (!n) {
        add_seg(list, a, d);
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
        decomp_bezier4(list, a, mab, mabc, mabcd, flatness, n - 1);
        decomp_bezier4(list, mabcd, mbcd, mcd, d, flatness, n - 1);
    } else
        add_seg(list, a, d);
}
static int sort_tops(const void *ap, const void *bp) {
    const Segment * __restrict a = ap;
    const Segment * __restrict b = bp;
    return  a->a.y < b->a.y? -1:
            a->a.y > b->a.y? 1:
            a->a.x < b->a.x? -1:
            a->a.x > b->a.x? 1:
            0;
}
static void fill_evenodd(const Pg *gs, const Segment *segs, int nsegs, uint32_t color) {
    typedef struct {
        float y0;
        float y1;
        float x;
        float m;
    } Edge;
    int                     max_y = INT_MIN;
    int                     min_y = INT_MAX; // calculated later
    int                     min_x = 0; // per-line minimum used x
    int                     max_x = gs->width - 1; // per-line maximum used x
    for (int i = 0; i < nsegs; i++) {
        if (segs[i].a.y < min_y) min_y = segs[i].a.y;
        if (segs[i].b.y > max_y) max_y = segs[i].b.y;
    }
    min_y = clamp(0, min_y / gs->subsamples + 1, gs->height - 1);
    max_y = clamp(0, max_y / gs->subsamples + 1, gs->height - 1);
    
    uint8_t * __restrict    buffer = malloc(gs->width);
    Edge * __restrict       edges = calloc(1, nsegs * sizeof *edges);
        
    // Rasterise each line
    int nedges = 0;
    float max_alpha = (color >> 24) / gs->subsamples;
    for (int scan_y = min_y, min_seg = 0; scan_y <= max_y; scan_y++) {
        // Clear line buffer
        if (min_x <= max_x)
            memset(buffer + min_x, 0, max_x - min_x + 1);
        min_x = gs->width;
        max_x = 0;
        
        
        for (int s = -gs->subsamples / 2; s < gs->subsamples / 2; s++) {
            float y = gs->subsamples * scan_y + s + .5;
            
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
                int start = clamp(0, edges[i - 1].x, gs->width - 1);
                int end = clamp(0, edges[i].x, gs->width - 1);
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
        if ((gs->width & 3) == 0) {
            if (min_x & 3)
                min_x &= ~3;
            if (max_x & 3)
                max_x = clamp(0, max_x + 3 & ~3, gs->width - 1);
            else
                max_x = clamp(0, max_x + 4, gs->width - 1);
                
            uint32_t * __restrict   screen = gs->buf + scan_y * gs->width;
            __m128i fg = _mm_set_epi32(color, color, color, color);
            for (int i = min_x; i < max_x; i += 4) {
                __m128i a = _mm_setr_epi32(buffer[i], buffer[i+1], buffer[i+2], buffer[i+3]);
                __m128i na = _mm_sub_epi32(_mm_set1_epi32(255), a);
                __m128i bg = _mm_load_si128((__m128i*)(screen + i));
                
                __m128i rb_fg = _mm_and_si128(fg, _mm_set1_epi32(0x00ff00ff));
                __m128i rb_bg = _mm_and_si128(bg, _mm_set1_epi32(0x00ff00ff));
                __m128i  g_fg = _mm_and_si128(fg, _mm_set1_epi32(0x0000ff00));
                __m128i  g_bg = _mm_and_si128(bg, _mm_set1_epi32(0x0000ff00));
                
                rb_fg = _mm_or_si128(
                            _mm_mul_epu32(rb_fg, a),
                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(rb_fg, 4), _mm_srli_si128(a, 4)), 4));
                rb_bg = _mm_or_si128(
                            _mm_mul_epu32(rb_bg, na),
                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(rb_bg, 4), _mm_srli_si128(na, 4)), 4));
                __m128i rb = _mm_and_si128(_mm_add_epi32(rb_bg, rb_fg), _mm_set1_epi32(0xff00ff00));
                
                g_fg = _mm_or_si128(
                            _mm_mul_epu32(g_fg, a),
                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(g_fg, 4), _mm_srli_si128(a, 4)), 4));
                g_bg = _mm_or_si128(
                            _mm_mul_epu32(g_bg, na),
                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(g_bg, 4), _mm_srli_si128(na, 4)), 4));
                __m128i g = _mm_and_si128(_mm_add_epi32(g_bg, g_fg), _mm_set1_epi32(0x00ff0000));
                
                *(__m128i*)(screen + i) = _mm_srli_epi32(_mm_or_si128(rb, g), 8);
            }
        } else {
            uint32_t * __restrict   screen = gs->buf + scan_y * gs->width;
            for (int i = min_x; i <= max_x; i++)
                if (buffer[i]) screen[i] = pgBlend(screen[i], color, buffer[i]);
        }
    }
    free(buffer);
    free(edges);
}
static void fill_nonzero(const Pg *gs, const Segment *segs, int nsegs, uint32_t color) {
    // TODO Do even-odd fill rule
    fill_evenodd(gs, segs, nsegs, color);
}

void pgFill(
    const Pg *gs,
    const PgPath *path,
    uint32_t color)
{
    if (path->nparts == 0) return;
    
    SegList list = { 0 };
    
    // Decompose curves into a list of lines
    PgPt a = {0, 0};
    for (int i = 0, ip = 0; i < path->nparts; ip += pgPathPartTypeArgs(path->types[i]), i++)
        switch (path->types[i]) {
        case PG_PATH_SUBPATH:
            a = path->points[ip];
            break;
        case PG_PATH_LINE:
            add_seg(&list, a, path->points[ip]);
            a = path->points[ip];
            break;
        case PG_PATH_BEZIER3:
            decomp_bezier3(&list, a, path->points[ip], path->points[ip+1], gs->flatness, BEZIER_RECURSION_LIMIT);
            a = path->points[ip+1];
            break;
        case PG_PATH_BEZIER4:
            decomp_bezier4(&list, a, path->points[ip], path->points[ip+1], path->points[ip+2], gs->flatness, BEZIER_RECURSION_LIMIT);
            a = path->points[ip+2];
            break;
        }
        
    // Subsample in Y direction
    for (int i = 0; i < list.n; i++) {
        list.segs[i].a.y *= gs->subsamples;
        list.segs[i].b.y *= gs->subsamples;
        list.segs[i].m = list.segs[i].a.y == list.segs[i].b.y
            ? 0
            : (list.segs[i].b.x - list.segs[i].a.x) / (list.segs[i].b.y - list.segs[i].a.y);
    }
    
    // Sort line segments by their tops
    const Segment *const __restrict segs = list.segs;
    const int nsegs = list.n;
    qsort(list.segs, nsegs, sizeof *segs, sort_tops);
    
    if (path->fill_rule == PG_EVENODD_WINDING)
        fill_evenodd(gs, segs, nsegs, color);
    else
        fill_nonzero(gs, segs, nsegs, color);
    
    free(list.segs);
}

#ifdef _MSC_VER
    #define be32(x) _byteswap_ulong(x)
    #define be16(x) _byteswap_ushort(x)
#else
    #define be32(x) __builtin_bswap32(x)
    #define be16(x) __builtin_bswap16(x)
#endif
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

#define trailing(n) ((input[n] & 0xC0) == 0x80)
#define overlong(n) do { if (o[-1] < n) o[-1] = 0xfffd; } while(0)
uint16_t *pgUtf8To16(const uint8_t *input, int len, int *lenp) {
    if (len < 0) len = strlen(input);
    uint16_t *output = malloc((len + 1) * sizeof *output);
    uint16_t *o = output;
    const uint8_t *end = input + len;
    
    while (input < end)
        if (*input < 0x80)
            *o++ = *input++;
        else if (~*input & 0x20 && input + 1 < end && trailing(1)) { // two byte
            *o++ =     (input[0] & 0x1f) << 6
                    |(input[1] & 0x3f);
            input += 2;
            overlong(0x80);
        } else if (~*input & 0x10 && input + 2 < end && trailing(1) && trailing(2)) { // three byte
            *o++ =     (input[0] & 0x0f) << 12
                    |(input[1] & 0x3f) << 6
                    |(input[2] & 0x3f);
            input += 3;
            overlong(0x800);
        } else {
            // Discard malformed or non-BMP characters
            do
                input++;
            while ((*input & 0xc0) == 0x80);
            *o++ = 0xfffd; /* replacement char */
        }
    *o = 0;
    len = o - output;
    if (lenp) *lenp = len;
    return realloc(output, (len + 1) * sizeof *output);
}
uint8_t *pgUtf16To8(const uint16_t *input, int len, int *lenp) {
    if (len < 0) len = wcslen(input);
    uint8_t *o, *output = malloc(len * 3 + 1);
    for (o = output; len-->0; input++) {
        unsigned c = *input;
        if (c < 0x80)
            *o++ = c;
        else if (be16(*input) < 0x800) {
            *o++ = ((c >> 6) & 0x1f) | 0xc0;
            *o++ = ((c >> 0) & 0x3f) | 0x80;
        } else {
            *o++ = ((c >> 12) & 0x0f) | 0xe0;
            *o++ = ((c >>  6) & 0x3f) | 0x80;
            *o++ = ((c >>  0) & 0x3f) | 0x80;
        }
    }
    *o = 0;
    len = o - output;
    if (lenp) *lenp = len;
    return realloc(output, len + 1);
}

static void scan_fonts_per_file(const wchar_t *filename, void *data) {
    int nfonts = 1;
    for (int index = 0; index < nfonts; index++) {
        PgFont *font = pgLoadFont(data, index, true);
        nfonts = pgGetFontCount(font);
        if (!font) continue;
        
        
        const wchar_t *family = pgGetFontFamily(font);
        int i, dir;
        
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
        
        int weight = pgGetFontWeight(font) / 100;
        if (weight >= 0 && weight < 10) {
            const wchar_t **slot = pgIsFontItalic(font)
                ? &Families[i].italic[weight]
                : &Families[i].roman[weight];
            
            if (*slot)
                free((void*)*slot);
            *slot = wcsdup(filename);
            if (pgIsFontItalic(font))
                Families[i].italic_index[weight] = index;
            else
                Families[i].roman_index[weight] = index;
        }

        pgFree_font(font);
    }
}

void pgFree_font_family(PgFontFamily *family) {
    if (family) {
        free((void*)family->name);
        for (int i = 0; i < 10; i++) {
            free((void*)family->roman[i]);
            free((void*)family->italic[i]);
        }
    }
}
static PgFontFamily copy_font_family(PgFontFamily *src) {
    PgFontFamily out;
    
    out.name = wcsdup(src->name);
    for (int i = 0; i < 10; i++) {
        out.roman[i] = src->roman[i];
        out.italic[i] = src->italic[i];
        out.roman_index[i] = src->roman_index[i];
        out.italic_index[i] = src->italic_index[i];
    }
    return out;
}

PgFontFamily *pgScanFonts(const wchar_t *dir, int *countp) {
//    for (int i = 0; i < NFamilies; i++)
//        pgFree_font_family(&Families[i]);
    Families = NULL;
    NFamilies = 0;
    _pgScanDirectory(dir, scan_fonts_per_file);
    
    // Copy families
    PgFontFamily *families = malloc(NFamilies * sizeof *families);
    for (int i = 0; i < NFamilies; i++) 
        families[i] = copy_font_family(&Families[i]);
    
    if (countp) *countp = NFamilies;
    return families;
}

PgOpenType *pgLoadOpenType(const void *file, int font_index, bool scan_only) {

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
    
    PgOpenType *font = calloc(1, sizeof *font);
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
                        font->style_name = output;
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
                        font->style_name = output;
                    else if (id == 4)
                        font->name = output;
                    else if (id == 16) { // Preferred font family
                        free((void*)font->family);
                        font->family = output;
                    }
                }
        }
        if (!font->family) font->family = wcsdup(L"");
        if (!font->style_name) font->style_name = wcsdup(L"");
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
    
    font->base.file = file;
    font->scale_x = font->scale_y = 1;
    font->nfonts = nfonts;
    return font;
}
int pgGetOpenTypeFontCount(PgOpenType*font) {
    return font? font->nfonts: 0;
}

void pgFree_otf(PgOpenType *font) {
    if (font) {
        free((void*)font->family);
        free((void*)font->style_name);
        free((void*)font->name);
        free(font->features);
        free(font->subst);
        free(font);
    }
}
void pgScale_otf(PgOpenType *font, float height, float width) {
    float s = font->ascender - font->descender;
    if (width <= 0)
        width = height;
    font->scale_x = width / font->em;
    font->scale_y = height / font->em;
}

float getOpenTypeAscender(const PgOpenType *font) {
    return font->ascender * font->scale_y;
}
float getOpenTypeDescender(const PgOpenType *font) {
    return font->descender * font->scale_y;
}
float getOpenTypeLeading(const PgOpenType *font) {
    return font->leading * font->scale_y;
}
float getOpenTypeEm(const PgOpenType *font) {
    return font->em * font->scale_y;
}
float pgGetOpenTypeXHeight(const PgOpenType *font) {
    return font->x_height * font->scale_y;
}
float pgGetOpenTypeCapHeight(const PgOpenType *font) {
    return font->cap_height * font->scale_y;
}
PgFontWeight getOpenTypeWeight(const PgOpenType *font) {
    return font->weight;
}
PgFontStretch getOpenTypeStretch(const PgOpenType *font) {
    return font->stretch;
}
PgRect getOpenTypeSubscript(const PgOpenType *font) {
    return font->superscript_box;
}
PgRect getOpenTypeSuperscript(const PgOpenType *font) {
    return font->superscript_box;
}
bool isOpenTypeMonospaced(const PgOpenType *font) {
    return font->panose[3] == 9; // PANOSE porportion = 9 (monospaced)
}
bool isOpenTypeItalic(const PgOpenType *font) {
    return font->is_italic;
}
const wchar_t *getOpenTypeFamily(const PgOpenType *font) {
    return font->family;
}
const wchar_t *getOpenTypeName(const PgOpenType *font) {
    return font->name;
}
const wchar_t *getOpenTypeStyleName(const PgOpenType *font) {
    return font->style_name;
}
static char *lookup_otf_features(
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
                    pgSubstituteOpenTypeGlyph(font, input, output);
                }
            else if (coverage_format == 2)
                for (int i = 0; i < count; i++) {
                    uint16_t start, end, start_index;
                    unpack(&coverage, "SSS", &start, &end, &start_index);
                    for (int glyph = start; glyph <= end; glyph++) {
                        uint16_t input = glyph;
                        uint16_t output = input + delta;
                        pgSubstituteOpenTypeGlyph(font, input, output);
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
                    pgSubstituteOpenTypeGlyph(font, input, output);
                }
            else if (coverage_format == 2)
                for (int i = 0; i < count; i++) {
                    uint16_t start, end, start_index;
                    unpack(&coverage, "SSS", &start, &end, &start_index);
                    for (int glyph = start; glyph <= end; glyph++) {
                        uint16_t output;
                        uint16_t input = glyph;
                        unpack(&subtable, "S", &output);
                        pgSubstituteOpenTypeGlyph(font, input, output);
                    }
                }
        }
    }
}
char *pgGetOpenTypeFontFeatures(const PgOpenType *font) {
    return lookup_otf_features((PgOpenType*)font, font->gsub, 'latn', 'eng ', "", NULL);
}

void pgSetOpenTypeFontFeatures(PgOpenType *font, const uint8_t *features) {
    free(font->features);
    free(font->subst);
    font->nsubst = 0;
    font->subst = NULL;
    font->features = (void*)strdup(features);
    if (*features)
        lookup_otf_features(font, font->gsub, font->script, font->lang, features, gsub_handler);
}
void pgSubstituteOpenTypeGlyph(PgOpenType *font, uint16_t in, uint16_t out) {
    font->subst = realloc(font->subst, (font->nsubst + 1) * 2 * sizeof *font->subst);
    font->subst[font->nsubst][0] = in;
    font->subst[font->nsubst][1] = out;
    font->nsubst++;
}

float getOpenTypeGlyph_lsb(const PgOpenType *font, unsigned g) {
    return  g < font->nhmtx?    be16(font->hmtx[g * 2 + 1]) * font->scale_x:
            g < font->nglyphs?  be16(font->hmtx[(font->nhmtx - 1) * 2 + 1]) * font->scale_x:
            0;
}
float getOpenTypeGlyph_width(const PgOpenType *font, unsigned g) {
    return  g < font->nhmtx?    be16(font->hmtx[g * 2]) * font->scale_x:
            g < font->nglyphs?  be16(font->hmtx[(font->nhmtx - 1) * 2]) * font->scale_x:
            0;
}

static void glyph_path(PgPath *path, const PgOpenType *font, const PgMatrix *ctm, unsigned g) {
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
        int16_t arg1, arg2;
        do {
            unpack(&data, "SS", &flags, &glyph);
            
            PgMatrix new_ctm = PgIdentityMatrix;
            
            if (flags & 1) // args are words
                unpack(&data, "SS", &arg1, &arg2);
            else { // args are bytes
                uint8_t a, b;
                unpack(&data, "BB", &a, &b);
                arg1 = a;
                arg2 = b;
            }
            
            if (flags & 2) // args are x & y
                pgTranslateMatrix(&new_ctm, arg1, arg2);
            else {
                // TODO: "matching points"
            }
            
            int16_t sx = 1, sy = 1;
            int16_t shearx=0, sheary=0;
            if (flags & 8) { // scale
                unpack(&data, "S", &sx);
                sy = sx;
            } else if (flags & 64) // x & y scale
                unpack(&data, "SS", &sx, &sy);
            else if (flags & 128) // 2x2 matrix
                unpack(&data, "SSSS", &sx, &shearx, &sheary, &sy);
            
            pgScaleMatrix(&new_ctm, sx, sy);
            pgMultiplyMatrix(&new_ctm, ctm);
            glyph_path(path, font, &new_ctm, glyph);
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
                    pgQuadratic(path, ctm, b, start);
                else
                    pgClosePath(path);
                if (i != npoints - 1)
                    pgSubpath(path, ctm, p);
                start = a = p;
                in_curve = false;
            } else if (~*flags & 1) // curve
                if (in_curve) {
                    PgPt q = mid(b, p);
                    pgQuadratic(path, ctm, b, q);
                    a = q;
                    b = p;
                } else {
                    b = p;
                    in_curve = true;
                }
            else if (in_curve) { // curve to line
                pgQuadratic(path, ctm, b, p);
                a = p;
                in_curve = false;
            } else { // line to line
                pgLine(path, ctm, p);
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
            pgQuadratic(path, ctm, b, start);
        else
            pgClosePath(path);
    }
}
PgPath *getOpenTypeGlyphPath(
    const PgOpenType *font,
    const PgMatrix *ctm,
    unsigned g)
{
    PgPath *path = pgNew_path();
    PgMatrix new_ctm = {1,0,0, 1,0,0};
    pgTranslateMatrix(&new_ctm, 0, -font->ascender);
    pgScaleMatrix(&new_ctm, font->scale_x, -font->scale_y);
    pgMultiplyMatrix(&new_ctm, ctm);
    glyph_path(path, font, &new_ctm, g);
    return path;
}
float ags_otf_fill_glyph(
    Pg *gs,
    const PgOpenType *font,
    PgPt at,
    unsigned g,
    uint32_t color)
{
    float width = getOpenTypeGlyph_width(font, g);
    float em = getOpenTypeEm(font);
    if (!within(-em, at.x, gs->width+em) || !within(-em, at.y, gs->height+em))
        return width;
    
    PgMatrix ctm = gs->ctm;
    pgTranslateMatrix(&ctm, at.x, at.y);
    PgPath *path = getOpenTypeGlyphPath(font, &ctm, g);
    if (path) {
        pgFill(gs, path, color);
        pgFree_path(path);
    }
    return width;
}

unsigned getOpenTypeGlyph(const PgOpenType *font, unsigned c) {
    unsigned g = font->cmap[c & 0xffff];
    for (int i = 0; i < font->nsubst; i++)
        if (g == font->subst[i][0])
            g = font->subst[i][1];
    return g;
}
float getOpenTypeCharLsb(const PgOpenType *font, unsigned c) {
    return getOpenTypeGlyph_lsb(font, getOpenTypeGlyph(font, c));
}
float getOpenTypeCharWidth(const PgOpenType *font, unsigned c) {
    return getOpenTypeGlyph_width(font, getOpenTypeGlyph(font, c));
}
PgPath *ags_get_otf_char_path(const PgOpenType *font, const PgMatrix *ctm, unsigned c) {
    return getOpenTypeGlyphPath(font, ctm, getOpenTypeGlyph(font, c));
}
float ags_otf_fill_char(
    Pg *gs,
    const PgOpenType *font,
    PgPt at,
    unsigned c,
    uint32_t color)
{
    return ags_otf_fill_glyph(gs, font, at, getOpenTypeGlyph(font, c), color);
}

float fillOpenTypeString_utf8(
    Pg *gs,
    const PgOpenType *font,
    PgPt at,
    const char chars[],
    int len,
    uint32_t color)
{
    wchar_t *wchars = pgUtf8To16(chars, len, &len);
    float width = fillOpenTypeString(gs, font, at, wchars, len, color);
    free(wchars);
    return width;
}
float fillOpenTypeString(
    Pg *gs,
    const PgOpenType *font,
    PgPt at,
    const uint16_t chars[],
    int len,
    uint32_t color)
{
    float org = at.x;
    if (len < 0) len = wcslen(chars);
    for (int i = 0; i < len; i++)
        at.x += ags_otf_fill_glyph(gs, font, at, getOpenTypeGlyph(font, chars[i]), color);
    return at.x - org;
}

PgFont *pgOpenFontFile(const wchar_t *filename, int font_index, bool scan_only) {
    return _pgOpenFontFile(filename, font_index, scan_only);
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
            uint8_t *index = italic? Families[f].italic_index: Families[f].roman_index;
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

void pgSetFontFeatures(PgFont *font, const uint8_t *features) {
    pgSetOpenTypeFontFeatures((void*)font, features);
}
void ags_substitute_glyph(PgFont *font, uint16_t in, uint16_t out) {
    pgSubstituteOpenTypeGlyph((void*)font, in, out);
}

PgFont *pgLoadFont(const void *file, int font_index, bool scan_only) {
    return (void*)pgLoadOpenType((void*)file, font_index, scan_only);
}
int pgGetFontCount(PgFont *font) {
    return pgGetOpenTypeFontCount((void*)font);
}
void pgFree_font(PgFont *font) {
    if (font) {
        if (font->host_free)
            font->host_free(font);
        pgFree_otf((void*)font);
    }
}
void pgScale_font(PgFont *font, float height, float width) {
    pgScale_otf((void*)font, height, width);
}
float pgGetFontAscender(const PgFont *font) {
    return getOpenTypeAscender((void*)font);
}
float pgGetFontDescender(const PgFont *font) {
    return getOpenTypeDescender((void*)font);
}
float pgGetFontLeading(const PgFont *font) {
    return getOpenTypeLeading((void*)font);
}
float pgGetFontEm(const PgFont *font) {
    return getOpenTypeEm((void*)font);
}
float pgGetFontXHeight(const PgFont *font) {
    return pgGetOpenTypeXHeight((void*)font);
}
float pgGetFontCapHeight(const PgFont *font) {
    return pgGetOpenTypeCapHeight((void*)font);
}
PgFontWeight pgGetFontWeight(const PgFont *font) {
    return getOpenTypeWeight((void*)font);
}
PgFontStretch pgGetFontStretch(const PgFont *font) {
    return getOpenTypeStretch((void*)font);
}
PgRect pgGetFontSubscript(const PgFont *font) {
    return getOpenTypeSubscript((void*)font);
}
PgRect pgGetFontSuperscript(const PgFont *font) {
    return getOpenTypeSuperscript((void*)font);
}
bool pgIsFontMonospaced(const PgFont *font) {
    return isOpenTypeMonospaced((void*)font);
}
bool pgIsFontItalic(const PgFont *font) {
    return isOpenTypeItalic((void*)font);
}
const wchar_t *pgGetFontFamily(const PgFont *font) {
    return getOpenTypeFamily((void*)font);
}
const wchar_t *pgGetFontName(const PgFont *font) {
    return getOpenTypeName((void*)font);
}
const wchar_t *pgGetFontStyleName(const PgFont *font) {
    return getOpenTypeStyleName((void*)font);
}
char *pgGetFontFeatures(const PgFont *font) {
    return pgGetOpenTypeFontFeatures((void*)font);
}

float pgGetCharLsb(const PgFont *font, unsigned c) {
    return getOpenTypeCharLsb((void*)font, c);
}
float pgGetCharWidth(const PgFont *font, unsigned c) {
    return getOpenTypeCharWidth((void*)font, c);
}
float pgGetCharsWidthUtf8(const PgFont *font, const char chars[], int len) {
    float width = 0;
    if (len < 0) len = strlen(chars);
    for (int i = 0; i < len; i++) width += pgGetCharWidth(font, chars[i]);
    return width;
}
float pgGetCharsWidth(const PgFont *font, const wchar_t chars[], int len) {
    float width = 0;
    if (len < 0) len = wcslen(chars);
    for (int i = 0; i < len; i++) width += pgGetCharWidth(font, chars[i]);
    return width;
}
float pgFillChar(
    Pg *gs,
    const PgFont *font,
    PgPt at,
    unsigned c,
    uint32_t color)
{
    return ags_otf_fill_char(gs, (void*)font, at, c, color);
}
float pgFillString_utf8(
    Pg *gs,
    const PgFont *font,
    PgPt at,
    const uint8_t chars[],
    int len,
    uint32_t color)
{
    return fillOpenTypeString_utf8(gs, (void*)font, at, chars, len, color);
}
float pgFillString(
    Pg *gs,
    const PgFont *font,
    PgPt at,
    const wchar_t chars[],
    int len,
    uint32_t color)
{
    return fillOpenTypeString(gs, (void*)font, at, chars, len, color);
}
PgPath *pgGetCharPath(const PgFont *font, const PgMatrix *ctm, unsigned c) {
    return ags_get_otf_char_path((void*)font, ctm, c);
}
unsigned pgGetGlyph(const PgFont *font, unsigned c) {
    return getOpenTypeGlyph((void*)font, c);
}
float pgGetGlyph_lsb(const PgFont *font, unsigned g) {
    return getOpenTypeGlyph_lsb((void*)font, g);
}
float pgGetGlyph_width(const PgFont *font, unsigned g) {
    return getOpenTypeGlyph_width((void*)font, g);
}
float pgFillGlyph(
    Pg *gs,
    const PgFont *font,
    PgPt at,
    unsigned g,
    uint32_t color)
{
    return ags_otf_fill_glyph(gs, (void*)font, at, g, color);
}
PgPath *pgGetGlyph_path(const PgFont *font, const PgMatrix *ctm, unsigned g) {
    return getOpenTypeGlyphPath((void*)font, ctm, g);
}

PgPath *pgInterpretSvgPath(const char *svg, const PgMatrix *initial_ctm) {
    static char params[256];
    if (!params['m']) {
        char *name = "mzlhvcsqt";
        int n[] = { 2, 0, 2, 1, 1, 6, 4, 4, 2 };
        memset(params, -1, 256);
        for (int i = 0; name[i]; i++)
            params[name[i]] = params[toupper(name[i])] = n[i];
    }
    
    PgMatrix   ctm = initial_ctm? *initial_ctm: PgIdentityMatrix;
    PgPath     *path = pgNew_path();
    PgPt    cur = {0,0};
    PgPt    start = {0,0};
    PgPt    reflect = {0,0};
    PgPt    b, c;
    int         cmd;
    float       a[6];
    while (*svg) {
        while (isspace(*svg) || *svg==',') svg++;
        if (params[*svg] >= 0) // continue last command
            cmd = *svg++;
        
        for (int i = 0; i < params[cmd]; i++) {
            while (isspace(*svg) || *svg==',') svg++;
            a[i] = strtof(svg, (char**)&svg);
        }
        
//        printf("%c", cmd);
//        for (int i = 0; i < params[cmd]; i++)
//            printf(" %g", a[i]);
//        putchar('\n');

        switch (cmd) {
        case 'm':
            start = cur = pgPt(cur.x + a[0], cur.y + a[1]);
            pgSubpath(path, &ctm, cur);
            break;
        case 'M':
            start = cur = pgPt(a[0], a[1]);
            pgSubpath(path, &ctm, cur);
            break;
        case 'Z':
        case 'z':
            pgClosePath(path);
            cur = start;
            break;
        case 'L':
            cur = pgPt(a[0], a[1]);
            pgLine(path, &ctm, cur);
            break;
        case 'l':
            cur = pgPt(cur.x + a[0], cur.y + a[1]);
            pgLine(path, &ctm, cur);
            break;
        case 'h':
            cur = pgPt(cur.x + a[0], cur.y);
            pgLine(path, &ctm, cur);
            break;
        case 'H':
            cur = pgPt(a[0], cur.y);
            pgLine(path, &ctm, cur);
            break;
        case 'v':
            cur = pgPt(cur.x, cur.y + a[0]);
            pgLine(path, &ctm, cur);
            break;
        case 'V':
            cur = pgPt(cur.x, a[0]);
            pgLine(path, &ctm, cur);
            break;
        case 'c':
            b = pgPt(cur.x + a[0], cur.y + a[1]);
            reflect = pgPt(cur.x + a[2], cur.y + a[3]);
            cur = pgPt(cur.x + a[4], cur.y + a[5]);
            pgCubic(path, &ctm, b, reflect, cur);
            break;
        case 'C':
            b = pgPt(a[0], a[1]);
            reflect = pgPt(a[2], a[3]);
            cur = pgPt(a[4], a[5]);
            pgCubic(path, &ctm, b, reflect, cur);
            break;
        case 's':
            b = pgPt( cur.x + (cur.x - reflect.x),
                    cur.y + (cur.y - reflect.y));
            reflect = pgPt(cur.x + a[0], cur.y + a[1]);
            cur = pgPt(cur.x + a[2], cur.y + a[3]);
            pgCubic(path, &ctm, b, reflect, cur);
            break;
        case 'S':
            b = pgPt( cur.x + (cur.x - reflect.x),
                    cur.y + (cur.y - reflect.y));
            reflect = pgPt(a[0], a[1]);
            cur = pgPt(a[2], a[3]);
            pgCubic(path, &ctm, b, reflect, cur);
            break;
        case 'q':
            reflect = pgPt(cur.x + a[0], cur.y + a[1]);
            cur = pgPt(cur.x + a[2], cur.y + a[3]);
            pgQuadratic(path, &ctm, reflect, cur);
            break;
        case 'Q':
            reflect = pgPt(a[0], a[1]);
            cur = pgPt(a[2], a[3]);
            pgQuadratic(path, &ctm, reflect, cur);
            break;
        case 't':
            reflect = pgPt(cur.x + (cur.x - reflect.x),
                         cur.y + (cur.y - reflect.y));
            cur = pgPt(cur.x + a[0], cur.y + a[1]);
            pgQuadratic(path, &ctm, reflect, cur);
            break;
        case 'T':
            reflect = pgPt(cur.x + (cur.x - reflect.x),
                         cur.y + (cur.y - reflect.y));
            cur = pgPt(a[0], a[1]);
            pgQuadratic(path, &ctm, reflect, cur);
            break;
        }
    }
    return path;
}
