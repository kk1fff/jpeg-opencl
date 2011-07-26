#include "jopenclstore.h"
#include <stdlib.h>
#include <string.h>
#define MAX_ELEMENT_COUNT (5)

struct j_opencl_store_element
{
    cl_program program;
    void * data;
    pfn_opencl_store_free_data free_fun;
};

struct j_opencl_store
{
    j_opencl_store_element elements[MAX_ELEMENT_COUNT];
};

struct j_opencl_store * j_opencl_store_create(void)
{
    struct j_opencl_store * store;

    store = malloc(sizeof(struct j_opencl_store));
    if(!store)
    {
        memset(store , 0 , sizeof(struct j_opencl_store));
    }
    return store;
}

void j_opencl_store_destroy(struct j_opencl_store * store)
{
    int i ;
    for ( i = 0 ; i < MAX_ELEMENT_COUNT ; ++i)
    {
        j_opencl_store_element * element;

        element =  &store->elements[i];

        if(element->program)
        {
            clReleaseProgram(element->program);
        }
        if(element->data && element->free_fun)
        {
            element->free_fun(element->data);
        }
    }
    free(store);
}

int j_opencl_store_set_program(struct j_opencl_store * store,size_t state,cl_program program)
{
    struct j_opencl_store_element * element;
    if(state >= MAX_ELEMENT_COUNT)
    {
        return 1;
    }
    element = &store->elements[state];
    if(element->program)
    {
        // not support to set program more than once
        return 1;
    }
    
    element->program = program;
    return 0;
}
        

cl_program j_opencl_store_get_program(struct j_opencl_store * store,size_t state)
{
    return store->element[state].program;
}

int j_opencl_store_set_state_data(struct j_opencl_store * store , size_t state,void * data,pfn_opencl_store_free_data free_fun)
{
    j_opencl_store_element * element;
    
    if(state >= MAX_ELEMENT_COUNT)
    {
        return  1;
    }
    element = &store->elements[state];
    
    element->data = data;
    element->free_fun = free_fun;
    return 0;
}

void * j_opencl_store_get_state_data(struct j_opencl_store * store,size_t state)
{
    return store->elements[state].data;
}

