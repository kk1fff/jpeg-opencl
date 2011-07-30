#include "jopenclprogpool.h"
#include <stdlib.h>
#include <string.h>
#include "ReadFile.h"

#define SAFE_RELEASE_PROGRAM(a)\
    if(a)\
    {\
        clReleaseProgram(a);\
    }

#define GET_FUNC_HEADER(a)\
    ;

#define GENERATE_FUNC(func_name,file_name) \
    cl_int error_code; \
    j_decompress_ptr cinfo; \
\
    if(pool->func_name) \
    {\
        *pprog = pool->func_name; \
        return CL_SUCCESS; \
    }\
\
    cinfo = pool->cinfo; \
\
    error_code = create_with_file_name(cinfo,&pool->func_name,file_name);\
    if(error_code == CL_SUCCESS)\
    {\
        *pprog = pool->func_name;\
    }\
    else\
    {\
        *pprog = 0;\
    }\
    return error_code;
    

struct j_opencl_prog_pool
{
    j_decompress_ptr cinfo;
    cl_program idct;
    cl_program h2v1;
    cl_program h2v2;
};

struct j_opencl_prog_pool * j_opencl_prog_pool_create(j_decompress_ptr cinfo)
{
    struct j_opencl_prog_pool * pool;

    pool = (struct j_opencl_prog_pool*)malloc(sizeof(struct j_opencl_prog_pool));
    if(pool)
    {
        memset(pool,0,sizeof(struct j_opencl_prog_pool));
        pool->cinfo = cinfo;
    }
    return pool;
}   

void j_opencl_prog_pool_destroy(struct j_opencl_prog_pool * pool)
{
    SAFE_RELEASE_PROGRAM(pool->idct);
    SAFE_RELEASE_PROGRAM(pool->h2v1);
    SAFE_RELEASE_PROGRAM(pool->h2v2);
    free(pool);
}

static cl_int create_with_file_name(j_decompress_ptr cinfo,cl_program * pprog,const char * file_name)
{
    cl_int error_code;
    int file_size;
    char* file_content;

    file_size = read_all_bytes(file_name,&file_content);
    if(!file_size)
    {
        return CL_OUT_OF_RESOURCES;
    }

    *pprog = clCreateProgramWithBinary(cinfo->current_cl_context,1,&cinfo->current_device_id,&file_size
                ,&file_content,NULL,&error_code);
    free_all_bytes(file_content);
    if(error_code == CL_SUCCESS)
    {
        error_code = clBuildProgram(*pprog,1,&cinfo->current_device_id,NULL,NULL,NULL);
    }
    return error_code;
}

cl_int j_opencl_prog_pool_get_idct(struct j_opencl_prog_pool * pool,cl_program * pprog )
{
    GENERATE_FUNC(idct,"decode_idct.clc");
}

cl_int j_opencl_prog_pool_get_h2v1(struct j_opencl_prog_pool * pool,cl_program * pprog )
{
    GENERATE_FUNC(h2v1,"h2v1_fancy_upsample.clc");
}

cl_int j_opencl_prog_pool_get_h2v2(struct j_opencl_prog_pool * pool,cl_program * pprog )
{
    GENERATE_FUNC(idct,"h2v2_fancy_upsample.clc");
}

