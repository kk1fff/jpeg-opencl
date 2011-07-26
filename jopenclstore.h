#pragma once
#include <cl/cl.h>

typedef void (* pfn_opencl_store_free_data)(void *);
struct j_opencl_store;

struct j_opencl_store * j_opencl_store_create(void);

void j_opencl_store_destroy(struct j_opencl_store * );

int j_opencl_store_set_program(struct j_opencl_store * store,size_t state,cl_program program);
        

cl_program j_opencl_store_get_program(struct j_opencl_store * store,size_t state);

int j_opencl_store_set_state_data(struct j_opencl_store * store , size_t state,void * data,pfn_opencl_store_free_data free_fun);

void * j_opencl_store_get_state_data(struct j_opencl_store * store,size_t state);

