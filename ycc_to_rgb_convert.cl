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
  __global JSAMPLE * range_limit = convInfo->sample_range_limit + (MAXJSAMPLE+1);
  __global int * Crrtab = convInfo->Cr_r_tab;
  __global int * Cbbtab = convInfo->Cb_b_tab;
  __global int * Crgtab = convInfo->Cr_g_tab;
  __global int * Cbgtab = convInfo->Cb_g_tab;
  int yoffset = get_global_id(0);
  int col = get_global_id(1);
  int width = get_global_size(1);
  int height = get_global_size(0);
  int component_image_size = width * height;
  float3 outputv1 = (float3)(1.0f,0.0f,1.40200f);
  float3 outputv2 = (float3)(1.0f,-0.34414,-0.71414);
  float3 outputv3 = (float3)(1.0f,1.77200f,0.0f);
  float3 components;

  inptr0 = input_buf + yoffset * width + col;
  inptr1 = inptr0 + component_image_size;
  inptr2 = inptr1 + component_image_size;
  
  components.x  = convert_float((inptr0[0]) & 0xff);
  components.y = convert_float(((inptr1[0]) & 0xff) - CENTERJSAMPLE);
  components.z = convert_float(((inptr2[0]) & 0xff) - CENTERJSAMPLE);
  outptr = output_buf + (yoffset * width + col) * 3;
  outptr[0] = range_limit[convert_int(dot(components , outputv1))] ;
  outptr[1] = range_limit[convert_int(dot(components,outputv2))];
  outptr[2] = range_limit[convert_int(dot(components,outputv3))] ;
}
