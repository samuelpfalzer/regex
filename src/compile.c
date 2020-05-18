#include "regex.h"
#include "stack.h"
#include "helper_functions.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


//========== DEFINITIONS ==========


#define MAX_LEVELS 20

const char* REGULAR_SYMBOLS = "\"\'#/&=@!%abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const char* ESCAPED_SYMBOLS = "-^$()[]{}\\*+?.|";
const char* ALL_SYMBOLS = "\"\'#/&=@!%abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789^$()[]{}\\*+?.|-";


//========== PRIVATE FUNCTION DECLARATIONS ==========


// compiles the input string to a regex structure -> 1 on success, else 0
static int parse(regex** r, char* input);

static int remove_epsilon_transitions(regex* r);

static int make_deterministic(regex* r);


//========== COMPILE FUNCTION ==========


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

    // keep track how many regex objects there currently are on each level
    int elements[MAX_LEVELS];
    for (int i = 0; i < MAX_LEVELS; i++) {
        elements[i] = 0;
    }
    int pos = 0;
    
    // keep track on which levels there currently is an active alternativ
    // necessary to implement precedence | -> concat
    int alternative[MAX_LEVELS];
    for (int i = 0; i < MAX_LEVELS; i++) {
        alternative[i] = 0;
    }

    // temporarily save single regex objects to concatenate them later
    regex*** rs = malloc(MAX_LEVELS * sizeof(regex**));
    for (int i = 0; i < MAX_LEVELS; i++) {
        rs[i] = NULL;
    }

    while (!in(input[pos], "\n\0", 2)) {
        // store the partial regex built in this iteration
        // and mark if something was added
        // regex_built will stay 0 if e.g. a new block is opened
        int regex_built = 0;
        regex* r_temp;

        // alphanumerical character
        // or end of block
        // or random character
        // or escaped character
        if (
            (in(input[pos], REGULAR_SYMBOLS, strlen(REGULAR_SYMBOLS))) ||
            (input[pos] == '[') ||
            (input[pos] == '.') ||
            (input[pos] == ')' && level > 0) ||
            (input[pos] == '\\')
        ) {
            // escape
            if (input[pos] == '\\') {
                if (!in(input[pos+1], ESCAPED_SYMBOLS, strlen(ESCAPED_SYMBOLS))) {
                    printf("ERROR: invalid escape sequence \\%c\n", input[pos+1]);
                    return 0;
                } else {
                    r_temp = new_single_regex(input[pos]);
                    regex_built = 1;
                }
            } else if (input[pos] == ')') {
                if (alternative[level] == 1) {
                    printf("ERROR: alternative not satisfied\n");
                    return 0;
                }
                
                if (elements[level] < 1) {
                    printf("ERROR: empty block\n");
                    return 0;
                }

                // concatenate all elements of the current level into one
                // erase the current level's element table
                // and store the concatenated element in r_temp
                for (int i = elements[level]-1; i > 0; i--) {
                    regex_concat(rs[level][i-1], rs[level][i]);
                }
                r_temp = rs[level][0];
                regex_built = 1;

                rs[level] = NULL;
                elements[level] = 0;

                level--;
            } else if (input[pos] == '.') {          
                // in this case, only two states are needed
                // state 0 has a transition to state 1 for every valid symbol
                r_temp = new_single_regex(ALL_SYMBOLS[0]);
                r_temp->states[0]->transitions =
                    realloc(r_temp->states[0]->transitions, strlen(ALL_SYMBOLS) * sizeof(transition*));
                for (int i = 1; i < strlen(ALL_SYMBOLS); i++) {
                    r_temp->states[0]->transitions[i] = new_transition(0, ALL_SYMBOLS[i], 1);
                }
                r_temp->states[0]->size = strlen(ALL_SYMBOLS);

                regex_built = 1;                
            } else if (input[pos] == '[') {
                int inverted = 0;
                if (input[pos+1] == '^') {
                    inverted = 1;
                    pos++;
                }

                if (input[pos++] == ']') {
                    printf("\nERROR: empty class not allowed\n");
                    return 0;
                }

                int nr_symbols = 0;
                char* symbols = NULL;

                while (input[pos] != ']') {
                    if (in(input[pos], "\n\0", 2)) {
                        printf("\nERROR: class not closed");
                        return 0;
                    }

                    // range
                    if (input[pos+1] == '-') {
                        if (
                            in(input[pos+1], "\n\0", 2) ||
                            in(input[pos+2], "\n\0", 2)
                        ) {
                            printf("\nERROR: class not closed");
                            return 0;
                        }
                        if (input[pos+2] == ']') {
                            printf("\nERROR: range must specify an end\n");
                            return 0;
                        } else if (!(
                            (input[pos] <= input[pos+2]) &&
                            (
                                (input[pos] >= 'a' && input[pos] <= 'z') ||
                                (input[pos] >= 'A' && input[pos] <= 'Z') ||
                                (input[pos] >= '0' && input[pos] <= '9')
                            )
                        )) {
                            printf("\nERROR: invalid range\n");
                            return 0;
                        } else {
                            for (char i = input[pos]; i <= input[pos+2]; i++) {
                                // TODO: ugly
                                if (symbols == NULL) {
                                    nr_symbols++;
                                    symbols = realloc(symbols, nr_symbols * sizeof(char));
                                    symbols[nr_symbols-1] = i;
                                } else if (!in(i, symbols, strlen(symbols))) {
                                    nr_symbols++;
                                    symbols = realloc(symbols, nr_symbols * sizeof(char));
                                    symbols[nr_symbols-1] = i;
                                }
                            }

                            pos += 3;
                        }
                    } 
                    // single symbol
                    else {
                        if (symbols == NULL) {
                            nr_symbols++;
                            symbols = realloc(symbols, nr_symbols * sizeof(char));
                            symbols[nr_symbols-1] = input[pos];
                        } else if (!in(input[pos], symbols, strlen(symbols))) {
                            nr_symbols++;
                            symbols = realloc(symbols, nr_symbols * sizeof(char));
                            symbols[nr_symbols-1] = input[pos];
                        }

                        pos++;
                    }
                }

                // like with ., only 2 states are needed for this part
                r_temp = new_single_regex(symbols[0]);
                r_temp->states[0]->transitions =
                    realloc(r_temp->states[0]->transitions, nr_symbols * sizeof(transition*));
                for (int i = 1; i < nr_symbols; i++) {
                    r_temp->states[0]->transitions[i] = new_transition(0, symbols[i], 1);
                }
                r_temp->states[0]->size = nr_symbols;
                regex_built = 1;

                free(symbols);
            } 

            // single symbol
            else {
                r_temp = new_single_regex(input[pos]);
                regex_built = 1;
            }

            // a?
            if (input[pos+1] == '?') {
                pos += 2;
                regex_optional(r_temp);
            } 

            // a{2,5}
            else if (input[pos+1] == '{') {
                char temp = input[pos];
                int min = 0;
                int max = 0;
                pos += 2;

                // read min number
                while (input[pos] != ',') {
                    if (in(input[pos], "0123456789", 10)) {
                        min = min * 10 + (input[pos++] -'0');
                    } else {
                        printf("\nERROR: %c is no number and cannot specify a range\n", input[pos]);
                        return 0;
                    }
                }

                pos++;

                // read max number
                while (input[pos] != '}') {
                    if (in(input[pos], "0123456789", 10)) {
                        max = max * 10 + (input[pos++] -'0');
                    } else {
                        printf("\nERROR: %c is no number and cannot specify a range\n", input[pos]);
                        return 0;
                    }
                }

                pos++;

                if (!(min >= 0 && max > 0 && min <= max)) {
                    printf("\nERROR: invalid range\n");
                    return 0;
                }
                

                // create max-1 copies to be concatenated behind r_temp
                regex** r2 = malloc((max-1) * sizeof(regex*));
                for(int i = 0; i < (max-1); i++ ) {
                    r2[i] = copy_regex(r_temp);
                }
                

                // fold the additional regexes into a single one
                for (int i = max-2; i >= 0; i--) {
                    if (i >= min-1) {
                        regex_optional(r2[i]);
                    }
                    if (i > 0) {
                        regex_concat(r2[i-1], r2[i]);
                    }
                }

                // concat the additional regexes to the original one
                regex_concat(r_temp, r2[0]);

                // in case {0,x} the whole construct may be repeated 0 times
                if (min == 0) {
                    regex_optional(r_temp);
                }
            }
            
            // TODO: remove redundant parts (next 4 for loops can be merged into one)

            // a* or a*?
            else if (input[pos+1] == '*') {
                regex_repeat(r_temp);

                // a*?
                if (input[pos+2] == '?') {
                    for (int i = 0; i < r_temp->size; i++) {
                        if (r_temp->states[i]->type == end || r_temp->states[i]->type == start_end) {
                            r_temp->states[i]->behavior = lazy;
                        }
                    }
                    pos += 3;
                } 
                
                // a*
                else {
                    for (int i = 0; i < r_temp->size;  i++) {
                        if (r_temp->states[i]->type == end || r_temp->states[i]->type == start_end) {
                            r_temp->states[i]->behavior = greedy;
                        }
                    }
                    pos += 2;
                }
            }
            
            // a+ or a+?
            else if (input[pos+1] == '+') {
                regex* r2 = copy_regex(r_temp);
                regex_repeat(r2);
                regex_concat(r_temp, r2);

                // a+?
                if (input[pos+2] == '?') {
                    for (int i = 0; i < r_temp->size; i++) {
                        if (r_temp->states[i]->type == end || r_temp->states[i]->type == start_end) {
                            r_temp->states[i]->behavior = lazy;
                        }
                    }
                    pos += 3;
                } 
                
                // a+
                else {
                    for (int i = 0; i < r_temp->size; i++) {
                        if (r_temp->states[i]->type == end || r_temp->states[i]->type == start_end) {
                            r_temp->states[i]->behavior = greedy;
                        }
                    }
                    pos += 2;
                }
            } 
            
            // a
            else {
                pos++;
            }
        }
        
        // ^
        else if (input[pos] == '^') {
            // must be the first symbol in a regex string
            if (pos != 0) {
                printf("ERROR: ^ can only be used as the first symbol of a regex string\n");
                return 0;
            } else {
                pos++;
                rs[0][0]->line_start = 1;
                continue;
            }
        }
        
        // $
        else if (input[pos] == '$') {
            // must be the last symbol in a regex string
            if (!in(input[pos+1], "\n\0", 2)) {
                printf("ERROR: $ can only be used as the last symbol of a regex string\n");
                return 0;
            } else {
                if (level == 0) {
                    rs[0][0]->line_end = 1;
                    return 1;
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
            elements[level] = 0;
        }

        // alternative
        else if (input[pos] == '|') {
            if (elements[level] == 0 || alternative[level] == 1) {
                printf("ERROR: invalid alternative\n");
                return 0;
            } else {
                alternative[level] = 1;
                pos++;
            }
        }

        // invalid symbol
        else {
            printf("ERROR: invalid symbol %c\n", input[pos]);
            return 0;
        }

        // if a value was assigned to r_temp: integrate it into the object list
        if (regex_built) {
            if (alternative[level]) {
                regex_alternative(rs[level][elements[level]-1], r_temp);
                alternative[level] = 0;
            } else {
                elements[level]++;
                rs[level] = realloc(rs[level], elements[level] * sizeof(regex*));
                rs[level][elements[level]-1] = r_temp;
            }
        }
    }

    if (!(level == 0 && alternative[level] == 0)) {
        if (level != 0) {
            printf("ERROR: %i blocks are not closed at the end of your expression\n", level);
            return 0;
        } else {
            printf("ERROR: alternative not satisfied\n");
            return 0;
        }
    } else {
        // concatenate all level 0 regex elements
        for (int i = elements[0]-1; i > 0; i--) {
            regex_concat(rs[0][i-1], rs[0][i]);
            rs[0][i] = NULL;
        }

        *r = rs[0][0];
        free(rs);
        return 1;
    }
}

//========== EPSILON FUNCTION ==========


static int remove_epsilon_transitions(regex* r) {
    // calculate epsilon closure for every state
    // visited is initialized with 0
    // and marked with 1 when visited
    int* ec_size = malloc(r->size * sizeof(int));
    int** ec = malloc(r->size * sizeof(int*));
    int* marked = malloc(r->size * sizeof(int));
    stack* s = new_stack(sizeof(int), NULL);

    for (int i = 0; i < r->size; i++) {
        ec_size[i] = 0;
    }

    // iterate over all states and calculate the epsilon closures
    for (int i = 0; i < r->size; i++) {
        for (int j = 0; j < r->size; j++) {
            marked[j] = 0;
        }
        // look if there's already a marked set of states from previous iterations
        if (ec_size[i] > 0) {
            for (int j = 0; j < ec_size[i]; j++) {
                stack_push(s, &(ec[i][j]));
                marked[j] = 1;
            }
        } else {
            stack_push(s, &i);
            marked[i] = 1;
        }
        
        // TODO: make more efficient by writing intermediate results
        // mark all reachable states
        int c;
        while (stack_pop(s, &c)) {
            // iterate over all transitions of state c
            for (int j = 0; j < r->states[c]->size; j++) {
                if (r->states[c]->transitions[j]->epsilon) {
                    if (!marked[r->states[c]->transitions[j]->next_state]) {
                        marked[r->states[c]->transitions[j]->next_state] = 1;
                        stack_push(s, &(r->states[c]->transitions[j]->next_state));
                    }
                }
            }
        }
        
        // put the reachable states into ec
        for (int j = 0; j < r->size; j++) {
            if (marked[j]) {
                ec[i] = realloc(ec[i], ++ec_size[i]);
                ec[i][ec_size[i]-1] = j;
            }
        }
    }
    delete_stack(&s);

    // first remove all epsilon transitions to save time
    for (int i = 0; i < r->size; i++) {
        for (int j = r->states[i]->size - 1; j >= 0; j--) {
            if (r->states[i]->transitions[j]->epsilon) {
                free(r->states[i]->transitions[j]);
                r->states[i]->transitions = realloc(r->states[i]->transitions, --(r->states[i]->size) * sizeof(transition*));
            }
        }
    }

    // make a list of all symbols the automaton knows
    int nr_symbols = 0;
    char* symbols;

    for (int i = 0; i < r->size; i++) {
        for (int j = 0; j < r->states[i]->size; j++) {
            if (nr_symbols == 0) {
                symbols = realloc(symbols, ++nr_symbols * sizeof(char));
                symbols[nr_symbols-1] = r->states[i]->transitions[j]->symbol;
            } else if (!in(r->states[i]->transitions[j]->symbol, symbols, nr_symbols)) {
                symbols = realloc(symbols, ++nr_symbols * sizeof(char));
                symbols[nr_symbols-1] = r->states[i]->transitions[j]->symbol;
            }
        }
    }
    
    // iterate over all states and write the new transitions
    for (int i = 0; i < r->size; i++) {

        // iterate over possible symbols
        for (int j = 0; j < nr_symbols; j++) {

            // mark all states as not reachable
            for (int k = 0; k < r->size; k++) {
                marked[k] = 0;
            }

            char symbol = symbols[j];

            // iterate over all states in the epsilon closure
            // mark their successor if they have a transition with the current symbol
            for (int k = 0; k < ec_size[i]; k++) {
                for (int l = 0; l < r->states[ec[i][k]]->size; l++) {
                    if (r->states[ec[i][k]]->transitions[l]->symbol == symbol) {
                        marked[r->states[ec[i][k]]->transitions[l]->next_state] = 1;
                    }
                }
            }

            // now all states directly reachable with symbol from i's epsilon closure
            // are marked -> mark the epsilon closure of those symbols
            for (int k = 0; k < r->size; k++) {
                if (marked[k]) {
                    for (int l = 0; l < ec_size[k]; l++) {
                        marked[ec[k][l]] = 1;
                    }
                }
            }

            // remove duplicates: iterate over the state's transitions
            for (int k = 0; k < r->states[i]->size; k++) {
                if (r->states[i]->transitions[k]->symbol == symbols[j] && marked[r->states[i]->transitions[k]->next_state]) {
                    marked[r->states[i]->transitions[k]->next_state] = 0;
                }
            }
            
            // now all states that can be reached with symbol are marked
            // add transitions for them
            for (int k = 0; k < r->size; k++) {
                if (marked[k]) {
                    r->states[i]->transitions = realloc(r->states[i]->transitions, ++(r->states[i]->size) * sizeof(transition*));
                    r->states[i]->transitions[r->states[i]->size-1] = new_transition(0, symbol, k);
                }
            }
            
        }

    }
    
    free(marked);
    free(ec_size);
    for (int i = 0; i < r->size; i++) {
        free(ec[i]);
    }
    free(ec);

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
    for (int i = 0; i < r->size; i++) {
        // iterate over transitions
        for (int j = 0; j < r->states[i]->size; j++) {
            // accumulate all symbols
            if (!in(r->states[i]->transitions[j]->symbol, symbols, nr_symbols)) {
                symbols = realloc(symbols, ++nr_symbols * sizeof(char));
                symbols[nr_symbols-1] = r->states[i]->transitions[j]->symbol;
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
    states[0] = new_state(0, none, (r->states[0]->type == start_end) ? start_end : start);

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

            // iterate over all old states that belong to the current combined state
            for (int j = 0; j < nr_states[state_pos]; j++) {
                int state_nr = state_sets[state_pos][j];

                // find all next states with the current symbol
                for (int k = 0; k < r->states[state_nr]->size; k++) {
                    if (r->states[state_nr]->transitions[k]->symbol == symbol) {
                        int already_there = 0;
                        // check if the next_state is already contained in next_states
                        for (int l = 0; l < nr_next_states; l++) {
                            if (r->states[state_nr]->transitions[k]->next_state == next_states[l]) {
                                already_there = 1;
                                break;
                            }
                        }
                        if (!already_there) {
                            next_states = realloc(next_states, ++nr_next_states * sizeof(int));
                            next_states[nr_next_states-1] = r->states[state_nr]->transitions[k]->next_state;
                            
                            // check if it is an end state -> must be marked in the new state
                            if (r->states[r->states[state_nr]->transitions[k]->next_state]->type == end || r->states[r->states[state_nr]->transitions[k]->next_state]->type == start_end) {
                                end_state_marker = 1;
                            }
                        }
                    }
                }
            }

            if (nr_next_states == 0) {
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
                // save the state with its partial states and create the corresponding transition
                state_sets = realloc(state_sets, ++nr_state_sets * (sizeof(int*)));
                state_sets[nr_state_sets-1] = malloc(nr_next_states * sizeof(int));
                for (int j = 0; j < nr_next_states; j++) {
                    state_sets[nr_state_sets-1][j] = next_states[j];
                }
                nr_states = realloc(nr_states, nr_state_sets * sizeof(int));
                nr_states[nr_state_sets-1] = nr_next_states;
                states = realloc(states, nr_state_sets * sizeof(state));
                
                states[nr_state_sets-1] = new_state(0, none, ((end_state_marker == 1) ? end : middle));
                
                exists = nr_state_sets - 1;

                // push the new state to the stack so it will be processed
                stack_push(s, &exists);
            }

            // now the current state can be linked to the created next_state
            states[state_pos]->transitions = realloc(states[state_pos]->transitions, ++(states[state_pos]->size) * sizeof(transition*));
            states[state_pos]->transitions[states[state_pos]->size-1] = new_transition(0, symbol, exists);

            free(next_states);
        }
    }

    // replace the old regex object with the new one
    for (int i = 0; i < r->size; i++) {
        free_state(r->states[i]);
        free(r->states[i]);
    }
    free(r->states);
    r->states = states;
    r->size = nr_state_sets;

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