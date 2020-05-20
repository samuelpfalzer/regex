#ifndef VECTOR_H
#define VECTOR_H


// A generic stack implementation
// Create an instance with stack* s = new_stack(sizof(type), (NULL|function pointer));
// the second parameter takes a pointer to a function that frees an instance of the contained type
// if provided, every remaining element on the stack will be freed upon deletion


typedef struct {
    int type_size;
    int size;
    int iterator;
    void* content;
    void (*free_func)(void*);
} vector;


vector* new_vector(int type_size, void (*free_func)(void*));

int vector_push(vector* v, void* element);
int vector_pop(vector* v, void* element);

// needs a specified position
int vector_get_at(vector* v, int pos, void* element);
int vector_set_at(vector* v, int pos, void* element);
int vector_insert_at(vector* v, int pos, void* element);
int vector_remove_at(vector* v, int pos);

// uses the current iterator position
int vector_reset_iterator(vector* v);
int vector_move_iterator(vector* v);
int vector_get(vector* v, void* element);
int vector_set(vector* v, void* element);
int vector_insert(vector* v, void* element);
int vector_remove(vector* v);

int vector_next(vector* v, void* element);

int delete_vector(vector** v);


#endif