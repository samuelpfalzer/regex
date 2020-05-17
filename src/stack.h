#ifndef STACK_H
#define STACK_H


typedef struct {
    int type_size;
    int size;
    void* content;
    void (*free_func)(void*);
} stack;


stack* new_stack(int type_size, void (*free_func)(void*));
int stack_push(stack* s, void* element_ptr);
int stack_pop(stack* s, void* element_ptr);
int delete_stack(stack** s);


#endif