/*
 * jdcoefct.c
 *
 * Copyright (C) 1994-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains the coefficient buffer controller for decompression.
 * This controller is the top level of the JPEG decompressor proper.
 * The coefficient buffer lies between entropy decoding and inverse-DCT steps.
 *
 * In buffered-image mode, this controller is the interface between
 * input-oriented processing and output-oriented processing.
 * Also, the input side (only) is used when reading a file for transcoding.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"

/* Block smoothing is only applicable for progressive JPEG, so: */
#ifndef D_PROGRESSIVE_SUPPORTED
#undef BLOCK_SMOOTHING_SUPPORTED
#endif

/* Private buffer controller object */

typedef struct {
    struct jpeg_d_coef_controller pub; /* public fields */

    /* These variables keep track of the current location of the input side. */
    /* cinfo->input_iMCU_row is also used for this. */
    JDIMENSION MCU_ctr;		/* counts MCUs processed in current row */
    int MCU_vert_offset;		/* counts MCU rows within iMCU row */
    int MCU_rows_per_iMCU_row;	/* number of such rows needed */

    /* The output side's location is represented by cinfo->output_iMCU_row. */

    /* In single-pass modes, it's sufficient to buffer just one MCU.
     * We allocate a workspace of D_MAX_BLOCKS_IN_MCU coefficient blocks,
     * and let the entropy decoder write into that workspace each time.
     * (On 80x86, the workspace is FAR even though it's not really very big;
     * this is to keep the module interfaces unchanged when a large coefficient
     * buffer is necessary.)
     * In multi-pass modes, this array points to the current MCU's blocks
     * within the virtual arrays; it is used only by the input side.
     */
    JBLOCKROW MCU_buffer[D_MAX_BLOCKS_IN_MCU];

#ifdef D_MULTISCAN_FILES_SUPPORTED
    /* In multi-pass modes, we need a virtual block array for each component. */
    jvirt_barray_ptr whole_image[MAX_COMPONENTS];
#endif

#ifdef BLOCK_SMOOTHING_SUPPORTED
    /* When doing block smoothing, we latch coefficient Al values here */
    int * coef_bits_latch;
#define SAVED_COEFS  6		/* we save coef_bits[0..5] */
#endif
} my_coef_controller;

typedef my_coef_controller * my_coef_ptr;

/* Forward declarations */
METHODDEF(int) decompress_onepass
JPP((j_decompress_ptr cinfo, JSAMPIMAGE output_buf));
#ifdef D_MULTISCAN_FILES_SUPPORTED
METHODDEF(int) decompress_data
JPP((j_decompress_ptr cinfo, JSAMPIMAGE output_buf));
#endif
#ifdef BLOCK_SMOOTHING_SUPPORTED
LOCAL(boolean) smoothing_ok JPP((j_decompress_ptr cinfo));
METHODDEF(int) decompress_smooth_data
JPP((j_decompress_ptr cinfo, JSAMPIMAGE output_buf));
#endif


    LOCAL(void)
start_iMCU_row (j_decompress_ptr cinfo)
    /* Reset within-iMCU-row counters for a new row (input side) */
{
    my_coef_ptr coef = (my_coef_ptr) cinfo->coef;

    /* In an interleaved scan, an MCU row is the same as an iMCU row.
     * In a noninterleaved scan, an iMCU row has v_samp_factor MCU rows.
     * But at the bottom of the image, process only what's left.
     */
    if (cinfo->comps_in_scan > 1) {
        coef->MCU_rows_per_iMCU_row = 1;
    } else {
        if (cinfo->input_iMCU_row < (cinfo->total_iMCU_rows-1))
            coef->MCU_rows_per_iMCU_row = cinfo->cur_comp_info[0]->v_samp_factor;
        else
            coef->MCU_rows_per_iMCU_row = cinfo->cur_comp_info[0]->last_row_height;
    }

    coef->MCU_ctr = 0;
    coef->MCU_vert_offset = 0;
}


/*
 * Initialize for an input processing pass.
 */

    METHODDEF(void)
start_input_pass (j_decompress_ptr cinfo)
{
    cinfo->input_iMCU_row = 0;
    start_iMCU_row(cinfo);
}


/*
 * Initialize for an output processing pass.
 */

    METHODDEF(void)
start_output_pass (j_decompress_ptr cinfo)
{
#ifdef BLOCK_SMOOTHING_SUPPORTED
    my_coef_ptr coef = (my_coef_ptr) cinfo->coef;

    /* If multipass, check to see whether to use block smoothing on this pass */
    if (coef->pub.coef_arrays != NULL) {
        if (cinfo->do_block_smoothing && smoothing_ok(cinfo))
            coef->pub.decompress_data = decompress_smooth_data;
        else
            coef->pub.decompress_data = decompress_data;
    }
#endif
    cinfo->output_iMCU_row = 0;
}


    METHODDEF(int)
decompress_onepass2 (j_decompress_ptr cinfo, JSAMPIMAGE output_buf);
/*
 * Decompress and return some data in the single-pass case.
 * Always attempts to emit one fully interleaved MCU row ("iMCU" row).
 * Input and output must run in lockstep since we have only a one-MCU buffer.
 * Return value is JPEG_ROW_COMPLETED, JPEG_SCAN_COMPLETED, or JPEG_SUSPENDED.
 *
 * NB: output_buf contains a plane for each component in image,
 * which we index according to the component's SOF position.
 */

    METHODDEF(int)
decompress_onepass (j_decompress_ptr cinfo, JSAMPIMAGE output_buf)
{
    my_coef_ptr coef = (my_coef_ptr) cinfo->coef;
    JDIMENSION MCU_col_num;	/* index of current MCU within row */
    JDIMENSION last_MCU_col = cinfo->MCUs_per_row - 1;
    int yoffset;
    int rows;

    cinfo->decoded_mcus_base = cinfo->mem->alloc_large((j_common_ptr)cinfo,JPOOL_IMAGE,
            sizeof(JBLOCK) * cinfo->blocks_in_MCU 
            * coef->MCU_rows_per_iMCU_row * cinfo->MCUs_per_row * cinfo->MCU_rows_in_scan);
    if(!cinfo->decoded_mcus_base)
    {
        return JPEG_SUSPENDED;
    }
    cinfo->decoded_mcus_current = cinfo->decoded_mcus_base;
    for(rows = 0 ; rows < cinfo->MCU_rows_in_scan ; ++ rows)
    {
        for (yoffset = coef->MCU_vert_offset; yoffset < coef->MCU_rows_per_iMCU_row;
                yoffset++) {
            for (MCU_col_num = coef->MCU_ctr; MCU_col_num <= last_MCU_col;
                    MCU_col_num++) {
                /* Try to fetch an MCU.  Entropy decoder expects buffer to be zeroed. */
                jzero_far((void FAR *) coef->MCU_buffer[0],
                        (size_t) (cinfo->blocks_in_MCU * SIZEOF(JBLOCK)));
                if (! (*cinfo->entropy->decode_mcu) (cinfo, coef->MCU_buffer)) {
                    /* this is deadly status */
                    return JPEG_SUSPENDED;
                }
                {
                    int i;
                    for (i = 0 ; i < cinfo->blocks_in_MCU ; ++i, ++cinfo->decoded_mcus_current)
                    {
                        JBLOCK * sCurrent = coef->MCU_buffer[i];
                        memcpy(cinfo->decoded_mcus_current,sCurrent, sizeof(JBLOCK));
                    }
                }
            }
        }
    }
    cinfo->decoded_mcus_current = cinfo->decoded_mcus_base;
    coef->pub.decompress_data = decompress_onepass2;
    decompress_onepass2(cinfo,output_buf);
    return 1; // no error
}

static void print_build_log(j_decompress_ptr cinfo,cl_program program)
{
#define LOG_BUFFER_SIZE (1<<20)
    cl_int errorcode;
    char *log_buffer;
    size_t log_size;
    
    log_buffer = malloc(LOG_BUFFER_SIZE);
    if(!log_buffer)
    {
        return ;
    }
    errorcode = clGetProgramBuildInfo(program,cinfo->current_device_id,CL_PROGRAM_BUILD_LOG,
                LOG_BUFFER_SIZE - 1,log_buffer,&log_size);
    if(errorcode == CL_SUCCESS)
    {
        log_buffer[log_size] = 0;
        printf("%s\n",log_buffer);
    }
    free(log_buffer);
}

struct ComponentInfo
{
    unsigned int MCU_width;
    unsigned int MCU_height;
    unsigned int last_col_width;
    unsigned int MCU_sample_width;
    unsigned int DCT_scaled_size;
    unsigned int row_buffer_size;
    unsigned int previous_image_size;
    unsigned int previous_decoded_mcu_size; 
    int dct_table[DCTSIZE2];
};

#define MAX_COMPONENT_INFO_COUNT 5
struct DecodeInfo
{
   unsigned int componets_mcu_width;
   JSAMPLE  sample_range_limit[(5 * (MAXJSAMPLE+1) + CENTERJSAMPLE)]; 
   struct ComponentInfo component_infos[MAX_COMPONENT_INFO_COUNT]; 
};
    METHODDEF(int)
decompress_onepass2 (j_decompress_ptr cinfo, JSAMPIMAGE output_buf)
{
    my_coef_ptr coef = (my_coef_ptr) cinfo->coef;
    JDIMENSION MCU_col_num;	/* index of current MCU within row */
    JDIMENSION last_MCU_col = cinfo->MCUs_per_row - 1;
    JDIMENSION last_iMCU_row = cinfo->total_iMCU_rows - 1;
    int  ci, xindex, yindex, yoffset,yheightoffset, useful_width;
    JSAMPARRAY output_ptr;
    JSAMPROW cur_row;
    JDIMENSION start_col, output_col;
    jpeg_component_info *compptr;
    inverse_DCT_method_ptr inverse_DCT;
    unsigned int componets_mcu_width;
    cl_kernel my_kernel;
    cl_program my_program;


    for (componets_mcu_width = 0 ,ci = 0; ci < cinfo->comps_in_scan; ci++) {
        compptr = cinfo->cur_comp_info[ci];
        componets_mcu_width += compptr->MCU_width * compptr->MCU_height;
    }

    for( yheightoffset = 0 ; yheightoffset < cinfo->total_iMCU_rows ; ++ yheightoffset)
    {
        /* Loop to process as much as one whole iMCU row */
        for (MCU_col_num = coef->MCU_ctr; MCU_col_num <= last_MCU_col; MCU_col_num++) {
            /* Determine where data should go in output_buf and do the IDCT thing.
             * We skip dummy blocks at the right and bottom edges (but blkn gets
             * incremented past them!).  Note the inner loop relies on having
             * allocated the MCU_buffer[] blocks sequentially.
             */
            for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
                JBLOCK * sCurrentBlock;

                compptr = cinfo->cur_comp_info[ci];
                /* Don't bother to IDCT an uninteresting component. */
                if (! compptr->component_needed) {
                    cinfo->decoded_mcus_current += compptr->MCU_blocks;
                    continue;
                }
                inverse_DCT = cinfo->idct->inverse_DCT[compptr->component_index];
                useful_width = (MCU_col_num < last_MCU_col) ? compptr->MCU_width
                    : compptr->last_col_width;
                {
                    int k;
                    for ( k = ci , cur_row = **output_buf ; k > 0 ; --k)
                    {
                        cur_row += compptr[ -k ].image_buffer_size;
                    }
                    cur_row +=  yheightoffset * compptr->DCT_scaled_size * compptr->row_buffer_size ;
                }
                output_ptr = &cur_row;

                start_col = MCU_col_num * compptr->MCU_sample_width;
                for (yindex = 0; yindex < compptr->MCU_height; yindex++) {
                    output_col = start_col;
                    sCurrentBlock = cinfo->decoded_mcus_base + (( yheightoffset * cinfo->MCUs_per_row  + MCU_col_num) * componets_mcu_width) ;
                    {
                        int k;
                        for( k = ci ; k > 0 ; --k)
                        {
                            sCurrentBlock += compptr[-k].MCU_width;
                        }
                    }
                    for (xindex = 0; xindex < useful_width; xindex++) {
                        (*inverse_DCT) (cinfo, compptr,
                                (JCOEFPTR) (sCurrentBlock +  xindex),
                                output_ptr, output_col);
                        output_col += compptr->DCT_scaled_size;
                    }
                    sCurrentBlock += compptr->MCU_width;
                    cur_row += compptr->DCT_scaled_size * compptr->row_buffer_size ;
                }
            }
        }
        /* Completed an MCU row, but perhaps not an iMCU row */
        coef->MCU_ctr = 0;

        /* Completed the iMCU row, advance counters for next one */
    }
    cinfo->output_iMCU_row = cinfo->total_iMCU_rows;
    cinfo->input_iMCU_row = cinfo->total_iMCU_rows;
    /* Completed the scan */
    (*cinfo->inputctl->finish_input_pass) (cinfo);

    // testing the opencl's output
    {
        char * source_string;
        cl_int error_code;
        struct DecodeInfo * decode_info;
        int i ;
        int previous_image_size;
        int previous_decoded_mcu_size;
        cl_kernel dct_kernel;
        cl_mem constant_decode_info;
        cl_mem constant_decoded_mcu;
        cl_mem my_cl_output_buffer; 
        size_t work_dim[3];
        size_t local_work_dim[3];
        JSAMPLE * from_cl_output;

        extern int read_all_bytes(const char * aFile,char ** aContent);
        
        if(0 == read_all_bytes("decode_idct.cl",&source_string))
        {
            ERREXIT(cinfo,1);
        }
        my_program = clCreateProgramWithSource(cinfo->current_cl_context,1,&source_string,NULL,&error_code);
        if(error_code != CL_SUCCESS)
        {
            ERREXIT(cinfo,error_code);
        }
        free_all_bytes(source_string); 
        error_code = clBuildProgram(my_program,1,&cinfo->current_device_id,NULL,NULL,NULL);
        print_build_log(cinfo,my_program);
        if(error_code != CL_SUCCESS)
        {
            ERREXIT(cinfo,error_code);
        }
        decode_info = (struct DecodeInfo*)malloc(sizeof(struct DecodeInfo));
        if(!decode_info)
        {
            ERREXIT(cinfo,JERR_OUT_OF_MEMORY);
        }
        decode_info->componets_mcu_width = componets_mcu_width;
        memcpy(decode_info->sample_range_limit,cinfo->sample_range_limit,(5 * (MAXJSAMPLE+1) + CENTERJSAMPLE) * sizeof(JSAMPLE));
        previous_image_size = 0;
        previous_decoded_mcu_size = 0 ;
        for( i = 0 ; i < cinfo->comps_in_scan ; ++i)
        {
            struct ComponentInfo * compptr_info;
            
            compptr_info = &decode_info->component_infos[i];
            compptr = cinfo->cur_comp_info[i];
            
            compptr_info->MCU_width = compptr->MCU_width;
            compptr_info->MCU_height = compptr->MCU_height;
            compptr_info->last_col_width = compptr->last_col_width;
            compptr_info->MCU_sample_width = compptr->MCU_sample_width;
            compptr_info->DCT_scaled_size = compptr->DCT_scaled_size;
            compptr_info->row_buffer_size = compptr->row_buffer_size;
            compptr_info->previous_image_size = previous_image_size;
            compptr_info->previous_decoded_mcu_size = previous_decoded_mcu_size;
            memcpy(compptr_info->dct_table,compptr->dct_table,sizeof(int) * DCTSIZE2);

            previous_image_size += compptr->image_buffer_size;
            previous_decoded_mcu_size += compptr->MCU_width;
        }

        constant_decode_info = clCreateBuffer(cinfo->current_cl_context,
                CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY,
                sizeof(struct DecodeInfo),
                decode_info,
                &error_code);
        constant_decoded_mcu = clCreateBuffer(cinfo->current_cl_context,
                CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY,
                sizeof(JBLOCK) *  cinfo->blocks_in_MCU * 
                coef->MCU_rows_per_iMCU_row * cinfo->MCUs_per_row * cinfo->MCU_rows_in_scan,
                cinfo->decoded_mcus_base,
                &error_code);
        my_cl_output_buffer = clCreateBuffer(cinfo->current_cl_context,
                CL_MEM_READ_WRITE,
                sizeof(JSAMPLE) * previous_image_size,
                NULL,
                &error_code);
        dct_kernel = clCreateKernel(my_program,"idct",&error_code);
        error_code = clSetKernelArg(dct_kernel,0,sizeof(cl_mem),&constant_decode_info);
        error_code = clSetKernelArg(dct_kernel,1,sizeof(cl_mem),&constant_decoded_mcu);
        error_code = clSetKernelArg(dct_kernel,2,sizeof(cl_mem),&my_cl_output_buffer);
        work_dim[0] = cinfo->total_iMCU_rows;
        work_dim[1] = cinfo->MCUs_per_row;
        work_dim[2] = cinfo->comps_in_scan;
        local_work_dim[0] = 1;
        local_work_dim[1] = 1;
        local_work_dim[2] = 1;

        error_code = clEnqueueNDRangeKernel(cinfo->current_cl_queue,dct_kernel,
                    3,
                    NULL,
                    work_dim,
                    local_work_dim,
                    NULL,
                    NULL,
                    NULL);
        from_cl_output = malloc(sizeof(JSAMPLE) * previous_image_size);
        error_code = clEnqueueReadBuffer(cinfo->current_cl_queue,
                            my_cl_output_buffer,
                            CL_TRUE,
                            0,
                            sizeof(JSAMPLE) * previous_image_size,
                            from_cl_output,
                            0,
                            NULL,
                            NULL);
        clReleaseMemObject(constant_decode_info);
        clReleaseMemObject(constant_decoded_mcu);
        clReleaseMemObject(my_cl_output_buffer);
        clReleaseKernel(dct_kernel);
        clReleaseProgram(my_program);
        if(0 != memcmp(from_cl_output,**output_buf,sizeof(JSAMPLE) * previous_image_size))
        {
            fprintf(stderr,"LIN:Failed\n");
        }
        free(from_cl_output);
    }

    return JPEG_SCAN_COMPLETED;

}

/*
 * Dummy consume-input routine for single-pass operation.
 */

    METHODDEF(int)
dummy_consume_data (j_decompress_ptr cinfo)
{
    return JPEG_SUSPENDED;	/* Always indicate nothing was done */
}


#ifdef D_MULTISCAN_FILES_SUPPORTED

/*
 * Consume input data and store it in the full-image coefficient buffer.
 * We read as much as one fully interleaved MCU row ("iMCU" row) per call,
 * ie, v_samp_factor block rows for each component in the scan.
 * Return value is JPEG_ROW_COMPLETED, JPEG_SCAN_COMPLETED, or JPEG_SUSPENDED.
 */

    METHODDEF(int)
consume_data (j_decompress_ptr cinfo)
{
    my_coef_ptr coef = (my_coef_ptr) cinfo->coef;
    JDIMENSION MCU_col_num;	/* index of current MCU within row */
    int blkn, ci, xindex, yindex, yoffset;
    JDIMENSION start_col;
    JBLOCKARRAY buffer[MAX_COMPS_IN_SCAN];
    JBLOCKROW buffer_ptr;
    jpeg_component_info *compptr;

    /* Align the virtual buffers for the components used in this scan. */
    for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
        compptr = cinfo->cur_comp_info[ci];
        buffer[ci] = (*cinfo->mem->access_virt_barray)
            ((j_common_ptr) cinfo, coef->whole_image[compptr->component_index],
             cinfo->input_iMCU_row * compptr->v_samp_factor,
             (JDIMENSION) compptr->v_samp_factor, TRUE);
        /* Note: entropy decoder expects buffer to be zeroed,
         * but this is handled automatically by the memory manager
         * because we requested a pre-zeroed array.
         */
    }

    /* Loop to process one whole iMCU row */
    for (yoffset = coef->MCU_vert_offset; yoffset < coef->MCU_rows_per_iMCU_row;
            yoffset++) {
        for (MCU_col_num = coef->MCU_ctr; MCU_col_num < cinfo->MCUs_per_row;
                MCU_col_num++) {
            /* Construct list of pointers to DCT blocks belonging to this MCU */
            blkn = 0;			/* index of current DCT block within MCU */
            for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
                compptr = cinfo->cur_comp_info[ci];
                start_col = MCU_col_num * compptr->MCU_width;
                for (yindex = 0; yindex < compptr->MCU_height; yindex++) {
                    buffer_ptr = buffer[ci][yindex+yoffset] + start_col;
                    for (xindex = 0; xindex < compptr->MCU_width; xindex++) {
                        coef->MCU_buffer[blkn++] = buffer_ptr++;
                    }
                }
            }
            /* Try to fetch the MCU. */
            if (! (*cinfo->entropy->decode_mcu) (cinfo, coef->MCU_buffer)) {
                /* Suspension forced; update state counters and exit */
                coef->MCU_vert_offset = yoffset;
                coef->MCU_ctr = MCU_col_num;
                return JPEG_SUSPENDED;
            }
        }
        /* Completed an MCU row, but perhaps not an iMCU row */
        coef->MCU_ctr = 0;
    }
    /* Completed the iMCU row, advance counters for next one */
    if (++(cinfo->input_iMCU_row) < cinfo->total_iMCU_rows) {
        start_iMCU_row(cinfo);
        return JPEG_ROW_COMPLETED;
    }
    /* Completed the scan */
    (*cinfo->inputctl->finish_input_pass) (cinfo);
    return JPEG_SCAN_COMPLETED;
}


/*
 * Decompress and return some data in the multi-pass case.
 * Always attempts to emit one fully interleaved MCU row ("iMCU" row).
 * Return value is JPEG_ROW_COMPLETED, JPEG_SCAN_COMPLETED, or JPEG_SUSPENDED.
 *
 * NB: output_buf contains a plane for each component in image.
 */

    METHODDEF(int)
decompress_data (j_decompress_ptr cinfo, JSAMPIMAGE output_buf)
{
    my_coef_ptr coef = (my_coef_ptr) cinfo->coef;
    JDIMENSION last_iMCU_row = cinfo->total_iMCU_rows - 1;
    JDIMENSION block_num;
    int ci, block_row, block_rows;
    JBLOCKARRAY buffer;
    JBLOCKROW buffer_ptr;
    JSAMPARRAY output_ptr;
    JDIMENSION output_col;
    jpeg_component_info *compptr;
    inverse_DCT_method_ptr inverse_DCT;

    /* Force some input to be done if we are getting ahead of the input. */
    while (cinfo->input_scan_number < cinfo->output_scan_number ||
            (cinfo->input_scan_number == cinfo->output_scan_number &&
             cinfo->input_iMCU_row <= cinfo->output_iMCU_row)) {
        if ((*cinfo->inputctl->consume_input)(cinfo) == JPEG_SUSPENDED)
            return JPEG_SUSPENDED;
    }

    /* OK, output from the virtual arrays. */
    for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
            ci++, compptr++) {
        /* Don't bother to IDCT an uninteresting component. */
        if (! compptr->component_needed)
            continue;
        /* Align the virtual buffer for this component. */
        buffer = (*cinfo->mem->access_virt_barray)
            ((j_common_ptr) cinfo, coef->whole_image[ci],
             cinfo->output_iMCU_row * compptr->v_samp_factor,
             (JDIMENSION) compptr->v_samp_factor, FALSE);
        /* Count non-dummy DCT block rows in this iMCU row. */
        if (cinfo->output_iMCU_row < last_iMCU_row)
            block_rows = compptr->v_samp_factor;
        else {
            /* NB: can't use last_row_height here; it is input-side-dependent! */
            block_rows = (int) (compptr->height_in_blocks % compptr->v_samp_factor);
            if (block_rows == 0) block_rows = compptr->v_samp_factor;
        }
        inverse_DCT = cinfo->idct->inverse_DCT[ci];
        output_ptr = output_buf[ci];
        /* Loop over all DCT blocks to be processed. */
        for (block_row = 0; block_row < block_rows; block_row++) {
            buffer_ptr = buffer[block_row];
            output_col = 0;
            for (block_num = 0; block_num < compptr->width_in_blocks; block_num++) {
                (*inverse_DCT) (cinfo, compptr, (JCOEFPTR) buffer_ptr,
                        output_ptr, output_col);
                buffer_ptr++;
                output_col += compptr->DCT_scaled_size;
            }
            output_ptr += compptr->DCT_scaled_size;
        }
    }

    if (++(cinfo->output_iMCU_row) < cinfo->total_iMCU_rows)
        return JPEG_ROW_COMPLETED;
    return JPEG_SCAN_COMPLETED;
}

#endif /* D_MULTISCAN_FILES_SUPPORTED */


#ifdef BLOCK_SMOOTHING_SUPPORTED

/*
 * This code applies interblock smoothing as described by section K.8
 * of the JPEG standard: the first 5 AC coefficients are estimated from
 * the DC values of a DCT block and its 8 neighboring blocks.
 * We apply smoothing only for progressive JPEG decoding, and only if
 * the coefficients it can estimate are not yet known to full precision.
 */

/* Natural-order array positions of the first 5 zigzag-order coefficients */
#define Q01_POS  1
#define Q10_POS  8
#define Q20_POS  16
#define Q11_POS  9
#define Q02_POS  2

/*
 * Determine whether block smoothing is applicable and safe.
 * We also latch the current states of the coef_bits[] entries for the
 * AC coefficients; otherwise, if the input side of the decompressor
 * advances into a new scan, we might think the coefficients are known
 * more accurately than they really are.
 */

    LOCAL(boolean)
smoothing_ok (j_decompress_ptr cinfo)
{
    my_coef_ptr coef = (my_coef_ptr) cinfo->coef;
    boolean smoothing_useful = FALSE;
    int ci, coefi;
    jpeg_component_info *compptr;
    JQUANT_TBL * qtable;
    int * coef_bits;
    int * coef_bits_latch;

    if (! cinfo->progressive_mode || cinfo->coef_bits == NULL)
        return FALSE;

    /* Allocate latch area if not already done */
    if (coef->coef_bits_latch == NULL)
        coef->coef_bits_latch = (int *)
            (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                    cinfo->num_components *
                    (SAVED_COEFS * SIZEOF(int)));
    coef_bits_latch = coef->coef_bits_latch;

    for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
            ci++, compptr++) {
        /* All components' quantization values must already be latched. */
        if ((qtable = compptr->quant_table) == NULL)
            return FALSE;
        /* Verify DC & first 5 AC quantizers are nonzero to avoid zero-divide. */
        if (qtable->quantval[0] == 0 ||
                qtable->quantval[Q01_POS] == 0 ||
                qtable->quantval[Q10_POS] == 0 ||
                qtable->quantval[Q20_POS] == 0 ||
                qtable->quantval[Q11_POS] == 0 ||
                qtable->quantval[Q02_POS] == 0)
            return FALSE;
        /* DC values must be at least partly known for all components. */
        coef_bits = cinfo->coef_bits[ci];
        if (coef_bits[0] < 0)
            return FALSE;
        /* Block smoothing is helpful if some AC coefficients remain inaccurate. */
        for (coefi = 1; coefi <= 5; coefi++) {
            coef_bits_latch[coefi] = coef_bits[coefi];
            if (coef_bits[coefi] != 0)
                smoothing_useful = TRUE;
        }
        coef_bits_latch += SAVED_COEFS;
    }

    return smoothing_useful;
}


/*
 * Variant of decompress_data for use when doing block smoothing.
 */

    METHODDEF(int)
decompress_smooth_data (j_decompress_ptr cinfo, JSAMPIMAGE output_buf)
{
    my_coef_ptr coef = (my_coef_ptr) cinfo->coef;
    JDIMENSION last_iMCU_row = cinfo->total_iMCU_rows - 1;
    JDIMENSION block_num, last_block_column;
    int ci, block_row, block_rows, access_rows;
    JBLOCKARRAY buffer;
    JBLOCKROW buffer_ptr, prev_block_row, next_block_row;
    JSAMPARRAY output_ptr;
    JDIMENSION output_col;
    jpeg_component_info *compptr;
    inverse_DCT_method_ptr inverse_DCT;
    boolean first_row, last_row;
    JBLOCK workspace;
    int *coef_bits;
    JQUANT_TBL *quanttbl;
    INT32 Q00,Q01,Q02,Q10,Q11,Q20, num;
    int DC1,DC2,DC3,DC4,DC5,DC6,DC7,DC8,DC9;
    int Al, pred;

    /* Force some input to be done if we are getting ahead of the input. */
    while (cinfo->input_scan_number <= cinfo->output_scan_number &&
            ! cinfo->inputctl->eoi_reached) {
        if (cinfo->input_scan_number == cinfo->output_scan_number) {
            /* If input is working on current scan, we ordinarily want it to
             * have completed the current row.  But if input scan is DC,
             * we want it to keep one row ahead so that next block row's DC
             * values are up to date.
             */
            JDIMENSION delta = (cinfo->Ss == 0) ? 1 : 0;
            if (cinfo->input_iMCU_row > cinfo->output_iMCU_row+delta)
                break;
        }
        if ((*cinfo->inputctl->consume_input)(cinfo) == JPEG_SUSPENDED)
            return JPEG_SUSPENDED;
    }

    /* OK, output from the virtual arrays. */
    for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
            ci++, compptr++) {
        /* Don't bother to IDCT an uninteresting component. */
        if (! compptr->component_needed)
            continue;
        /* Count non-dummy DCT block rows in this iMCU row. */
        if (cinfo->output_iMCU_row < last_iMCU_row) {
            block_rows = compptr->v_samp_factor;
            access_rows = block_rows * 2; /* this and next iMCU row */
            last_row = FALSE;
        } else {
            /* NB: can't use last_row_height here; it is input-side-dependent! */
            block_rows = (int) (compptr->height_in_blocks % compptr->v_samp_factor);
            if (block_rows == 0) block_rows = compptr->v_samp_factor;
            access_rows = block_rows; /* this iMCU row only */
            last_row = TRUE;
        }
        /* Align the virtual buffer for this component. */
        if (cinfo->output_iMCU_row > 0) {
            access_rows += compptr->v_samp_factor; /* prior iMCU row too */
            buffer = (*cinfo->mem->access_virt_barray)
                ((j_common_ptr) cinfo, coef->whole_image[ci],
                 (cinfo->output_iMCU_row - 1) * compptr->v_samp_factor,
                 (JDIMENSION) access_rows, FALSE);
            buffer += compptr->v_samp_factor;	/* point to current iMCU row */
            first_row = FALSE;
        } else {
            buffer = (*cinfo->mem->access_virt_barray)
                ((j_common_ptr) cinfo, coef->whole_image[ci],
                 (JDIMENSION) 0, (JDIMENSION) access_rows, FALSE);
            first_row = TRUE;
        }
        /* Fetch component-dependent info */
        coef_bits = coef->coef_bits_latch + (ci * SAVED_COEFS);
        quanttbl = compptr->quant_table;
        Q00 = quanttbl->quantval[0];
        Q01 = quanttbl->quantval[Q01_POS];
        Q10 = quanttbl->quantval[Q10_POS];
        Q20 = quanttbl->quantval[Q20_POS];
        Q11 = quanttbl->quantval[Q11_POS];
        Q02 = quanttbl->quantval[Q02_POS];
        inverse_DCT = cinfo->idct->inverse_DCT[ci];
        output_ptr = output_buf[ci];
        /* Loop over all DCT blocks to be processed. */
        for (block_row = 0; block_row < block_rows; block_row++) {
            buffer_ptr = buffer[block_row];
            if (first_row && block_row == 0)
                prev_block_row = buffer_ptr;
            else
                prev_block_row = buffer[block_row-1];
            if (last_row && block_row == block_rows-1)
                next_block_row = buffer_ptr;
            else
                next_block_row = buffer[block_row+1];
            /* We fetch the surrounding DC values using a sliding-register approach.
             * Initialize all nine here so as to do the right thing on narrow pics.
             */
            DC1 = DC2 = DC3 = (int) prev_block_row[0][0];
            DC4 = DC5 = DC6 = (int) buffer_ptr[0][0];
            DC7 = DC8 = DC9 = (int) next_block_row[0][0];
            output_col = 0;
            last_block_column = compptr->width_in_blocks - 1;
            for (block_num = 0; block_num <= last_block_column; block_num++) {
                /* Fetch current DCT block into workspace so we can modify it. */
                jcopy_block_row(buffer_ptr, (JBLOCKROW) workspace, (JDIMENSION) 1);
                /* Update DC values */
                if (block_num < last_block_column) {
                    DC3 = (int) prev_block_row[1][0];
                    DC6 = (int) buffer_ptr[1][0];
                    DC9 = (int) next_block_row[1][0];
                }
                /* Compute coefficient estimates per K.8.
                 * An estimate is applied only if coefficient is still zero,
                 * and is not known to be fully accurate.
                 */
                /* AC01 */
                if ((Al=coef_bits[1]) != 0 && workspace[1] == 0) {
                    num = 36 * Q00 * (DC4 - DC6);
                    if (num >= 0) {
                        pred = (int) (((Q01<<7) + num) / (Q01<<8));
                        if (Al > 0 && pred >= (1<<Al))
                            pred = (1<<Al)-1;
                    } else {
                        pred = (int) (((Q01<<7) - num) / (Q01<<8));
                        if (Al > 0 && pred >= (1<<Al))
                            pred = (1<<Al)-1;
                        pred = -pred;
                    }
                    workspace[1] = (JCOEF) pred;
                }
                /* AC10 */
                if ((Al=coef_bits[2]) != 0 && workspace[8] == 0) {
                    num = 36 * Q00 * (DC2 - DC8);
                    if (num >= 0) {
                        pred = (int) (((Q10<<7) + num) / (Q10<<8));
                        if (Al > 0 && pred >= (1<<Al))
                            pred = (1<<Al)-1;
                    } else {
                        pred = (int) (((Q10<<7) - num) / (Q10<<8));
                        if (Al > 0 && pred >= (1<<Al))
                            pred = (1<<Al)-1;
                        pred = -pred;
                    }
                    workspace[8] = (JCOEF) pred;
                }
                /* AC20 */
                if ((Al=coef_bits[3]) != 0 && workspace[16] == 0) {
                    num = 9 * Q00 * (DC2 + DC8 - 2*DC5);
                    if (num >= 0) {
                        pred = (int) (((Q20<<7) + num) / (Q20<<8));
                        if (Al > 0 && pred >= (1<<Al))
                            pred = (1<<Al)-1;
                    } else {
                        pred = (int) (((Q20<<7) - num) / (Q20<<8));
                        if (Al > 0 && pred >= (1<<Al))
                            pred = (1<<Al)-1;
                        pred = -pred;
                    }
                    workspace[16] = (JCOEF) pred;
                }
                /* AC11 */
                if ((Al=coef_bits[4]) != 0 && workspace[9] == 0) {
                    num = 5 * Q00 * (DC1 - DC3 - DC7 + DC9);
                    if (num >= 0) {
                        pred = (int) (((Q11<<7) + num) / (Q11<<8));
                        if (Al > 0 && pred >= (1<<Al))
                            pred = (1<<Al)-1;
                    } else {
                        pred = (int) (((Q11<<7) - num) / (Q11<<8));
                        if (Al > 0 && pred >= (1<<Al))
                            pred = (1<<Al)-1;
                        pred = -pred;
                    }
                    workspace[9] = (JCOEF) pred;
                }
                /* AC02 */
                if ((Al=coef_bits[5]) != 0 && workspace[2] == 0) {
                    num = 9 * Q00 * (DC4 + DC6 - 2*DC5);
                    if (num >= 0) {
                        pred = (int) (((Q02<<7) + num) / (Q02<<8));
                        if (Al > 0 && pred >= (1<<Al))
                            pred = (1<<Al)-1;
                    } else {
                        pred = (int) (((Q02<<7) - num) / (Q02<<8));
                        if (Al > 0 && pred >= (1<<Al))
                            pred = (1<<Al)-1;
                        pred = -pred;
                    }
                    workspace[2] = (JCOEF) pred;
                }
                /* OK, do the IDCT */
                (*inverse_DCT) (cinfo, compptr, (JCOEFPTR) workspace,
                        output_ptr, output_col);
                /* Advance for next column */
                DC1 = DC2; DC2 = DC3;
                DC4 = DC5; DC5 = DC6;
                DC7 = DC8; DC8 = DC9;
                buffer_ptr++, prev_block_row++, next_block_row++;
                output_col += compptr->DCT_scaled_size;
            }
            output_ptr += compptr->DCT_scaled_size;
        }
    }

    if (++(cinfo->output_iMCU_row) < cinfo->total_iMCU_rows)
        return JPEG_ROW_COMPLETED;
    return JPEG_SCAN_COMPLETED;
}

#endif /* BLOCK_SMOOTHING_SUPPORTED */


/*
 * Initialize coefficient buffer controller.
 */

    GLOBAL(void)
jinit_d_coef_controller (j_decompress_ptr cinfo, boolean need_full_buffer)
{
    my_coef_ptr coef;

    coef = (my_coef_ptr)
        (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                SIZEOF(my_coef_controller));
    cinfo->coef = (struct jpeg_d_coef_controller *) coef;
    coef->pub.start_input_pass = start_input_pass;
    coef->pub.start_output_pass = start_output_pass;
#ifdef BLOCK_SMOOTHING_SUPPORTED
    coef->coef_bits_latch = NULL;
#endif

    /* Create the coefficient buffer. */
    if (need_full_buffer) {
#ifdef D_MULTISCAN_FILES_SUPPORTED
        /* Allocate a full-image virtual array for each component, */
        /* padded to a multiple of samp_factor DCT blocks in each direction. */
        /* Note we ask for a pre-zeroed array. */
        int ci, access_rows;
        jpeg_component_info *compptr;

        for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
                ci++, compptr++) {
            access_rows = compptr->v_samp_factor;
#ifdef BLOCK_SMOOTHING_SUPPORTED
            /* If block smoothing could be used, need a bigger window */
            if (cinfo->progressive_mode)
                access_rows *= 3;
#endif
            coef->whole_image[ci] = (*cinfo->mem->request_virt_barray)
                ((j_common_ptr) cinfo, JPOOL_IMAGE, TRUE,
                 (JDIMENSION) jround_up((long) compptr->width_in_blocks,
                     (long) compptr->h_samp_factor),
                 (JDIMENSION) jround_up((long) compptr->height_in_blocks,
                     (long) compptr->v_samp_factor),
                 (JDIMENSION) access_rows);
        }
        coef->pub.consume_data = consume_data;
        coef->pub.decompress_data = decompress_data;
        coef->pub.coef_arrays = coef->whole_image; /* link to virtual arrays */
#else
        ERREXIT(cinfo, JERR_NOT_COMPILED);
#endif
    } else {
        /* We only need a single-MCU buffer. */
        JBLOCKROW buffer;
        int i;

        buffer = (JBLOCKROW)
            (*cinfo->mem->alloc_large) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                    D_MAX_BLOCKS_IN_MCU * SIZEOF(JBLOCK));
        for (i = 0; i < D_MAX_BLOCKS_IN_MCU; i++) {
            coef->MCU_buffer[i] = buffer + i;
        }
        coef->pub.consume_data = dummy_consume_data;
        coef->pub.decompress_data = decompress_onepass;
        coef->pub.coef_arrays = NULL; /* flag for no virtual arrays */
    }
}

