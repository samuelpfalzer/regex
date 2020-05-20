// clang-format off
#define DEBUG_MODE 1
#if DEBUG_MODE > 0
#define DEBUG(...) fprintf(stdout, ...);
#else
#define DEBUG(...) while(0);
#endif
#define ERROR(...) fprintf(stderr, ...);
// clang-format on


#include "helper_functions.h"
#include "regex.h"
#include "stack.h"
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//========== DEFINITIONS ==========

const char* REGULAR_SYMBOLS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUV"
                              "WXYZ0123456789\"\'#/&=@!%_: \t";
const char* ESCAPED_SYMBOLS = "-^$()[]{}\\*+?.|";
const char* ALL_SYMBOLS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUV"
                          "WXYZ0123456789\"\'#/&=@!%_: \t-^$()[]{}\\*+?.|";


//========== PRIVATE FUNCTION DECLARATIONS ==========


// compiles the input string to a regex structure -> 1 on success, else 0
static int parse(regex** r, char* input);

static int remove_epsilon_transitions(regex* r);

static int make_deterministic(regex* r);


//========== COMPILE FUNCTION ==========

// TODO: free r if compilation fails!

int regex_compile(regex** r, char* input) {
    free_regex(r);

    if (!parse(r, input)) {
        printf("ERROR: parse failed\n");
        return 0;
    };

    if (!remove_epsilon_transitions(*r)) {
        printf("ERROR: remove_epsilon_transitions failed\n");
        return 0;
    };


    if (!make_deterministic(*r)) {
        printf("ERROR: make_deterministic failed\n");
        return 0;
    };

    return 1;
}


//========== PARSE FUNCTION ==========


static int parse(regex** r, char* input) {
    int level = 0;

    regex* current_regex = NULL;
    vector* temp_vector = NULL;
    regex* temp_regex = NULL;
    regex* temp_regex2 = NULL;
    int temp_int;

    vector* elements_on_level = new_vector(sizeof(int), NULL);
    vector_push(elements_on_level, &level);

    int pos = 0;

    vector* alternative_on_level = new_vector(sizeof(int), NULL);
    vector_push(alternative_on_level, &level);

    vector* regex_objects = new_vector(sizeof(vector*), NULL);
    temp_vector = new_vector(sizeof(regex*), NULL);
    vector_push(regex_objects, &temp_vector);

    int line_start = 0;
    int line_end = 0;


    while (!in(input[pos], "\n\0", 2)) {
        printf("DEBUG: start of parse loop: level %d, pos %d (char %c)\n",
               level, pos, input[pos]);

        // alphanumerical character | end of block | any | escaped character
        if ((in(input[pos], REGULAR_SYMBOLS, strlen(REGULAR_SYMBOLS))) ||
            (input[pos] == '[') || (input[pos] == '.') ||
            (input[pos] == ')' && level > 0) || (input[pos] == '\\')) {

            switch (input[pos]) {
            case '\\': // escaped character
                if (!in(input[pos + 1], ESCAPED_SYMBOLS,
                        strlen(ESCAPED_SYMBOLS))) {
                    printf("ERROR: invalid escape sequence \\%c\n",
                           input[pos + 1]);
                    return 0;
                } else {
                    current_regex = new_single_regex(input[++pos]);
                    printf("DEBUG: escaped char\n");
                }
                break;

            case ')': // closed block
            {
                int alternative_in_block;
                vector_pop(alternative_on_level, &alternative_in_block);
                if (alternative_in_block) {
                    printf("ERROR: alternative not satisfied\n");
                    return 0;
                }

                int elements_in_block;
                vector_pop(elements_on_level, &elements_in_block);
                if (!elements_in_block) {
                    printf("ERROR: empty block\n");
                    return 0;
                }

                // concatenate all elements of the current level into one
                // erase the current level's element table
                // and store the concatenated element in current_regex
                vector* block_merge_vector = NULL;
                vector_pop(regex_objects, &block_merge_vector);
                vector_reset_iterator(block_merge_vector);

                if (vector_next(block_merge_vector, &current_regex)) {
                    regex* buffer_regex = NULL;
                    while (vector_next(block_merge_vector, &buffer_regex)) {
                        regex_concat(current_regex, buffer_regex);
                    }
                }

                delete_vector(&block_merge_vector);
                level--;
                printf("DEBUG: block end\n");
                break;
            }
            case '.': // any character
                current_regex = new_single_regex(ALL_SYMBOLS[0]);
                current_regex->states[0]->transitions =
                    realloc(current_regex->states[0]->transitions,
                            strlen(ALL_SYMBOLS) * sizeof(transition*));
                for (int i = 1; i < strlen(ALL_SYMBOLS); i++) {
                    current_regex->states[0]->transitions[i] =
                        new_transition(ts_active, ALL_SYMBOLS[i], 1);
                }
                current_regex->states[0]->nr_transitions = strlen(ALL_SYMBOLS);
                printf("DEBUG: any char\n");
                break;

            case '[': // character class
            {
                int inverted = 0; // TODO: implement
                if (input[pos + 1] == '^') {
                    inverted = 1;
                    pos++;
                }

                if (input[++pos] == ']') {
                    printf("\nERROR: empty class not allowed\n");
                    return 0;
                }

                int nr_symbols = 0;
                char* symbols = NULL;

                while (input[pos] != ']') {
                    printf("DEBUG: parsing class, symbol = %c\n", input[pos]);
                    if (in(input[pos], "\n\0", 2)) {
                        printf("\nERROR: class not closed");
                        return 0;
                    }

                    // escaped symbol
                    if (input[pos] == '\\' &&
                        in(input[pos + 1], ESCAPED_SYMBOLS,
                           strlen(ESCAPED_SYMBOLS))) {
                        pos++;
                        symbols =
                            realloc(symbols, (++nr_symbols) * sizeof(char));
                        printf("->  added %c to class", input[pos]);
                        symbols[nr_symbols - 1] = input[pos++];
                    }

                    // range
                    else if (input[pos + 1] == '-') {
                        if (in(input[pos + 2], "\n\0", 2)) {
                            printf("\nERROR: class not closed");
                            return 0;
                        }
                        if (input[pos + 2] == ']') {
                            printf("\nERROR: range must specify an end\n");
                            return 0;
                        } else if (!((input[pos] <= input[pos + 2]) &&
                                     ((input[pos] >= 'a' &&
                                       input[pos] <= 'z') ||
                                      (input[pos] >= 'A' &&
                                       input[pos] <= 'Z') ||
                                      (input[pos] >= '0' &&
                                       input[pos] <= '9')))) {
                            printf("\nERROR: invalid range\n");
                            return 0;
                        } else {
                            for (char i = input[pos]; i <= input[pos + 2];
                                 i++) {
                                if (!in(i, symbols, nr_symbols)) {
                                    symbols = realloc(
                                        symbols, (++nr_symbols) * sizeof(char));
                                    symbols[nr_symbols - 1] = i;
                                }
                            }
                            // 3 chars: a-z
                            pos += 3;
                        }
                    }
                    // single symbol
                    else {
                        symbols =
                            realloc(symbols, (++nr_symbols) * sizeof(char));
                        symbols[nr_symbols - 1] = input[pos++];
                    }
                }

                // like with '.', only 2 states are needed for this part
                // just iterate over the valid symbol string and create a
                // transition for each symbol
                current_regex = new_single_regex(symbols[0]);
                current_regex->states[0]->transitions =
                    realloc(current_regex->states[0]->transitions,
                            nr_symbols * sizeof(transition*));
                for (int i = 1; i < nr_symbols; i++) {
                    current_regex->states[0]->transitions[i] =
                        new_transition(ts_active, symbols[i], 1);
                }
                current_regex->states[0]->nr_transitions = nr_symbols;

                free(symbols);
                printf("DEBUG: char class\n");
                break;
            }
            default: // regular character
                current_regex = new_single_regex(input[pos]);
                printf("DEBUG: regular char\n");
                break;
            }


            //===== MODIFIERS =====

            // repetition range a{2,5}
            if (input[pos + 1] == '{') {
                int min = 0;
                int max = 0;
                pos += 2;

                // parse min number
                while (input[pos] != ',') {
                    if (input[pos] >= '0' && input[pos] <= '9') {
                        min = min * 10 + (input[pos++] - '0');
                    } else {
                        printf("\nERROR: %c is no number and cannot specify a "
                               "range\n",
                               input[pos]);
                        return 0;
                    }
                }

                pos++;

                // read max number
                while (input[pos] != '}') {
                    if (input[pos] >= '0' && input[pos] <= '9') {
                        max = max * 10 + (input[pos++] - '0');
                    } else {
                        printf("\nERROR: %c is no number and cannot specify a "
                               "range\n",
                               input[pos]);
                        return 0;
                    }
                }

                pos++;

                if (min < 0 || max < 1 || min > max) {
                    printf("\nERROR: invalid range\n");
                    return 0;
                }

                // create max-1 copies to be concatenated behind current_regex
                temp_vector = new_vector(sizeof(regex*), NULL);
                for (int i = 0; i < (max - 1); i++) {
                    temp_regex = copy_regex(current_regex);
                    vector_push(temp_vector, &temp_regex);
                }


                // fold the additional regexes into a single one
                for (int i = max - 2; i >= 0; i--) {
                    vector_pop(temp_vector, &temp_regex);
                    if (i >= min - 1) {
                        regex_optional(temp_regex);
                    }
                    if (i > 0) {
                        vector_pop(temp_vector, &temp_regex2);
                        regex_concat(temp_regex2, temp_regex);
                        vector_push(temp_vector, &temp_regex2);
                    }
                }

                // concat the additional regexes to the original one
                vector_pop(temp_vector, &temp_regex);
                regex_concat(current_regex, temp_regex);

                delete_vector(&temp_vector);

                // in case {0,x} the whole construct may be repeated 0 times
                if (min == 0) {
                    regex_optional(current_regex);
                }
                printf("DEBUG: range modifier\n");
            }

            // zero or one repetition a?
            else if (input[pos + 1] == '?') {
                regex_optional(current_regex);

                // a??
                if (input[pos + 2] == '?') {
                    regex_make_lazy(current_regex);
                    pos += 3;
                }

                // a?
                else {
                    regex_make_greedy(current_regex);
                    pos += 2;
                }

                printf("DEBUG: ? modifier\n");
            }

            // zero to infty repetitions a* or a*?
            else if (input[pos + 1] == '*') {
                regex_repeat(current_regex);

                // a*?
                if (input[pos + 2] == '?') {
                    regex_make_lazy(current_regex);
                    pos += 3;
                }

                // a*
                else {
                    regex_make_greedy(current_regex);
                    pos += 2;
                }
                printf("DEBUG: * modifier\n");
            }

            // one to infty repetitions a+ or a+?
            else if (input[pos + 1] == '+') {
                temp_regex = copy_regex(current_regex);
                regex_repeat(temp_regex);
                regex_concat(current_regex, temp_regex);

                // a+?
                if (input[pos + 2] == '?') {
                    regex_make_lazy(current_regex);
                    pos += 3;
                }

                // a+
                else {
                    regex_make_greedy(current_regex);
                    pos += 2;
                }
                printf("DEBUG: + modifier\n");
            }

            // no modifiers
            else {
                pos++;
                printf("DEBUG: no modifiers\n");
            }
        }

        // ^
        else if (input[pos] == '^') {
            // must be the first symbol in a regex string
            if (pos > 0) {
                printf("ERROR: ^ can only be used as the first symbol of a "
                       "regex string\n");
                return 0;
            } else {
                pos++;
                line_start = 1;
                printf("DEBUG: line start\n");
                continue;
            }
        }

        // $
        else if (input[pos] == '$') {
            // must be the last symbol in a regex string
            if (!in(input[pos + 1], "\n\0", 2)) {
                printf("ERROR: $ can only be used as the last symbol of a "
                       "regex string\n");
                return 0;
            } else {
                if (level == 0) {
                    pos++;
                    line_end = 1;
                    printf("DEBUG: line end\n");
                } else {
                    printf("ERROR: %d () blocks not closed\n", level);
                    return 0;
                }
            }
        }

        // block start
        else if (input[pos] == '(') {
            pos++;
            level++;
            int i = 0;
            vector_push(elements_on_level, &i);
            vector_push(alternative_on_level, &i);
            temp_vector = new_vector(sizeof(vector*), NULL);
            vector_push(regex_objects, &temp_vector);
            printf("DEBUG: block start\n");
        }

        // alternative
        else if (input[pos] == '|') {
            int elements, alternative;
            vector_pop(elements_on_level, &elements);
            vector_pop(alternative_on_level, &alternative);
            if (elements == 0 || alternative == 1) {
                printf("ERROR: invalid alternative\n");
                return 0;
            } else {
                alternative = 1;
                vector_push(elements_on_level, &elements);
                vector_push(alternative_on_level, &alternative);
                pos++;
            }
            printf("DEBUG: alternative\n");
        }

        // invalid symbol
        else {
            printf("ERROR: invalid symbol %c\n", input[pos]);
            return 0;
        }

        // if a value was assigned to current_regex: integrate it into the
        // object list
        if (current_regex != NULL) {
            vector_pop(regex_objects, &temp_vector);
            int alternative;
            vector_pop(alternative_on_level, &alternative);
            if (alternative) {
                vector_pop(temp_vector, &temp_regex);
                regex_alternative(temp_regex, current_regex);
                vector_push(temp_vector, &temp_regex);
                alternative = 0;
                printf("DEBUG: pushed alternative\n");
            } else {
                int elements;
                vector_pop(elements_on_level, &elements);
                elements++;
                vector_push(elements_on_level, &elements);
                vector_push(temp_vector, &current_regex);
                printf("DEBUG: pushed element\n");
            }
            vector_push(alternative_on_level, &alternative);
            vector_push(regex_objects, &temp_vector);
        }

        current_regex = NULL;
        temp_vector = NULL;
        temp_regex = NULL;
        temp_regex2 = NULL;
        temp_int = 0;
    }

    printf("DEBUG: end of parse loop\n");

    int alternative;
    vector_pop(alternative_on_level, &alternative);

    if (level || alternative) {
        if (level) {
            printf("ERROR: %i blocks are not closed at the end of your "
                   "expression\n",
                   level);
            return 0;
        } else {
            printf("ERROR: alternative not satisfied\n");
            return 0;
        }
    } else {
        // concatenate all level 0 regex elements
        vector_pop(regex_objects, &temp_vector);
        temp_vector->iterator = 0;

        if (vector_next(temp_vector, &current_regex)) {
            while (vector_next(temp_vector, &temp_regex)) {
                regex_concat(current_regex, temp_regex);
            }
        }

        printf("DEBUG: concatenated all elements\n");
        delete_vector(&temp_vector);

        current_regex->line_start = line_start;
        current_regex->line_end = line_end;

        delete_vector(&elements_on_level);
        delete_vector(&alternative_on_level);

        delete_vector(&regex_objects);

        *r = current_regex;

        printf("DEBUG: end of parsing\n");

        return 1;
    }
}

//========== EPSILON FUNCTION ==========


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
                !in(r->states[state_nr]->transitions[transition_nr]->symbol,
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


static int make_deterministic(regex* r) {
    // parallel arrays for the new combined states
    int** state_sets = NULL;
    int* nr_states = NULL;
    int nr_state_sets = 0;
    state** states = NULL;

    // stack for storing newly found states
    stack* s = new_stack(sizeof(int), NULL);

    // string with all symbols the automaton knows
    char* symbols = NULL;
    int nr_symbols = 0;

    // iterate over states
    for (int i = 0; i < r->nr_states; i++) {
        // iterate over transitions
        for (int j = 0; j < r->states[i]->nr_transitions; j++) {
            // accumulate all symbols
            if (!in(r->states[i]->transitions[j]->symbol, symbols,
                    nr_symbols)) {
                symbols = realloc(symbols, ++nr_symbols * sizeof(char));
                symbols[nr_symbols - 1] = r->states[i]->transitions[j]->symbol;
            }
        }
    }

    // initialize the stack
    int start_state = 0;
    stack_push(s, &start_state);

    // initialize the state structures
    state_sets = malloc(sizeof(int*));
    state_sets[0] = malloc(sizeof(int));
    state_sets[0][0] = 0;
    nr_states = malloc(sizeof(int));
    nr_states[0] = 1;
    nr_state_sets++;
    states = malloc(sizeof(state*));
    states[0] = new_state(0, sb_none,
                          (r->states[0]->type == st_start_end) ? st_start_end
                                                               : st_start);

    // process all states on the stack
    int state_pos;
    while (stack_pop(s, &state_pos)) {
        // iterate over symbols
        for (int i = 0; i < nr_symbols; i++) {
            int end_state_marker = 0;
            char symbol = symbols[i];

            // accumulate all possible next states
            int* next_states = NULL;
            int nr_next_states = 0;

            // iterate over all old states that belong to the current combined
            // state
            for (int j = 0; j < nr_states[state_pos]; j++) {
                int state_nr = state_sets[state_pos][j];

                // find all next states with the current symbol
                for (int k = 0; k < r->states[state_nr]->nr_transitions; k++) {
                    if (r->states[state_nr]->transitions[k]->symbol == symbol) {
                        int already_there = 0;
                        // check if the next_state is already contained in
                        // next_states
                        for (int l = 0; l < nr_next_states; l++) {
                            if (r->states[state_nr]
                                    ->transitions[k]
                                    ->next_state == next_states[l]) {
                                already_there = 1;
                                break;
                            }
                        }
                        if (!already_there) {
                            next_states = realloc(
                                next_states, ++nr_next_states * sizeof(int));
                            next_states[nr_next_states - 1] =
                                r->states[state_nr]->transitions[k]->next_state;

                            // check if it is an end state -> must be marked in
                            // the new state
                            if (r->states[r->states[state_nr]
                                              ->transitions[k]
                                              ->next_state]
                                        ->type == st_end ||
                                r->states[r->states[state_nr]
                                              ->transitions[k]
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
                if (nr_states[j] == nr_next_states) {
                    exists = j;
                    for (int k = 0; k < nr_next_states; k++) {
                        if (next_states[k] != state_sets[j][k]) {
                            exists = -1;
                            break;
                        }
                    }
                    if (exists > -1) {
                        break;
                    }
                }
            }

            // state is new and needs to be created
            if (exists < 0) {
                // save the state with its partial states and create the
                // corresponding transition
                state_sets =
                    realloc(state_sets, ++nr_state_sets * (sizeof(int*)));
                state_sets[nr_state_sets - 1] =
                    malloc(nr_next_states * sizeof(int));
                for (int j = 0; j < nr_next_states; j++) {
                    state_sets[nr_state_sets - 1][j] = next_states[j];
                }
                nr_states = realloc(nr_states, nr_state_sets * sizeof(int));
                nr_states[nr_state_sets - 1] = nr_next_states;
                states = realloc(states, nr_state_sets * sizeof(state));

                states[nr_state_sets - 1] = new_state(
                    0, sb_none, ((end_state_marker == 1) ? st_end : st_middle));

                exists = nr_state_sets - 1;

                // push the new state to the stack so it will be processed
                stack_push(s, &exists);
            }

            // now the current state can be linked to the created next_state
            states[state_pos]->transitions = realloc(
                states[state_pos]->transitions,
                ++(states[state_pos]->nr_transitions) * sizeof(transition*));
            states[state_pos]
                ->transitions[states[state_pos]->nr_transitions - 1] =
                new_transition(ts_active, symbol, exists);

            free(next_states);
        }
    }

    // replace the old regex object with the new one
    for (int i = 0; i < r->nr_states; i++) {
        free_state(r->states[i]);
        free(r->states[i]);
    }
    free(r->states);
    r->states = states;
    r->nr_states = nr_state_sets;

    // free resources
    free(nr_states);
    for (int i = 0; i < nr_state_sets; i++) {
        free(state_sets[i]);
    }
    free(state_sets);
    delete_stack(&s);
    free(symbols);

    return 1;
}