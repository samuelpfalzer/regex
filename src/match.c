#include "regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//========== MATCHING ==========

// returns either the next state or -1 on error
int next_state(regex* r, int current_state, char symbol) {
    // iterate over all possible transitions
    for (int i = 0; i < r->states[current_state]->nr_transitions; i++) {
        if (r->states[current_state]->transitions[i]->symbol == symbol) {
            return r->states[current_state]->transitions[i]->next_state;
        }
    }
    return -1;
}


int regex_match_first(regex* r, char* input, int* location, int* length) {
    int pos = 0;
    int state = 0;
    int match_start = -1;
    int checkpoint = -1;

    char* input_copy = malloc((strlen(input) + 2) * sizeof(char));
    strcpy(input_copy, input);
    input_copy[strlen(input)] = LINE_END;
    input_copy[strlen(input) + 1] = 0;

    if (r->states[0]->type == st_start_end &&
        r->states[0]->behaviour == sb_lazy) {
        *location = 0;
        *length = 0;
        return 1;
    }

    // consume an artificially produced start of line symbol :)
    state = next_state(r, state, LINE_START);

    DEBUG("matching:\n");
    // try to match until there is no more input
    while (input_copy[pos] != '\0') {
        DEBUG("position %d: %c (state %d)\n", pos, input_copy[pos], state);
        int temp_state = next_state(r, state, input_copy[pos]);
        DEBUG("-> state %d\n", temp_state);

        // no possible transition -> try again
        if (temp_state < 0) {
            if (checkpoint >= 0) {
                *location = match_start;
                *length = checkpoint + 1 - match_start;
                return 1;
            } else {
                match_start = -1;
                state = 0;
                pos++;
                continue;
            }
        }

        // accepting state
        if (r->states[temp_state]->type == st_end ||
            r->states[temp_state]->type == st_start_end) {
            if (match_start < 0) {
                match_start = pos;
            }

            // greedy -> try to continue, even though in an end state
            if (r->states[state]->behaviour == sb_greedy) {
                checkpoint = pos;
                DEBUG("checkpoint at %d\n", checkpoint);
            } else {
                *location = match_start;
                *length = pos + 1 - match_start;
                return 1;
            }
        }

        // made transition to a possible state
        else {
            if (match_start < 0) {
                match_start = pos;
            }
            state = temp_state;
        }

        pos++;
    }
    if (checkpoint >= 0) {
        *location = match_start;
        *length = checkpoint + 1 - match_start;
        return 1;
    }

    free(input_copy);

    return 0;
}


int regex_match(regex* r, char* input, int** locations, int** lengths) {
    int pos = 0;
    int matches = 0;

    // reset locations and lengths in order for realloc() to work
    locations = NULL;
    lengths = NULL;

    while (input) {
    }

    return matches;
}
