#ifndef REGEX_H
#define REGEX_H

// clang-format off
#define DEBUG_MODE 1
#if DEBUG_MODE > 0
#define DEBUG(fmt, ...) fprintf(stdout, "[DEBUG] " fmt, ##__VA_ARGS__)
#else
#define DEBUG(fmt, ...) while(0) {};
#endif
#define ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt, ##__VA_ARGS__)
// clang-format on


/*
supported regular expression subset:
- a -> match letter a
- a* -> match zero or more occurrences of a (greedy)
- a*? -> match zero or more occurrences of a (lazy)
- a+ -> match at least one occurrence of a (greedy)
- a+? -> match at least one occurrence of a (lazy)
- a? -> match zero or one occurrences of a (greedy)
- a?? -> match zero or one occurrences of a (lazy)
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


typedef enum { ts_dead, ts_active, ts_epsilon } transition_status;

typedef struct {
    transition_status status;
    char symbol;
    int next_state;
} transition;


typedef enum { sb_none, sb_greedy, sb_lazy } state_behaviour;

typedef enum { st_start, st_middle, st_end, st_start_end } state_type;

typedef struct {
    int nr_transitions;
    state_behaviour behaviour;
    state_type type;
    transition** transitions;
} state;


typedef struct {
    int line_start;
    int line_end;
    int nr_states;
    state** states;
} regex;


//========== LIBRARY FUNCTIONS ==========


// compiles the input string to a regex structure
// returns 1 on success, 0 else
int regex_compile(regex** r, char* input);

// matches the previously compiled regex r against the input string
// returns the number of matches
// match positions are reported via the arrays locations and lengths
int regex_match(regex* r, char* input, int** locations, int** lengths);

// matches the previously compiled regex r against the input string
int regex_match_first(regex* r, char* input, int* location, int* length);

// compiles and matches the regular expression r similar to the regex_match
// function
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

// only one start_end state
regex* new_empty_regex();

// returns a malloced copy of the regex r
regex* copy_regex(regex* r);

// returns a malloced state instance with all elements set to 0
state*
new_state(int nr_transitions, state_behaviour behaviour, state_type type);

// returns a malloced transition
transition*
new_transition(transition_status status, char symbol, int next_state);

// concatenate b at the end of a
void regex_concat(regex* a, regex* b);

// expand a to implement a choice between itself and b
void regex_alternative(regex* a, regex* b);

// repeat the regex a 0 or more times
void regex_repeat(regex* a);

// repeat the regex a 0 or 1 times
void regex_optional(regex* a);

// mark the regex as lazy (accepting the smallest possible amount of chars)
void regex_make_lazy(regex* a);

// mark the regex as greedy (accepting as many chars as possible)
void regex_make_greedy(regex* a);


#endif