#ifdef _MSC_VER
    #define be32(x) _byteswap_ulong(x)
    #define be16(x) _byteswap_ushort(x)
#else
    #define be32(x) __builtin_bswap32(x)
    #define be16(x) __builtin_bswap16(x)
#endif

void _pgScanDirectory(const wchar_t *dir, void per_file(const wchar_t *name, void *data));
wchar_t **_pgListFonts(int *countp);
PgFont *_pgOpenFontFile(const wchar_t *filename, int font_index, bool scan_only);