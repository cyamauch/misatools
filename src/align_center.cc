/*
 * $ s++ align_center.cc -ltiff
 */
#include <sli/stdstreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>

#include "tiff_funcs.h"
#include "image_funcs.h"

using namespace sli;

/**
 * @file   align_center.cc
 * @brief  estimate center of object and output aligned image.
 *         8/16-bit integer and 32-bit float images are supported.
 */

/* for qsort() arg */
static int compar_fnc( const void *_a, const void *_b )
{
    const float *a = (const float *)_a;
    const float *b = (const float *)_b;
    /* read pixel vals */
    if ( *a < *b ) return 1;
    else if ( *b < *a ) return -1;
    else return 0;
}

static int do_align( const char *in_filename,
		     long z_select, long object_diameter,
		     const long crop_prms[], int scale, bool binning )
{
    stdstreamio sio, f_in;

    mdarray img_buf0(UCHAR_ZT,false);		/* 8/16-bit RGB image (in) */
    mdarray img_buf1(UCHAR_ZT,false);		/* 8/16-bit RGB image (out) */
    mdarray_float arr_stat(false);
    mdarray_uchar icc_buf(false);
    tstring filename_in, filename_out;
    tarray_tstring sarr_buf;
    float camera_calibration1[12];			/* for TIFF tag */

    size_t width = 0, height = 0;
    size_t obj_x_cen, obj_y_cen;
    size_t x_out, y_out, width_out, height_out;	/* actual crop area */
    int tiff_szt = 0;
    size_t i, j, k;
    
    int ret_status = -1;

    filename_in = in_filename;
    
    /* read tiff-24or48-bit file and store its data to array */
    if ( load_tiff( filename_in.cstr(), &img_buf0, &tiff_szt,
			  &icc_buf, camera_calibration1 ) < 0 ) {
        sio.eprintf("[ERROR] load_tiff() failed\n");
	goto quit;
    }

    if ( 1 < scale ) {

	if ( scale_image( scale, &img_buf0 ) < 0 ) {
	    sio.eprintf("[ERROR] scale_image() failed\n");
	    goto quit;
	}

	object_diameter *= scale;

    }
    width = img_buf0.x_length();
    height = img_buf0.y_length();
    
    img_buf1.init(img_buf0.size_type(), false);
    img_buf1.resize_3d(width,height,3);

    //img_buf0.dprint();


    if ( width < (size_t)object_diameter ) {
	object_diameter = width;
        sio.eprintf("[WARNING] Too large object diameter, changed: %ld\n",
		    object_diameter);
    }

    if ( height < (size_t)object_diameter ) {
	object_diameter = height;
        sio.eprintf("[WARNING] Too large object diameter, changed: %ld\n",
		    object_diameter);
    }

    
    /* copy array data 0 -> 1: one-channel only */
    img_buf1 = img_buf0;
    img_buf1.crop(2, z_select, 1);
    
    /* calculate pseudo center */

    /* get statistics and estimate center of x,y */

    arr_stat = md_total_x(img_buf1);	/* 2D -> 1D with stacking x */
    arr_stat.resize(0, 2);
    for ( i=0 ; i < arr_stat.y_length() ; i++ ) {
      arr_stat(1,i) = i;
    }
    /* sort by pixel val */
    qsort(arr_stat.array_ptr(), arr_stat.y_length(), 
	  sizeof(float) * 2, &compar_fnc);
    //arr_stat.dprint();

    arr_stat.resize(1,object_diameter);	/* limit with object size */
    arr_stat.erase(0,0,1);		/* erase pixel vals */
    //arr_stat.dprint();

    /* get median of positions having brightest pixels */
    obj_y_cen = md_median(arr_stat);
    
    
    arr_stat = md_total_y(img_buf1);
    arr_stat.rotate_xy(90);
    arr_stat.resize(0, 2);
    for ( i=0 ; i < arr_stat.y_length() ; i++ ) {
      arr_stat(1,i) = i;
    }
    qsort(arr_stat.array_ptr(), arr_stat.y_length(), 
	  sizeof(float) * 2, &compar_fnc);
    //arr_stat.dprint();

    arr_stat.resize(1,object_diameter);
    arr_stat.erase(0,0,1);
    //arr_stat.dprint();

    /* get median of positions having brightest pixels */
    obj_x_cen = md_median(arr_stat);
    
    
    sio.printf("Estimated center of object = %zd, %zd\n",obj_x_cen,obj_y_cen);

    
    /* copy array data 0 -> 1 */
    img_buf1 = img_buf0;

    /* adjuct object position */
    img_buf1.paste(img_buf0,
		   (ssize_t)(width/2) - (ssize_t)obj_x_cen,
		   (ssize_t)(height/2) - (ssize_t)obj_y_cen,
		   0);
    

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

	if ( tiff_szt == 1 ) {
	    /* get 3-d array ptr */
	    unsigned char *const *const *img_arr1_ptr
		= (unsigned char *const *const *)(img_buf1.data_ptr_3d(true));

	    for ( i=0 ; i < height1 ; i++ ) {
		size_t ii = i*2;
		for ( j=0 ; j < width1 ; j++ ) {
		    size_t jj = j*2;
		    for ( k=0 ; k < 3 ; k++ ) {
			double v = img_arr1_ptr[k][ii][jj] + img_arr1_ptr[k][ii][jj+1] + 
			    img_arr1_ptr[k][ii+1][jj] + img_arr1_ptr[k][ii+1][jj+1];
			v /= 4.0;
			v += 0.5;
			if ( 255.0 < v ) v = 255.0;
			img_arr1_ptr[k][i][j] = (unsigned char)v;
		    }
		}
	    }
	}
	else if ( tiff_szt == 2 ) {
	    /* get 3-d array ptr */
	    float *const *const *img_arr1_ptr
		= (float *const *const *)(img_buf1.data_ptr_3d(true));

	    for ( i=0 ; i < height1 ; i++ ) {
		size_t ii = i*2;
		for ( j=0 ; j < width1 ; j++ ) {
		    size_t jj = j*2;
		    for ( k=0 ; k < 3 ; k++ ) {
			double v = img_arr1_ptr[k][ii][jj] + img_arr1_ptr[k][ii][jj+1] + 
			    img_arr1_ptr[k][ii+1][jj] + img_arr1_ptr[k][ii+1][jj+1];
			v /= 4.0;
			v += 0.5;
			if ( 65535.0 < v ) v = 65535.0;
			img_arr1_ptr[k][i][j] = (float)v;
		    }
		}
	    }
	}
	else if ( tiff_szt == -4 ) {
	    /* get 3-d array ptr */
	    float *const *const *img_arr1_ptr
		= (float *const *const *)(img_buf1.data_ptr_3d(true));

	    for ( i=0 ; i < height1 ; i++ ) {
		size_t ii = i*2;
		for ( j=0 ; j < width1 ; j++ ) {
		    size_t jj = j*2;
		    for ( k=0 ; k < 3 ; k++ ) {
			double v = img_arr1_ptr[k][ii][jj] + img_arr1_ptr[k][ii][jj+1] + 
			    img_arr1_ptr[k][ii+1][jj] + img_arr1_ptr[k][ii+1][jj+1];
			v /= 4.0;
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

    /* write image data file */
    
    if ( icc_buf.length() == 0 ) {
	icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
    }
    
    if ( tiff_szt == 1 ) {
	make_tiff_filename(filename_in.cstr(), "centered", "8bit",
			   &filename_out);
	sio.printf("Writing %s ...\n", filename_out.cstr());
	if ( save_tiff(img_buf1, tiff_szt,
		     icc_buf, camera_calibration1, filename_out.cstr()) < 0 ) {
	    sio.eprintf("[ERROR] save_tiff() failed\n");
	    goto quit;
	}
    }
    else if ( tiff_szt == 2 ) {
	make_tiff_filename(filename_in.cstr(), "centered", "16bit",
			   &filename_out);
	sio.printf("Writing %s ...\n", filename_out.cstr());
	if ( save_tiff(img_buf1, tiff_szt,
		     icc_buf, camera_calibration1, filename_out.cstr()) < 0 ) {
	    sio.eprintf("[ERROR] save_tiff() failed\n");
	    goto quit;
	}
    }
    else {
	make_tiff_filename(filename_in.cstr(), "centered", "float",
			   &filename_out);
	sio.printf("Writing %s ...\n", filename_out.cstr());
	if ( save_float_to_tiff(img_buf1, icc_buf, camera_calibration1,
				 1.0, filename_out.cstr()) < 0 ) {
	    sio.eprintf("[ERROR] save_float_to_tiff() failed\n");
	    goto quit;
	}
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
    
    int scale = 1;
    long z_select = 1;			/* 0..R  1..G  2..B */
    long object_diameter = 128;		/* diameter of object (pixels) */
    bool binning = false;
    long crop_prms[4] = {-1,-1,-1,-1};
    
    tstring line_buf;
    int arg_cnt;
    const char *rgb_str[3] = {"Red","Green","Blue"};
    
    int return_status = -1;

    if ( argc < 3 ) {
	sio.eprintf("Estimate center of object and output result as image\n");
	sio.eprintf("\n");
        sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s [-b r,g or b] [-s scale] [-c param] [-h] diameter_of_object(pixels) filename.tiff\n",argv[0]);
	sio.eprintf("\n");
	sio.eprintf("-b r,g or b ... Band (channel) selection. (default: g).\n");
	sio.eprintf("-s scale    ... Scaling factor (1 < scale ; integer) of images for estimating\n");
	sio.eprintf("                center of object. This arg is also effective for output images.\n");
	sio.eprintf("-c [x,y,]width,height ... Crop images.  Center when x and y are omitted.\n");
	sio.eprintf("                          Cropping will be performed after scaling.\n");
	sio.eprintf("-h          ... Half-size (binning) image is written.\n");
	sio.eprintf("                Binning will be performed after scaling and cropping.\n");
	sio.eprintf("\n");
	sio.eprintf("Note that diameter_of_object is the size of square inscribed in the object in\n");
	sio.eprintf("the original image (before rescaling/binning).\n");
	sio.eprintf("\n");
	sio.eprintf("example of G-channel, object diameter of 128-pixels and binning:\n");
	sio.eprintf("$ %s -h -b g 128 file1.tiff file2.tiff ...\n",argv[0]);
	sio.eprintf("example of G-channel, object diameter of 128-pixels and 2x rescaling:\n");
	sio.eprintf("$ %s -b g -s 2 128 file1.tiff file2.tiff ...\n",argv[0]);
	goto quit;
    }
 
    arg_cnt = 1;

    while ( arg_cnt < argc ) {
	line_buf = argv[arg_cnt];
	if ( line_buf == "-h" ) {
	    binning = true;
	    arg_cnt ++;
	}
	else if ( line_buf == "-s" ) {
	    arg_cnt ++;
	    line_buf = argv[arg_cnt];
	    scale = line_buf.atoi();
	    if ( scale < 1 ) {
		sio.eprintf("[ERROR] Invalid scale: %d\n", scale);
		goto quit;
	    }
	    arg_cnt ++;
	}
	else if ( line_buf == "-b" ) {
	    int ch;
	    arg_cnt ++;
	    line_buf = argv[arg_cnt];
	    ch = line_buf[0];
	    if ( ch == 'r' || ch == 'R' ) z_select = 0;
	    else if ( ch == 'g' || ch == 'G' ) z_select = 1;
	    else if ( ch == 'b' || ch == 'B' ) z_select = 2;
	    else {
		sio.eprintf("[ERROR] Invalid arg: %s\n", argv[arg_cnt]);
		goto quit;
	    }
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

    line_buf = argv[arg_cnt];
    if ( line_buf.strspn("0123456789") != line_buf.length() ) {
        sio.eprintf("[ERROR] Invalid object diameter (arg 2). Set integer\n");
	goto quit;
    }
    object_diameter = line_buf.atol();
    if ( object_diameter < 1 ) {
        sio.eprintf("[ERROR] Invalid object diameter (arg 2). Set integer\n");
	goto quit;
    }
    arg_cnt ++;

    sio.printf("Using %s channel\n", rgb_str[z_select]);
    
    while ( arg_cnt < argc ) {
	const char *filename_in = argv[arg_cnt];
	if ( do_align(filename_in,
	      z_select, object_diameter, crop_prms, scale, binning) < 0 ) {
	    sio.eprintf("[ERROR] do_align() failed\n");
	    goto quit;
	}
	arg_cnt ++;
    }
    
    return_status = 0;
 quit:
    return return_status;
}
