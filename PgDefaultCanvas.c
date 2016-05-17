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

static void _identity(Pg *gs) {
    pgIdentityMatrix(&gs->ctm);
}
static void _translate(Pg *gs, float x, float y) {
    pgTranslateMatrix(&gs->ctm, x, y);
}
static void _scale(Pg *gs, float x, float y) {
    pgScaleMatrix(&gs->ctm, x, y);
}
static void _shear(Pg *gs, float x, float y) {
    pgShearMatrix(&gs->ctm, x, y);
}
static void _rotate(Pg *gs, float rad) {
    pgRotateMatrix(&gs->ctm, rad);
}
static void _multiply(Pg *gs, const PgMatrix * __restrict mat) {
    pgMultiplyMatrix(&gs->ctm, mat);
}
static void _ignore() { }
static float _ignoreF() { return 0.0f; }
Pg pgDefaultCanvas() {
    return (Pg){
        .width = 0,
        .height = 0,
        .flatness = 1.001f,
        .subsamples = 3,
        .ctm = { 1, 0, 0, 1, 0, 0 },
        .free = (void*)_ignore,
        .clear = (void*)_ignore,
        .clearSection = (void*)_ignore,
        .fill = (void*)_ignore,
        .fillChar = (void*)_ignoreF,
        .fillGlyph = (void*)_ignoreF,
        .fillString = (void*)_ignoreF,
        .fillUtf8 = (void*)_ignoreF,
        .identity = _identity,
        .translate = _translate,
        .scale = _scale,
        .shear = _shear,
        .rotate = _rotate,
        .multiply = _multiply,
    };
}