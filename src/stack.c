#include "stack.h"
#include <stdlib.h>
#include <string.h>


stack* new_stack(int type_size, void (*free_func)(void*)) {
    stack* s = (stack*)malloc(sizeof(stack));
    s->type_size = type_size;
    s->size = 0;
    s->content = NULL;
    s->free_func = free_func;
    return s;
}


int stack_push(stack* s, void* element) {
    s->content = realloc(s->content, ++(s->size) * s->type_size);
    memcpy((void*)(s->content + (s->size - 1) * s->type_size), element,
           s->type_size);
    return 1;
}


int stack_pop(stack* s, void* element) {
    if (!s->size) {
        return 0;
    }
    memcpy(element, (void*)(s->content + (s->size - 1) * s->type_size),
           s->type_size);
    if (s->free_func != NULL) {
        s->free_func((void*)s->content + (s->size - 1) * s->type_size);
    }
    s->content = realloc(s->content, --(s->size) * s->type_size);
    return 1;
}


int delete_stack(stack** s) {
    if ((*s)->free_func != NULL) {
        while ((*s)->size) {
            (*s)->free_func((void*)(*s)->content +
                            ((*s)->size - 1) * (*s)->type_size);
        }
    }
    free(*s);
    *s = NULL;
    return 1;
}