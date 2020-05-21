#include "regex.h"
#include "helper_functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//========== REGEX FUNCTIONS ==========


regex* new_regex() {
    regex* r = malloc(sizeof(regex));
    r->line_start = 0;
    r->line_end = 0;
    r->nr_states = 0;
    r->states = NULL;
    return r;
}


regex* new_single_regex(char symbol) {
    regex* r = malloc(sizeof(regex));
    r->line_start = 0;
    r->line_end = 0;
    r->nr_states = 2;
    r->states = malloc(2 * sizeof(state*));
    r->states[0] = new_state(1, sb_none, st_start);
    r->states[0]->transitions[0] = new_transition(ts_active, symbol, 1);
    r->states[1] = new_state(0, sb_none, st_end);
    return r;
}


regex* new_empty_regex() {
    regex* r = malloc(sizeof(regex));
    r->line_start = 0;
    r->line_end = 0;
    r->nr_states = 1;
    r->states = NULL;
    r->states = malloc(sizeof(state*));
    r->states[0] = new_state(0, sb_none, st_start_end);
    return r;
}


// TODO: change signature to void free_regex(regex*)
void free_regex(regex** r) {
    if ((*r) == NULL) {
        return;
    }

    for (int i = 0; i < (*r)->nr_states; i++) {
        free_state((*r)->states[i]);
        free((*r)->states[i]);
    }
    free((*r)->states);

    free(*r);
    *r = NULL;
}


//========== STATE FUNCTIONS ==========


state*
new_state(int nr_transitions, state_behaviour behaviour, state_type type) {
    state* s = malloc(sizeof(state));
    s->nr_transitions = nr_transitions;
    s->behaviour = behaviour;
    s->type = type;
    s->transitions = NULL;
    s->transitions = malloc(s->nr_transitions * sizeof(transition*));
    return s;
}


void free_state(state* s) {
    for (int i = 0; i < s->nr_transitions; i++) {
        free(s->transitions[i]);
    }
    free(s->transitions);
}


//========== TRANSITION FUNCTIONS ==========


transition*
new_transition(transition_status status, char symbol, int next_state) {
    transition* t = malloc(sizeof(transition));
    t->status = status;
    t->symbol = symbol;
    t->next_state = next_state;
    return t;
}


//========== REGEX PROCESSING FUNCTIONS ==========


void regex_concat(regex* a, regex* b) {
    // shift all targets of b to create a combined address space
    for (int i = 0; i < b->nr_states; i++) {
        for (int j = 0; j < b->states[i]->nr_transitions; j++) {
            b->states[i]->transitions[j]->next_state += a->nr_states;
        }
    }

    // b's first state is no longer a start state
    b->states[0]->type = (b->states[0]->type == st_start) ? st_middle : st_end;

    // resize a's state array to fit in b completely
    a->states =
        realloc(a->states, (a->nr_states + b->nr_states) * sizeof(state*));

    // copy all states from b to a
    for (int i = 0; i < b->nr_states; i++) {
        a->states[i + a->nr_states] = b->states[i];
    }

    // connect all end states of a to b's start state and mark them
    // as normal states
    for (int i = 0; i < a->nr_states; i++) {
        state* s = a->states[i];
        if (s->type == st_end || s->type == st_start_end) {
            s->transitions = realloc(s->transitions, ++(s->nr_transitions) *
                                                         sizeof(transition*));
            // connect to a->size because the start state always is
            // the first
            s->transitions[s->nr_transitions - 1] =
                new_transition(ts_epsilon, ' ', a->nr_states);
            s->type = (s->type == st_end) ? st_middle : st_start;
        }
    }

    a->nr_states += b->nr_states;
    b->nr_states = 0;
    free_regex(&b);
}


void regex_alternative(regex* a, regex* b) {
    print_regex(a);
    print_regex(b);

    // expand a to fit in b and the new start node
    a->states = realloc(a->states,
                        ((++(a->nr_states)) + b->nr_states) * sizeof(state*));

    // shift all of a's states one to the right and correct their next
    // pointers
    for (int i = a->nr_states - 1; i > 0; i--) {
        a->states[i] = a->states[i - 1];
        for (int j = 0; j < a->states[i]->nr_transitions; j++) {
            a->states[i]->transitions[j]->next_state++;
        }
    }

    // correct b's next pointers and copy them to a
    for (int i = 0; i < b->nr_states; i++) {
        for (int j = 0; j < b->states[i]->nr_transitions; j++) {
            b->states[i]->transitions[j]->next_state += a->nr_states;
        }
        a->states[a->nr_states + i] = b->states[i];
    }

    // check if one of the old start states was also an end state
    // to preserve this information
    state_type start_state_type = st_start;
    if (a->states[1]->type == st_start_end ||
        a->states[a->nr_states]->type == st_start_end) {
        start_state_type = st_start_end;
    }

    // create the new start state, link it to the old start state and mark
    // that as deprecated
    a->states[0] = new_state(2, sb_none, start_state_type);
    a->states[0]->transitions[0] = new_transition(ts_epsilon, 0, 1);
    a->states[1]->type = st_middle;

    // link the new start state to b's old start state
    a->states[0]->transitions[1] = new_transition(ts_epsilon, 0, a->nr_states);
    a->states[a->nr_states]->type = st_middle;

    // correct a's size and free b
    a->nr_states += b->nr_states;
    b->nr_states = 0;
    free_regex(&b);

    print_regex(a);
}


void regex_repeat(regex* a) {
    // make the start state a start_end state
    regex_optional(a);

    // connect all end states to the start state
    for (int i = 0; i < a->nr_states; i++) {
        if (a->states[i]->type == st_end) {
            a->states[i]->transitions =
                realloc(a->states[i]->transitions,
                        ++(a->states[i]->nr_transitions) * sizeof(transition*));
            a->states[i]->transitions[a->states[i]->nr_transitions - 1] =
                new_transition(ts_epsilon, 0, 0);
        }
    }
}


void regex_optional(regex* a) {
    // make the start state an end state as well
    a->states[0]->type = st_start_end;
}


void print_regex(regex* r) {
    printf("\n\nREGEX - nr_states: %d\n", r->nr_states);
    if (r->line_start) {
        printf("line_start=true\n");
    }
    if (r->line_end) {
        printf("line_end=true\n");
    }
    for (int i = 0; i < r->nr_states; i++) {
        state* s = r->states[i];
        printf(" - %d: type ", i);

        switch (s->type) {
        case st_start:
            printf(" start");
            break;
        case st_start_end:
            printf(" start+end");
            break;
        case st_end:
            printf(" end");
            break;
        default:
            printf(" middle");
            break;
        }

        switch (s->behaviour) {
        case sb_greedy:
            printf(", greedy\n");
            break;
        case sb_lazy:
            printf(", lazy\n");
            break;
        default:
            printf("\n");
            break;
        }

        for (int j = 0; j < s->nr_transitions; j++) {
            switch (s->transitions[j]->status) {
            case ts_epsilon:
                printf("   - epsilon -> %d\n", s->transitions[j]->next_state);
                break;
            case ts_active:
                printf("   - %c -> %d\n", s->transitions[j]->symbol,
                       s->transitions[j]->next_state);
                break;
            default:
                printf("   - dead\n");
                break;
            }
        }
    }
}


regex* copy_regex(regex* r) {
    regex* r2 = malloc(sizeof(regex));

    // match the size of r2 to that of r
    r2->nr_states = r->nr_states;
    r2->states = malloc(r2->nr_states * sizeof(state*));

    // copy each state
    for (int i = 0; i < r2->nr_states; i++) {
        r2->states[i] = malloc(sizeof(state));
        r2->states[i]->nr_transitions = r->states[i]->nr_transitions;
        r2->states[i]->behaviour = r->states[i]->behaviour;
        r2->states[i]->type = r->states[i]->type;
        r2->states[i]->transitions =
            malloc(r2->states[i]->nr_transitions * sizeof(transition*));

        // copy each transition
        for (int j = 0; j < r2->states[i]->nr_transitions; j++) {
            r2->states[i]->transitions[j] = malloc(sizeof(transition));
            r2->states[i]->transitions[j]->status =
                r->states[i]->transitions[j]->status;
            r2->states[i]->transitions[j]->next_state =
                r->states[i]->transitions[j]->next_state;
            r2->states[i]->transitions[j]->symbol =
                r->states[i]->transitions[j]->symbol;
        }
    }

    return r2;
}


void regex_make_lazy(regex* a) {
    for (int i = 0; i < a->nr_states; i++) {
        if (a->states[i]->type == st_end ||
            a->states[i]->type == st_start_end) {
            a->states[i]->behaviour = sb_lazy;
        }
    }
}


void regex_make_greedy(regex* a) {
    for (int i = 0; i < a->nr_states; i++) {
        if (a->states[i]->type == st_end ||
            a->states[i]->type == st_start_end) {
            a->states[i]->behaviour = sb_greedy;
        }
    }
}