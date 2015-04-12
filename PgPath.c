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

static void addPart(PgPath *path, const PgMatrix *ctm, PgPathPartType type, ...) {
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
static void _move(PgPath *path, const PgMatrix *ctm, PgPt p) {
    path->start = pgTransformPoint(ctm, p);
    addPart(path, ctm, PG_PATH_MOVE, &p);
}
static void _close(PgPath *path) {
    addPart(path, &PgIdentityMatrix, PG_PATH_LINE, &path->start);
}
static void _line(PgPath *path, const PgMatrix *ctm, PgPt b) {
    addPart(path, ctm, PG_PATH_LINE, &b);
}
static void _quadratic(PgPath *path, const PgMatrix *ctm, PgPt b, PgPt c) {
    addPart(path, ctm, PG_PATH_QUADRATIC, &b, &c);
}
static void _cubic(PgPath *path, const PgMatrix *ctm, PgPt b, PgPt c, PgPt d) {
    addPart(path, ctm, PG_PATH_CUBIC, &b, &c, &d);
}
static PgRect _box(PgPath *path) {
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
static void _free(PgPath *path) {
    if (path) {
        free(path->types);
        free(path->points);
        free(path);
    }
}
PgPath pgDefaultPath() {
    return (PgPath) {
        .nparts = 0,
        .npoints = 0,
        .cap = 0,
        .types = NULL,
        .points = NULL,
        .start = {0, 0},
        .fillRule = PG_NONZERO_WINDING,
        
        .free = _free,
        .move = _move,
        .close = _close,
        .line = _line,
        .quadratic = _quadratic,
        .cubic = _cubic,
        .box = _box,
    };
}
PgPath *pgNewPath(void) {
    PgPath *path = NEW(PgPath);
    *path = pgDefaultPath();
    return path;
}
