#include "regex.h"
#include "stack.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    if (argc == 3) {
        regex* r = NULL;
        if (!regex_compile(&r, argv[1])) {
            printf("compiler error\n");
            return 1;
        };
        print_regex(r);

        int location, length;
        int success = regex_match_first(r, argv[2], &location, &length);


        if (success) {
            char* match = malloc((length + 1) * sizeof(char));
            match[length] = '\0';
            for (int i = 0; i < length; i++) {
                match[i] = argv[2][i + location];
            }
            printf("\n\n==========\n\nmatch at position %d: %s\n", location,
                   match);
        } else {
            printf("\n\nno match\n");
        }

        // getchar();
        return 0;
    } else {
        for (int i = 0; i < 10000; i++) {
            regex* r = NULL;
            if (!regex_compile(&r, "abc")) {
                printf("compiler error\n");
                return 1;
            };
            free_regex(&r);
        }

        getchar();
        return 0;
    }
    return 1;
}