#ifndef VECTOR_H
#define VECTOR_H


// A generic vector implementation
// Create an instance with vector* s = new_vector(sizof(type), (NULL|function
// pointer)); the second parameter takes a pointer to a function that frees an
// instance of the contained type if provided, every remaining element in the
// vector will be freed upon deletion


typedef struct {
    int type_size;
    int size;
    int iterator;
    void* content;
    void (*free_func)(void*);
} vector;

// constructors
vector* new_vector(int type_size, void (*free_func)(void*));
// create a vector from an existing array
// sets *array to NULL to guarantee exclusive control over the content
vector* new_vector_from_array(int type_size,
                              void (*free_func)(void*),
                              void** array,
                              int size);

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
int vector_next(vector* v, void* element);
int vector_get(vector* v, void* element);
int vector_set(vector* v, void* element);
int vector_insert(vector* v, void* element);
int vector_remove(vector* v);

// extract the array from a vector and reset the vector
// sets *array to the content address and returns the content size
int vector_extract(vector* v, void** array);

// destructor
int delete_vector(vector** v);


#endif