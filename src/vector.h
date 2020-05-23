#ifndef VECTOR_H
#define VECTOR_H


/* A generic vector */


typedef struct {
    int type_size;
    int size;
    int iterator;
    void* content;
    void (*free_func)(void*);
} vector;


/* constructor: use like vector* v = new_vector(sizeof(type), NULL) */
vector* new_vector(int type_size, void (*free_func)(void*));
/* create a vector from an existing array and point *array to NULL */
vector* new_vector_from_array(int type_size,
                              void (*free_func)(void*),
                              void** array,
                              int size);
int delete_vector(vector** v);


int vector_push(vector* v, void* element);
int vector_pop(vector* v, void* element);
int vector_top(vector* v, void* element);


/* random access */
int vector_get_at(vector* v, int pos, void* element);
int vector_set_at(vector* v, int pos, void* element);
int vector_insert_at(vector* v, int pos, void* element);
int vector_remove_at(vector* v, int pos);


/* iterator access */
int vector_reset_iterator(vector* v);
int vector_move_iterator(vector* v);
int vector_next(vector* v, void* element);
int vector_get(vector* v, void* element);
int vector_set(vector* v, void* element);
int vector_insert(vector* v, void* element);
int vector_remove(vector* v);


/* vector to c array conversion; resets the vector; returns the content size */
int vector_extract(vector* v, void** array);


#endif