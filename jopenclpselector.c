#include "jopenclpselector.h"
#include "ReadFile.h"

static int create_idct(j_decompress_ptr cinfo,cl_program * program);
static int create_upsample(j_decompress_ptr cinfo,cl_program * program);

int j_opencl_pselector_select(j_decompress_ptr cinfo,cl_program * program_buffer,size_t * count)
{
    int error_code;

    error_code = create_idct(cinfo,program_buffer);
    if(error_code != CL_SUCCESS)
    {
        goto IDCT;
    }
    error_code = create_upsample(cinfo ,program_buffer + 1);
    if(error_code != CL_SUCCESS)
    {
        goto UPSAMPLE;
    }
    *count = 1 + cinfo->comps_in_scan;
    return CL_SUCCESS;
UPSAMPLE:
    clReleaseProgram(program_buffer[0]);
IDCT:
    *count = 0U;
    return error_code;
}


static int create_with_file_name(j_decompress_ptr cinfo,cl_program * program,const char * file_name)
{
    int bytes;
    void * file_content;
    int error_code;

    bytes = read_all_bytes(file_name,&file_content);
    if(!bytes)
    {
        return CL_OUT_OF_RESOURCES;
    }
    *program = clCreateProgramWithBinary(cinfo->current_cl_context,1,&cinfo->current_device_id,&bytes
                ,&file_content,NULL,&error_code);
    free(file_content);
    return error_code;
}

int create_idct(j_decompress_ptr cinfo,cl_program * program)
{
    return create_with_file_name(cinfo,program,"decode_idct.clc");
}


int create_upsample(j_decompress_ptr cinfo,cl_program * program)
{
    const char * file_name;
    jpeg_component_info * compptr;
    int ci;

    for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
            ci++, compptr++) {
        int h_in_group, v_in_group, h_out_group, v_out_group;
        /* Compute size of an "input group" after IDCT scaling.  This many samples
         * are to be converted to max_h_samp_factor * max_v_samp_factor pixels.
         */
        h_in_group = (compptr->h_samp_factor * compptr->DCT_scaled_size) /
            cinfo->min_DCT_scaled_size;
        v_in_group = (compptr->v_samp_factor * compptr->DCT_scaled_size) /
            cinfo->min_DCT_scaled_size;
        h_out_group = cinfo->max_h_samp_factor;
        v_out_group = cinfo->max_v_samp_factor;
        upsample->rowgroup_height[ci] = v_in_group; /* save for use later */
        need_buffer = TRUE;
        if (! compptr->component_needed) {
            /* Don't bother to upsample an uninteresting component. */
            upsample->methods[ci] = noop_upsample;
        } else if (h_in_group == h_out_group && v_in_group == v_out_group) {
            /* Fullsize components can be processed without any work. */
            upsample->methods[ci] = fullsize_upsample;
        } else if (h_in_group * 2 == h_out_group &&
                v_in_group == v_out_group) {
            /* Special cases for 2h1v upsampling */
            upsample->methods[ci] = h2v1_fancy_upsample;
        } else if (h_in_group * 2 == h_out_group &&
                v_in_group * 2 == v_out_group) {
            /* Special cases for 2h2v upsampling */
            upsample->methods[ci] = h2v2_fancy_upsample;
        } else if ((h_out_group % h_in_group) == 0 &&
                (v_out_group % v_in_group) == 0) {
            /* Generic integral-factors upsampling method */
            upsample->methods[ci] = int_upsample;
        } else
            ERREXIT(cinfo, JERR_FRACT_SAMPLE_NOTIMPL);
    }
}
