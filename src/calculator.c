// Windows clone of macOS Calculator 12.0 (Basic mode) — pure Win32, no deps.
// Spec extracted from /System/Applications/Calculator.app via REA.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00   // Win10: DPI awareness APIs
#endif
#ifdef CALC_TEST
#include "calc_stub.h"
#else
#include <windows.h>
#include <windowsx.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ---------- geometry & colors (window-relative px) ----------
#define WIN_W   230
#define WIN_H   408
#define BTN_D   48
#define GRID_X0 10
#define GRID_Y0 133
#define GRID_P  54

#define C_BG     RGB(37, 35, 35)
#define C_DIGIT  RGB(93, 92, 92)
#define C_TOP    RGB(133, 133, 133)
#define C_OP     RGB(255, 148, 2)
#define C_GLYPH  RGB(250, 250, 250)
#define C_EXPR   RGB(160, 159, 159)
#define C_RESULT RGB(228, 227, 227)
#define C_RED    RGB(255, 95, 86)
#define C_YELLOW RGB(254, 188, 46)
#define C_GREEN  RGB(40, 201, 64)

#define IDC_BASE 1000

// ---------- expression model ----------
// Values as exact rationals: num/den, den>0. int64 math is enough:
// display caps at ~16 significant digits; intermediate products stay < 9e18
// when inputs are bounded like the real app (max ~16 digits per operand).

typedef enum { T_NUM, T_ADD, T_SUB, T_MUL, T_DIV, T_PCT, T_OPEN, T_CLOSE } Tok;

typedef struct { int64_t n, d; } Rat;

static Rat rat(int64_t n, int64_t d) {
    if (d < 0) { n = -n; d = -d; }
    Rat r = { n, d ? d : 1 };
    return r;
}
static int64_t gcd64(int64_t a, int64_t b) {
    a = a < 0 ? -a : a; b = b < 0 ? -b : b;
    while (b) { int64_t t = a % b; a = b; b = t; }
    return a ? a : 1;
}
static Rat rnorm(Rat r) {
    int64_t g = gcd64(r.n, r.d);
    return rat(r.n / g, r.d / g);
}
static Rat radd(Rat a, Rat b) { return rnorm(rat(a.n * b.d + b.n * a.d, a.d * b.d)); }
static Rat rsub(Rat a, Rat b) { return rnorm(rat(a.n * b.d - b.n * a.d, a.d * b.d)); }
static Rat rmul(Rat a, Rat b) { return rnorm(rat(a.n * b.n, a.d * b.d)); }
static Rat rdiv(Rat a, Rat b, int *undef) {
    if (b.n == 0) { *undef = 1; return rat(0, 1); }
    return rnorm(rat(a.n * b.d, a.d * b.n));
}
static int rneg(Rat r) { return r.n < 0; }

// decimal-string operand buffer (what the user is typing)
typedef struct {
    char digits[32];   // significant digits, no separators
    int  ndig;         // digit count
    int  frac;         // digits after the decimal point
    int  neg;          // leading minus
    int  has_dec;      // a ',' was typed (frac may still be 0)
} NumBuf;

// rebuild a NumBuf from a formatted number string (no group separators)
static void str_numbuf(const char *s, NumBuf *nb) {
    *nb = (NumBuf){0};
    const char *p = s;
    if (p[0] == (char)0xE2) { nb->neg = 1; p += 3; }   // '−' UTF-8
    for (; *p && nb->ndig < 30; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == ',') { nb->has_dec = 1; continue; }
        if (c == 0xE2) break;                           // '…' or '−': stop
        if (c >= '0' && c <= '9') {
            nb->digits[nb->ndig++] = (char)c;
            if (nb->has_dec) nb->frac++;
        }
    }
    if (!nb->ndig) { nb->digits[0] = '0'; nb->ndig = 1; }
}

// expression token stream (numbers stored as text, converted on eval)
#define MAX_TOK 64
typedef struct {
    Tok t;
    NumBuf nb;         // valid when t==T_NUM
    int  negated;      // ± applied to this num (display "(negative n)")
} Expr;

// ---------- app state ----------
static NumBuf g_cur;            // current operand being typed
static int    g_typing;         // currently entering a number
static Expr   g_expr[MAX_TOK];  // committed expression tokens
static int    g_nexpr;
static int    g_undef;          // last eval produced Undefined
static int    g_shown_result;   // a result is displayed (post '=')
static Rat    g_result;         // last computed result
static Tok    g_lastop;         // last binary op for repeat-'='
static NumBuf g_lastoperand;    // operand for repeat-'='
static int    g_has_repeat;     // repeat info valid
static char   g_expr_text[512]; // rendered expression line
static char   g_res_text[64];   // rendered result line
static int    g_hover = -1;     // hovered button id
static int    g_press = -1;     // pressed button id
static HWND   g_hwnd;
static HFONT  g_fntBtn, g_fntExpr, g_fntRes;
static int    g_expr_ellipsis;  // result truncated with …
static int    g_error;          // display "Undefined"

// ---------- button table ----------
// row: 0..4, col: 0..3 — cell function ids
enum { F_BS, F_AC, F_PCT, F_DIV, F_7, F_8, F_9, F_MUL, F_4, F_5, F_6, F_SUB,
       F_1, F_2, F_3, F_ADD, F_PM, F_0, F_DEC, F_EQ };

typedef struct { int id; const WCHAR *label; COLORREF bg; } Btn;

static const Btn g_btns[20] = {
    { F_BS,  L"\x232B",    C_TOP   },
    { F_AC,  L"AC",        C_TOP   },
    { F_PCT, L"%",         C_TOP   },
    { F_DIV, L"\x00F7",    C_OP    },
    { F_7,   L"7",         C_DIGIT },
    { F_8,   L"8",         C_DIGIT },
    { F_9,   L"9",         C_DIGIT },
    { F_MUL, L"\x00D7",    C_OP    },
    { F_4,   L"4",         C_DIGIT },
    { F_5,   L"5",         C_DIGIT },
    { F_6,   L"6",         C_DIGIT },
    { F_SUB, L"\x2212",    C_OP    },
    { F_1,   L"1",         C_DIGIT },
    { F_2,   L"2",         C_DIGIT },
    { F_3,   L"3",         C_DIGIT },
    { F_ADD, L"+",         C_OP    },
    { F_PM,  L"\x207A\x2044\x208B", C_DIGIT }, // ⁺/₋
    { F_0,   L"0",         C_DIGIT },
    { F_DEC, L",",         C_DIGIT },
    { F_EQ,  L"=",         C_OP    },
};

// ---------- rational <-> text ----------

// NumBuf -> rational value
static Rat numbuf_rat(const NumBuf *nb) {
    int64_t v = 0;
    for (int i = 0; i < nb->ndig; i++)
        v = v * 10 + (nb->digits[i] - '0');
    int64_t den = 1;
    for (int i = 0; i < nb->frac; i++) den *= 10;
    Rat r = rat(v, den);
    if (nb->neg) r.n = -r.n;
    return rnorm(r);
}

// rational -> decimal text "1234,56" or "…" truncated at 8 decimals.
// returns 1 if truncated (non-terminating within 8 decimals shown)
static void rat_str(Rat r, int group, char *out, size_t cap, int *trunc) {
    *trunc = 0;
    int neg = rneg(r);
    int64_t n = neg ? -r.n : r.n, d = r.d;
    int64_t ip = n / d, rem = n % d;
    char digs[64]; int nd = 0;
    if (ip == 0) digs[nd++] = '0';
    int64_t t = ip;
    while (t) { digs[nd++] = (char)('0' + t % 10); t /= 10; }
    for (int i = 0, j = nd - 1; i < j; i++, j--) { char c = digs[i]; digs[i] = digs[j]; digs[j] = c; }
    // up to 8 decimals, then detect residue -> ellipsis
    char frac[16]; int nf = 0;
    for (int i = 0; i < 8 && rem; i++) { rem *= 10; frac[nf++] = (char)('0' + rem / d); rem %= d; }
    if (rem) *trunc = 1;
    // trim trailing zeros in frac
    while (nf && frac[nf - 1] == '0') nf--;
    // assemble with grouping thin-space every 3 int digits (from right)
    char *o = out, *end = out + cap - 2;
    if (neg) *o++ = (char)0xE2, *o++ = (char)0x88, *o++ = (char)0x92; // −
    int first = 1;
    for (int i = 0; i < nd; i++) {
        int rem3 = nd - i;
        if (!first && group && rem3 % 3 == 0 && o + 3 < end) {
            *o++ = (char)0xE2; *o++ = (char)0x80; *o++ = (char)0xAF; // U+202F
        }
        first = 0;
        *o++ = digs[i];
    }
    if (nf) {
        *o++ = ',';
        memcpy(o, frac, (size_t)nf); o += nf;
    }
    if (*trunc) { *o++ = (char)0xE2; *o++ = (char)0x80; *o++ = (char)0xA6; } // …
    *o = 0;
}

// NumBuf -> display text with grouping + decimal comma
static void numbuf_str(const NumBuf *nb, char *out, size_t cap) {
    char *o = out, *end = out + cap - 2;
    if (nb->neg) { *o++ = (char)0xE2; *o++ = (char)0x88; *o++ = (char)0x92; }
    int intd = nb->ndig - nb->frac;
    if (intd <= 0) intd = 0;
    if (nb->ndig == 0) { *o++ = '0'; }
    for (int i = 0; i < nb->ndig; i++) {
        if (i < intd) {
            int rem3 = intd - i;
            if (i && rem3 % 3 == 0 && o + 3 < end) {
                *o++ = (char)0xE2; *o++ = (char)0x80; *o++ = (char)0xAF;
            }
        } else if (i == intd) {
            *o++ = ',';
        }
        *o++ = nb->digits[i];
    }
    if (nb->has_dec && nb->frac == 0) *o++ = ',';
    *o = 0;
}

static const char *op_str(Tok t) {
    switch (t) {
        case T_ADD: return " + ";
        case T_SUB: return " \xE2\x88\x92 ";   // −
        case T_MUL: return " \xC3\x97 ";       // ×
        case T_DIV: return " \xC3\xB7 ";       // ÷
        default: return "";
    }
}

// ---------- expression -> display text ----------
static void render_expr(void) {
    char *o = g_expr_text; char *end = o + sizeof(g_expr_text) - 8;
    *o = 0;
    for (int i = 0; i < g_nexpr; i++) {
        Expr *e = &g_expr[i];
        if (e->t == T_NUM) {
            char tmp[128];
            if (e->negated) {
                // "(negative n)" — render "(−n)"
                char inner[96]; numbuf_str(&e->nb, inner, sizeof(inner));
                snprintf(tmp, sizeof(tmp), "(%s)", inner);
            } else {
                numbuf_str(&e->nb, tmp, sizeof(tmp));
            }
            size_t l = strlen(tmp);
            if (o + l >= end) break;
            memcpy(o, tmp, l); o += l;
        } else if (e->t == T_PCT) {
            if (o + 2 >= end) break;
            *o++ = ' '; *o++ = '%';
        } else {
            const char *s = op_str(e->t);
            size_t l = strlen(s);
            if (o + l >= end) break;
            memcpy(o, s, l); o += l;
        }
    }
    if (g_typing) {
        char tmp[128]; numbuf_str(&g_cur, tmp, sizeof(tmp));
        size_t l = strlen(tmp);
        if (o + l < end) { memcpy(o, tmp, l); o += l; }
    }
    while (o > g_expr_text && o[-1] == ' ') o--;
    *o = 0;
    if (g_nexpr == 0 && !g_typing) strcpy(g_expr_text, "0");
}

// ---------- evaluation ----------
// tokens: NUM (op NUM)* with precedence × ÷ % over + −
// % applies to preceding operand: after +/− -> pct of that term's left context;
// after ×/÷ or standalone -> /100.
static int eval_expr(const Expr *ex, int n, Rat *out) {
    // shunting-yard with % as tight postfix; int64 rationals.
    typedef struct { Rat v; } V;
    V stack[2 * MAX_TOK]; int sp = 0;
    Tok opstack[2 * MAX_TOK]; int osp = 0;
    int i = 0;
    #define PUSHV(r) stack[sp++].v = (r)
    int undef = 0;
    while (i < n) {
        if (ex[i].t == T_NUM) {
            Rat v = numbuf_rat(&ex[i].nb);
            if (ex[i].negated) v.n = -v.n;
            PUSHV(v); i++;
            // postfix percents immediately following
            while (i < n && ex[i].t == T_PCT) {
                // % context: look at pending binary op (top of opstack)
                Tok ctx = osp ? opstack[osp - 1] : 0;
                if (ctx == T_ADD || ctx == T_SUB) {
                    // a + b% : b% = a * b/100   (need 'a' = value below pending op)
                    // find the left operand of the pending +/- op:
                    // it's the value pushed before it (accounting for ×÷ already folded)
                    if (sp >= 2) {
                        Rat base = stack[sp - 2].v;
                        stack[sp - 1].v = rmul(base, rdiv(stack[sp - 1].v, rat(100, 1), &undef));
                    } else {
                        stack[sp - 1].v = rdiv(stack[sp - 1].v, rat(100, 1), &undef);
                    }
                } else {
                    stack[sp - 1].v = rdiv(stack[sp - 1].v, rat(100, 1), &undef);
                }
                i++;
            }
            continue;
        }
        Tok t = ex[i].t; i++;
        while (osp && ((opstack[osp - 1] == T_MUL || opstack[osp - 1] == T_DIV) ||
                       ((opstack[osp - 1] == T_ADD || opstack[osp - 1] == T_SUB) &&
                        (t == T_ADD || t == T_SUB)))) {
            Tok op = opstack[--osp];
            if (sp < 2) break;
            Rat b = stack[--sp].v, a = stack[--sp].v, r = rat(0, 1);
            if (op == T_ADD) r = radd(a, b);
            else if (op == T_SUB) r = rsub(a, b);
            else if (op == T_MUL) r = rmul(a, b);
            else if (op == T_DIV) r = rdiv(a, b, &undef);
            PUSHV(r);
        }
        opstack[osp++] = t;
    }
    while (osp) {
        Tok op = opstack[--osp];
        if (sp < 2) break;
        Rat b = stack[--sp].v, a = stack[--sp].v, r = rat(0, 1);
        if (op == T_ADD) r = radd(a, b);
        else if (op == T_SUB) r = rsub(a, b);
        else if (op == T_MUL) r = rmul(a, b);
        else if (op == T_DIV) r = rdiv(a, b, &undef);
        PUSHV(r);
    }
    #undef PUSHV
    if (undef) return 0;
    if (sp < 1) { *out = rat(0, 1); return 1; }
    *out = stack[0].v;
    return 1;
}

// ---------- state transitions ----------
static void clear_all(void) {
    g_nexpr = 0; g_typing = 0; g_cur = (NumBuf){0};
    g_error = 0; g_shown_result = 0; g_res_text[0] = 0; g_expr_ellipsis = 0;
    render_expr();
}

static void commit_num(void) {
    if (g_typing) {
        g_expr[g_nexpr].t = T_NUM;
        g_expr[g_nexpr].nb = g_cur;
        g_expr[g_nexpr].negated = 0;
        g_nexpr++;
        g_typing = 0; g_cur = (NumBuf){0};
    }
}

static void refresh_result(void) {
    g_res_text[0] = 0;
    render_expr();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void press_digit(int d) {
    if (g_error || g_shown_result) clear_all();
    g_error = 0;
    if (!g_typing) { g_typing = 1; g_cur = (NumBuf){0}; }
    if (g_cur.ndig >= 16) return;               // input cap like the real app
    if (g_cur.ndig == 1 && g_cur.digits[0] == '0' && !g_cur.has_dec)
        g_cur.ndig = 0;                          // leading zero collapses
    g_cur.digits[g_cur.ndig++] = (char)('0' + d);
    if (g_cur.has_dec) g_cur.frac++;
    refresh_result();
}

static void press_dec(void) {
    if (g_error || g_shown_result) clear_all();
    g_error = 0;
    if (!g_typing) {
        g_typing = 1; g_cur = (NumBuf){0};
        g_cur.digits[0] = '0'; g_cur.ndig = 1;     // ',' -> "0,"
    }
    if (g_cur.has_dec) return;                   // one comma only
    g_cur.has_dec = 1;
    refresh_result();
}

static void press_op(Tok op) {
    if (g_error) return;
    if (g_shown_result) {
        // continue from result
        NumBuf nb; char s[64]; int tr;
        rat_str(g_result, 0, s, sizeof(s), &tr);
        str_numbuf(s, &nb);
        g_nexpr = 0; g_expr[g_nexpr].t = T_NUM; g_expr[g_nexpr].nb = nb; g_expr[g_nexpr].negated = 0; g_nexpr = 1;
        g_shown_result = 0; g_res_text[0] = 0;
    }
    commit_num();
    // replace trailing operator if last token is an op
    if (g_nexpr && g_expr[g_nexpr - 1].t != T_NUM) {
        g_expr[g_nexpr - 1].t = op;
    } else {
        if (!g_nexpr) {                          // "0 +" case
            g_expr[g_nexpr].t = T_NUM;
            g_expr[g_nexpr].nb = (NumBuf){0};
            g_expr[g_nexpr].nb.digits[0] = '0';
            g_expr[g_nexpr].nb.ndig = 1;
            g_expr[g_nexpr].negated = 0;
            g_nexpr++;
        }
        g_expr[g_nexpr].t = op; g_expr[g_nexpr].negated = 0; g_nexpr++;
    }
    refresh_result();
}

static void press_pct(void) {
    if (g_error) return;
    commit_num();
    if (!g_nexpr) {                              // % on empty = "0 %"
        g_expr[g_nexpr].t = T_NUM;
        g_expr[g_nexpr].nb = (NumBuf){0};
        g_expr[g_nexpr].nb.digits[0] = '0'; g_expr[g_nexpr].nb.ndig = 1;
        g_expr[g_nexpr].negated = 0; g_nexpr++;
    }
    g_expr[g_nexpr].t = T_PCT; g_expr[g_nexpr].negated = 0; g_nexpr++;
    refresh_result();
}

static void press_pm(void) {
    if (g_error) return;
    if (g_typing) { g_cur.neg = !g_cur.neg; refresh_result(); return; }
    if (g_shown_result) {
        // negate result into a fresh expression "(negative r)"
        NumBuf nb; char s[64]; int tr;
        rat_str(g_result, 0, s, sizeof(s), &tr);
        str_numbuf(s, &nb); nb.neg = 0;
        int neg = s[0] == (char)0xE2;
        g_nexpr = 0;
        g_expr[0].t = T_NUM; g_expr[0].nb = nb;
        g_expr[0].negated = !neg;
        g_nexpr = 1;
        g_result.n = -g_result.n;
        int tr2; rat_str(g_result, 1, g_res_text, sizeof(g_res_text), &tr2);
        render_expr(); InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    // negate last committed number or whole current entry
    if (g_nexpr && g_expr[g_nexpr - 1].t == T_NUM) {
        g_expr[g_nexpr - 1].negated = !g_expr[g_nexpr - 1].negated;
    } else {
        g_typing = 1; g_cur = (NumBuf){0}; g_cur.neg = 1;
        g_cur.digits[0] = '0'; g_cur.ndig = 1;
    }
    refresh_result();
}

static void press_bs(void) {
    if (g_error) { clear_all(); return; }
    if (g_typing) {
        if (g_cur.has_dec && g_cur.frac == 0) { g_cur.has_dec = 0; }
        else if (g_cur.ndig > 0) {
            g_cur.ndig--;
            if (g_cur.has_dec && g_cur.frac > 0) g_cur.frac--;
        }
        if (g_cur.ndig == 0) { g_typing = 0; g_cur = (NumBuf){0}; }
    } else if (g_nexpr) {
        g_nexpr--;                               // delete last token
    }
    refresh_result();
}

static void press_ac(void) {
    if (g_error) { clear_all(); return; }
    if (g_typing) {                              // first: clear current entry
        g_typing = 0; g_cur = (NumBuf){0};
        refresh_result(); return;
    }
    if (g_shown_result || g_nexpr) {             // second: clear all
        clear_all(); return;
    }
    refresh_result();
}

static void press_eq(void) {
    if (g_error) return;
    if (g_shown_result && g_has_repeat) {
        // repeat last op on result: "r op lastoperand ="
        NumBuf first, last = g_lastoperand;
        char s[64]; int tr;
        rat_str(g_result, 0, s, sizeof(s), &tr);
        str_numbuf(s, &first);
        g_nexpr = 0;
        g_expr[g_nexpr].t = T_NUM; g_expr[g_nexpr].nb = first; g_expr[g_nexpr].negated = 0; g_nexpr++;
        g_expr[g_nexpr].t = g_lastop; g_expr[g_nexpr].negated = 0; g_nexpr++;
        g_expr[g_nexpr].t = T_NUM; g_expr[g_nexpr].nb = last; g_expr[g_nexpr].negated = 0; g_nexpr++;
        commit_num();
        goto evalnow;
    }
    commit_num();
    // incomplete expression: drop trailing binary ops (keep trailing %)
    while (g_nexpr && (g_expr[g_nexpr - 1].t == T_ADD || g_expr[g_nexpr - 1].t == T_SUB ||
                       g_expr[g_nexpr - 1].t == T_MUL || g_expr[g_nexpr - 1].t == T_DIV))
        g_nexpr--;
    if (!g_nexpr) { refresh_result(); return; }
evalnow:;
    // remember last op + operand for repeat
    for (int i = g_nexpr - 1; i >= 0; i--) {
        if (g_expr[i].t == T_NUM && i > 0 && g_expr[i - 1].t != T_NUM && g_expr[i - 1].t != T_PCT) {
            g_lastop = g_expr[i - 1].t; g_lastoperand = g_expr[i].nb;
            g_has_repeat = 1; break;
        }
    }
    Rat r; int undef = !eval_expr(g_expr, g_nexpr, &r);
    if (undef) {
        g_error = 1; g_result = rat(0, 1);
        g_res_text[0] = 0;
        render_expr(); InvalidateRect(g_hwnd, NULL, FALSE); return;
    }
    g_result = r;
    int tr; rat_str(r, 1, g_res_text, sizeof(g_res_text), &tr);
    g_expr_ellipsis = tr;
    g_shown_result = 1;
    render_expr(); InvalidateRect(g_hwnd, NULL, FALSE);
}

// ---------- input ----------
static void on_btn(int id) {
    switch (id) {
        case F_BS:  press_bs();  break;
        case F_AC:  press_ac();  break;
        case F_PCT: press_pct(); break;
        case F_DIV: press_op(T_DIV); break;
        case F_MUL: press_op(T_MUL); break;
        case F_SUB: press_op(T_SUB); break;
        case F_ADD: press_op(T_ADD); break;
        case F_PM:  press_pm();  break;
        case F_DEC: press_dec(); break;
        case F_EQ:  press_eq();  break;
    }
}

// digit ids aren't contiguous with F_0; map explicitly
static void on_btn_real(int id) {
    if (id >= F_7 && id <= F_9) press_digit(id - F_7 + 7);
    else if (id >= F_4 && id <= F_6) press_digit(id - F_4 + 4);
    else if (id >= F_1 && id <= F_3) press_digit(id - F_1 + 1);
    else if (id == F_0) press_digit(0);
    else on_btn(id);
}

// ---------- drawing ----------
#ifndef CALC_TEST
// All spec numbers are in "design px" (the measured macOS window, 230×408).
// g_s maps them to real pixels for the monitor DPI; rendering additionally
// happens at g_s*SS then StretchBlt down — cheap antialiasing, since GDI
// never AA's its primitives.
#define SS 4
static float g_s = 1.0f, g_rsc = SS;
static int S(int v) { return (int)(v * g_s + 0.5f); }
#define R(v) ((int)((v) * g_rsc + 0.5f))

static void make_fonts(void) {
    g_rsc = g_s * SS;
    if (g_fntBtn) { DeleteObject(g_fntBtn); DeleteObject(g_fntExpr); DeleteObject(g_fntRes); }
    // grayscale AA: ClearType's subpixel fringes turn to color noise when the
    // 4x buffer is downsampled
    g_fntBtn  = CreateFontW(-R(20), 0, 0, 0, FW_REGULAR, 0, 0, 0, DEFAULT_CHARSET,
                            0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    g_fntExpr = CreateFontW(-R(22), 0, 0, 0, FW_REGULAR, 0, 0, 0, DEFAULT_CHARSET,
                            0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    g_fntRes  = CreateFontW(-R(31), 0, 0, 0, FW_LIGHT, 0, 0, 0, DEFAULT_CHARSET,
                            0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI Light");
}

static void set_rgn(HWND h) {
    SetWindowRgn(h, CreateRoundRectRgn(0, 0, S(WIN_W) + 1, S(WIN_H) + 1, S(20), S(20)), TRUE);
}

// vector-drawn button glyphs — no font dependency (⌫ and ⁺/₋ are missing
// from many fallback fonts)
static void draw_icon(HDC m, int id, int cx, int cy) {
    HPEN p = CreatePen(PS_SOLID, R(2), C_GLYPH);
    HPEN op = (HPEN)SelectObject(m, p);
    if (id == F_BS) {
        POINT tag[6] = { {cx+R(-11),cy}, {cx+R(-4),cy+R(-7)}, {cx+R(11),cy+R(-7)},
                         {cx+R(11),cy+R(7)}, {cx+R(-4),cy+R(7)}, {cx+R(-11),cy} };
        HBRUSH ob = (HBRUSH)SelectObject(m, GetStockObject(NULL_BRUSH));
        Polygon(m, tag, 6);
        SelectObject(m, ob);
        MoveToEx(m, cx+R(1), cy+R(-3), NULL); LineTo(m, cx+R(7), cy+R(3));
        MoveToEx(m, cx+R(7), cy+R(-3), NULL); LineTo(m, cx+R(1), cy+R(3));
    } else { // F_PM: "+/−" drawn inline
        MoveToEx(m, cx+R(-12), cy, NULL); LineTo(m, cx+R(-6), cy);
        MoveToEx(m, cx+R(-9), cy+R(-3), NULL); LineTo(m, cx+R(-9), cy+R(3));
        MoveToEx(m, cx+R(-2), cy+R(5), NULL); LineTo(m, cx+R(2), cy+R(-5));
        MoveToEx(m, cx+R(5), cy, NULL);  LineTo(m, cx+R(11), cy);
    }
    SelectObject(m, op); DeleteObject(p);
}

// toolbar icons, top-right: sidebar toggle + converter, dark discs + light glyph
static void draw_toolbar_icon(HDC m, int which, int cx, int cy) {
    COLORREF face = RGB(53, 51, 51), glyph = RGB(218, 217, 217);
    HBRUSH b = CreateSolidBrush(face);
    HBRUSH ob = (HBRUSH)SelectObject(m, b);
    HPEN np = (HPEN)SelectObject(m, GetStockObject(NULL_PEN));
    Ellipse(m, cx - R(11), cy - R(11), cx + R(11), cy + R(11));
    SelectObject(m, ob); SelectObject(m, np); DeleteObject(b);

    HPEN p = CreatePen(PS_SOLID, R(1) ? R(1) : 1, glyph);
    HPEN op = (HPEN)SelectObject(m, p);
    HBRUSH nb = (HBRUSH)SelectObject(m, GetStockObject(NULL_BRUSH));
    if (which == 0) { // sidebar: rounded square split in two panes
        RoundRect(m, cx - R(6), cy - R(5), cx + R(6), cy + R(5), R(3), R(3));
        MoveToEx(m, cx - R(2), cy - R(5), NULL); LineTo(m, cx - R(2), cy + R(5));
        MoveToEx(m, cx - R(4), cy - R(2), NULL); LineTo(m, cx - R(4), cy + R(2));
    } else {          // calculator: rounded square, display band + 3x3 pad
        RoundRect(m, cx - R(4), cy - R(6), cx + R(4), cy + R(6), R(2), R(2));
        MoveToEx(m, cx - R(4), cy - R(3), NULL); LineTo(m, cx + R(4), cy - R(3));
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                SetPixel(m, cx + R(-2 + c * 2), cy + R(0 + r * 2), glyph);
    }
    SelectObject(m, nb); SelectObject(m, op); DeleteObject(p);
}

static void draw_all(HDC hdc, RECT rc) {
    (void)rc;
    g_rsc = g_s * SS;
    int dw = S(WIN_W), dh = S(WIN_H);      // window (device px)
    int rw = dw * SS, rh = dh * SS;        // render buffer, exact multiple
    g_rsc = rw / (float)WIN_W;             // absorb rounding in the draw scale

    // 32bpp top-down DIBs so we can read back pixels for the AA downsample
    BITMAPINFO bi = { { 40, 0, 0, 1, 32, 0, 0, 0, 0, 0, 0 } };
    void *rbits, *dbits;
    bi.bmiHeader.biWidth = rw; bi.bmiHeader.biHeight = -rh;
    HDC m = CreateCompatibleDC(hdc);
    HBITMAP dib1 = CreateDIBSection(m, &bi, DIB_RGB_COLORS, &rbits, NULL, 0);
    HBITMAP old = (HBITMAP)SelectObject(m, dib1);
    HDC m2 = CreateCompatibleDC(hdc);
    bi.bmiHeader.biWidth = dw; bi.bmiHeader.biHeight = -dh;
    HBITMAP dib2 = CreateDIBSection(m2, &bi, DIB_RGB_COLORS, &dbits, NULL, 0);
    HBITMAP old2 = (HBITMAP)SelectObject(m2, dib2);
    HBRUSH bg = CreateSolidBrush(C_BG);
    RECT all = { 0, 0, rw, rh };
    FillRect(m, &all, bg); DeleteObject(bg);

    // traffic lights — zoom is disabled on the real app (fixed size): gray dot
    const COLORREF tl[3] = { C_RED, C_YELLOW, RGB(88, 86, 86) };
    for (int i = 0; i < 3; i++) {
        HBRUSH b = CreateSolidBrush(tl[i]);
        HBRUSH ob = (HBRUSH)SelectObject(m, b);
        HPEN p = (HPEN)SelectObject(m, GetStockObject(NULL_PEN));
        Ellipse(m, R(12 + i * 20), R(10), R(12 + i * 20 + 12), R(22));
        SelectObject(m, ob); SelectObject(m, p); DeleteObject(b);
    }
    draw_toolbar_icon(m, 0, R(115), R(17));
    draw_toolbar_icon(m, 1, R(205), R(17));

    // display text (right aligned)
    SetBkMode(m, TRANSPARENT);
    RECT dr = { R(10), R(56), R(220), R(90) };
    SetTextColor(m, C_EXPR);
    SelectObject(m, g_fntExpr);
    WCHAR wbuf[512];
    MultiByteToWideChar(CP_UTF8, 0, g_expr_text, -1, wbuf, 512);
    DrawTextW(m, wbuf, -1, &dr, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE);

    RECT rr = { R(10), R(90), R(220), R(130) };
    SetTextColor(m, C_RESULT);
    SelectObject(m, g_fntRes);
    if (g_error) {
        DrawTextW(m, L"Undefined", -1, &rr, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE);
    } else if (g_res_text[0]) {
        WCHAR wr[64]; MultiByteToWideChar(CP_UTF8, 0, g_res_text, -1, wr, 64);
        DrawTextW(m, wr, -1, &rr, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE);
    }

    // buttons
    SelectObject(m, g_fntBtn);
    SetTextColor(m, C_GLYPH);
    for (int i = 0; i < 20; i++) {
        int row = i / 4, col = i % 4;
        int x = GRID_X0 + col * GRID_P, y = GRID_Y0 + row * GRID_P;
        COLORREF c = g_btns[i].bg;
        if (g_press == i) {
            // macOS flashes the disc *lighter* on press — the "glow"
            int r = GetRValue(c) + (255 - GetRValue(c)) * 45 / 100;
            int g = GetGValue(c) + (255 - GetGValue(c)) * 45 / 100;
            int b = GetBValue(c) + (255 - GetBValue(c)) * 45 / 100;
            c = RGB(r, g, b);
        } else if (g_hover == i) {
            int r = GetRValue(c) + (255 - GetRValue(c)) / 5;
            int g = GetGValue(c) + (255 - GetGValue(c)) / 5;
            int b = GetBValue(c) + (255 - GetBValue(c)) / 5;
            c = RGB(r, g, b);
        }
        HBRUSH b = CreateSolidBrush(c);
        HBRUSH ob = (HBRUSH)SelectObject(m, b);
        HPEN p = (HPEN)SelectObject(m, GetStockObject(NULL_PEN));
        Ellipse(m, R(x), R(y), R(x + BTN_D), R(y + BTN_D));
        SelectObject(m, ob); SelectObject(m, p); DeleteObject(b);
        RECT tr = { R(x), R(y), R(x + BTN_D), R(y + BTN_D) };
        if (g_btns[i].id == F_BS || g_btns[i].id == F_PM)
            draw_icon(m, g_btns[i].id, R(x + BTN_D / 2), R(y + BTN_D / 2));
        else
            DrawTextW(m, g_btns[i].label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // box-filter downsample ×SS — deterministic AA on every platform
    uint32_t *sp = rbits, *dp = dbits;
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            unsigned r = 0, g = 0, b = 0;
            const uint32_t *blk = sp + (size_t)y * SS * rw + x * SS;
            for (int j = 0; j < SS; j++)
                for (int i = 0; i < SS; i++) {
                    uint32_t p = blk[j * rw + i];
                    r += (p >> 16) & 255; g += (p >> 8) & 255; b += p & 255;
                }
            dp[y * dw + x] = 0xFF000000 | ((r / (SS * SS)) << 16) |
                             ((g / (SS * SS)) << 8) | (b / (SS * SS));
        }
    }
    BitBlt(hdc, 0, 0, dw, dh, m2, 0, 0, SRCCOPY);

    SelectObject(m, old); SelectObject(m2, old2);
    DeleteObject(dib1); DeleteObject(dib2);
    DeleteDC(m); DeleteDC(m2);
}

static int btn_at(int mx, int my) {
    for (int i = 0; i < 20; i++) {
        int row = i / 4, col = i % 4;
        int cx = S(GRID_X0 + col * GRID_P + BTN_D / 2), cy = S(GRID_Y0 + row * GRID_P + BTN_D / 2);
        int dx = mx - cx, dy = my - cy;
        if (dx * dx + dy * dy <= S(24) * S(24)) return i;
    }
    return -1;
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_CREATE:
            g_hwnd = h;
            make_fonts();
            clear_all();
            return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            draw_all(hdc, rc); EndPaint(h, &ps); return 0;
        }
        case WM_MOUSEMOVE: {
            int b = btn_at(GET_X_LPARAM(l), GET_Y_LPARAM(l));
            if (b != g_hover) { g_hover = b; InvalidateRect(h, NULL, FALSE); }
            return 0;
        }
        case WM_LBUTTONDOWN:
            g_press = btn_at(GET_X_LPARAM(l), GET_Y_LPARAM(l));
            if (g_press >= 0) { SetCapture(h); InvalidateRect(h, NULL, FALSE); }
            return 0;
        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
            // traffic lights: close / minimize / (zoom ignored)
            for (int i = 0; i < 3; i++) {
                int dx = x - S(18 + i * 20), dy = y - S(16);
                if (dx * dx + dy * dy <= S(8) * S(8)) {
                    if (i == 0) { PostMessage(h, WM_CLOSE, 0, 0); }
                    else if (i == 1) { ShowWindow(h, SW_MINIMIZE); }
                    return 0;
                }
            }
            int b = btn_at(x, y);
            if (g_press >= 0 && b == g_press) on_btn_real(g_btns[b].id);
            g_press = -1; ReleaseCapture(); InvalidateRect(h, NULL, FALSE);
            return 0;
        }
        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
            ScreenToClient(h, &pt);
            if (pt.y < S(30)) return HTCAPTION;      // drag by title bar
            return HTCLIENT;
        }
        case WM_CHAR: {
            WCHAR c = (WCHAR)w;
            if (c >= L'0' && c <= L'9') press_digit(c - L'0');
            else if (c == L'+' ) press_op(T_ADD);
            else if (c == L'-' ) press_op(T_SUB);
            else if (c == L'*' ) press_op(T_MUL);
            else if (c == L'/' ) press_op(T_DIV);
            else if (c == L'%' ) press_pct();
            else if (c == L',' || c == L'.') press_dec();
            else if (c == L'=' || c == L'\r') press_eq();
            else if (c == 0x1B) press_ac();
            else if (c == 8)    press_bs();
            return 0;
        }
        case WM_KEYDOWN:
            if (w == VK_BACK) press_bs();
            else if (w == VK_ESCAPE) press_ac();
            else if (w == VK_RETURN) press_eq();
            else if (w == VK_DELETE) press_ac();
            else if (w == VK_OEM_PLUS) press_op(T_ADD);
            else if (w == VK_OEM_MINUS) press_op(T_SUB);
            else if (w == VK_MULTIPLY) press_op(T_MUL);
            else if (w == VK_DIVIDE) press_op(T_DIV);
            else if (w == VK_DECIMAL) press_dec();
            else if (w == VK_SUBTRACT) press_op(T_SUB);
            else if (w == VK_ADD) press_op(T_ADD);
            return 0;
        case WM_DPICHANGED: {
            g_s = LOWORD(w) / 96.0f;
            RECT *nr = (RECT *)l;
            SetWindowPos(h, NULL, nr->left, nr->top,
                         nr->right - nr->left, nr->bottom - nr->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            set_rgn(h);
            make_fonts();
            InvalidateRect(h, NULL, TRUE);
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE p, LPSTR c, int s) {
    (void)p; (void)c;
    // Per-Monitor V2: receive real pixels instead of OS bitmap-stretching
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_s = GetDpiForSystem() / 96.0f;
    if (g_s <= 0.5f) g_s = 1.0f;
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"MacCalcClone";
    wc.hbrBackground = NULL;
    RegisterClassW(&wc);
    DWORD style = WS_POPUP | WS_VISIBLE;
    HWND h = CreateWindowW(wc.lpszClassName, L"Calculator", style,
                           CW_USEDEFAULT, CW_USEDEFAULT, S(WIN_W), S(WIN_H),
                           NULL, NULL, hi, NULL);
    // rounded corners like macOS
    set_rgn(h);
    ShowWindow(h, s);
    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) { TranslateMessage(&m); DispatchMessage(&m); }
    return 0;
}
#endif // CALC_TEST
