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

typedef struct {
    int epsilon;
    char symbol;
    int next_state;
} transition;

typedef struct {
    int size;
    enum {lazy, greedy, none} behavior;
    enum {start, middle, end} type;
    transition** transitions;
} state;

typedef struct {
    int size;
    state** states;
} regex;


regex* new_regex();
void free_regex(regex** r);


// compile a string to a regex
// returns 1 on success, 0 else
// places the compiled regex at location r
int compile(regex **r, char *input);

// matches the given string to the regex
// returns number of matches found
// writes the matched locations and match lengths
int match(regex *r, char *input, int *locations, int *lengths);

void print_regex(regex* r);



regex* new_single_regex(char symbol);
void regex_concat(regex* a, regex* b);
void regex_repeat(regex* a);
void regex_maybe(regex* a);
void regex_alternative(regex* a, regex* b);