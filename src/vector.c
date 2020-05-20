#include "vector.h"
#include <stdlib.h>
#include <string.h>


//===== constructor


vector* new_vector(int type_size, void (*free_func)(void*)) {
    vector* v = (vector*)malloc(sizeof(vector));
    v->type_size = type_size;
    v->size = 0;
    v->content = NULL;
    v->free_func = free_func;
    return v;
}


//===== utility functions


static void vector_grow(vector* v) {
    v->content = realloc(v->content, ++(v->size) * v->type_size);
}


static void vector_shrink(vector* v) {
    v->content = realloc(v->content, --(v->size) * v->type_size);
}


//===== stack functions


int vector_push(vector* v, void* element) {
    vector_grow(v);
    memcpy((void*)(v->content + (v->size - 1) * v->type_size), element,
           v->type_size);
    return 1;
}


int vector_pop(vector* v, void* element) {
    if (!v->size) {
        return 0;
    }
    memcpy(element, (void*)(v->content + (v->size - 1) * v->type_size),
           v->type_size);
    if (v->free_func != NULL) {
        v->free_func((void*)v->content + (v->size - 1) * v->type_size);
    }
    vector_shrink(v);
    return 1;
}


//===== random access


int vector_get_at(vector* v, int pos, void* element) {
    if (v->size <= pos) {
        return 0;
    }
    memcpy(element, (void*)(v->content + pos * v->type_size), v->type_size);
    return 1;
}

int vector_set_at(vector* v, int pos, void* element) {
    if (v->size <= pos) {
        return 0;
    }
    memcpy((void*)(v->content + pos * v->type_size), element, v->type_size);
    return 1;
}

int vector_insert_at(vector* v, int pos, void* element) {
    if (v->size <= pos) {
        return 0;
    }
    vector_grow(v);

    // move all objects right of the insert position
    // one to the right to make space
    for (int i = v->size - 1; i > pos; i--) {
        memcpy((void*)(v->content + i * v->type_size),
               (void*)(v->content + (i - 1) * v->type_size), v->type_size);
    }

    memcpy((void*)(v->content + pos * v->type_size), element, v->type_size);
    return 1;
}


int vector_remove_at(vector* v, int pos) {
    if (v->size <= pos) {
        return 0;
    }
    if (v->free_func != NULL) {
        v->free_func((void*)v->content + pos * v->type_size);
    }

    // move all objects right of the hole one to the left
    for (int i = pos + 1; i < v->size; i++) {
        memcpy((void*)(v->content + (i - 1) * v->type_size),
               (void*)(v->content + i * v->type_size), v->type_size);
    }

    vector_shrink(v);
    return 1;
}


//===== iterator access

int vector_next(vector* v, void* element) {
    if (!(v->iterator < v->size)) {
        return 0;
    }
    memcpy(element, (void*)(v->content + v->iterator * v->type_size),
           v->type_size);
    v->iterator++;
    return 1;
}


int vector_reset_iterator(vector* v) { v->iterator = 0; }

int vector_move_iterator(vector* v) {
    if (v->iterator >= v->size) {
        return 0;
    }
    v->iterator++;
    return 1;
}


int vector_get(vector* v, void* element) {
    if (v->size <= v->iterator) {
        return 0;
    }
    memcpy(element, (void*)(v->content + v->iterator * v->type_size),
           v->type_size);
    return 1;
}

int vector_set(vector* v, void* element) {
    if (v->size <= v->iterator) {
        return 0;
    }
    memcpy((void*)(v->content + v->iterator * v->type_size), element,
           v->type_size);
    return 1;
}

int vector_insert(vector* v, void* element) {
    if (v->size <= v->iterator) {
        return 0;
    }
    vector_grow(v);

    // move all objects right of the insert position
    // one to the right to make space
    for (int i = v->size - 1; i > v->iterator; i--) {
        memcpy((void*)(v->content + i * v->type_size),
               (void*)(v->content + (i - 1) * v->type_size), v->type_size);
    }

    memcpy((void*)(v->content + v->iterator * v->type_size), element,
           v->type_size);
    return 1;
}


int vector_remove(vector* v) {
    if (v->size <= v->iterator) {
        return 0;
    }
    if (v->free_func != NULL) {
        v->free_func((void*)v->content + v->iterator * v->type_size);
    }

    // move all objects right of the hole one to the left
    for (int i = v->iterator + 1; i < v->size; i++) {
        memcpy((void*)(v->content + (i - 1) * v->type_size),
               (void*)(v->content + i * v->type_size), v->type_size);
    }

    vector_shrink(v);
    return 1;
}


//===== destructor


int delete_vector(vector** v) {
    if ((*v)->free_func != NULL) {
        while ((*v)->size) {
            (*v)->free_func((void*)(*v)->content +
                            ((*v)->size - 1) * (*v)->type_size);
        }
    }
    free(*v);
    *v = NULL;
    return 1;
}