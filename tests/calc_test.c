// Test harness: replay the button scenarios observed on real Calculator.app
// and compare rendered expression/result text. Compile on macOS:
//   clang -DCALC_TEST -o calc_test calc_test.c && ./calc_test
#include <stdio.h>
#include <string.h>
#include "calculator.c"

static int fails, total;

static void press(int id) {
    if (id >= F_7 && id <= F_9) press_digit(id - F_7 + 7);
    else if (id >= F_4 && id <= F_6) press_digit(id - F_4 + 4);
    else if (id >= F_1 && id <= F_3) press_digit(id - F_1 + 1);
    else if (id == F_0) press_digit(0);
    else on_btn(id);
}

static void run(const char *name, const int *seq, int n,
                const char *want_expr, const char *want_res, int want_err) {
    clear_all();
    for (int i = 0; i < n; i++) press(seq[i]);
    total++;
    int ok = !strcmp(g_expr_text, want_expr) &&
             !strcmp(g_res_text, want_res) && g_error == want_err;
    if (!ok) {
        fails++;
        printf("FAIL %-22s expr=%-28s res=%-18s err=%d  (want expr=%s res=%s err=%d)\n",
               name, g_expr_text, g_res_text, g_error, want_expr, want_res, want_err);
    } else {
        printf("ok   %-22s expr=%s res=%s err=%d\n", name, g_expr_text, g_res_text, g_error);
    }
}

int main(void) {
    // sequences end without implicit '=' unless listed — mirror real probes
    run("2+3=", (int[]){F_2,F_ADD,F_3,F_EQ}, 4, "2 + 3", "5", 0);
    run("200+10%=", (int[]){F_2,F_0,F_0,F_ADD,F_1,F_0,F_PCT,F_EQ}, 8, "200 + 10 %", "220", 0);
    run("200x10%=", (int[]){F_2,F_0,F_0,F_MUL,F_1,F_0,F_PCT,F_EQ}, 8, "200 \xC3\x97 10 %", "20", 0);
    run("5%=", (int[]){F_5,F_PCT,F_EQ}, 3, "5 %", "0,05", 0);
    run("5/0=", (int[]){F_5,F_DIV,F_0,F_EQ}, 4, "5 \xC3\xB7 0", "", 1);
    run("1/3=", (int[]){F_1,F_DIV,F_3,F_EQ}, 4, "1 \xC3\xB7 3", "0,33333333\xE2\x80\xA6", 0);
    run("big=", (int[]){F_9,F_9,F_9,F_9,F_9,F_9,F_9,F_9,F_MUL,F_9,F_9,F_9,F_9,F_9,F_9,F_9,F_9,F_EQ}, 18,
        "99\xE2\x80\xAF" "999\xE2\x80\xAF" "999 \xC3\x97 99\xE2\x80\xAF" "999\xE2\x80\xAF" "999",
        "9\xE2\x80\xAF" "999\xE2\x80\xAF" "999\xE2\x80\xAF" "800\xE2\x80\xAF" "000\xE2\x80\xAF" "001", 0);
    run("prec=", (int[]){F_2,F_ADD,F_3,F_MUL,F_4,F_EQ}, 6, "2 + 3 \xC3\x97 4", "14", 0);
    run("005", (int[]){F_0,F_0,F_5}, 3, "5", "", 0);
    run("5,5,5", (int[]){F_5,F_DEC,F_5,F_DEC,F_5}, 5, "5,55", "", 0);
    run("1234567", (int[]){F_1,F_2,F_3,F_4,F_5,F_6,F_7}, 7, "1\xE2\x80\xAF" "234\xE2\x80\xAF" "567", "", 0);
    run("plus_alone", (int[]){F_ADD}, 1, "0 +", "", 0);
    run("5-8=", (int[]){F_5,F_SUB,F_8,F_EQ}, 4, "5 \xE2\x88\x92 8", "\xE2\x88\x92" "3", 0);
    // multi-step scenarios below
    total++; clear_all();
    press(F_1); press(F_2); press(F_ADD); press(F_3); press(F_BS);
    { int ok = !strcmp(g_expr_text,"12 +") && !g_res_text[0];
      printf("%s %-22s expr=%s res=%s\n", ok?"ok  ":"FAIL", "12+3BS", g_expr_text, g_res_text); if(!ok)fails++; }
    total++; clear_all();
    press(F_0); press(F_BS);
    { int ok = !strcmp(g_expr_text,"0");
      printf("%s %-22s expr=%s\n", ok?"ok  ":"FAIL", "0BS", g_expr_text); if(!ok)fails++; }
    total++; clear_all();
    press(F_5); press(F_ADD); press(F_9); press(F_AC);
    { int ok = !strcmp(g_expr_text,"5 +");
      printf("%s %-22s expr=%s\n", ok?"ok  ":"FAIL", "5+9 AC1", g_expr_text); if(!ok)fails++; }
    press(F_AC);
    { int ok = !strcmp(g_expr_text,"0");
      printf("%s %-22s expr=%s\n", ok?"ok  ":"FAIL", "5+9 AC2", g_expr_text); if(!ok)fails++; total++; }
    total++; clear_all();
    press(F_1); press(F_ADD); press(F_2); press(F_EQ);
    { int ok = !strcmp(g_expr_text,"1 + 2") && !strcmp(g_res_text,"3");
      printf("%s %-22s expr=%s res=%s\n", ok?"ok  ":"FAIL", "1+2=1", g_expr_text, g_res_text); if(!ok)fails++; }
    press(F_EQ);
    { int ok = !strcmp(g_expr_text,"3 + 2") && !strcmp(g_res_text,"5");
      printf("%s %-22s expr=%s res=%s\n", ok?"ok  ":"FAIL", "1+2==", g_expr_text, g_res_text); if(!ok)fails++; total++; }
    total++; clear_all();
    press(F_1); press(F_DIV); press(F_7); press(F_EQ); press(F_EQ);
    { int ok = !strcmp(g_res_text,"0,02040816\xE2\x80\xA6");
      printf("%s %-22s res=%s\n", ok?"ok  ":"FAIL", "1/7==", g_res_text); if(!ok)fails++; }
    total++; clear_all();
    press(F_5); press(F_EQ); press(F_PM);
    { int ok = !strcmp(g_res_text,"\xE2\x88\x92" "5");
      printf("%s %-22s expr=%s res=%s\n", ok?"ok  ":"FAIL", "5=pm", g_expr_text, g_res_text); if(!ok)fails++; }
    total++; clear_all();
    press(F_2); press(F_ADD); press(F_3); press(F_EQ); press(F_5);
    { int ok = !strcmp(g_expr_text,"5") && !g_res_text[0];
      printf("%s %-22s expr=%s\n", ok?"ok  ":"FAIL", "2+3= then5", g_expr_text); if(!ok)fails++; }
    printf("\n%d/%d passed\n", total - fails, total);
    return fails ? 1 : 0;
}
