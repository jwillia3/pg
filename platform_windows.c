#define UNICODE
#define WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#pragma comment(lib, "advapi32")
#include <pg/pg.h>
#include <pg/platform.h>

typedef struct {
    HANDLE file;
    HANDLE mapping;
    HANDLE view;
} Host;

static void *loadFile(Host *host, const wchar_t *filename) {
    host->file = CreateFile(filename,
        GENERIC_READ,
        FILE_SHARE_DELETE | FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (host->file == INVALID_HANDLE_VALUE)
        return NULL;
    size_t size = GetFileSize(host->file, NULL);
    
    host->mapping = CreateFileMapping(host->file,
        NULL,
        PAGE_READONLY,
        0, size,
        NULL);
    
    if (!host->mapping) {
        CloseHandle(host->file);
        return NULL;
    }
    
    host->view = MapViewOfFile(host->mapping, FILE_MAP_READ, 0, 0, 0);
    if (!host->view) {
        CloseHandle(host->mapping);
        CloseHandle(host->file);
        return NULL;
    }
    
    return host->view;
}

static void freeFileMapping(Host *host) {
    UnmapViewOfFile(host->view);
    CloseHandle(host->mapping);
    CloseHandle(host->file);
}

void _pgScanDirectory(const wchar_t *dir, void perFile(const wchar_t *name, void *data)) {
    WIN32_FIND_DATA data;
    if (!dir)
        dir = L"C:\\Windows\\Fonts";
    
    wchar_t search[MAX_PATH];
    if (wcslen(dir) >= MAX_PATH - 2)
        return;
    wcscpy(search, dir);
    wcscat(search, L"/*");
    
    HANDLE h = FindFirstFile(search, &data);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            wchar_t full[MAX_PATH*2];
            Host host;
            
            swprintf(full, MAX_PATH*2, L"%ls\\%ls", dir, data.cFileName);
            if (loadFile(&host, full)) {
                perFile(full, host.view);
                freeFileMapping(&host);
            }
        } while (FindNextFile(h, &data));
        FindClose(h);
    }
}

static void freeHost(PgFont *font) {
    freeFileMapping(font->host);
    free(font->host);
}

PgFont *_pgOpenFontFile(const wchar_t *filename, int font_index, bool scan_only) {
    Host *host = calloc(1, sizeof *host);
    void *data = loadFile(host, filename);
    if (data) {
        PgFont *font = pgLoadFont(data, font_index, scan_only);
        if (font) {
            font->host = host;
            font->_freeHost = freeHost;
        }
        return font;
    } else {
        free(host);
        return NULL;
    }
}