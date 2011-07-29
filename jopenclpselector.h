#pragma once
#include <cl/opencl.h>
#include "jpeglib.h"

int j_opencl_pselector_select(j_decompress_ptr cinfo,cl_program * program_buffer,size_t * count);
