#include <stdio.h>

static long percent_value(long previous, long current, const char *op) {
    if (op[0] == '*' || op[0] == '/') {
        return current / 100;
    }
    return current * previous / 100;
}

static long apply_op(long a, const char *op, long b) {
    switch (op[0]) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': return b ? a / b : 0;
    }
    return b;
}

int main(int argc, char **argv) {
    long previous = argc > 1 ? 200 : 200;
    long current = 10;
    const char *ops[] = {"+", "*"};
    for (int i = 0; i < 2; i++) {
        long pct = percent_value(previous, current, ops[i]);
        long result = apply_op(previous, ops[i], pct);
        printf("200 %s 10%% -> pct=%ld result=%ld\n", ops[i], pct, result);
    }
    return 0;
}
