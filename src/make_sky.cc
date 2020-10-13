#include <algorithm>
#include <sli/stdstreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>

#include "tiff_funcs.h"
using namespace sli;

/* Maximum byte length of 3-d image buffer to calculate sky values */
static const uint64_t Max_stat_buf_bytes = (uint64_t)500 * 1024 * 1024;

static int calc_sky_value( const mdarray &img_stat_buf,
			   mdarray *img_stat_buf_result )
{
    int ret_status = -1;
    
    mdarray tmp_buf;
    int sz_type;
    size_t z_len, xy_len, i, j;
    const unsigned char *uchar_p = NULL;
    const float *float_p = NULL;
    unsigned char *tmp_uchar_p = NULL;
    float *tmp_float_p = NULL;
    unsigned char *uchar_p_result = NULL;
    float *float_p_result = NULL;
    
    if ( img_stat_buf_result == NULL ) goto quit;

    sz_type = img_stat_buf.size_type();
    z_len = img_stat_buf.z_length();
    xy_len = img_stat_buf.x_length() * img_stat_buf.y_length();

    if ( img_stat_buf_result->size_type() != sz_type ) {
	img_stat_buf_result->init(sz_type, false);
    }
    if ( img_stat_buf_result->x_length() != img_stat_buf.x_length() ||
	 img_stat_buf_result->y_length() != img_stat_buf.y_length() ||
	 img_stat_buf_result->z_length() != 1 ||
	 img_stat_buf_result->dim_length() != 3 ) {
	img_stat_buf_result->resize_3d(img_stat_buf.x_length(),
				       img_stat_buf.y_length(),
				       1);
    }

    if ( sz_type == UCHAR_ZT ) {
	tmp_buf.init(UCHAR_ZT, false);
	tmp_buf.resize_1d(z_len);
	uchar_p = (const unsigned char *)img_stat_buf.data_ptr();
	uchar_p_result = (unsigned char *)img_stat_buf_result->data_ptr();
	tmp_uchar_p = (unsigned char *)tmp_buf.data_ptr();
    }
    else if ( sz_type == FLOAT_ZT ) {
	tmp_buf.init(FLOAT_ZT, false);
	tmp_buf.resize_1d(z_len);
	float_p = (const float *)img_stat_buf.data_ptr();
	float_p_result = (float *)img_stat_buf_result->data_ptr();
	tmp_float_p = (float *)tmp_buf.data_ptr();
    }
    else
	goto quit;
    
    if ( z_len == 2 ) {
	if ( uchar_p != NULL ) {
	    for ( i=0 ; i < xy_len ; i++ ) {
		if ( uchar_p[xy_len + i] < uchar_p[i] ) 
		    uchar_p_result[i] = uchar_p[xy_len + i];
		else
		    uchar_p_result[i] = uchar_p[i];
	    }
	}
	else {
	    for ( i=0 ; i < xy_len ; i++ ) {
		if ( float_p[xy_len + i] < float_p[i] ) 
		    float_p_result[i] = float_p[xy_len + i];
		else 
		    float_p_result[i] = float_p[i];
	    }
	}
    }
    else {
	/* optimization for SKY values */
	size_t selected_idx = 0;	/* z_len <= 3 */ /* o- o-- */
	/* o--- o----  */
	if ( z_len == 4 || z_len == 5 ) selected_idx = 0;
	/* -o---- -o-----  */
	if ( z_len == 6 || z_len == 7 ) selected_idx = 1;
	/* --o----- --o------ */
	if ( z_len == 8 || z_len == 9 ) selected_idx = 2;
	/* --o------- */
	else selected_idx = (size_t)(z_len * 0.2);
	
	if ( uchar_p != NULL ) {
	    for ( i=0 ; i < xy_len ; i++ ) {
		for ( j=0 ; j < z_len ; j++ ) {
		    tmp_uchar_p[j] = uchar_p[xy_len * j + i];
		}
		std::sort(tmp_uchar_p, tmp_uchar_p + z_len);
		uchar_p_result[i] = tmp_uchar_p[selected_idx];
	    }
	}
	else {
	    for ( i=0 ; i < xy_len ; i++ ) {
		for ( j=0 ; j < z_len ; j++ ) {
		    tmp_float_p[j] = float_p[xy_len * j + i];
		}
		std::sort(tmp_float_p, tmp_float_p + z_len);
		float_p_result[i] = tmp_float_p[selected_idx];
	    }
	}
    }
    
    ret_status = 0;
 quit:
    return ret_status;
}

int main( int argc, char *argv[] )
{
    stdstreamio sio;

    mdarray img_load_buf(UCHAR_ZT,false);	/* RGB: load a image */
    mdarray_uchar icc_buf(false);
    mdarray result_buf(UCHAR_ZT,false);		/* RGB */
    mdarray img_stat_buf(UCHAR_ZT,false);	/* Single-band */
    mdarray img_stat_buf_result(UCHAR_ZT,false);
    tarray_tstring filenames_in;
    const char *filename_in;
    tstring filename_out = "";
    const char *rgb_str[] = {"R","G","B"};
    
    size_t i, j, k, k_step, width, height;
    int sz_type, tiff_szt;
    uint64_t nbytes_img_stat_full;
    
    int return_status = -1;

    if ( argc < 2 ) {
        sio.eprintf("Create master sky frame\n");
        sio.eprintf("\n");
        sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s [-o output] sky_1.tiff sky_2.tiff ...\n",argv[0]);
	sio.eprintf("NOTE: Default filename of output is 'sky.[8bit|16bit|float].tiff'.\n");
	goto quit;
    }

    filenames_in = argv;
    filenames_in.erase(0, 1);	/* erase command name */

    if ( filenames_in[0] == "-o" ) {
	filename_out = filenames_in[1];
	filenames_in.erase(0, 2);
    }
    
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
    else if ( tiff_szt == 2 ) sio.printf("Found an 16-bit RGB image\n");
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
    img_stat_buf_result.init(sz_type, false);
    img_stat_buf_result.resize_3d(width, k_step, 1);
    
    for ( j=0 ; j < 3 ; j++ ) {					/* R,G,B */
	sio.printf("Starting channel [%s]\n",rgb_str[j]);
	for ( k=0 ; k < height ; k+=k_step ) {			/* block[y] */
	    sio.printf(" y of section = %zd\n",k);
	    for ( i=0 ; i < filenames_in.length() ; i++ ) {	/* files */
		int tiff_szt0;
		filename_in = filenames_in[i].cstr();
		sio.printf("  Reading %s\n",filename_in);
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
	    /* Calculate sky values and paste it to result_buf */
	    sio.printf(" Calculating sky values ...\n");
	    calc_sky_value(img_stat_buf, &img_stat_buf_result);
	    result_buf.paste(img_stat_buf_result, 0, k, j);
	}
    }

    /* freeing buffer */
    img_load_buf.init(sz_type, false);
    img_stat_buf.init(sz_type, false);

    if ( icc_buf.length() == 0 ) {
	icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
    }
    
    if ( filename_out.length() == 0 ) {
	if ( tiff_szt == 1 ) filename_out="sky.8bit.tiff";
	else if ( tiff_szt == 2 ) filename_out="sky.16bit.tiff";
	else filename_out="sky.float.tiff";
    }
    
    sio.printf("Writing %s ...\n", filename_out.cstr());

    if ( tiff_szt < 0 ) {
	if ( save_float_to_tiff(result_buf, icc_buf, NULL,
				1.0, filename_out.cstr()) < 0 ) {
	    sio.eprintf("[ERROR] save_float_to_tiff() failed\n");
	    goto quit;
	}
    }
    else {
	if ( save_tiff(result_buf, tiff_szt, icc_buf, NULL,
		       filename_out.cstr()) < 0 ) {
	    sio.eprintf("[ERROR] save_tiff() failed\n");
	    goto quit;
	}
    }
  
    return_status = 0;
 quit:
    return return_status;
}
