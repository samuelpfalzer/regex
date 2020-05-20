#include "helper_functions.h"
#include <stdlib.h>


int in(const char a, const char *b, int length) {
    if (b == NULL || length < 1) {
        return 0;
    }
    for (int i = 0; i < length; i++) {
        if (b[i] == a) {
            return 1;
        }
    }
    return 0;
}


int sort_int_array(int *array, int size) {
    int changes = 1;
    while (changes > 0) {
        changes = 0;
        for (int i = 0; i < size - 1; i++) {
            if (array[i] > array[i + 1]) {
                int temp = array[i];
                array[i] = array[i + 1];
                array[i + 1] = temp;
                changes++;
            }
        }
    }
    return 1;
}