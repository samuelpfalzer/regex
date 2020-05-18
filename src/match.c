#include "regex.h"
#include <stdlib.h>
#include <stdio.h>


//========== MATCHING ==========

// returns either the next state or -1 on error
int next_state(regex* r, int current_state, char symbol) {  
    // iterate over all possible transitions
    for (int i = 0; i < r->states[current_state]->size; i++) {
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

    // TODO: check if the input must start at the beginning of the line
    
    // try to match until there is no more input
    while (input[pos] != '\0') {
        int temp_state = next_state(r, state, input[pos]);

        // no possible transition -> try again
        if (temp_state < 0) {
            match_start = -1;
            state = 0;
        }
        
        // accepting state
        else if (r->states[temp_state]->type == end || r->states[temp_state]->type == start_end) {
            if (match_start < 0) {
                match_start = pos;
            }
            *location = match_start;
            *length = pos + 1 - match_start;
            return 1;
        }

        // made transition to a normal state
        else {
            if (match_start < 0) {
                match_start = pos;
            }
            state = temp_state;
        }

        pos++;
    }
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
