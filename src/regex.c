#include "regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LEVELS 20

const char* REGULAR_SYMBOLS = "\"\'#/&=@!%abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const char* ESCAPED_SYMBOLS = "^$()[]{}\\*+?.|";
const char* ALL_SYMBOLS = "\"\'#/&=@!%abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789^$()[]{}\\*+?.|";

static int in(const char a, const char* b, int length) {
    for (int i = 0; i < length; i++) {
        if (b[i] == a) {
            return 1;
        }
    }
    return 0;
}


//========== COMPILING ==========

// helper functions
int parse(regex** r, char* input);
int make_epsilon_free(regex** r);
int minify(regex** r);

void free_regex(regex** r);
void free_state(state* s);

regex* new_regex();
regex* new_single_regex(char symbol);
regex* copy_regex(regex* r);

state* new_state();
transition* new_transition(int epsilon, char symbol, int next_state);

// concatenate b into a; erases b
void regex_concat(regex* a, regex* b);

// expand a to implement a choice between itself and b
void regex_alternative(regex* a, regex* b);

// build the kleen-* of a
void regex_repeat(regex* a);

// a or epsilon
void regex_maybe(regex* a);



int compile(regex** r, char* input) {
    int success = parse(r, input);
    //make_epsilon_free(r);
    //minify(r);

    return success;    
}


int parse(regex** r, char* input) {
    free_regex(r);

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
        for (int i = 0; i < level; i++) {
            printf("  ");
        }

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
                    printf("symbol %c ", input[++pos]);
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
                printf(") ");
            } else if (input[pos] == '.') {
                printf("any symbol ");
                
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
                    printf("inverted class ");
                    inverted = 1;
                    pos++;
                } else {
                   printf("class ");
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
                            printf("<symbols %c to %c> ", input[pos], input[pos+2]);

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
                        printf("<symbol %c> ", input[pos]);
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
                printf("symbol %c ", input[pos]);
                r_temp = new_single_regex(input[pos]);
                regex_built = 1;
            }

            // a?
            if (input[pos+1] == '?') {
                printf("(optional)\n");
                pos += 2;
                regex_maybe(r_temp);
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
                        regex_maybe(r2[i]);
                    }
                    if (i > 0) {
                        regex_concat(r2[i-1], r2[i]);
                    }
                }

                // concat the additional regexes to the original one
                regex_concat(r_temp, r2[0]);

                // in case {0,x} the whole construct may be repeated 0 times
                if (min == 0) {
                    regex_maybe(r_temp);
                }

                printf("(repeated %i to %i times)\n", min, max);
            }
            
            // TODO: remove redundant parts (next 4 for loops can be merged into one)

            // a* or a*?
            else if (input[pos+1] == '*') {
                regex_repeat(r_temp);

                // a*?
                if (input[pos+2] == '?') {
                    printf("(lazy * iteration)\n");
                    for (int i = 0; i < r_temp->size; i++) {
                        if (r_temp->states[i]->type == end) {
                            r_temp->states[i]->behavior = lazy;
                        }
                    }
                    pos += 3;
                } 
                
                // a*
                else {
                    printf("(greedy * iteration)\n");
                    for (int i = 0; i < r_temp->size;  i++) {
                        if (r_temp->states[i]->type == end) {
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
                    printf("(lazy + iteration)\n");
                    for (int i = 0; i < r_temp->size; i++) {
                        if (r_temp->states[i]->type == end) {
                            r_temp->states[i]->behavior = lazy;
                        }
                    }
                    pos += 3;
                } 
                
                // a+
                else {
                    printf("(greedy + iteration)\n");
                    for (int i = 0; i < r_temp->size; i++) {
                        if (r_temp->states[i]->type == end) {
                            r_temp->states[i]->behavior = greedy;
                        }
                    }
                    pos += 2;
                }
            } 
            
            // a
            else {
                printf("\n");
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
                printf("start of line symbol\n");
                pos++;
                // TODO: mark this in the state somehow?
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
                printf("end of line symbol\n");
                if (level == 0) {
                    printf("parse successful\n");
                    // TODO: mark this in the state somehow?
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
            printf("(\n");
            level++;
            elements[level] = 0;
        }

        // alternative
        else if (input[pos] == '|') {
            if (elements[level] == 0 || alternative[level] == 1) {
                printf("ERROR: invalid alternative\n");
                return 0;
            } else {
                printf(" or\n");
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
        printf("parse successful\n");

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


int make_epsilon_free(regex** r) {
    return 0;
}


int minify(regex** r) {
    return 0;
}


void free_regex(regex** r) {
    if ((*r) == NULL) {
        return;
    }

    for (int i = 0; i < (*r)->size; i++) {
        free_state((*r)->states[i]);
        free((*r)->states[i]);
    }
    
    free(*r);
    *r = NULL;
}


void free_state(state* s) {
    for (int i = 0; i < s->size; i++) {
        free(s->transitions[i]);
    }
}


regex* new_regex() {
    regex* r = malloc(sizeof(regex));
    r->size = 0;
    return r;
}


regex* new_single_regex(char symbol) {
    regex* r = malloc(sizeof(regex));
    r->size = 2;
    r->states = malloc(r->size * sizeof(state*));

    r->states[0] = new_state(1, none, start);
    r->states[0]->transitions[0] = new_transition(0, symbol, 1);
    
    r->states[1] = new_state(0, none, end);
    return r;
}


state* new_state(int size, int behavior, int type) {
    state* s = malloc(sizeof(state));
    s->size = size;
    s->behavior = behavior;
    s->type = type;
    s->transitions = malloc(s->size * sizeof(transition*));
    return s;
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
        if (s->type == end) {
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
    a->states = realloc(a->states, ++(a->size) * sizeof(state*));
    a->states[a->size-1] = new_state(0, none, end);
    a->states[0]->transitions = realloc(a->states[0]->transitions, ++(a->states[0]->size) * sizeof(transition*));
    a->states[0]->transitions[a->states[0]->size-1] = new_transition(1, ' ', a->size-1);

    // connect all end states to the start state
    for (int i = 0; i < a->size; i++) {
        if (a->states[i]->type == end) {
            a->states[i]->transitions = realloc(a->states[i]->transitions, ++(a->states[i]->size) * sizeof(transition*));
            a->states[i]->transitions[a->states[i]->size-1] = new_transition(1, ' ', 0);
        }
    }

}


void regex_maybe(regex* a) {
    // create one additional end state and connect the start state to it
    a->states = realloc(a->states, ++(a->size) * sizeof(state*));
    a->states[a->size-1] = new_state(0, none, end);
    a->states[0]->transitions = realloc(a->states[0]->transitions, ++(a->states[0]->size) * sizeof(transition*));
    a->states[0]->transitions[a->states[0]->size-1] = new_transition(1, ' ', a->size-1);
}


void print_regex(regex* r) {
    printf("regex; size: %i\n", r->size);
    for (int i = 0; i < r->size; i++) {
        state* s = r->states[i];
        printf(
            " - %i: %s; %i; %s\n",
            i,
            (s->type == start) ? "start" : ((s->type == end) ? "end" : "middle"),
            s->size,
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




//========== MATCHING ==========


// helper functions
int match_one(regex* r, char* c, int* length);



int match(regex* r, char* c, int* locations, int* lengths) {
    int pos = 0;
    int matches = 0;

    while (c[pos] != '\0' && c[pos != '\0']) {
        int match_length = 0;
        int success = match_one(r, &c[pos], &match_length);
        if (success) {
            matches++;

            // TODO: save pos and length for returning

            pos += match_length;
        } else {
            pos++;
        }
    }

    return matches;
}


int match_one(regex* r, char* c, int* length) {
    return 1;
}