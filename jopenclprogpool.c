#include "jopenclprogpool.h"
#include <stdlib.h>
#include <string.h>
#include "ReadFile.h"

#define SAFE_RELEASE_PROGRAM(a)\
    if(a)\
    {\
        clReleaseProgram(a);\
        a = NULL;\
    }

#define GET_FUNC_HEADER(a)\
    ;

#define GENERATE_FUNC(file_name) \
    cl_int error_code; \
    j_decompress_ptr cinfo; \
\
    SAFE_RELEASE_PROGRAM(pool->current_prog);\
\
    cinfo = pool->cinfo; \
\
    error_code = create_with_file_name(cinfo,&pool->current_prog,file_name);\
    if(error_code == CL_SUCCESS)\
    {\
        *pprog = pool->current_prog;\
    }\
    else\
    {\
        *pprog = 0;\
    }\
    return error_code;
    

struct j_opencl_prog_pool
{
    j_decompress_ptr cinfo;
    cl_program current_prog;
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
    SAFE_RELEASE_PROGRAM(pool->current_prog);
    free(pool);
}

static cl_int create_with_file_name(j_decompress_ptr cinfo,cl_program * pprog,const char * file_name)
{
    cl_int error_code;
    int file_size;
    char* file_content;

    *pprog = NULL;
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
    GENERATE_FUNC("decode_idct.clc");
}

cl_int j_opencl_prog_pool_get_h2v1(struct j_opencl_prog_pool * pool,cl_program * pprog )
{
    GENERATE_FUNC("h2v1_fancy_upsample.clc");
}

cl_int j_opencl_prog_pool_get_h2v2(struct j_opencl_prog_pool * pool,cl_program * pprog )
{
    GENERATE_FUNC("h2v2_fancy_upsample.clc");
}

cl_int j_opencl_prog_pool_get_ycc_to_rgb(struct j_opencl_prog_pool * pool,cl_program * pprog )
{
    GENERATE_FUNC("ycc_to_rgb_convert.clc");
}
