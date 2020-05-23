#include "regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* returns the next state or -1 on error */
static int next_state(regex* r, int current_state, char symbol) {
    /* iterate over all possible transitions */
    for (int i = 0; i < r->states[current_state]->nr_transitions; i++) {
        if (r->states[current_state]->transitions[i]->symbol == symbol) {
            return r->states[current_state]->transitions[i]->next_state;
        }
    }
    return -1;
}


int regex_match_first(regex* r, char* input, int* location, int* length) {
    int pos = 0;
    int restart_pos = 0;
    int current_state = 0;
    int match_start = -1; /* -1: no partial match, >-1: start position */
    int checkpoint = -1;  /* -1: no checkpoint, >-1: end position */
    int success = 0;

    /* copy the input to append the LINE_END symbol */
    char* input_copy = malloc((strlen(input) + 2) * sizeof(char));
    strcpy(input_copy, input);
    input_copy[strlen(input)] = LINE_END;
    input_copy[strlen(input) + 1] = 0;

    /* consume an artificially produced LINE_START symbol */
    current_state = next_state(r, current_state, LINE_START);

    /* try to match until there is no more input */
    while ((input_copy[pos] != '\0') && (!success)) {
        int temp_state = next_state(r, current_state, input_copy[pos]);

        /* no valid transition */
        if (temp_state < 0) {
            /* existing checkpoint: set return values and exit */
            if (checkpoint >= 0) {
                *location = match_start;
                *length = checkpoint + 1 - match_start;
                success = 1;
                break;
            }
            /* reset the matching progress and retry */
            else {
                match_start = -1;
                checkpoint = -1;
                current_state = 0;
                pos = ++restart_pos;
                continue;
            }
        }

        /* end state */
        if (r->states[temp_state]->type == st_end ||
            r->states[temp_state]->type == st_start_end) {
            if (match_start < 0) {
                match_start = pos;
            }

            /* line end must not be included in result length */
            if (input_copy[pos] == LINE_END) {
                if (match_start == pos) {
                    *location = 0;
                    *length = 0;
                } else {
                    *location = match_start;
                    *length = pos - match_start;
                }
                success = 1;
                break;
            }

            /* greedy: try to continue, even though in an end state */
            else if (r->states[current_state]->behaviour == sb_greedy) {
                current_state = temp_state;
                checkpoint = pos++;
            }

            /* not greedy: set return values end exit */
            else {
                *location = match_start;
                *length = pos + 1 - match_start;
                success = 1;
                break;
            }
        }

        /* valid transition, but no end state */
        else {
            if (match_start < 0) {
                match_start = pos;
            }
            current_state = temp_state;
            pos++;
        }
    }

    if (checkpoint > 0 && !success) {
        *location = match_start;
        *length = checkpoint + 1 - match_start;
        success = 1;
    }

    /* cleaning up */
    free(input_copy);

    return success;
}
