# regex
A regular expression matching engine based on finite automata written in C

## usage

### compiling
First, a regular expression must be compiled into the internal representation used for matching:
```C
regex* r = NULL;
int success = regex_compile(&r, "a|(ab)*");
```

### matching
Given a compiled regular expression `r`, the first occurrence in the null-terminated (or multiline, in that case only the first line is considered) input string `s` can be found with `regex_match_first()` *(more matching functions are soon to be implemented)*
```C
int position, length;
int success = regex_match_first(r, s, &position, &length);
```
`regex_match_first()` returns **1** on success, **0** else, so the result can be easily checked with `if (!success)`. If a match is found, the position of its first character and its length are returned via the reference parameters position and length.


## supported regular expression subset

Expression | Meaning
---------- | -------
**a** | match letter a
**a*** | match zero or more occurrences of a
**a+** | match at least one occurrence of a
**a?** | match zero or one occurrences of a
**a{2,5}** | match at least 2, but at most 5 occurrences of a
**a\|b** | match either a or b
**[abc0123]** | match any character given in the class
**[^abc03]** | match any character NOT given in the class *not yet implemented*
**[a-zA-F3-7]** | match all characters in the given ranges from a to z, A to F and 3 to 7. Can be combined with non-range classes and works for both classes and inverted classes
**.** | match any character
**(...)** | match the group inside the parentheses (all modifiers can be applied to a group)
**^** | match the beginning of a line *not yet implemented*
**$** | match the end of a line *not yet implemented*
**\\}** | match a literal **}** (applies for all control characters **()[]{}+-*?.**)
**\\\\** | match a literal **\\**

### greedy and lazy matching
**\***, **+** and **?** match greedyly (take as many characters as possible) by default, but lazily (take the smallest possible amount of characters) if appended with an additional **?**. Given an input string

```html
<div>content</div>
```
* the expression ` <.*> ` will match the whole line `<div>content</div>`
* but ` <.*?> ` will only match `<div>`

## build and run the example
```bash
> make
> ./bin/example 'regular expression' 'string to match against'
```
Either gcc must be installed or you have to change the compiler in the makefile