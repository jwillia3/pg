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

#define Gamma 2.2f
uint32_t pgBlend(uint32_t bg, uint32_t fg, uint32_t a255) {
    if (a255 == 255) return fg;
    if (a255 == 0) return bg;
    static float enc[256];
    if (!enc[1]) {
        for (float i = 0; i < 256.0f; i++)
            enc[(int)i] = powf(i, Gamma);
    }
    float a = a255 / 255.0f;
    float na = 1.0f - a;
    uint8_t r = powf(a * enc[fg >> 16 & 255] + na * enc[bg >> 16 & 255], 1.0f / Gamma);
    uint8_t g = powf(a * enc[fg >>  8 & 255] + na * enc[bg >>  8 & 255], 1.0f / Gamma);
    uint8_t b = powf(a * enc[fg >>  0 & 255] + na * enc[bg >>  0 & 255], 1.0f / Gamma);
    return (r << 16) + (g << 8) + b;
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
    float m = cosf(rad);
    float n = sinf(rad);
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

PgPt pgTransformPoint(const PgMatrix *ctm, PgPt p) {
    PgPt out = {
        ctm->a * p.x + ctm->c * p.y + ctm->e,
        ctm->b * p.x + ctm->d * p.y + ctm->f
    };
    return out;
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
    PgPath     *path = pgNewPath();
    PgPt        cur = {0,0};
    PgPt        start = {0,0};
    PgPt        reflect = {0,0};
    PgPt        b;
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
            $(move, path, &ctm, cur);
            break;
        case 'M':
            start = cur = pgPt(a[0], a[1]);
            $(move, path, &ctm, cur);
            break;
        case 'Z':
        case 'z':
            $(close, path);
            cur = start;
            break;
        case 'L':
            cur = pgPt(a[0], a[1]);
            $(line, path, &ctm, cur);
            break;
        case 'l':
            cur = pgPt(cur.x + a[0], cur.y + a[1]);
            $(line, path, &ctm, cur);
            break;
        case 'h':
            cur = pgPt(cur.x + a[0], cur.y);
            $(line, path, &ctm, cur);
            break;
        case 'H':
            cur = pgPt(a[0], cur.y);
            $(line, path, &ctm, cur);
            break;
        case 'v':
            cur = pgPt(cur.x, cur.y + a[0]);
            $(line, path, &ctm, cur);
            break;
        case 'V':
            cur = pgPt(cur.x, a[0]);
            $(line, path, &ctm, cur);
            break;
        case 'c':
            b = pgPt(cur.x + a[0], cur.y + a[1]);
            reflect = pgPt(cur.x + a[2], cur.y + a[3]);
            cur = pgPt(cur.x + a[4], cur.y + a[5]);
            $(cubic, path, &ctm, b, reflect, cur);
            break;
        case 'C':
            b = pgPt(a[0], a[1]);
            reflect = pgPt(a[2], a[3]);
            cur = pgPt(a[4], a[5]);
            $(cubic, path, &ctm, b, reflect, cur);
            break;
        case 's':
            b = pgPt( cur.x + (cur.x - reflect.x),
                    cur.y + (cur.y - reflect.y));
            reflect = pgPt(cur.x + a[0], cur.y + a[1]);
            cur = pgPt(cur.x + a[2], cur.y + a[3]);
            $(cubic, path, &ctm, b, reflect, cur);
            break;
        case 'S':
            b = pgPt( cur.x + (cur.x - reflect.x),
                    cur.y + (cur.y - reflect.y));
            reflect = pgPt(a[0], a[1]);
            cur = pgPt(a[2], a[3]);
            $(cubic, path, &ctm, b, reflect, cur);
            break;
        case 'q':
            reflect = pgPt(cur.x + a[0], cur.y + a[1]);
            cur = pgPt(cur.x + a[2], cur.y + a[3]);
            $(quadratic, path, &ctm, reflect, cur);
            break;
        case 'Q':
            reflect = pgPt(a[0], a[1]);
            cur = pgPt(a[2], a[3]);
            $(quadratic, path, &ctm, reflect, cur);
            break;
        case 't':
            reflect = pgPt(cur.x + (cur.x - reflect.x),
                         cur.y + (cur.y - reflect.y));
            cur = pgPt(cur.x + a[0], cur.y + a[1]);
            $(quadratic, path, &ctm, reflect, cur);
            break;
        case 'T':
            reflect = pgPt(cur.x + (cur.x - reflect.x),
                         cur.y + (cur.y - reflect.y));
            cur = pgPt(a[0], a[1]);
            $(quadratic, path, &ctm, reflect, cur);
            break;
        }
    }
    return path;
}
