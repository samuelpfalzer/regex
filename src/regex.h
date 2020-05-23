#ifndef REGEX_H
#define REGEX_H

// clang-format off
#define ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt, ##__VA_ARGS__)
// clang-format on


// avoid conflicts with the literal use of ^ and $
#define LINE_START 2
#define LINE_END 3


/* TYPES */


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


/* PUBLIC FUNCTIONS */


/* compiles the input string to an internal regex representation and stores it
 * in r; *r must point to NULL or a dynamically allocated value; function
 * returns 1 on success, 0 on error */
int regex_compile(regex** r, char* input);

/* matches the previously compiled regex r against the input string */
int regex_match_first(regex* r, char* input, int* location, int* length);


/* UTILITY FUNCTIONS */


/* regex constructors */
regex* new_empty_regex();
regex* new_single_transition_regex(char symbol);
regex* new_single_state_regex();
regex* copy_regex(regex* r);
/* free a regex object and all its elements recursively, set *r to NULL */
void delete_regex(regex** r);


/* state constructor */
state*
new_state(int nr_transitions, state_behaviour behaviour, state_type type);
/* free a state and all its elements recursively */
void free_state(state* s);


/* transition constructor */
transition*
new_transition(transition_status status, char symbol, int next_state);


/* chain b after a */
void regex_chain(regex* a, regex** b);
void regex_alternative(regex* a, regex** b);
/* 0-n repetitions */
void regex_repeat(regex* a);
/* 0-1 repetitions */
void regex_optional(regex* a);
/* mark all end states of a as lazy */
void regex_make_lazy(regex* a);
/* mark all end states of a as greedy */
void regex_make_greedy(regex* a);


/* print a compiled regex to the terminal */
void print_regex(regex* r);

#endif
