static bool within(int a, int b, int c) {
    return a <= b && b < c;
}
static int clamp(int a, int b, int c) {
    return b <= a? a: b >= c? c: b;
}
static float dist(float x, float y) {
    return sqrtf(x*x + y*y);
}
static PgPt mid(PgPt a, PgPt b) {
    return pgPt((a.x + b.x) / 2, (a.y + b.y) / 2);
}
static float fraction(float x) {
    return x - floorf(x);
}

#define MIN(X,Y) ((X) < (Y)? (X): (Y))
#define MAX(X,Y) ((X) > (Y)? (X): (Y))

#define $(ACTION, SELF, ...) ((SELF)->ACTION)((SELF),__VA_ARGS__)
#define $$(ACTION, SELF, ...) ((SELF)->_.ACTION)(&(SELF)->_,__VA_ARGS__)
#define _$(ACTION, SELF, ...) ((SELF) && (SELF)->ACTION? ((SELF)->ACTION)((SELF),__VA_ARGS__): 0)
#define _$$(ACTION, SELF, ...) ((SELF) && (SELF)->ACTION? ((SELF->_)->ACTION)(&(SELF)->_,__VA_ARGS__): 0)
#define NEW(TYPE) malloc(sizeof(TYPE))
#define NEW_ARRAY(TYPE, N) malloc(sizeof(TYPE)*(N))
#define REALLOC(TARGET,TYPE,N) ((TARGET) = realloc((TARGET), sizeof(TYPE) * (N)))
