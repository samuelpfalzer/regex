#include "../../src/regex.h"
#include <stdio.h>
#include <time.h>

int main() {
    int success;
    regex* r = NULL;
    clock_t start, end;
    int nr_compile_cases = 26;
    char* compile_input[] = {
        "test",        "a*b",          "a*?b",
        "a+b",         "a+?b",         "a?b",
        "a??b",        "a|b",          "(ab)|c",
        "a{2,5}b",     "a{0,5}b",      "a{,5}b",
        "a{5,}b",      "a{5}b",        "[abc]d",
        "[a-f]b",      "[a-zA-Z0-9]b", "[^a-z]b",
        ".*b",         "\\.*b",        "((a)*|([a-x]+?09\\\\)de)?",
        "^ab",         "ab$",          "^ab$",
        "(ab$)|(^ab)", "^\\^\\$$"};

    printf("\n");

    for (int i = 0; i < nr_compile_cases; i++) {
        start = clock();
        success = regex_compile(&r, compile_input[i]);
        end = clock();
        printf("[COMPILE] %s  input \"%s\"  in %f s\n",
               success ? "\033[1;32m[OK]\033[0m" : "\033[1;31m[FAILED]\033[0m",
               compile_input[i], (double)(end - start) / CLOCKS_PER_SEC);
        delete_regex(&r);
    }

    printf("\n");

    return 0;
}