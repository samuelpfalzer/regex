#ifndef REGEX_H
#define REGEX_H

/*

supported regular expression subset:
- a -> match letter a
- a* -> match zero or more occurrences of a (gready)
- a*? -> match zero or more occurrences of a (lazy)
- a+ -> match at least one occurrence of a
- a+? -> match at least one occurrence of a (lazy)
- a? -> match zero or one occurrences of a
- a{2,5} -> match at least 2, but at most 5 occurrences of a
- a|b -> match a or b
- [abc03] -> match any character given in the class
- [^abc03] -> match any character NOT given in the class
- . -> match any character
- () -> match a group (all other modifiers can be applied to a group)
- ^ -> match the beginning of a line
- $ -> match the end of a line
- \( -> match a literal ( (applies for all special characters)
- \\ -> match a literal \

*/


//========== TYPE DEFINITIONS ==========



typedef struct {
    int epsilon;
    char symbol;
    int next_state;
} transition;



typedef struct {
    int size;
    enum {lazy, greedy, none} behavior;
    enum {start, middle, end, start_end} type;
    transition** transitions;
} state;



typedef struct {
    int line_start;
    int line_end;
    int size;
    state** states;
} regex;



//========== FUNCTION DECLARATIONS ==========

// compiles the input string to a regex structure
// returns 1 on success, 0 else
int regex_compile(regex** r, char* input);

// matches the previously compiled regex r against the input string
// returns the number of matches
// match positions are reported via the arrays locations and lengths
int regex_match(regex* r, char* input, int** locations, int** lengths);

// matches the previously compiled regex r against the input string
int regex_match_first(regex* r, char* input, int* location, int* length);

// compiles and matches the regular expression r similar to the regex_match function
int regex_compile_match(char* r, char* c, int* locations, int* lengths);

// prints a compiled regex to the terminal
void print_regex(regex* r);



// ========== UTILITY FUNCTIONS ============


// free a regex and all its elements recursively
void free_regex(regex** r);

// free a state and all its elements recursively
void free_state(state* s);

// returns a new malloced regex instance with all elements set to 0
regex* new_regex();

// returns a new malloced regex instance with a single transition
regex* new_single_regex(char symbol);

// returns a malloced copy of the regex r
regex* copy_regex(regex* r);

// returns a malloced state instance with all elements set to 0
state* new_state(int size, int behavior, int type);

// returns a malloced transition
transition* new_transition(int epsilon, char symbol, int next_state);

// concatenate b at the end of a
void regex_concat(regex* a, regex* b);

// expand a to implement a choice between itself and b
void regex_alternative(regex* a, regex* b);

// repeat the regex a 0 or more times
void regex_repeat(regex* a);

// repeat the regex a 0 or 1 times
void regex_optional(regex* a);



#endif