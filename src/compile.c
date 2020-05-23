#include "helper_functions.h"
#include "regex.h"
#include "stack.h"
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* CONSTANTS */

const char* REGULAR_SYMBOLS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUV"
                              "WXYZ0123456789\"\'#/&=@!%_: \t<>";
const char* ESCAPED_SYMBOLS = "-^$()[]{}\\*+?.|";
const char* ALL_SYMBOLS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUV<>"
                          "WXYZ0123456789\"\'#/&=@!%_: \t-^$()[]{}\\*+?.|";


/* PRIVATE FUNCTIONS */


static int string_to_regex(regex** r, char* input);
static int remove_epsilon_transitions(regex* r);
static int nfa_to_dfa(regex* r);

/* a = a - b */
static void string_subtract(vector* a, vector* b);


/* main function called from outside */
int regex_compile(regex** r, char* input) {
    int success;
    delete_regex(r);

    success = string_to_regex(r, input);
    /*
        if (success) {
            success = remove_epsilon_transitions(*r);
        }

        if (success) {
            success = nfa_to_dfa(*r);
        }
    */
    if (!success) {
        delete_regex(r);
    }

    return success;
}


static int string_to_regex(regex** r, char* input) {
    int level = 0;
    int success = 1;

    regex* current_regex = NULL;

    int pos = 0;

    vector* alternative_on_level = new_vector(sizeof(int), NULL);
    vector_push(alternative_on_level, &level);

    /* initialize the regex objects vector with an optional start of line */
    vector* regex_objects = new_vector(sizeof(vector*), NULL);
    {
        vector* temp_vector = new_vector(sizeof(regex*), NULL);
        regex* temp_regex = new_single_transition_regex(LINE_START);
        regex_optional(temp_regex);
        vector_push(temp_vector, &temp_regex);
        vector_push(regex_objects, &temp_vector);
    }

    while (success && input[pos] != 0) {
        current_regex = NULL;
        if (contains(input[pos], REGULAR_SYMBOLS, strlen(REGULAR_SYMBOLS)) ||
            contains(input[pos], "[.)\\", 4)) {

            switch (input[pos]) {
            /* escaped character */
            case '\\':
                if (!contains(input[pos + 1], ESCAPED_SYMBOLS,
                              strlen(ESCAPED_SYMBOLS))) {
                    success = 0;
                } else {
                    current_regex = new_single_transition_regex(input[++pos]);
                }
                break;

            /* closed block */
            case ')': {
                int open_alternative;
                vector_pop(alternative_on_level, &open_alternative);
                if (open_alternative) {
                    success = 0;
                    break;
                }

                vector* block_regex_objects = NULL;
                vector_pop(regex_objects, &block_regex_objects);
                if (!block_regex_objects->size) {
                    success = 0;
                    vector_push(regex_objects, &block_regex_objects);
                    break;
                }

                /* chain all elements inside the block together */
                vector_reset_iterator(block_regex_objects);
                if (vector_next(block_regex_objects, &current_regex)) {
                    regex* buffer_regex = NULL;
                    while (vector_next(block_regex_objects, &buffer_regex)) {
                        regex_chain(current_regex, &buffer_regex);
                    }
                }
                delete_vector(&block_regex_objects);

                level--;
                break;
            }

            /* any character */
            case '.':
                current_regex = new_single_transition_regex(ALL_SYMBOLS[0]);
                current_regex->states[0]->transitions =
                    realloc(current_regex->states[0]->transitions,
                            strlen(ALL_SYMBOLS) * sizeof(transition*));
                for (int i = 1; i < strlen(ALL_SYMBOLS); i++) {
                    current_regex->states[0]->transitions[i] =
                        new_transition(ts_active, ALL_SYMBOLS[i], 1);
                }
                current_regex->states[0]->nr_transitions = strlen(ALL_SYMBOLS);
                break;

            /* character class */
            case '[': {
                int inverted = 0;

                if (input[pos + 1] == '^') {
                    inverted = 1;
                    pos++;
                }

                if (input[++pos] == ']') {
                    success = 0;
                    break;
                }

                vector* v_symbols = new_vector(sizeof(char), NULL);

                while (input[pos] != ']' && input[pos] != 0) {
                    if (contains(input[pos], "\n\0", 2)) {
                        success = 0;
                        break;
                    }

                    /* escaped symbol */
                    if (input[pos] == '\\' &&
                        contains(input[pos + 1], ESCAPED_SYMBOLS,
                                 strlen(ESCAPED_SYMBOLS))) {
                        pos++;
                        vector_push(v_symbols, &input[pos++]);
                    }

                    /* range */
                    else if (input[pos + 1] == '-') {
                        if (contains(input[pos + 2], "\n\0]", 3)) {
                            success = 0;
                            break;
                        }
                        /* possible ranges a-zA-Z0-9 and between */
                        else if (!((input[pos] <= input[pos + 2]) &&
                                   ((input[pos] >= 'a' && input[pos] <= 'z') ||
                                    (input[pos] >= 'A' && input[pos] <= 'Z') ||
                                    (input[pos] >= '0' &&
                                     input[pos] <= '9')))) {
                            success = 0;
                            break;
                        } else {
                            for (char c = input[pos]; c <= input[pos + 2];
                                 c++) {
                                if (!contains(c, v_symbols->content,
                                              v_symbols->size)) {
                                    vector_push(v_symbols, &c);
                                }
                            }
                            pos += 3; /* every range consists of 3 characters */
                        }
                    }

                    /* standard symbol */
                    else {
                        vector_push(v_symbols, &input[pos++]);
                    }
                }

                if (!success) {
                    break;
                }

                /* inverted class: ALL_SYMBOLS - v_symbols */
                if (inverted) {
                    vector* v_inverted_symbols = new_vector(sizeof(char), NULL);
                    for (int i = 0; i < strlen(ALL_SYMBOLS); i++) {
                        vector_push(v_inverted_symbols, &ALL_SYMBOLS[i]);
                    }
                    string_subtract(v_inverted_symbols, v_symbols);
                    delete_vector(&v_symbols);
                    v_symbols = v_inverted_symbols;
                    v_inverted_symbols = NULL;
                }

                char symbol;
                vector_reset_iterator(v_symbols);
                vector_next(v_symbols, &symbol);
                current_regex = new_single_transition_regex(symbol);
                current_regex->states[0]->transitions =
                    realloc(current_regex->states[0]->transitions,
                            v_symbols->size * sizeof(transition*));
                while (vector_next(v_symbols, &symbol)) {
                    current_regex->states[0]
                        ->transitions[v_symbols->iterator - 1] =
                        new_transition(ts_active, symbol, 1);
                }
                current_regex->states[0]->nr_transitions = v_symbols->size;

                delete_vector(&v_symbols);
                break;
            }

            /* standard character */
            default:
                current_regex = new_single_transition_regex(input[pos]);
                break;
            }


            /* modifiers */
            switch (input[pos + 1]) {
            /* repetition range a{2,5} */
            case '{': {
                int min = 0;
                int max = 0;
                pos += 2;

                /* parse min */
                while (input[pos] != ',' && input[pos] != '}') {
                    if (input[pos] >= '0' && input[pos] <= '9') {
                        min = min * 10 + (input[pos++] - '0');
                    } else {
                        success = 0;
                        break;
                    }
                }

                if (!success) {
                    break;
                }

                if (input[pos] == ',') {
                    if (input[++pos] == '}') {
                        max = min;
                    } else {
                        /* parse max */
                        while (input[pos] != '}') {
                            if (input[pos] >= '0' && input[pos] <= '9') {
                                max = max * 10 + (input[pos++] - '0');
                            } else {
                                success = 0;
                                break;
                            }
                        }
                    }
                } else if (input[pos] == '}') {
                    max = min;
                }


                if (!success) {
                    break;
                }

                pos++;

                if (min < 0 || max < 1 || min > max) {
                    success = 0;
                    break;
                }

                /* chain max-1 copies of current_regex */
                vector* copy_vector = new_vector(sizeof(regex*), NULL);
                for (int i = 0; i < (max - 1); i++) {
                    regex* temp_regex = copy_regex(current_regex);
                    vector_push(copy_vector, &temp_regex);
                }


                /* fold all copied regexes into one */
                regex* left = NULL;
                regex* right = NULL;
                for (int i = max - 2; i >= 0; i--) {
                    vector_pop(copy_vector, &right);
                    if (i >= min - 1) {
                        regex_optional(right);
                    }
                    if (i > 0) {
                        vector_pop(copy_vector, &left);
                        regex_chain(left, &right);
                        vector_push(copy_vector, &left);
                    }
                }

                /* chain the copies behind current_regex */
                vector_pop(copy_vector, &right);
                regex_chain(current_regex, &right);

                delete_vector(&copy_vector);

                if (min == 0) {
                    regex_optional(current_regex);
                }
            } break;

            /* zero or one repetition a? */
            case '?':
                regex_optional(current_regex);
                if (input[pos + 2] == '?') {
                    regex_make_lazy(current_regex);
                    pos += 3;
                } else {
                    regex_make_greedy(current_regex);
                    pos += 2;
                }
                break;

            /* zero or many repetitions a* */
            case '*':
                regex_repeat(current_regex);
                if (input[pos + 2] == '?') {
                    regex_make_lazy(current_regex);
                    pos += 3;
                } else {
                    regex_make_greedy(current_regex);
                    pos += 2;
                }
                break;

            /* one or many repetitions a+ */
            case '+': {
                regex* temp_regex = copy_regex(current_regex);
                regex_repeat(temp_regex);
                regex_chain(current_regex, &temp_regex);
                if (input[pos + 2] == '?') {
                    regex_make_lazy(current_regex);
                    pos += 3;
                } else {
                    regex_make_greedy(current_regex);
                    pos += 2;
                }
            } break;

            /* no modifiers */
            default:
                pos++;
                break;
            }
        }

        /* ^ line start */
        else if (input[pos] == '^') {
            pos++;
            current_regex = new_single_transition_regex(LINE_START);
        }

        /* line end */
        else if (input[pos] == '$') {
            pos++;
            current_regex = new_single_transition_regex(LINE_END);
        }

        /* block start */
        else if (input[pos] == '(') {
            pos++;
            level++;
            int temp_int = 0;
            vector_push(alternative_on_level, &temp_int);
            vector* temp_vector = new_vector(sizeof(vector*), NULL);
            vector_push(regex_objects, &temp_vector);
        }

        /* alternative */
        else if (input[pos] == '|') {
            int alternative;
            vector* regex_objects_top;
            vector_top(regex_objects, &regex_objects_top);
            vector_top(alternative_on_level, &alternative);
            if (!regex_objects_top->size || alternative) {
                success = 0;
                break;
            } else {
                alternative = 1;
                pos++;
            }
        }

        /* invalid symbol */
        else {
            success = 0;
            break;
        }

        /* if necessary, integrate current_regex */
        if (current_regex != NULL) {
            vector* v_temp_regex;
            vector_pop(regex_objects, &v_temp_regex);
            int alternative;
            vector_pop(alternative_on_level, &alternative);
            if (alternative) {
                regex* temp_regex;
                vector_pop(v_temp_regex, &temp_regex);
                regex_alternative(temp_regex, &current_regex);
                vector_push(v_temp_regex, &temp_regex);
                alternative = 0;
            } else {
                vector_push(v_temp_regex, &current_regex);
            }
            vector_push(alternative_on_level, &alternative);
            vector_push(regex_objects, &v_temp_regex);
            current_regex = NULL;
        }
    }

    if (success) {
        int alternative;
        vector_pop(alternative_on_level, &alternative);

        if (level || alternative) {
            success = 0;
        } else {
            /* chain all level 0 elements */
            vector* level_0_elements;
            vector_pop(regex_objects, &level_0_elements);
            vector_reset_iterator(level_0_elements);
            regex* temp_regex;
            if (vector_next(level_0_elements, &current_regex)) {
                while (vector_next(level_0_elements, &temp_regex)) {
                    regex_chain(current_regex, &temp_regex);
                }
            }

            /* chain an optional end of line regex to the end */
            temp_regex = new_single_transition_regex(LINE_END);
            regex_optional(temp_regex);
            regex_chain(current_regex, &temp_regex);

            *r = current_regex;
            current_regex = NULL;
        }
    }


    /* clean up */
    delete_regex(&current_regex);
    delete_vector(&regex_objects);
    delete_vector(&alternative_on_level);

    return success;
}

// EPSILON FUNCTION


static int remove_epsilon_transitions(regex* r) {
    // calculate epsilon closure for every state
    // visited is initialized with 0 (?)
    // and marked with 1 when visited
    vector* epsilon_closure_size = new_vector(sizeof(int), NULL);
    vector* epsilon_closure_list = new_vector(sizeof(vector*), NULL);
    vector* marked = new_vector(sizeof(int), NULL);


    int null_const = 0;
    int one_const = 1;
    int two_const = 2;

    // initialize vectors
    for (int i = 0; i < r->nr_states; i++) {
        vector_push(epsilon_closure_size, &null_const);
        vector_push(marked, &null_const);
        vector* y = new_vector(sizeof(int), NULL);
        vector_push(epsilon_closure_list, &y);
    }

    stack* s = new_stack(sizeof(int), NULL);

    // iterate over all states and calculate the epsilon closures
    for (int state_nr = 0; state_nr < r->nr_states; state_nr++) {
        // reset the "marked" vector
        vector_reset_iterator(marked);
        do {
            vector_set(marked, &null_const);
        } while (vector_move_iterator(marked));

        // get a "reference" to the partial vector (contains a list of all
        // states that are in the epsilon closure)
        vector* current_epsilon_closure;
        vector_get_at(epsilon_closure_list, state_nr, &current_epsilon_closure);

        // look if there's already a marked set of states from previous
        // iterations
        int prev_marked_set = 0;
        vector_get_at(epsilon_closure_size, state_nr, &prev_marked_set);
        if (prev_marked_set) {
            vector_reset_iterator(current_epsilon_closure);
            // push all to the stack for further processing
            // + mark them as already visited, but with a 2 so they won't get
            // stored twice
            int state_to_examine;
            while (vector_next(current_epsilon_closure, &state_to_examine)) {
                stack_push(s, &state_to_examine);
                vector_set_at(marked, state_to_examine, &two_const);
            }
        } else {
            stack_push(s, &state_nr);
            vector_set_at(marked, state_nr, &one_const);
        }

        // TODO: make more efficient by writing intermediate results
        // mark all reachable states
        int processed_state;
        while (stack_pop(s, &processed_state)) {
            // iterate over all transitions of the processed state
            for (int transition_nr = 0;
                 transition_nr < r->states[processed_state]->nr_transitions;
                 transition_nr++) {
                if (r->states[processed_state]
                        ->transitions[transition_nr]
                        ->status == ts_epsilon) {
                    int next_state_marked;
                    vector_get_at(marked,
                                  r->states[processed_state]
                                      ->transitions[transition_nr]
                                      ->next_state,
                                  &next_state_marked);
                    if (!next_state_marked) {
                        vector_set_at(marked,
                                      r->states[processed_state]
                                          ->transitions[transition_nr]
                                          ->next_state,
                                      &one_const);
                        stack_push(s, &(r->states[processed_state]
                                            ->transitions[transition_nr]
                                            ->next_state));
                    }
                }
            }
        }

        // put the reachable states into the epsilon closure list
        vector_reset_iterator(marked);
        int examined_state = 0;
        int state_reachable = 0;
        while (vector_next(marked, &state_reachable)) {
            if (state_reachable == 1) {
                examined_state = marked->iterator - 1;
                vector_push(current_epsilon_closure, &examined_state);
            }
        }

        // copy the (maybe) changed partial vector back and write its size
        vector_set_at(epsilon_closure_list, state_nr, &current_epsilon_closure);
        vector_set_at(epsilon_closure_size, state_nr,
                      &(current_epsilon_closure->size));
    }
    delete_stack(&s);

    // remove all epsilon transitions by marking them as dead
    for (int state_nr = 0; state_nr < r->nr_states; state_nr++) {
        for (int j = r->states[state_nr]->nr_transitions - 1; j >= 0; j--) {
            if (r->states[state_nr]->transitions[j]->status == ts_epsilon) {
                r->states[state_nr]->transitions[j]->status = ts_dead;
            }
        }
    }

    // make a list of all symbols the automaton knows
    int nr_symbols = 0;
    char* symbols = NULL;

    for (int state_nr = 0; state_nr < r->nr_states; state_nr++) {
        for (int transition_nr = 0;
             transition_nr < r->states[state_nr]->nr_transitions;
             transition_nr++) {
            if (r->states[state_nr]->transitions[transition_nr]->status ==
                    ts_active &&
                !contains(
                    r->states[state_nr]->transitions[transition_nr]->symbol,
                    symbols, nr_symbols)) {
                symbols = realloc(symbols, ++nr_symbols * sizeof(char));
                symbols[nr_symbols - 1] =
                    r->states[state_nr]->transitions[transition_nr]->symbol;
            }
        }
    }


    // iterate over all states and write the new transitions
    for (int state_nr = 0; state_nr < r->nr_states; state_nr++) {
        vector* current_epsilon_closure;
        vector_get_at(epsilon_closure_list, state_nr, &current_epsilon_closure);

        // iterate over possible symbols
        for (int symbol_iterator = 0; symbol_iterator < nr_symbols;
             symbol_iterator++) {

            // mark all states as not reachable
            vector_reset_iterator(marked);
            do {
                vector_set(marked, &null_const);
            } while (vector_move_iterator(marked));

            char symbol = symbols[symbol_iterator];

            // iterate over all states in the epsilon closure
            // mark their successor if they have a transition with the current
            // symbol
            vector_reset_iterator(current_epsilon_closure);
            int processed_state;
            while (vector_next(current_epsilon_closure, &processed_state)) {
                for (int transition_iterator = 0;
                     transition_iterator <
                     r->states[processed_state]->nr_transitions;
                     transition_iterator++) {
                    if (r->states[processed_state]
                            ->transitions[transition_iterator]
                            ->symbol == symbol) {
                        vector_set_at(marked,
                                      r->states[processed_state]
                                          ->transitions[transition_iterator]
                                          ->next_state,
                                      &one_const);
                    }
                }
            }

            // now all states directly reachable with symbol from the current
            // epsilon closure are marked -> mark the epsilon closure of those
            // states
            for (int checked_state = 0; checked_state < r->nr_states;
                 checked_state++) {
                int is_reachable;
                vector_get_at(marked, checked_state, &is_reachable);
                if (is_reachable) {
                    vector* checked_epsilon_closure;
                    vector_get_at(epsilon_closure_list, checked_state,
                                  &checked_epsilon_closure);
                    vector_reset_iterator(checked_epsilon_closure);
                    int next_reachable_state;
                    while (vector_next(checked_epsilon_closure,
                                       &next_reachable_state)) {
                        vector_set_at(marked, next_reachable_state, &one_const);
                    }
                }
            }

            // remove duplicates: iterate over the current state's transitions
            // and unmark all states that are already reachable with the current
            // symbol
            for (int transition_iterator = 0;
                 transition_iterator < r->states[state_nr]->nr_transitions;
                 transition_iterator++) {
                if (r->states[state_nr]
                            ->transitions[transition_iterator]
                            ->symbol == symbol &&
                    r->states[state_nr]
                            ->transitions[transition_iterator]
                            ->status == ts_active) {
                    vector_set_at(marked,
                                  r->states[state_nr]
                                      ->transitions[transition_iterator]
                                      ->next_state,
                                  &null_const);
                }
            }

            // now all states that can be reached with symbol are marked
            // so finally, we can add transitions for them
            int is_reachable;
            vector_reset_iterator(marked);
            while (vector_next(marked, &is_reachable)) {
                if (is_reachable) {
                    r->states[state_nr]->transitions =
                        realloc(r->states[state_nr]->transitions,
                                ++(r->states[state_nr]->nr_transitions) *
                                    sizeof(transition*));
                    r->states[state_nr]
                        ->transitions[r->states[state_nr]->nr_transitions - 1] =
                        new_transition(ts_active, symbol, marked->iterator - 1);
                }
            }
        }
    }


    free(symbols);
    delete_vector(&marked);
    delete_vector(&epsilon_closure_size);
    vector* temp_vector;
    while (vector_pop(epsilon_closure_list, &temp_vector)) {
        delete_vector(&temp_vector);
    }
    delete_vector(&epsilon_closure_list);

    return 1;
}


// NFA-DFA-CONVERSION


static int nfa_to_dfa(regex* r) {
    // store the new combined states
    int nr_state_sets = 0;
    vector* state_sets = new_vector(sizeof(vector*), NULL);
    vector* states = new_vector(sizeof(state*), NULL);

    // stack for storing states that need to be processed
    stack* s = new_stack(sizeof(int), NULL);

    // accumulate a string with all symbols the automaton knows
    char* symbols = NULL;
    int nr_symbols = 0;
    for (int state_nr = 0; state_nr < r->nr_states; state_nr++) {
        for (int transition_nr = 0;
             transition_nr < r->states[state_nr]->nr_transitions;
             transition_nr++) {
            if ((r->states[state_nr]->transitions[transition_nr]->status ==
                 ts_active) &&
                !contains(
                    r->states[state_nr]->transitions[transition_nr]->symbol,
                    symbols, nr_symbols)) {
                symbols = realloc(symbols, ++nr_symbols * sizeof(char));
                symbols[nr_symbols - 1] =
                    r->states[state_nr]->transitions[transition_nr]->symbol;
            }
        }
    }
    {
        // initialize the stack
        int start_state_nr = 0;
        stack_push(s, &start_state_nr);

        // initialize the state structures
        vector* start_state_set = new_vector(sizeof(int), NULL);
        vector_push(start_state_set, &start_state_nr);
        vector_push(state_sets, &start_state_set);

        nr_state_sets++;

        state* state_0 = new_state(0, sb_none, r->states[0]->type);
        state_0->behaviour = r->states[0]->behaviour;
        vector_push(states, &state_0);
    }

    // process all states on the stack
    // state_pos is an index into the states vector
    int state_pos;
    while (stack_pop(s, &state_pos)) {
        // iterate over symbols
        for (int symbol_iterator = 0; symbol_iterator < nr_symbols;
             symbol_iterator++) {
            int end_state_marker = 0;
            char symbol = symbols[symbol_iterator];

            // accumulate all possible next states
            int* next_states = NULL;
            int nr_next_states = 0;

            vector* current_state_set;
            vector_get_at(state_sets, state_pos, &current_state_set);
            vector_reset_iterator(current_state_set);
            int state_nr;

            // iterate over all old states that belong to the current combined
            // state
            while (vector_next(current_state_set, &state_nr)) {
                // find all next states with the current symbol
                for (int transition_iterator = 0;
                     transition_iterator < r->states[state_nr]->nr_transitions;
                     transition_iterator++) {
                    if (r->states[state_nr]
                                ->transitions[transition_iterator]
                                ->symbol == symbol &&
                        r->states[state_nr]
                                ->transitions[transition_iterator]
                                ->status == ts_active) {
                        int already_there = 0;
                        // check if the next_state is already contained in
                        // next_states
                        for (int next_states_iterator = 0;
                             next_states_iterator < nr_next_states;
                             next_states_iterator++) {
                            if (r->states[state_nr]
                                    ->transitions[transition_iterator]
                                    ->next_state ==
                                next_states[next_states_iterator]) {
                                already_there = 1;
                                break;
                            }
                        }
                        if (!already_there) {
                            next_states = realloc(
                                next_states, ++nr_next_states * sizeof(int));
                            next_states[nr_next_states - 1] =
                                r->states[state_nr]
                                    ->transitions[transition_iterator]
                                    ->next_state;

                            // check if it is an end state -> must be marked in
                            // the new state
                            if (r->states[r->states[state_nr]
                                              ->transitions[transition_iterator]
                                              ->next_state]
                                        ->type == st_end ||
                                r->states[r->states[state_nr]
                                              ->transitions[transition_iterator]
                                              ->next_state]
                                        ->type == st_start_end) {
                                end_state_marker = 1;
                            }
                        }
                    }
                }
            }

            if (!nr_next_states) {
                free(next_states);
                continue;
            }

            sort_int_array(next_states, nr_next_states);

            // check if this combination of old states already exists
            int exists = -1;

            for (int j = 0; j < nr_state_sets; j++) {
                vector* temp_state_set;
                vector_get_at(state_sets, j, &temp_state_set);
                int nr_states_comp = temp_state_set->size;
                vector* state_set_comp;
                vector_get_at(state_sets, j, &state_set_comp);
                if (nr_states_comp == nr_next_states) {
                    exists = j;
                    for (int k = 0; k < nr_next_states; k++) {
                        int state_comp;
                        vector_get_at(state_set_comp, k, &state_comp);
                        if (next_states[k] != state_comp) {
                            exists = -1;
                            break;
                        }
                    }
                    if (exists > -1) {
                        break; // found the state
                    }
                }
            }

            // state is new and needs to be created
            if (exists < 0) {
                // calculate the new state's behaviour
                // if all next_states are sb_none or sb_lazy, it is sb_lazy
                // as soon as one is sb_greedy, it is sb_greedy
                int lazy = 0;
                int greedy = 0;

                // save the state with its partial states and create the
                // corresponding transition to it from the current state
                // transform next_states into a vector
                vector* v_next_states = new_vector(sizeof(int), NULL);
                for (int next_states_iterator = 0;
                     next_states_iterator < nr_next_states;
                     next_states_iterator++) {
                    vector_push(v_next_states,
                                &next_states[next_states_iterator]);
                    if (r->states[next_states[next_states_iterator]]
                            ->behaviour == sb_lazy) {
                        lazy = 1;
                    } else if (r->states[next_states[next_states_iterator]]
                                   ->behaviour == sb_greedy) {
                        greedy = 1;
                    }
                }
                vector_push(state_sets, &v_next_states);


                state* created_state = new_state(
                    0, sb_none, ((end_state_marker == 1) ? st_end : st_middle));
                created_state->behaviour =
                    ((lazy && greedy)
                         ? sb_greedy
                         : (lazy) ? sb_lazy : (greedy) ? sb_greedy : sb_none);

                vector_push(states, &created_state);

                // set exists to the newly created state
                // if no state was created, it already points to the correct one
                exists = ++(nr_state_sets)-1;

                // push the new state to the stack so it will be processed
                stack_push(s, &exists);
            }

            // now the current state can be linked to the created next_state
            state* current_state;
            vector_get_at(states, state_pos, &current_state);
            current_state->transitions = realloc(
                current_state->transitions,
                ++(current_state->nr_transitions) * sizeof(transition*));
            current_state->transitions[current_state->nr_transitions - 1] =
                new_transition(ts_active, symbol, exists);
            vector_set_at(states, state_pos, &current_state);

            free(next_states);
        }
    }

    // empty the old regex object
    for (int state_nr = 0; state_nr < r->nr_states; state_nr++) {
        free_state(r->states[state_nr]);
        free(r->states[state_nr]);
    }
    free(r->states);

    // replace it with the new one
    state** state_array = malloc(states->size * sizeof(state*));
    for (int state_nr = 0; state_nr < states->size; state_nr++) {
        vector_get_at(states, state_nr, &state_array[state_nr]);
    }
    delete_vector(&states);

    r->states = state_array;
    r->nr_states = nr_state_sets;

    // free resources
    for (int i = 0; i < nr_state_sets; i++) {
        vector* state_set;
        vector_get_at(state_sets, i, &state_set);
        delete_vector(&state_set);
    }
    delete_vector(&state_sets);
    delete_stack(&s);
    free(symbols);

    return 1;
}


static void string_subtract(vector* a, vector* b) {
    vector_reset_iterator(b);
    char comp_char;
    while (vector_next(b, &comp_char)) {
        vector_reset_iterator(a);
        char current_char;
        while (vector_next(a, &current_char)) {
            if (current_char == comp_char && current_char != 0) {
                a->iterator--;
                vector_remove(a);
                vector_next(a, &current_char);
            }
        }
    }
}