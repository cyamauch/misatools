#include <sli/stdstreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>

#include "tiff_funcs.h"
using namespace sli;

/**
 * @file   make_dark.cc
 * @brief  construct a master dark image.
 *         8/16-bit integer and 32-bit float images are supported.
 */

/* Maximum byte length of 3-d image buffer to get median */
static const uint64_t Max_stat_buf_bytes = (uint64_t)200 * 1024 * 1024;

int main( int argc, char *argv[] )
{
    stdstreamio sio;

    mdarray img_load_buf(UCHAR_ZT,false);	/* RGB: load a image */
    mdarray_uchar icc_buf(false);
    mdarray result_buf(UCHAR_ZT,false);		/* RGB */
    mdarray img_stat_buf(UCHAR_ZT,false);	/* Single-band */
    tarray_tstring filenames_in;
    const char *filename_in;
    const char *filename_out = "dark.tiff";
    const char *rgb_str[] = {"R","G","B"};
    
    size_t i, j, k, k_step, width, height;
    int sz_type, tiff_szt;
    uint64_t nbytes_img_stat_full;
    
    int return_status = -1;
    
    if ( argc < 2 ) {
        sio.eprintf("Create master dark frame using median combine\n");
        sio.eprintf("\n");
        sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s dark_1.tiff dark_2.tiff ...\n",argv[0]);
	sio.eprintf("NOTE: Filename of output is '%s'.\n", filename_out);
	goto quit;
    }

    filenames_in = argv;
    filenames_in.erase(0, 1);	/* erase command name */

    filename_in = filenames_in[0].cstr();
    if ( load_tiff(filename_in, &img_load_buf, &tiff_szt,
		   &icc_buf, NULL) < 0 ) {
	sio.eprintf("[ERROR] load_tiff() failed\n");
	goto quit;
    }
    width = img_load_buf.x_length();
    height = img_load_buf.y_length();
    sz_type = img_load_buf.size_type();
    //sio.printf("sz_type = %d\n",(int)sz_type);
    if ( tiff_szt == 1 ) sio.printf("Found an 8-bit RGB image\n");
    else if ( tiff_szt == 2 ) sio.printf("Found a 16-bit RGB image\n");
    else sio.printf("Found a 32-bit float RGB image\n");
    
    result_buf.init(sz_type, false);
    result_buf.resize_3d(width, height, 3);

    /* Calculate k_step */
    //sio.printf("bytes = %zd\n",img_load_buf.bytes());
    nbytes_img_stat_full = img_load_buf.bytes();
    nbytes_img_stat_full *= width;
    nbytes_img_stat_full *= height;
    nbytes_img_stat_full *= filenames_in.length();
    if ( Max_stat_buf_bytes < nbytes_img_stat_full ) {
	size_t n_k = (nbytes_img_stat_full - 1) / Max_stat_buf_bytes;
	k_step = height / (n_k + 1);
	k_step ++;
    }
    else {
	k_step = height;
    }
    //sio.printf("k_step = %zd\n",k_step);
    
    img_stat_buf.init(sz_type, false);
    img_stat_buf.resize_3d(width, k_step, filenames_in.length());
    
    for ( j=0 ; j < 3 ; j++ ) {					/* R,G,B */
	sio.printf("Starting channel [%s]\n",rgb_str[j]);
	for ( k=0 ; k < height ; k+=k_step ) {			/* block[y] */
	    sio.printf(" y of section = %zd\n",k);
	    for ( i=0 ; i < filenames_in.length() ; i++ ) {	/* files */
		int tiff_szt0;
		filename_in = filenames_in[i].cstr();
		sio.printf("  Reading %s\n",filename_in);
		img_load_buf.init(sz_type, false);
		if ( load_tiff(filename_in, &img_load_buf, &tiff_szt0,
			       NULL, NULL) < 0 ) {
		    sio.eprintf("[ERROR] load_tiff() failed\n");
		    goto quit;
		}
		if ( tiff_szt0 != tiff_szt ) {
		    sio.eprintf("[ERROR] invalid type of image: %s\n",
				filename_in);
		    goto quit;
		}
		img_load_buf.crop(2 /* dim */, j /* R,G or B */, 1);	/* z */
		img_load_buf.crop(1, k, k_step);			/* y */
		img_stat_buf.paste(img_load_buf, 0, 0, i /* layer = file No. */);
	    }
	    //img_stat_buf.dprint();
	    /* Get median and paste it to result_buf */
	    sio.printf(" Calculating median ...\n");
	    result_buf.paste(md_median_small_z(img_stat_buf), 0, k, j);
	}
    }

    /* freeing buffer */
    img_load_buf.init(sz_type, false);
    img_stat_buf.init(sz_type, false);

    sio.printf("Writing %s ...\n", filename_out);

    if ( icc_buf.length() == 0 ) {
	icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
    }

    if ( tiff_szt < 0 ) {
	if ( save_float_to_tiff(result_buf, icc_buf,
				NULL, 1.0, filename_out) < 0 ) {
	    sio.eprintf("[ERROR] save_float_to_tiff() failed\n");
	    goto quit;
	}
    }
    else {
	if ( save_tiff(result_buf, tiff_szt, icc_buf,
		       NULL, filename_out) < 0 ) {
	    sio.eprintf("[ERROR] save_tiff() failed\n");
	    goto quit;
	}
    }
  
    return_status = 0;
 quit:
    return return_status;
}
