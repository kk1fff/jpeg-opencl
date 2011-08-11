#include "upsample.clh"


__kernel
void my_upsample( __global JSAMPLE * input_buf,
            __global JSAMPLE * output_buf)
{
   __global JSAMPLE * inptr0;
   __global JSAMPLE * inptr1;

   __global JSAMPLE * input_ptr;
   __global JSAMPLE * outptr;
   int invalue;
   int yoffset = get_global_id(0) ;
   int height = get_global_size(0);
   int col = get_global_id(1);
   int width = get_global_size(1);
   int input_offset = ( (yoffset >> 1 ) * width) + col ;

   inptr0 = input_buf + input_offset;
   if(yoffset == 0)
   {
       // the first row
       inptr1 = inptr0;
   }
   else if (yoffset == (height - 1))
   {
        // the last row
        inptr1 = inptr0;
   }
   else if( !(yoffset & 1 ))
   {
       inptr1 = inptr0 - width;
   }
   else
   {
       inptr1 = inptr0 + width;
   }
   outptr = output_buf + ( ( ( yoffset * width) + col ) << 1) ;
   
   if(col == 0)
   {
       // first column
       unsigned int thiscolsum = GETJSAMPLE(inptr0[0]) * 3 + GETJSAMPLE(inptr1[0]);
       unsigned int nextcolsum = GETJSAMPLE(inptr0[1]) * 3 + GETJSAMPLE(inptr1[1]);
       outptr[0] = (JSAMPLE) ((thiscolsum * 4 + 8) >> 4);
       outptr[1] = (JSAMPLE) ((thiscolsum * 3 + nextcolsum + 7) >> 4);
   }
   else if (col == (width - 1) )
   {
       // last column
       unsigned int thiscolsum = GETJSAMPLE(inptr0[0]) * 3 + GETJSAMPLE(inptr1[0]);
       unsigned int lastcolsum  = GETJSAMPLE(inptr0[-1]) * 3 + GETJSAMPLE(inptr1[-1]);
       outptr[0] = (JSAMPLE) ((thiscolsum * 3 + lastcolsum + 8) >> 4);
       outptr[1] = (JSAMPLE) ((thiscolsum * 4 + 7) >> 4);
   }
   else
   {
       // in the middle
       unsigned int lastcolsum = GETJSAMPLE(inptr0[-1]) * 3 + GETJSAMPLE(inptr1[-1]);
       unsigned int thiscolsum = GETJSAMPLE(inptr0[0]) * 3 + GETJSAMPLE(inptr1[0]);
       unsigned int nextcolsum = GETJSAMPLE(inptr0[1]) * 3 + GETJSAMPLE(inptr1[1]);
       outptr[0] = (JSAMPLE) ((thiscolsum * 3 + lastcolsum + 8) >> 4);
       outptr[1] = (JSAMPLE) ((thiscolsum * 3 + nextcolsum + 7) >> 4);
   }
}
