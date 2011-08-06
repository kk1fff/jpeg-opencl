#include "jopenclstore.h"
#include <string.h>
#include <stdlib.h>
#define MAX_ELEMENT_COUNT (5)
#define MAX_BUFFER_COUNT (5)

#define ELE_FROM_STATE(store)\
    struct j_opencl_store_element * element;\
    element = store->tail;



struct j_opencl_store_element
{
    void * data;
    pfn_opencl_store_free_data free_fun;
    cl_mem  buffers[MAX_BUFFER_COUNT];
    int buffer_index;
    struct j_opencl_store_element * next;
};

struct j_opencl_store
{
    struct j_opencl_store_element * elements;
    struct j_opencl_store_element * tail;
};

struct j_opencl_store * j_opencl_store_create(void)
{
    struct j_opencl_store * store;
    store = malloc(sizeof(struct j_opencl_store));
    if(store)
    {
        store->elements = NULL;
    }
    return store;
}

void j_opencl_store_destroy(struct j_opencl_store * store)
{
    while(!j_opencl_store_is_empty(store))
    {
        j_opencl_store_pop_session(store);
    }
    free(store);
}

int j_opencl_store_set_data(struct j_opencl_store * store ,void * data,pfn_opencl_store_free_data free_fun)
{
    ELE_FROM_STATE(store  );
    
    element->data = data;
    element->free_fun = free_fun;
    return 0;
}

void * j_opencl_store_get_data(struct j_opencl_store * store)
{
    ELE_FROM_STATE(store);
    return element->data;
}

int j_opencl_store_append_buffer(struct j_opencl_store * store,cl_mem buffer)
{
    ELE_FROM_STATE(store);
    if(element->buffer_index >= MAX_BUFFER_COUNT)
    {
        return 1;
    }
    element->buffers[element->buffer_index ++ ] = buffer;
    return 0;
}

cl_mem j_opencl_store_get_buffer(struct j_opencl_store * store,int index)
{
    ELE_FROM_STATE(store);
    if(index >= element->buffer_index)
    {
        return 0;
    }
    return element->buffers[index];
}


int j_opencl_store_new_session(struct j_opencl_store * store)
{
    struct j_opencl_store_element * element;

    element = malloc(sizeof ( struct j_opencl_store_element));
    if(!element)
    {
        return 1;
    }
    memset(element,0,sizeof(struct j_opencl_store_element));
    if(!store->elements)
    {
        // in this case store must have zero element
        store->elements = element;
        store->tail = element;
    }
    else
    {
        store->tail->next = element;
    }
    return 0;
}

int j_opencl_store_pop_session(struct j_opencl_store * store)
{
    struct j_opencl_store_element * element;

    element = store->elements;
    if(!element)
    {
        return 1; // free nothing
    }

    if(element->data && element->free_fun)
    {
        element->free_fun(element->data);
    }
    if(element->buffer_index)
    {
        // there is buffer object alive
        int i;
        for( i = 0 ; i < element->buffer_index; ++i)
        {
            clReleaseMemObject(element->buffers[i]);
        }
    }
    store->elements = element->next;
    free(element);
    if(store->elements == NULL)
    {
        // we have free every element including tail
        store->tail = NULL;
    }
    return 0;
}


int j_opencl_store_is_empty(struct j_opencl_store * store)
{
    return store->elements == NULL;
}
