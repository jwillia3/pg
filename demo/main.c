#define UNICODE
#define WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES
#include <float.h>
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#pragma comment(lib, "gdi32")
#pragma comment(lib, "user32")

HBITMAP Bitmap;

#include "../asg.h"
#include "test.h"

Asg *gs;
int Tick;
int animate;
void render();

void *set_size(int width, int height) {
    void *buffer;
    BITMAPINFO info = { {sizeof info.bmiHeader,
        width, -height, 1, 32,  0,width * height * 4, 72, 72, -1, -1 }};
    Bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &buffer, NULL, 0);
    return buffer;
}

LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        PAINTSTRUCT ps;
    switch (msg) {
    case WM_PAINT:
        BeginPaint(hwnd, &ps);
        HDC dc;
        dc = CreateCompatibleDC(ps.hdc);
        SelectObject(dc, Bitmap);
        BitBlt(ps.hdc,
            ps.rcPaint.left, ps.rcPaint.top,
            ps.rcPaint.right, ps.rcPaint.bottom,
            dc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        DeleteDC(dc);
        EndPaint(hwnd, &ps);
        return 0;
    case WM_TIMER:
        render();
        InvalidateRect(hwnd, 0, 0);
        Tick++;
        return 0;
    case WM_SIZE:
        DeleteObject(Bitmap);
        gs->width = LOWORD(lparam);
        gs->height = HIWORD(lparam);
        gs->buf = set_size(gs->width, gs->height);
        render();
        return 0;
    case WM_ERASEBKGND:
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void display(int width, int height) {
    WNDCLASS wc = { CS_HREDRAW|CS_VREDRAW, WndProc, 0, 0,
        GetModuleHandle(NULL), LoadIcon(NULL, IDI_APPLICATION),
        LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1),
        NULL, L"GenericWindow" };
    RegisterClass(&wc);
    
    RECT r = { 0, 0, width, height };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    
    HWND hwnd = CreateWindow(L"GenericWindow", L"Window",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        NULL, NULL, GetModuleHandle(NULL), NULL);
    if (animate)
        SetTimer(hwnd, 0, 1, 0);
//        SetTimer(hwnd, 0, 1000/60, 0);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

//int WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
//    display();
//}

//uint32_t bg = 0xff202020; uint32_t fg = 0xffa08020; uint32_t fg2 = 0xff505050;
uint32_t bg = 0xffe0e0d0; uint32_t fg = 0xff606050; uint32_t fg2 = 0xffa0c0a0;
//uint32_t bg = 0xffffffff; uint32_t fg = 0xff000000; uint32_t fg2 = 0xff800000;

//int animate = true;

void *load_file(char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    rewind(file);
    void *data = malloc(size);
    int read = fread(data, 1, size, file);
    fclose(file);
    return data;
}

int lex_sort(const void *a, const void *b) {
	return strcmp(a, b);
}

const float line_height = 12;
float list_font_test_variant(AsgFontFamily *family, bool italic, int weight, AsgPoint pt) {
	AsgFont *font = asg_open_font_file(
		italic? family->italic[weight]: family->roman[weight],
		italic? family->italic_index[weight]: family->roman_index[weight],
		false);
	
	if (!font)
		return 0;
	asg_scale_font(font, line_height, 0);
	float x2 = asg_fill_string(gs, font, pt, asg_get_font_name(font), -1, fg);
	char buf[10];
	sprintf(buf, " %d", weight*100);
	x2 += asg_fill_string_utf8(gs, font, asg_pt(pt.x + x2, pt.y), buf, -1, fg);
	asg_free_font(font);
	return x2;
}
void list_font_test() {
	
	int nfamilies;
	float max_width = 0;
	AsgFontFamily *families = asg_scan_fonts(NULL, &nfamilies);
	
	AsgPoint pt = { 0, 0 };
	for (int f = 0; f < nfamilies; f++) {
	
		for (int weight = 0; weight < 10; weight++)
			for (int italic = 0; italic < 2; italic++) {
				float width = list_font_test_variant(&families[f], italic, weight, pt);
				if (width) {
					max_width = max(width, max_width);
					
					pt.y += line_height;
					if (pt.y + line_height >= gs->height) {
						pt.y = 0;
						pt.x += max_width;
						max_width = 0;
					}
				}
			}
		asg_free_font_family(&families[f]);
	}
}
void alice_test() {
    static AsgFont *font;
    if (!font) {
//        font = asg_open_font_variant(L"Cambria", 400, false, 0);
        font = asg_open_font_variant(L"RijksoverheidSerif", 400, false, 0);
		asg_set_font_features(font, "onum");
	}
    if (!font) return;


    {
        static char *alice;
        static int skip;
        if (!alice) alice = load_file("alice.txt");
        float font_size = 15.f;
        float line_height = (font_size * 1.125);
		float measure = 500;
		asg_scale_font(font, font_size, 0);
		for (int i = 0, start = skip; i < gs->height / line_height; i++) {
            if (i + start == 0)
                asg_scale_font(font, font_size*1.75, 0);
            else if (i == 1)
                asg_scale_font(font, font_size, 0);
			float max_space = asg_get_char_width(font, ' ') * 3.f;
			
			// Segment into words
			char buf[1024];
			char *words[128];
			int widths[128];
			int nwords = 0;
			
			char *in = alice + start;
			char *out = buf;
			while (*in && *in!='\n') {
				while (*in && isspace(*in)) in++;
				if (!*in || *in=='\n') break;
				words[nwords] = out;
				
				while (*in && !isspace(*in)) *out++ = *in++;
				*out++ = 0;
				widths[nwords] = asg_get_chars_width(font, words[nwords], -1);
				nwords++;
			}
			
			float space = measure;
			for (int i = 0; i < nwords; i++)
				space -= widths[i];
			space /= nwords;
			if (space > max_space)
				space = asg_get_char_width(font, ' ');
			
			// Print word at a time
			AsgPoint p = { gs->width / 2 - measure / 2, i * line_height };
			for (int i = 0; i < nwords; i++) {
				asg_fill_string_utf8(gs,
	                font,
	                p,
	                words[i],
	                -1,
	                fg);
				p.x += widths[i] + space;
			}
                
            int n = strcspn(alice + start, "\n");
            start += n + 1;
        }
        
		if (animate)
        	skip += 1 + strcspn(alice + skip, "\n");
    }
}
void letters_test(int language) {
    static AsgFont *font;
    char *japanese = "%-6.1f 色は匂へど散りぬるを我が世誰ぞ常ならん有為の奥山今日越えて浅き夢見じ酔ひもせず";
    char *english = "%-6.1f The Quick brown=fox jumped over the lazy dog, Götterdämmerung";
//    wchar_t *font_name = L"Calibri";
    wchar_t *font_name = L"Source Code Pro";
	int weight = 400;
	bool italic = false;
	
	if (!font) {
        font = asg_open_font_variant(font_name, weight, italic, 0);
		asg_set_font_features(font, "onum,tnum");
    }
	if (!font) return;
    
	for (float f = 8, x = 0, y = 0; y < gs->height; y += f * 1.25, f+=.5) {
        char buf[128];
        sprintf(buf, 
            language? japanese: english,
            f);
        asg_scale_font(font, f, 0);
        asg_fill_string_utf8(gs,
            font,
            asg_pt(x+Tick,y),
            buf,
            -1,
            fg);
    }
	asg_scale_font(font, 72, 0);
	asg_fill_string(gs,
		font,
		asg_pt(300, 0),
		font_name,
		-1,
		(fg & 0xffffff) | 0x60000000);
}
void glyph_test() {
	wchar_t *font_name = L"Fira Mono";
	AsgFont *font = asg_open_font_variant(font_name, 400, false, 0);
	if (!font) return;
	float font_height = sqrt(gs->width * gs->height / ((AsgOTF*)font)->nglyphs);
	
	unsigned g = 0;
    unsigned n = ((AsgOTF*)font)->nglyphs;
	for (int y = 0; y < gs->height && g < n; y += font_height)
	for (int x = 0; x < gs->width && g < n; x += font_height) {
        wchar_t buf[5];
		swprintf(buf, 5, L"%04X", g);
		asg_scale_font(font, 8, 0);
		asg_fill_string(gs, font, asg_pt(x,y), buf, 4, 0xff000000);
		
		
		asg_scale_font(font, font_height-5, 0);
		asg_fill_glyph(gs, font, asg_pt(x,y+5), g++, fg);
    }
	asg_free_font(font);
}

void svg_test() {
    asg_translate(gs, -396/2, -468/2);
    asg_rotate(gs, Tick / 3.f * M_PI / 180.f);
    asg_translate(gs, gs->width / 2, gs->height / 2);
    
    for (int i = 0; TestSVG[i]; i++) {
        AsgPath *path = asg_get_svg_path(TestSVG[i], &gs->ctm);
        asg_fill_path(gs, path, fg);
//        asg_stroke_path(gs, path, fg2);
        asg_free_path(path);
    }
}
void simple_test() {
    asg_scale(gs, .5, .5);
    asg_rotate(gs, -Tick * M_PI / 180.f);
    asg_translate(gs, 100, 100);
    AsgPath *path = asg_new_path();
    asg_add_subpath(path, &gs->ctm, asg_pt(0, 300));
    asg_add_bezier4(path, &gs->ctm,
        asg_pt(0, 250),
        asg_pt(300, 250),
        asg_pt(300, 300));
    asg_add_bezier4(path, &gs->ctm,
        asg_pt(300, 600),
        asg_pt(0, 600),
        asg_pt(0, 300));

    asg_close_subpath(path);
    asg_add_subpath(path, &gs->ctm, asg_pt(300, 0));
    asg_add_line(path, &gs->ctm, asg_pt(100, 400));
    asg_add_line(path, &gs->ctm, asg_pt(500, 400));
    asg_close_subpath(path);
    asg_fill_path(gs, path, fg);
//    asg_stroke_path(gs, path, fg2);
    asg_free_path(path);
}

void benchmark() {
    int start = GetTickCount();
    for (int i = 0; i < 100; i++)
        svg_test();
    int end = GetTickCount();
    printf("%d ms\n", end - start);
}

void typography_test(const wchar_t *family) {
	AsgFont *font = asg_open_font_variant(family, 0,0,0);
	if (!font) {
		font = asg_open_font_variant(L"Arial", 0,0,0);
		asg_scale_font(font, 30, 0);
		asg_fill_string(gs, font, asg_pt(0,0), L"Font Not Loaded", -1, fg);
		asg_free_font(font);
		return;
	}
	
	float heading = 16;
	float body = 12;
	
	char *features = asg_get_font_features(font);
	if (!features) return;
	
	float left = 0;
	asg_scale_font(font, body, 0);
	for (int f = 0; features[f]; f += 4) {
		float w = asg_fill_string_utf8(gs, font, asg_pt(0,f/4*body), features+f, 4, fg);
		left = max(left, w);
	}
	left += 10;
	
	AsgPoint p = { left, heading };
	asg_scale_font(font, body, 0);
	float column = 18 * asg_get_char_width(font, 'M');
	float max_height = heading;
	float top = heading;
	
	for (int f = 0; features[f]; f += 4) {
		char feature[5];
		uint16_t substitutions[4096][2];
		int nsubstitutions;
		memcpy(feature,features+f,4);
		feature[4] = 0;
		
		asg_scale_font(font, heading, 0);
		asg_fill_string_utf8(gs, font, asg_pt(p.x, p.y - heading), feature, -1, fg);
		asg_scale_font(font, body, 0);
		
		asg_set_font_features(font, feature);
		nsubstitutions = ((AsgOTF*)font)->nsubst;
		for (int i = 0; i < nsubstitutions ; i++) {
			substitutions[i][0] = ((AsgOTF*)font)->subst[i][0];
			substitutions[i][1] = ((AsgOTF*)font)->subst[i][1];
		}
		asg_set_font_features(font, "");
		
		for (int i = 0; i < nsubstitutions; i++) {
			float w = asg_fill_glyph(gs, font, p, substitutions[i][0], fg);
			w += asg_fill_char(gs, font, asg_pt(p.x+w, p.y), ' ', fg);
			w += asg_fill_glyph(gs, font, asg_pt(p.x+w, p.y), substitutions[i][1], fg);
			char buf[128];
			sprintf(buf, " %04x - %04x", substitutions[i][0] & 0xffff, substitutions[i][1] & 0xffff);
			asg_fill_string_utf8(gs, font, asg_pt(p.x+w, p.y), buf, -1, fg2);
			p.y += body;
		}
		
		max_height = max(max_height, p.y);
		p.x += column;
		p.y = top;
		if (p.x + column >= gs->width) {
			p.x = left;
			p.y = top = max_height + heading;
		}
	}
	
}

void render() {
    asg_clear(gs, bg);
    asg_load_identity(gs);
    
//    simple_test();
//    svg_test();
//    letters_test(0);
//    glyph_test();
//    alice_test();
    list_font_test();
//		typography_test(L"Source Code Pro");
//		typography_test(L"RijksoverheidSansText");
//    benchmark(); return false;

//	AsgFont *font = asg_open_font_variant(L"Source Code Pro", 0,0,0);
//	if (!font) return;
////	asg_fill_char(gs, font, asg_pt(0,0), '=', fg);
//	AsgPath *path = asg_get_char_path(font, &gs->ctm, '=');
//	asg_stroke_path(gs, path, fg);
//

}

int main() {
    HINSTANCE kernel32 = LoadLibrary(L"kernel32.dll");
	BOOL (*GetProcessUserModeExceptionPolicy)(DWORD*) = kernel32? (void*)GetProcAddress(kernel32, "GetProcessUserModeExceptionPolicy"): 0;
	BOOL (*SetProcessUserModeExceptionPolicy)(DWORD) = kernel32? (void*)GetProcAddress(kernel32, "SetProcessUserModeExceptionPolicy"): 0;
	if (!GetProcessUserModeExceptionPolicy)
		FatalAppExit(0, L"XXX");
	DWORD dwFlags;
	#define PROCESS_CALLBACK_FILTER_ENABLED     0x1
	if (GetProcessUserModeExceptionPolicy(&dwFlags)) {
	     SetProcessUserModeExceptionPolicy(dwFlags & ~PROCESS_CALLBACK_FILTER_ENABLED); // turn off bit 1
	}
    
    int width = 1024, height = 800;
    gs = asg_new(set_size(width, height), width, height);
    display(width, height);
}