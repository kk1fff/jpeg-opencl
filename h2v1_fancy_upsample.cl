#include "upsample.clh"

__kernel
void my_upsample( __global JSAMPLE * input_buf,
            __global JSAMPLE * output_buf)
{
   __global JSAMPLE * input_ptr;
   __global JSAMPLE * output_ptr;
   int invalue,othervalue;
   int yoffset = get_global_id(0) ;
   int height = get_global_size(0);
   int col = get_global_id(1);
   int width = get_global_size(1);

   input_ptr = input_buf + (yoffset * width) + col;
   output_ptr = output_buf + (((yoffset * width) + col) << 1);
   invalue = GETJSAMPLE(input_ptr[0]);
   if(col == 0)
   {
       // first column
       othervalue = GETJSAMPLE(input_ptr[1]);
       output_ptr[0] = invalue;
       output_ptr[1] = (JSAMPLE) ((invalue * 3 + othervalue  + 2) >> 2);
   }
   else if (col == (width - 1) )
   {
       // last column
       othervalue = GETJSAMPLE(input_ptr[-1]) ;
       output_ptr[0] = (JSAMPLE) ((invalue * 3 + othervalue + 1) >> 2);
       output_ptr[1] = (JSAMPLE) invalue;
   }
   else
   {
       // in the middle
       othervalue = GETJSAMPLE(input_ptr[-1]);
       output_ptr[0] = (JSAMPLE) ((invalue * 3 + othervalue + 1) >> 2);
       othervalue = GETJSAMPLE(input_ptr[1]);
       output_ptr[1] = (JSAMPLE) ((invalue * 3 + othervalue + 2) >> 2);
   }
}
