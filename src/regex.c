#include "regex.h"
#include "helper_functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


regex* new_empty_regex() {
    regex* r = malloc(sizeof(regex));
    r->nr_states = 0;
    r->states = NULL;
    return r;
}


regex* new_single_transition_regex(char symbol) {
    regex* r = malloc(sizeof(regex));
    r->nr_states = 2;
    r->states = malloc(2 * sizeof(state*));
    r->states[0] = new_state(1, sb_none, st_start);
    r->states[0]->transitions[0] = new_transition(ts_active, symbol, 1);
    r->states[1] = new_state(0, sb_none, st_end);
    return r;
}


regex* new_single_state_regex() {
    regex* r = malloc(sizeof(regex));
    r->nr_states = 1;
    r->states = malloc(sizeof(state*));
    r->states[0] = new_state(0, sb_none, st_start_end);
    return r;
}


void delete_regex(regex** r) {
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


state*
new_state(int nr_transitions, state_behaviour behaviour, state_type type) {
    state* s = malloc(sizeof(state));
    s->nr_transitions = nr_transitions;
    s->behaviour = behaviour;
    s->type = type;
    s->transitions = malloc(s->nr_transitions * sizeof(transition*));
    return s;
}


void free_state(state* s) {
    for (int i = 0; i < s->nr_transitions; i++) {
        free(s->transitions[i]);
    }
    free(s->transitions);
}


transition*
new_transition(transition_status status, char symbol, int next_state) {
    transition* t = malloc(sizeof(transition));
    t->status = status;
    t->symbol = symbol;
    t->next_state = next_state;
    return t;
}


void regex_chain(regex* a, regex** b) {
    /* shift all next_states of b to create a combined address space */
    for (int i = 0; i < (*b)->nr_states; i++) {
        for (int j = 0; j < (*b)->states[i]->nr_transitions; j++) {
            (*b)->states[i]->transitions[j]->next_state += a->nr_states;
        }
    }

    /* b's start state is now in the middle of the automaton */
    (*b)->states[0]->type =
        ((*b)->states[0]->type == st_start_end) ? st_end : st_middle;

    /* resize a's state array */
    a->states =
        realloc(a->states, (a->nr_states + (*b)->nr_states) * sizeof(state*));

    /* copy b into a */
    for (int i = 0; i < (*b)->nr_states; i++) {
        a->states[i + a->nr_states] = (*b)->states[i];
    }

    /* connect a's end states to b's start state */
    for (int i = 0; i < a->nr_states; i++) {
        state* s = a->states[i];
        if (s->type == st_end || s->type == st_start_end) {
            s->transitions = realloc(s->transitions, ++(s->nr_transitions) *
                                                         sizeof(transition*));
            s->transitions[s->nr_transitions - 1] =
                new_transition(ts_epsilon, 0, a->nr_states);
            s->type = (s->type == st_end) ? st_middle : st_start;
        }
    }

    a->nr_states += (*b)->nr_states;
    /* use free directly to preserve the states now stored in a */
    free(*b);
    *b = NULL;
}


void regex_alternative(regex* a, regex** b) {
    /* expand a to fit in b and the new start node */
    a->states = realloc(a->states, ((++(a->nr_states)) + (*b)->nr_states) *
                                       sizeof(state*));

    /* shift a's states for one position and correct their next_states */
    for (int i = a->nr_states - 1; i > 0; i--) {
        a->states[i] = a->states[i - 1];
        for (int j = 0; j < a->states[i]->nr_transitions; j++) {
            a->states[i]->transitions[j]->next_state++;
        }
    }

    /* correct b's next states and copy them to a */
    for (int i = 0; i < (*b)->nr_states; i++) {
        for (int j = 0; j < (*b)->states[i]->nr_transitions; j++) {
            (*b)->states[i]->transitions[j]->next_state += a->nr_states;
        }
        a->states[a->nr_states + i] = (*b)->states[i];
    }

    /* create the new start state and link it to the old start states */
    a->states[0] = new_state(2, sb_none, st_start);
    a->states[0]->transitions[0] = new_transition(ts_epsilon, 0, 1);
    a->states[0]->transitions[1] = new_transition(ts_epsilon, 0, a->nr_states);

    /* a's and b's start states are no longer start states */
    a->states[1]->type =
        (a->states[1]->type == st_start_end) ? st_end : st_middle;
    a->states[a->nr_states]->type =
        (a->states[a->nr_states]->type == st_start_end) ? st_end : st_middle;

    /* correct a's size */
    a->nr_states += (*b)->nr_states;

    /* free b, but don't delete its states */
    free(*b);
    *b = NULL;
}


void regex_optional(regex* a) { a->states[0]->type = st_start_end; }


void regex_repeat(regex* a) {
    regex_optional(a);

    /* connect all end states to the start state with epsilon transitions */
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


void print_regex(regex* r) {
    printf("\nREGEX - nr_states: %d\n", r->nr_states);
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
                if (s->transitions[j]->symbol == LINE_START) {
                    printf("   - line start -> %d\n",
                           s->transitions[j]->next_state);
                } else if (s->transitions[j]->symbol == LINE_END) {
                    printf("   - line end -> %d\n",
                           s->transitions[j]->next_state);
                } else {
                    printf("   - %c -> %d\n", s->transitions[j]->symbol,
                           s->transitions[j]->next_state);
                }
                break;
            default:
                printf("   - dead\n");
                break;
            }
        }
    }
    printf("\n");
}


regex* copy_regex(regex* r) {
    regex* r2 = malloc(sizeof(regex));

    /* match the size */
    r2->nr_states = r->nr_states;
    r2->states = malloc(r2->nr_states * sizeof(state*));

    /* copy each state */
    for (int i = 0; i < r2->nr_states; i++) {
        r2->states[i] = malloc(sizeof(state));
        r2->states[i]->nr_transitions = r->states[i]->nr_transitions;
        r2->states[i]->behaviour = r->states[i]->behaviour;
        r2->states[i]->type = r->states[i]->type;
        r2->states[i]->transitions =
            malloc(r2->states[i]->nr_transitions * sizeof(transition*));

        /* copy each transition */
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