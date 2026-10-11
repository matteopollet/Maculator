// Minimal Win32 stubs so the calculator logic compiles on macOS for testing.
#ifndef CALC_STUB_H
#define CALC_STUB_H
#include <stdint.h>
typedef void *HWND, *HFONT, *HDC, *HBITMAP, *HBRUSH, *HPEN, *HRGN, *HINSTANCE, *HICON, *HCURSOR;
typedef unsigned int UINT, COLORREF;
typedef long LPARAM, LRESULT;
typedef unsigned long long WPARAM, DWORD;
typedef int BOOL;
#include <stddef.h>
typedef wchar_t WCHAR;
typedef struct { int x, y; } POINT;
typedef struct { int left, top, right, bottom; } RECT;
#define RGB(r,g,b) ((COLORREF)(((uint8_t)(r))|((uint16_t)(g)<<8)|((uint32_t)(b)<<16)))
#define GetRValue(c) ((c)&0xFF)
#define GetGValue(c) (((c)>>8)&0xFF)
#define GetBValue(c) (((c)>>16)&0xFF)
#define InvalidateRect(a,b,c) ((void)0)
#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL ((void*)0)
#endif
#endif
