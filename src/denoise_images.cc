#include <sli/stdstreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>
using namespace sli;

#include "read_tiff24or48_to_float.h"
#include "write_float_to_tiff24or48.h"
#include "make_output_filename.cc"
#include "icc_srgb_profile.c"

#include "wavelet_denoise.c"

int main( int argc, char *argv[] )
{
    stdstreamio sio, f_in;
    tarray_tstring filenames_in;
    mdarray_float img_in_buf(false);
    mdarray_float img_work_buf(false);
    mdarray_uchar icc_buf(false);

    bool flag_output_8bit = false;
    bool flag_dither = true;
    float threshold[3] = {0,0,0};	/* for R,G and B each */

    int arg_cnt;
    size_t i;
    
    int return_status = -1;

#if 0
    { /* test */
      int scale = 1;
      unsigned maximum = 65536;
      maximum = 256;
      while (maximum << scale < 0x10000) scale++;
      maximum <<= --scale;
      sio.printf("maximum = %u, scale = %d\n",
		 maximum, scale);
    }
#endif
        
    if ( argc < 2 ) {
        sio.eprintf("Apply wavelet-denoise to images\n");
	sio.eprintf("\n");
        sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s -s threshold img_0.tiff img_1.tiff ...\n", argv[0]);
	sio.eprintf("$ %s -s threshold_r,threshold_g,threshold_b img_0.tiff img_1.tiff ...\n", argv[0]);
	sio.eprintf("-s param ... Threshold of wavelet-denoise\n");
	sio.eprintf("-t ... If set, output truncated real without dither\n");
	sio.eprintf("\n");
	sio.eprintf("NOTE: Set large threshold for noisy images\n");
	sio.eprintf("\n");
	sio.eprintf("[EXAMPLE]\n");
	sio.eprintf("$ %s -s 300,300,600 foo.tiff\n", argv[0]);
	goto quit;
    }

    filenames_in = argv;

    arg_cnt = 1;

    while ( arg_cnt < argc ) {
	tstring argstr;
	argstr = argv[arg_cnt];
	if ( argstr == "-8" ) {
	    flag_output_8bit = true;
	    arg_cnt ++;
	}
	else if ( argstr == "-t" ) {
	    flag_dither = false;
	    arg_cnt ++;
	}
	else if ( argstr == "-s" ) {
	    tarray_tstring arr_threshold_str;
	    arg_cnt ++;
	    arr_threshold_str.split(argv[arg_cnt],",",true);
	    /* R */
	    threshold[0] = arr_threshold_str[0].atof();
	    /* G */
	    if ( 2 <= arr_threshold_str.length() ) {
	        threshold[1] = arr_threshold_str[1].atof();
	    }
	    else {
	        threshold[1] = threshold[0];
	    }
	    /* B */
	    if ( 3 <= arr_threshold_str.length() ) {
	        threshold[2] = arr_threshold_str[2].atof();
	    }
	    else {
	        threshold[2] = threshold[1];
	    }
	    /* */
	    sio.printf("Using threshold(r,g,b): %g,%g,%g\n",
		       threshold[0],threshold[1],threshold[2]);
	    arg_cnt ++;
	}
	else {
	    break;
	}
    }

    filenames_in.erase(0, arg_cnt);	/* erase */
    //filenames_in.dprint();


    for ( i=0 ; i < filenames_in.length() ; i++ ) {
	int bytes;
	tstring filename, filename_out;
	float *ptr;
	size_t j;

	filename = filenames_in[i];
	sio.printf("Loading %s\n", filename.cstr());
	if ( read_tiff24or48_to_float(filename.cstr(),
				      &img_in_buf, &icc_buf, &bytes, NULL) < 0 ) {
	    sio.eprintf("[ERROR] cannot load '%s'\n", filename.cstr());
	    sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	    goto quit;
	}

	sio.printf("Applying wavelet denoising ...\n");

	img_work_buf.resize( img_in_buf.x_length() * img_in_buf.y_length() * 3
			     + img_in_buf.x_length() + img_in_buf.y_length() );

	wavelet_denoise( 3, img_in_buf.x_length(), img_in_buf.y_length(),
			 threshold, img_in_buf.array_ptr(),
			 img_work_buf.array_ptr() );
	
	/* create new filename */
	if ( bytes == 1 ) {
	    make_output_filename(filename.cstr(), "denoised", "8bit",
				 &filename_out);
	}
	else {
	    make_output_filename(filename.cstr(), "denoised", "16bit",
				 &filename_out);
	}

	if ( icc_buf.length() == 0 ) {
	    icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	    icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
	}

	/* check min, max and write a processed file */
	ptr = img_in_buf.array_ptr();
	if ( bytes == 1 && flag_output_8bit == true ) {
	    for ( j=0 ; j < img_in_buf.length() ; j++ ) {
		ptr[j] /= 256.0;
		if ( ptr[j] < 0 ) ptr[j] = 0.0;
		else if ( 255.0 < ptr[j] ) ptr[j] = 255.0;
	    }
	    sio.printf("Writing '%s' [8bit/ch] ", filename_out.cstr());
	    if ( flag_dither == true ) sio.printf("using dither ...\n");
	    else sio.printf("NOT using dither ...\n");
	    if ( write_float_to_tiff24or48(img_in_buf, 0.0, 255.0, flag_dither, 
				   icc_buf, NULL, filename_out.cstr()) < 0 ) {
		sio.eprintf("[ERROR] write_float_to_tiff24or48() failed\n");
		goto quit;
	    }
	}
	else {
	    for ( j=0 ; j < img_in_buf.length() ; j++ ) {
		if ( ptr[j] < 0 ) ptr[j] = 0.0;
		else if ( 65535.0 < ptr[j] ) ptr[j] = 65535.0;
	    }
	    sio.printf("Writing '%s' [16bit/ch] ", filename_out.cstr());
	    if ( flag_dither == true ) sio.printf("using dither ...\n");
	    else sio.printf("NOT using dither ...\n");
	    if ( write_float_to_tiff24or48(img_in_buf, 0.0, 65535.0, flag_dither,
				   icc_buf, NULL, filename_out.cstr()) < 0 ) {
		sio.eprintf("[ERROR] write_float_to_tiff24or48() failed\n");
		goto quit;
	    }
	}
	
    }
    
    
    return_status = 0;
 quit:
    return return_status;
}


#include "read_tiff24or48_to_float.cc"
#include "write_float_to_tiff24or48.cc"
