#pragma once
#include <cl/opencl.h>

typedef void (* pfn_opencl_store_free_data)(void *);
struct j_opencl_store;

struct j_opencl_store * j_opencl_store_create(void);

void j_opencl_store_destroy(struct j_opencl_store * );

int j_opencl_store_is_empty(struct j_opencl_store * );

int j_opencl_store_set_data(struct j_opencl_store * store , void * data,pfn_opencl_store_free_data free_fun);

void * j_opencl_store_get_data(struct j_opencl_store * store);

int j_opencl_store_append_buffer(struct j_opencl_store * store,cl_mem buffer);

cl_mem j_opencl_store_get_buffer(struct j_opencl_store * store,int index);

int j_opencl_store_new_session(struct j_opencl_store * store);

int j_opencl_store_pop_session(struct j_opencl_store * store);
