#define CENTERJSAMPLE	128
#define MAXJSAMPLE	255
typedef unsigned char JSAMPLE;
struct ConverterInfo
{
  int  Cr_r_tab[MAXJSAMPLE + 1];		/* => table for Cr to R conversion */
  int  Cb_b_tab[MAXJSAMPLE + 1];		/* => table for Cb to B conversion */
  int  Cr_g_tab[MAXJSAMPLE + 1];		/* => table for Cr to G conversion */
  int  Cb_g_tab[MAXJSAMPLE + 1];		/* => table for Cb to G conversion */
  JSAMPLE  sample_range_limit[(5 * (MAXJSAMPLE+1) + CENTERJSAMPLE)]; 
};
#define RIGHT_SHIFT(x,shft)	((x) >> (shft))
#define SCALEBITS	16	/* speediest right-shift on some machines */
__kernel
void convert(
                __global struct ConverterInfo * convInfo,
                __global JSAMPLE * input_buf,
                __global JSAMPLE * output_buf)
{
  int y,cb,cr;
  __global JSAMPLE * inptr0;
  __global JSAMPLE * inptr1;
  __global JSAMPLE * inptr2;
  __global JSAMPLE * outptr;
  __global JSAMPLE * range_limit = convInfo->sample_range_limit;
  __global int * Crrtab = convInfo->Cr_r_tab;
  __global int * Cbbtab = convInfo->Cb_b_tab;
  __global int * Crgtab = convInfo->Cr_g_tab;
  __global int * Cbgtab = convInfo->Cb_g_tab;
  int yoffset = get_global_id(0);
  int col = get_global_id(1);
  int width = get_global_size(1);

  inptr0 = input_buf + yoffset * width + col;
  inptr1 = inptr0 + convInfo->component_image_size;
  inptr2 = inptr1 + convInfo->component_image_size;
  
  y  = (inptr0[0]);
  cb = (inptr1[0]);
  cr = (inptr2[0]);
  outptr = output_buf + (yoffset * width + col) * 3;
  outptr[0] =   range_limit[y + Crrtab[cr]];
  outptr[1] = range_limit[y +
    	      ((int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr],
    				 SCALEBITS))];
  outptr[2] =  range_limit[y + Cbbtab[cb]];
}
