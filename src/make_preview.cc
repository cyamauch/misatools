/*
 * $ s++ make_preview.cc -ltiff
 */
#include <sli/stdstreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>
using namespace sli;

#include "read_tiff24or48.h"
#include "write_tiff24or48.h"
#include "make_output_filename.cc"
#include "icc_srgb_profile.c"

/**
 * @file   make_preview.cc
 * @brief  make preview image from raw TIFF image.
 *         8/16-bit images are supported.
 */

static int do_convert( const char *in_filename,
		       const double scale[], const long crop_prms[], bool binning, bool out_8bit )
{
    stdstreamio sio;
    size_t i, j, k;
    tstring filename_in, filename_out;
    size_t width = 0, height = 0;
    mdarray img_buf1(UCHAR_ZT,false);		/* 8/16-bit RGB image (in/out) */
    mdarray_uchar icc_buf(false);
    size_t x_out, y_out, width_out, height_out;	/* actual crop area */
    
    int ret_status = -1;

    filename_in = in_filename;
    
    /* read tiff-24or48-bit file and store its data to array */
    if ( read_tiff24or48( filename_in.cstr(), &img_buf1, &icc_buf ) < 0 ) {
        sio.eprintf("[ERROR] read_tiff24or48() failed\n");
	goto quit;
    }
    width = img_buf1.x_length();
    height = img_buf1.y_length();
    

    /* Adjust cropping parameters */
    
    if ( crop_prms[2] <= 0 ) width_out = width;
    else width_out = crop_prms[2];
    if ( width < width_out ) width_out = width;

    if ( crop_prms[3] <= 0 ) height_out = height;
    else height_out = crop_prms[3];
    if ( height < height_out ) height_out = height;
    
    if ( crop_prms[0] < 0 ) {	/* center crop */
        x_out = (width - width_out) / 2;
    }
    else {			/* crop with position */
        x_out = crop_prms[0];
	if ( width <= x_out ) x_out = width - 1;
	if ( width < x_out + width_out ) width_out = width - x_out;
    }

    if ( crop_prms[1] < 0 ) {	/* center crop */
        y_out = (height - height_out) / 2;
    }
    else {			/* crop with position */
        y_out = crop_prms[1];
	if ( height <= y_out ) y_out = height - 1;
	if ( height < y_out + height_out ) height_out = height - y_out;
    }

    /* crop */
    img_buf1.crop(1, y_out, height_out);
    img_buf1.crop(0, x_out, width_out);
    
    
    /* 2x2 binning */
    if ( binning == true ) {

	size_t width1, height1;

	width1 = img_buf1.x_length() / 2;
	height1 = img_buf1.y_length() / 2;

	if ( img_buf1.size_type() == UCHAR_ZT ) {
	    /* get 3-d array ptr */
	    unsigned char *const *const *img_arr1_ptr
		= (unsigned char *const *const *)(img_buf1.data_ptr_3d(true));

	    for ( k=0 ; k < 3 ; k++ ) {
		for ( i=0 ; i < height1 ; i++ ) {
		    size_t ii = i*2;
		    for ( j=0 ; j < width1 ; j++ ) {
			size_t jj = j*2;
			double v = img_arr1_ptr[k][ii][jj] + img_arr1_ptr[k][ii][jj+1] + 
			    img_arr1_ptr[k][ii+1][jj] + img_arr1_ptr[k][ii+1][jj+1];
			v /= 4.0;
			v += 0.5;
			v *= scale[k];
			if ( 255.0 < v ) v = 255.0;
			img_arr1_ptr[k][i][j] = (unsigned char)v;
		    }
		}
	    }
	}
	else if ( img_buf1.size_type() == FLOAT_ZT ) {
	    /* get 3-d array ptr */
	    float *const *const *img_arr1_ptr
		= (float *const *const *)(img_buf1.data_ptr_3d(true));

	    for ( k=0 ; k < 3 ; k++ ) {
		for ( i=0 ; i < height1 ; i++ ) {
		    size_t ii = i*2;
		    for ( j=0 ; j < width1 ; j++ ) {
			size_t jj = j*2;
			double v = img_arr1_ptr[k][ii][jj] + img_arr1_ptr[k][ii][jj+1] + 
			    img_arr1_ptr[k][ii+1][jj] + img_arr1_ptr[k][ii+1][jj+1];
			v /= 4.0;
			v += 0.5;
			v *= scale[k];
			if ( 65535.0 < v ) v = 65535.0;
			img_arr1_ptr[k][i][j] = (float)v;
		    }
		}
	    }
	}
	else {
	    sio.eprintf("[FATAL] unexpected error\n");
	    goto quit;
	}

	img_buf1.resize_3d(width1, height1, 3);
	
    }
    else {

	size_t width1, height1;

	width1 = img_buf1.x_length();
	height1 = img_buf1.y_length();

	if ( img_buf1.size_type() == UCHAR_ZT ) {
	    /* get 3-d array ptr */
	    unsigned char *const *const *img_arr1_ptr
		= (unsigned char *const *const *)(img_buf1.data_ptr_3d(true));

	    for ( k=0 ; k < 3 ; k++ ) {
		for ( i=0 ; i < height1 ; i++ ) {
		    for ( j=0 ; j < width1 ; j++ ) {
			double v = img_arr1_ptr[k][i][j];
			v *= scale[k];
			if ( 255.0 < v ) v = 255.0;
			img_arr1_ptr[k][i][j] = (unsigned char)v;
		    }
		}
	    }
	}
	else if ( img_buf1.size_type() == FLOAT_ZT ) {
	    /* get 3-d array ptr */
	    float *const *const *img_arr1_ptr
		= (float *const *const *)(img_buf1.data_ptr_3d(true));

	    for ( k=0 ; k < 3 ; k++ ) {
		for ( i=0 ; i < height1 ; i++ ) {
		    for ( j=0 ; j < width1 ; j++ ) {
			double v = img_arr1_ptr[k][i][j];
			v *= scale[k];
			if ( 65535.0 < v ) v = 65535.0;
			img_arr1_ptr[k][i][j] = (float)v;
		    }
		}
	    }
	}
	else {
	    sio.eprintf("[FATAL] unexpected error\n");
	    goto quit;
	}

    }

    if ( out_8bit == true && img_buf1.bytes() != 1 ) {
	img_buf1 /= 256.0;
	img_buf1.convert(UCHAR_ZT);
    }
    
    /* write image data file */
    
    if ( img_buf1.bytes() == 1 ) {
	make_output_filename(filename_in.cstr(), "preview", "8bit",
			     &filename_out);
    }
    else {
	make_output_filename(filename_in.cstr(), "preview", "16bit",
			     &filename_out);
    }
    
    sio.printf("Writing %s ...\n", filename_out.cstr());

    if ( icc_buf.length() == 0 ) {
	icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
    }
    
    if ( write_tiff24or48(img_buf1, icc_buf, filename_out.cstr()) < 0 ) {
        sio.eprintf("[ERROR] write_tiff24or48() failed\n");
	goto quit;
    }

    ret_status = 0;
 quit:
    return ret_status;
}

static int get_crop_prms( const char *opt, long prms[] )
{
    int return_status = -1;
    stdstreamio sio;
    size_t off, i;
    tstring optstr;
    long val[4];

    if ( opt == NULL ) goto quit;

    optstr = opt;
    
    off = 0;
    for ( i=0 ; ; i++ ) {
        size_t spn;
        spn = optstr.strspn(off,"0123456789");
	if ( spn == 0 ) val[i] = 0;
	else val[i] = optstr.atol(off,spn);
	if ( val[i] < 0 ) {
	    sio.eprintf("[ERROR] Invalid -c option\n");
	    goto quit;
	}
	off += spn;
	if ( i == 3 ) break;
	if ( optstr.length() <= off ) break;
	if ( optstr[off] != ',' ) {
	    sio.eprintf("[ERROR] Invalid -c option\n");
	    goto quit;
	}
	off ++;
    }
    if ( i == 0 ) {
        prms[0] = -1;
	prms[1] = -1;
	prms[2] = val[0];
	prms[3] = 0;
    }
    else if ( i == 1 ) {
        prms[0] = -1;
	prms[1] = -1;
	prms[2] = val[0];
	prms[3] = val[1];
    }
    else if ( i == 2 ) {
        prms[0] = val[0];
	prms[1] = -1;
	prms[2] = val[1];
	prms[3] = val[2];
    }
    else {
        prms[0] = val[0];
	prms[1] = val[1];
	prms[2] = val[2];
	prms[3] = val[3];
    }

    return_status = 0;
 quit:
    return return_status;
}


int main( int argc, char *argv[] )
{
    stdstreamio sio;
    
    double scale[3];			/* scale_r, scale_g, scale_b */
    bool binning = false;
    bool flag_output_8bit = false;
    long crop_prms[4] = {-1,-1,-1,-1};
    size_t i;
    
    tstring line_buf;
    int arg_cnt;
    
    int return_status = -1;

    if ( argc < 5 ) {
	sio.eprintf("Estimate center of object and output result as image\n");
	sio.eprintf("\n");
        sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s [-c param] [-h] scale_r scale_g scale_b filename.tiff\n",argv[0]);
	sio.eprintf("\n");
	sio.eprintf("-c [x,y,]width,height ... Crop images. Center when x and y are omitted.\n");
	sio.eprintf("-h ... Half-size (binning) image is written.\n");
	sio.eprintf("-8 ... If set, output 8-bit images\n");
	sio.eprintf("\n");
	sio.eprintf("example using binning:\n");
	sio.eprintf("$ %s -h 4.0 2.0 4.0 file1.tiff file2.tiff ...\n",argv[0]);
	goto quit;
    }
 
    arg_cnt = 1;

    while ( arg_cnt < argc ) {
	line_buf = argv[arg_cnt];
	if ( line_buf == "-h" ) {
	    binning = true;
	    arg_cnt ++;
	}
	else if ( line_buf == "-8" ) {
	    flag_output_8bit = true;
	    arg_cnt ++;
	}
	else if ( line_buf == "-c" ) {
	    arg_cnt ++;
	    if ( argv[arg_cnt] != NULL ) {
	        if ( get_crop_prms(argv[arg_cnt], crop_prms) < 0 ) {
		    sio.eprintf("[ERROR] get_crop_prms() failed\n");
		    goto quit;
		}
		arg_cnt ++;
	    }
	}
	else break;
    }

    for ( i=0 ; i < 3 ; i++ ) {
	line_buf = argv[arg_cnt];
	if ( line_buf.strspn("0123456789.") != line_buf.length() ) {
	    sio.eprintf("[ERROR] Invalid scale. Set real parameter\n");
	    goto quit;
	}
	scale[i] = line_buf.atof();
	if ( scale[i] < 0 ) {
	    sio.eprintf("[ERROR] Invalid scale. Set real parameter\n");
	    goto quit;
	}
	arg_cnt ++;
    }

    while ( arg_cnt < argc ) {
	const char *filename_in = argv[arg_cnt];
	if ( do_convert(filename_in, scale, crop_prms, binning, flag_output_8bit) < 0 ) {
	    sio.eprintf("[ERROR] do_convert() failed\n");
	    goto quit;
	}
	arg_cnt ++;
    }
    
    return_status = 0;
 quit:
    return return_status;
}

#include "read_tiff24or48.cc"
#include "write_tiff24or48.cc"
