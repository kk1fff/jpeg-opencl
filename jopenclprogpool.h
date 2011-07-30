#pragma once
#include <CL/opencl.h>
#include <stdio.h>
#include "jpeglib.h"

struct j_opencl_prog_pool;

struct j_opencl_prog_pool * j_open_cl_prog_pool_create(j_decompress_ptr cinfo);

void j_opencl_prog_pool_destroy(struct j_opencl_prog_pool * );

cl_int j_opencl_prog_pool_get_idct(struct j_opencl_prog_pool *,cl_program * );

cl_int j_opencl_prog_pool_get_h2v1(struct j_opencl_prog_pool *,cl_program * );

cl_int j_opencl_prog_pool_get_h2v2(struct j_opencl_prog_pool *,cl_program * );

