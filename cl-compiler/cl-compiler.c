#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <CL/opencl.h>
#include <ReadFile.h>
#include <string.h>

static void print_error(const char * ,...);
static char * get_binary_file_name(const char * file_name);
static cl_int my_build_program(cl_program program,cl_device_id device_id);
static cl_int my_write_binary_to_file(cl_program program,const char * input_file_name);
#define LOG_SIZE (1 << 20)


int main(int argc,char ** argv)
{
    cl_context context;
    cl_device_id device_id;
    cl_program program;
    cl_platform_id platform_id;
    char * file_source;
    cl_int error_code;
    int ret_value;

    if(argc <= 1)
    {
        print_error("a cl source should be given\n");
        return 1;
    }
    
    if(!read_all_bytes(argv[1],&file_source))
    {
        print_error("Failed to read file\n");
        return 1;
    }
    ret_value = 1; //default error

    if(CL_SUCCESS != (error_code = clGetPlatformIDs(1,&platform_id,NULL)) )
    {
        print_error("Failed to get platform id ,with error code %d\n",error_code);
        goto GET_PLATFORM;
    }
    if(CL_SUCCESS != (error_code = clGetDeviceIDs(platform_id,CL_DEVICE_TYPE_GPU,1,&device_id,NULL)) )
    {
        print_error("Failed to get device id ,with error code %d\n",error_code);
        goto DEVICE_ID;
    }
    context = clCreateContext(NULL,1,&device_id,NULL,NULL,&error_code);
    if(error_code != CL_SUCCESS)
    {
        print_error("Failed to create context ,with error code %d\n",error_code);
        goto CREATE_CONTEXT;
    }

    program = clCreateProgramWithSource(context,1,&file_source,NULL,&error_code);
    if(error_code != CL_SUCCESS)
    {
        print_error("Failed to create program ,with error code %d\n",error_code);
        goto CREATE_PROGRAM;
    }
    free_all_bytes(file_source);
    file_source = NULL;

    if(CL_SUCCESS != my_build_program(program,device_id))
    {
        goto BUILD_PROGRAM;
    }

    if(CL_SUCCESS != my_write_binary_to_file(program,argv[1]))
    {
        goto WRITE_BINARY;
    }
    ret_value = 0;
WRITE_BINARY:
BUILD_PROGRAM:
    clReleaseProgram(program);
CREATE_PROGRAM:
    clReleaseContext(context);
CREATE_CONTEXT:
DEVICE_ID:
GET_PLATFORM:
    if(file_source)
    {
        free_all_bytes(file_source);
    }
    return ret_value;
}


void print_error(const char * format,...)
{
    va_list args;

    va_start(args,format);
    vfprintf(stderr,format,args);
    va_end(args);    
}


char * get_binary_file_name(const char * file_name)
{
    char * ret;
    const char * last_dot_position;
    size_t ret_file_name_size;
    
    last_dot_position = strrchr(file_name,'.');
    if(last_dot_position == NULL)
    {
        return strdup(file_name);
    }

    ret_file_name_size = (size_t)( last_dot_position - file_name);
    ret = malloc(ret_file_name_size + 5); // for a zero terminator and .clc
    if(!ret)
    {
        return NULL;
    }
    memcpy(ret,file_name,ret_file_name_size);
    memcpy(ret + ret_file_name_size,".clc",4);
    ret[ret_file_name_size + 4] = 0;
    return ret;
}


cl_int my_build_program(cl_program program,cl_device_id device_id)
{
    cl_int error_code;
    char * build_log;

    error_code = clBuildProgram(program,1,&device_id,NULL,NULL,NULL);
    build_log =  malloc(LOG_SIZE);
    if(build_log)
    {
        size_t log_size_ret;

        if(CL_SUCCESS == clGetProgramBuildInfo(program,device_id,CL_PROGRAM_BUILD_LOG,LOG_SIZE - 1, build_log, &log_size_ret))
        {
            build_log[log_size_ret] = 0;
            printf("BUILD LOG :\n%s\n",build_log);
        }
        free(build_log);
    }
    if(error_code != CL_SUCCESS)
    {
        print_error("Failed to build the program ,with error code %d\n",error_code);
    }
    return error_code;
}

static cl_int my_write_binary_to_file_handle(cl_program program,FILE * file)
{
    cl_int error_code;
    char * binary;
    size_t binary_size;

    error_code = clGetProgramInfo(program,CL_PROGRAM_BINARY_SIZES,sizeof(size_t),&binary_size,NULL);
    if(error_code != CL_SUCCESS)
    {
        print_error("Failed to get program's binary size ,with error code %d\n",error_code);
        return error_code;
    }
    binary = malloc(binary_size);
    if(!binary)
    {
        print_error("Failed to allocate memory for binary\n");
        return CL_OUT_OF_HOST_MEMORY;
    }
    memset(binary,0,binary_size);
    error_code = clGetProgramInfo(program,CL_PROGRAM_BINARIES,binary_size,binary,NULL);
    if(error_code != CL_SUCCESS)
    {
        print_error("Failed to get program's binary ,with error code %d\n",error_code);
        goto PROGRAM_BINARY;
    }
    if(1 != fwrite(binary,binary_size,1,file))
    {
        print_error("Failed to write to the file\n");
        error_code = CL_OUT_OF_RESOURCES;
        goto WRITE_FILE;
    }
    free(binary);
    return CL_SUCCESS;
WRITE_FILE:
PROGRAM_BINARY:
    free(binary);
    return error_code;
}   

cl_int my_write_binary_to_file(cl_program program,const char * input_file_name)
{
    cl_int error_code;
    char * output_file_name;
    FILE * output_file_handle;
    
    output_file_name = get_binary_file_name(input_file_name);
    if(!output_file_name)
    {
        print_error("Failed to allocate memory for output_file_name \n");

        return CL_OUT_OF_HOST_MEMORY;
    }
    output_file_handle = fopen(output_file_name,"wb");
    if(!output_file_handle)
    {
        free(output_file_name);
        print_error("Failed to open output file\n");

        return CL_OUT_OF_RESOURCES;
    }
    error_code = my_write_binary_to_file_handle(program,output_file_handle);
    fclose(output_file_handle);
    free(output_file_name);
    return error_code;
}
