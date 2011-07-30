#include "jopenclstore.h"
#include <string.h>
#include <stdlib.h>
#define MAX_ELEMENT_COUNT (5)
#define MAX_BUFFER_COUNT (5)

#define ELE_FROM_STATE(store ,state , failed_action )\
    struct j_opencl_store_element * element;\
    if(state >= MAX_ELEMENT_COUNT) \
    { \
        failed_action ; \
    } \
    element = &store->elements[state];



struct j_opencl_store_element
{
    void * data;
    pfn_opencl_store_free_data free_fun;
    cl_mem  buffers[MAX_BUFFER_COUNT];
    int buffer_index;
};
struct j_opencl_store
{
    struct j_opencl_store_element elements[MAX_ELEMENT_COUNT];
};

struct j_opencl_store * j_opencl_store_create(void)
{
    struct j_opencl_store * store;
    store = malloc(sizeof(struct j_opencl_store));
    if(store)
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
        struct j_opencl_store_element * element;

        element =  &store->elements[i];
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
    }
    free(store);
}

int j_opencl_store_set_state_data(struct j_opencl_store * store , size_t state,void * data,pfn_opencl_store_free_data free_fun)
{
    ELE_FROM_STATE(store,state,return 1  );
    
    element->data = data;
    element->free_fun = free_fun;
    return 0;
}

void * j_opencl_store_get_state_data(struct j_opencl_store * store,size_t state)
{
    ELE_FROM_STATE(store,state,return NULL  );
    return element->data;
}

int j_opencl_store_append_buffer(struct j_opencl_store * store,size_t state,cl_mem buffer)
{
    ELE_FROM_STATE(store,state,return 1  );
    if(element->buffer_index >= MAX_BUFFER_COUNT)
    {
        return 1;
    }
    element->buffers[element->buffer_index ++ ] = buffer;
    return 0;
}

cl_mem j_opencl_store_get_buffer(struct j_opencl_store * store,size_t state,int index)
{
    ELE_FROM_STATE(store,state,return 0 );
    if(index >= element->buffer_index)
    {
        return 0;
    }
    return element->buffers[index];
}
