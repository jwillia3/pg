void _pgScanDirectory(const wchar_t *dir, void per_file(const wchar_t *name, void *data));
wchar_t **_pgListFonts(int *countp);
PgFont *_pgOpenFontFile(const wchar_t *filename, int font_index, bool scan_only);