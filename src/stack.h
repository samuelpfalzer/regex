#ifndef STACK_H
#define STACK_H


// A generic stack implementation
// Create an instance with stack* s = new_stack(sizof(type), (NULL|function pointer));
// the second parameter takes a pointer to a function that frees an instance of the contained type
// if provided, every remaining element on the stack will be freed upon deletion


typedef struct {
    int type_size;
    int size;
    void* content;
    void (*free_func)(void*);
} stack;


stack* new_stack(int type_size, void (*free_func)(void*));
int stack_push(stack* s, void* element);
int stack_pop(stack* s, void* element);
int delete_stack(stack** s);


#endif