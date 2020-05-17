#include <stdio.h>
#include <stdlib.h>
#include "regex.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("usage: ./main 'regex-string' 'input-string'\n");
        return 1;
    }

    regex* r = NULL;
    if(!regex_compile(&r, argv[1])) {
        printf("compiler error\n");
        return 1;
    };
    print_regex(r);

    int location, length;
    int success = regex_match_first(r, argv[2], &location, &length);
    
    

    if (success) {
        char* match = malloc((length+1) * sizeof(char));
        match[length] = '\0';
        for (int i = 0; i < length; i++) {
            match[i] = argv[2][i+location];
        }
        printf("\n\n==========\n\nmatch at position %d: %s\n", location, match);
    } else {
        printf("no match\n");
    }

    return 0;
}