#include "src/regex.h"
#include "src/stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char* argv[]) {
    if (argc == 3) {
        clock_t start = clock();

        regex* r = NULL;
        if (!regex_compile(&r, argv[1])) {
            return 1;
        };

        clock_t end = clock();

        print_regex(r);

        int location, length;
        if (regex_match_first(r, argv[2], &location, &length)) {
            char* match = malloc((length + 1) * sizeof(char));
            match[length] = '\0';
            for (int i = 0; i < length; i++) {
                match[i] = argv[2][i + location];
            }
            printf("match of length %d at position %d: %s\n", length, location,
                   match);
            free(match);
        } else {
            printf("no match\n");
        }

        printf("compiled in %f s\n", (double)(end - start) / CLOCKS_PER_SEC);

        delete_regex(&r);
        return 0;
    }
    printf("usage: bin/example 'regular expression' 'string to match "
           "against'\n");
    return 1;
}