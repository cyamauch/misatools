#include <sli/stdstreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>

#include "tiff_funcs.h"

using namespace sli;

#include "wavelet_denoise.c"

/**
 * @file   denoise_images.cc
 * @brief  a command-line tool to reduce noise.
 *         8/16-bit integer and 32-bit float images are supported.
 */

int main( int argc, char *argv[] )
{
    stdstreamio sio, f_in;
    tarray_tstring filenames_in;
    tstring suffix_denoised;
    mdarray_float img_in_buf(false);
    mdarray_float img_work_buf(false);
    mdarray_uchar icc_buf(false);
    float camera_calibration1[12];			/* for TIFF tag */

    bool flag_output_8bit = false;
    bool flag_output_16bit = false;
    bool flag_dither = true;
    float threshold[3] = {0,0,0};	/* for R,G and B each */
    bool flag_threshold = false;

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
	sio.eprintf("$ %s [-8] [-16] [-t] -l threshold img_0.tiff img_1.tiff ...\n", argv[0]);
	sio.eprintf("$ %s [-8] [-16] [-t] -l threshold_r,threshold_g,threshold_b img_0.tiff img_1.tiff ...\n", argv[0]);
	sio.eprintf("-8 ... If set, output 8-bit processed images for 8-bit original images\n");
	sio.eprintf("-16 .. If set, output 16-bit processed images\n");
	sio.eprintf("       If neither '-8' nor '-16' is set, output 32-bit float processed images\n");
	sio.eprintf("-l param ... Threshold of wavelet-denoise\n");
	sio.eprintf("-t ... If set, dither is not used to output 8/16-bit images\n");
	sio.eprintf("\n");
	sio.eprintf("NOTE: Set large threshold for noisy images\n");
	sio.eprintf("\n");
	sio.eprintf("[EXAMPLE]\n");
	sio.eprintf("$ %s -16 -l 300,300,600 foo.tiff\n", argv[0]);
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
	else if ( argstr == "-16" ) {
	    flag_output_16bit = true;
	    arg_cnt ++;
	}
	else if ( argstr == "-t" ) {
	    flag_dither = false;
	    arg_cnt ++;
	}
	else if ( argstr == "-l" ) {
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
	    if ( threshold[0] == threshold[1] &&
		 threshold[0] == threshold[2] ) {
		suffix_denoised.printf("denoised%g",(double)threshold[0]);
	    }
	    else {
		suffix_denoised.printf("denoised%g-%g-%g",
		       (double)threshold[0],(double)threshold[1],(double)threshold[2]);
	    }
	    flag_threshold = true;
	    /* */
	    sio.printf("Using threshold(r,g,b): %g,%g,%g\n",
		       (double)threshold[0],(double)threshold[1],(double)threshold[2]);
	    arg_cnt ++;
	}
	else {
	    break;
	}
    }

    if ( flag_threshold == false ) {
	sio.eprintf("[ERROR] Set -l param arg\n");
	goto quit;
    }

    filenames_in.erase(0, arg_cnt);	/* erase */
    //filenames_in.dprint();


    for ( i=0 ; i < filenames_in.length() ; i++ ) {
	int tiff_szt = 0;
	tstring filename, filename_out;
	float *ptr;
	float min_value = 0.0;
	size_t j;

	filename = filenames_in[i];
	sio.printf("Loading %s\n", filename.cstr());
	if ( load_tiff_into_float(filename.cstr(), 65536.0,
		    &img_in_buf, &tiff_szt, &icc_buf, camera_calibration1) < 0 ) {
	    sio.eprintf("[ERROR] cannot load '%s'\n", filename.cstr());
	    sio.eprintf("[ERROR] load_tiff_into_float() failed\n");
	    goto quit;
	}

	sio.printf("Applying wavelet denoising ...\n");

	img_work_buf.resize( img_in_buf.x_length() * img_in_buf.y_length() * 3
			     + img_in_buf.x_length() + img_in_buf.y_length() );

	/* replace NaN/Inf values ... */
	ptr = img_in_buf.array_ptr();
	for ( j=0 ; j < img_in_buf.length() ; j++ ) {
	    if ( isfinite(ptr[j]) != 0 ) {
		min_value = ptr[j];
		break;
	    }
	}
	for ( ; j < img_in_buf.length() ; j++ ) {
	    if ( isfinite(ptr[j]) != 0 ) {
		if ( ptr[j] < min_value ) min_value = ptr[j];
	    }
	}
	for ( j=0 ; j < img_in_buf.length() ; j++ ) {
	    if ( isfinite(ptr[j]) == 0 ) {
		ptr[j] = min_value;
	    }
	}

	wavelet_denoise( 3, img_in_buf.x_length(), img_in_buf.y_length(),
			 threshold, img_in_buf.array_ptr(),
			 img_work_buf.array_ptr() );
	
	if ( icc_buf.length() == 0 ) {
	    icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	    icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
	}

	/* check min, max and write a processed file */
	ptr = img_in_buf.array_ptr();
	if ( flag_output_16bit == true ) {
	    make_tiff_filename(filename.cstr(), suffix_denoised.cstr(), "16bit",
			       &filename_out);
	    for ( j=0 ; j < img_in_buf.length() ; j++ ) {
		if ( ptr[j] < 0 ) ptr[j] = 0.0;
		else if ( 65535.0 < ptr[j] ) ptr[j] = 65535.0;
	    }
	    sio.printf("Writing '%s' [16bit/ch] ", filename_out.cstr());
	    if ( flag_dither == true ) sio.printf("using dither ...\n");
	    else sio.printf("NOT using dither ...\n");
	    if ( save_float_to_tiff24or48(img_in_buf,
			icc_buf, camera_calibration1,
			0.0, 65535.0, flag_dither, filename_out.cstr()) < 0 ) {
		sio.eprintf("[ERROR] save_float_to_tiff24or48() failed\n");
		goto quit;
	    }
	}
	else if ( tiff_szt == 1 && flag_output_8bit == true ) {
	    make_tiff_filename(filename.cstr(), suffix_denoised.cstr(), "8bit",
			       &filename_out);
	    for ( j=0 ; j < img_in_buf.length() ; j++ ) {
		ptr[j] /= 256.0;
		if ( ptr[j] < 0 ) ptr[j] = 0.0;
		else if ( 255.0 < ptr[j] ) ptr[j] = 255.0;
	    }
	    sio.printf("Writing '%s' [8bit/ch] ", filename_out.cstr());
	    if ( flag_dither == true ) sio.printf("using dither ...\n");
	    else sio.printf("NOT using dither ...\n");
	    if ( save_float_to_tiff24or48(img_in_buf,
			  icc_buf, camera_calibration1, 
			  0.0, 255.0, flag_dither, filename_out.cstr()) < 0 ) {
		sio.eprintf("[ERROR] save_float_to_tiff24or48() failed\n");
		goto quit;
	    }
	}
	else {
	    make_tiff_filename(filename.cstr(), suffix_denoised.cstr(), "float",
			       &filename_out);
	    sio.printf("Writing '%s' [32-bit_float/ch]\n", filename_out.cstr());
	    if ( save_float_to_tiff(img_in_buf, icc_buf, camera_calibration1, 
				    65536.0, filename_out.cstr()) < 0 ) {
		sio.eprintf("[ERROR] save_float_to_tiff() failed\n");
		goto quit;
	    }
	}
	
    }
    
    
    return_status = 0;
 quit:
    return return_status;
}
