#include "regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helper_functions.h"


//========== FUNCTIONS ==========


regex* new_regex() {
    regex* r = malloc(sizeof(regex));
    r->line_start = 0;
    r->line_end = 0;
    r->size = 0;
    r->states = NULL;
    return r;
}



regex* new_single_regex(char symbol) {
    regex* r = malloc(sizeof(regex));
    r->line_start = 0;
    r->line_end = 0;
    r->size = 2;
    r->states = malloc(2 * sizeof(state*));
    r->states[0] = new_state(1, none, start);
    r->states[0]->transitions[0] = new_transition(0, symbol, 1);
    r->states[1] = new_state(0, none, end);
    return r;
}

void free_regex(regex** r) {
    if ((*r) == NULL) {
        return;
    }

    for (int i = 0; i < (*r)->size; i++) {
        free_state((*r)->states[i]);
        free((*r)->states[i]);
    }
    free((*r)->states);
    
    free(*r);
    *r = NULL;
}


state* new_state(int size, int behavior, int type) {
    state* s = malloc(sizeof(state));
    s->size = size;
    s->behavior = behavior;
    s->type = type;
    s->transitions = malloc(s->size * sizeof(transition*));
    return s;
}


void free_state(state* s) {
    for (int i = 0; i < s->size; i++) {
        free(s->transitions[i]);
    }
    free(s->transitions);
}


transition* new_transition(int epsilon, char symbol, int next_state) {
    transition* t = malloc(sizeof(transition));
    t->epsilon = epsilon;
    t->symbol = symbol;
    t->next_state = next_state;
    return t;
}



void regex_concat(regex* a, regex* b) {
    // shift all targets of b to avoid collisions
    for (int i = 0; i < b->size; i++) {
        for (int j = 0; j < b->states[i]->size; j++) {
            b->states[i]->transitions[j]->next_state += a->size;
        }
    }

    // b's first state is no longer a start state
    b->states[0]->type = middle;

    // resize a's state array to fit in b completely
    a->states = realloc(a->states, (a->size + b->size) * sizeof(state*));


    // copy all states from b to a
    for (int i = 0; i < b->size; i++) {
        a->states[i + a->size] = b->states[i];
    }

    // connect all end states of a to b's start state and mark them
    // as normal states
    for (int i = 0; i < a->size; i++) {
        state* s = a->states[i];
        if (s->type == end || s->type == start_end) {
            s->size++;
            s->transitions = realloc(s->transitions, s->size * sizeof(transition*));
            // connect to a->size because the start state always is the first
            s->transitions[s->size-1] = new_transition(1, ' ', a->size);
            s->type = middle;
        }
    }

    a->size += b->size;
    b->size = 0;
    free_regex(&b);
}


void regex_alternative(regex* a, regex* b) {
    // expand a to fit in b and the new start node
    a->states = realloc(a->states, ((++(a->size))+b->size) * sizeof(state*));

    // shift all of a's states one to the right and correct their next pointers
    for (int i = a->size - 1; i > 0; i--) {
        a->states[i] = a->states[i-1];
        for (int j = 0; j < a->states[i]->size; j++) {
            a->states[i]->transitions[j]->next_state++;
        }
    }

    // create the new start state, link it to the old start state and mark that as deprecated
    a->states[0] = new_state(2, none, start);
    a->states[0]->transitions[0] = new_transition(1, ' ', 1);
    a->states[1]->type = middle;

    // correct b's next pointers and copy them to a
    for (int i = 0; i < b->size; i++) {
        for (int j = 0; j < b->states[i]->size; j++) {
            b->states[i]->transitions[j]->next_state += a->size;
        }
        a->states[a->size+i] = b->states[i];
    }

    // link the new start state to b's old start state
    a->states[0]->transitions[1] = new_transition(1, ' ', a->size);
    a->states[a->size]->type = middle;

    // correct a's size and free b
    a->size += b->size;
    b->size = 0;
    free_regex(&b);
}


void regex_repeat(regex* a) {
    // create one additional end state and connect the start state to it
    regex_optional(a);

    // connect all end states to the start state
    for (int i = 0; i < a->size; i++) {
        if (a->states[i]->type == end || a->states[i]->type == start_end) {
            a->states[i]->transitions = realloc(a->states[i]->transitions, ++(a->states[i]->size) * sizeof(transition*));
            a->states[i]->transitions[a->states[i]->size-1] = new_transition(1, ' ', 0);
        }
    }

}


void regex_optional(regex* a) {
    // create one additional end state and connect the start state to it
    a->states = realloc(a->states, ++(a->size) * sizeof(state*));
    a->states[a->size-1] = new_state(0, none, end);
    a->states[0]->transitions = realloc(a->states[0]->transitions, ++(a->states[0]->size) * sizeof(transition*));
    a->states[0]->transitions[a->states[0]->size-1] = new_transition(1, ' ', a->size-1);
    // make the start state an end state as well
    a->states[0]->type = start_end;
}


void print_regex(regex* r) {
    printf("regex; size: %i\n", r->size);
    for (int i = 0; i < r->size; i++) {
        state* s = r->states[i];
        printf(
            " - %i: %s; %s\n",
            i,
            (s->type == start) ? "start" : ((s->type == end || s->type == start_end) ? (s->type == end ? "end" : "start_end") : "middle"),
            (s->behavior == lazy) ? "lazy" : ((s->behavior == greedy) ? "greedy" : "none"));
        for (int j = 0; j < s->size; j++) {
            printf(
                "   - %c -> %i\n",
                (s->transitions[j]->epsilon == 1) ? 'E' : (s->transitions[j]->symbol),
                s->transitions[j]->next_state);
        }

    }
}


regex* copy_regex(regex* r) {
    regex* r2 = malloc(sizeof(regex));

    // match the size of r2 to that of r
    r2->size = r->size;
    r2->states = malloc(r2->size * sizeof(state*));

    // copy each state
    for (int i = 0; i < r2->size; i++) {
        r2->states[i] = malloc(sizeof(state));
        r2->states[i]->size = r->states[i]->size;
        r2->states[i]->behavior = r->states[i]->behavior;
        r2->states[i]->type = r->states[i]->type;
        r2->states[i]->transitions = malloc(r2->states[i]->size * sizeof(transition*));

        // copy each transition
        for (int j = 0; j < r2->states[i]->size; j++) {
            r2->states[i]->transitions[j] = malloc(sizeof(transition));
            r2->states[i]->transitions[j]->epsilon = r->states[i]->transitions[j]->epsilon;
            r2->states[i]->transitions[j]->next_state = r->states[i]->transitions[j]->next_state;
            r2->states[i]->transitions[j]->symbol = r->states[i]->transitions[j]->symbol;
        }
    }

    return r2;
}