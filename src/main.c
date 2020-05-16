#include <stdio.h>
#include <stdlib.h>
#include "regex.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage: ./main regex-string\n");
        return 1;
    }

    regex* r = NULL;
    compile(&r, argv[1]);

    print_regex(r);

    return 0;
}