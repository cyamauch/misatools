#include <sli/stdstreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>
using namespace sli;

#include "read_tiff24or48_to_float.h"
#include "write_float_to_tiff24or48.h"
#include "write_float_to_tiff.h"
#include "icc_srgb_profile.c"

/* Maximum byte length of 3-d image buffer to get median */
static const uint64_t Max_stat_buf_bytes = (uint64_t)500 * 1024 * 1024;

int main( int argc, char *argv[] )
{
    stdstreamio sio, f_in;

    mdarray_float img_dark_buf(false);		/* RGB: load a master dark */
    mdarray_float img_load_buf(false);		/* RGB: load a image */
    mdarray_uchar icc_buf(false);
    mdarray_float result_buf(false);		/* RGB: result */
    mdarray_float img_stat_buf(false);		/* Single-band */
    mdarray_double median_each(false);
    tarray_tstring darkfile_list;
    tarray_tstring filenames_in;
    const char *filename_dark = "dark.tiff";
    const char *filename_dark_list = "dark.txt";
    const char *filename_out[4] = {"flat_r.tiff","flat_g.tiff","flat_b.tiff",
				   "flat.tiff"};
    const char *filename_out_float[4] = {
		"flat_r.float.tiff","flat_g.float.tiff","flat_b.float.tiff",
		"flat.float.tiff"};
    const char *rgb_str[] = {"R","G","B"};
    
    size_t i, j, k, cnt_dark, k_step, width, height;
    int target_channel = 3;
    bool flag_float_tiff = true;
    bool flag_dither = true;
    double flat_pow = 1.0;
    uint64_t nbytes_img_stat_full;
    float *ptr;

    int arg_cnt;
    
    int return_status = -1;
    
    if ( argc < 2 ) {
	sio.eprintf("Create master flat frame using median combine.\n");
	sio.eprintf("Master dark '%s' is required.\n", filename_dark);
	sio.eprintf("\n");
        sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s [-b r,g or b] flat_1.tiff flat_2.tiff ...\n",
		    argv[0]);
	sio.eprintf("\n");
	sio.eprintf("-i ... output integer tiff (default: float tiff)\n");
	sio.eprintf("-b r,g or b ... If set, create single band (channel) flat\n");
	sio.eprintf("-t ... If set, output truncated real without dither\n");
	/*
	sio.eprintf("-x param\n");
	sio.eprintf("   param=1.0: normal flat\n");
	sio.eprintf("   param=0.5: flat with pow(v,0.5) when v < 1.0\n");
	*/
	goto quit;
    }

    filenames_in = argv;

    arg_cnt = 1;

    while ( arg_cnt < argc ) {
	tstring argstr;
	argstr = argv[arg_cnt];
	if ( argstr == "-b" ) {
	    int ch;
	    arg_cnt ++;
	    argstr = argv[arg_cnt];
	    ch = argstr[0];
	    if ( ch == 'r' || ch == 'R' ) target_channel = 0;
	    else if ( ch == 'g' || ch == 'G' ) target_channel = 1;
	    else if ( ch == 'b' || ch == 'B' ) target_channel = 2;
	    else {
		sio.eprintf("[ERROR] Invalid arg: %s\n", argv[arg_cnt]);
		goto quit;
	    }
	    arg_cnt ++;
	}
	else if ( argstr == "-i" ) {
	    flag_float_tiff = false;
	    arg_cnt ++;
	}
	else if ( argstr == "-t" ) {
	    flag_dither = false;
	    arg_cnt ++;
	}
	else if ( argstr == "-x" ) {	/* experiment */
	    arg_cnt ++;
	    argstr = argv[arg_cnt];
	    flat_pow = argstr.atof();
	    if ( flat_pow < 1.0 ) {
		sio.printf("Selected flat with pow(v,%g) when v < 1.0\n",
			   flat_pow);
	    }
	    else {
		sio.printf("Selected normal flat\n");
	    }
	    arg_cnt ++;
	}
	else {
	    break;
	}
    }
    
    filenames_in.erase(0, arg_cnt);	/* erase */
    median_each.resize_1d(filenames_in.length());
    
    /* check dark file */
    if ( f_in.open("r", filename_dark_list) == 0 ) {
	tstring line;
	while ( (line=f_in.getline()) != NULL ) {
	    line.trim(" \t\n\r\f\v*");
	    if ( 1 <= line.length() ) {
		darkfile_list.append(line,1);
	    }
	}
	f_in.close();
	if ( 0 < darkfile_list.length() ) {
	    sio.printf("List of dark files to apply processing:\n");
	    for ( i=0 ; i < darkfile_list.length() ; i++ ) {
		sio.printf(" %s\n",darkfile_list[i].cstr());
	    }
	    /* update */
	    filename_dark = darkfile_list[0].cstr();
	}
    }

    sio.printf("Loading %s\n", filename_dark);
    if ( read_tiff24or48_to_float(filename_dark,
				  &img_dark_buf, &icc_buf, NULL, NULL) < 0 ) {
	sio.eprintf("[ERROR] cannot load dark\n");
	sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	goto quit;
    }
    width = img_dark_buf.x_length();
    height = img_dark_buf.y_length();
    
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
    
    img_stat_buf.resize_3d(width, k_step, filenames_in.length());
    
    cnt_dark = 0;
    for ( j=0 ; j < 3 ; j++ ) {					/* R,G,B */
      if ( 3 <= target_channel || (int)j == target_channel ) {
	sio.printf("Starting channel [%s]\n",rgb_str[j]);
	for ( k=0 ; k < height ; k+=k_step ) {			/* block[y] */
	    sio.printf(" y of section = %zd\n",k);
	    for ( i=0 ; i < filenames_in.length() ; i++ ) {	/* files */
		const char *filename_in = filenames_in[i].cstr();
		//size_t l;
		sio.printf("  Reading %s\n",filename_in);
		if ( read_tiff24or48_to_float(filename_in,
					 &img_load_buf, &icc_buf, NULL, NULL) < 0 ) {
		    sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
		    goto quit;
		}
		/* Subtract dark */
		if ( 0 < darkfile_list.length() ) {
		    filename_dark =
		       darkfile_list[cnt_dark % darkfile_list.length()].cstr();
		    sio.printf("  Reading %s\n", filename_dark);
		    if ( read_tiff24or48_to_float(filename_dark,
					 &img_dark_buf, &icc_buf, NULL, NULL) < 0 ) {
			sio.eprintf("[ERROR] cannot load dark\n");
			sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
			goto quit;
		    }
		    cnt_dark ++;
		}
		img_load_buf -= img_dark_buf;
		/* */
		//ptr = img_load_buf.array_ptr();
		//for ( l=0 ; l < img_load_buf.length() ; l++ ) {
		//    if ( ptr[l] < 0 ) ptr[l] = 0.0;
		//}
		/* Crop z */
		img_load_buf.crop(2 /* dim */, j /* R,G or B */, 1);	/* z */
		/* Median for standardization */
		if ( k == 0 ) {
		    median_each[i] = md_median(img_load_buf);
		    sio.printf("  Updated median_each[%zd] = %g\n",
			       i, median_each[i]);
		}
		/* Crop y */
		img_load_buf.crop(1, k, k_step);			/* y */
		/* standardization */
		img_load_buf *= (1.0 / median_each[i]);
		/* Copy to img_stat_buf */
		img_stat_buf.paste(img_load_buf, 0, 0, i /* layer = file No. */);
	    }
	    //img_stat_buf.dprint();
	    /* Get median and paste it to result_buf */
	    sio.printf(" Calculating median ...\n");
	    result_buf.paste(md_median_small_z(img_stat_buf), 0, k, j);
	}
      }
    }

    /* freeing buffer */
    img_load_buf.init(false);
    img_stat_buf.init(false);

    if ( icc_buf.length() == 0 ) {
	icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
    }
    
    /* */
    if ( target_channel < 3 ) {
	if ( target_channel == 0 ) {
	    result_buf.paste(result_buf.sectionf("*,*,0"), 0,0,1);
	    result_buf.paste(result_buf.sectionf("*,*,0"), 0,0,2);
	}
	else if ( target_channel == 1 ) {
	    result_buf.paste(result_buf.sectionf("*,*,1"), 0,0,0);
	    result_buf.paste(result_buf.sectionf("*,*,1"), 0,0,2);
	}
	else {
	    result_buf.paste(result_buf.sectionf("*,*,2"), 0,0,0);
	    result_buf.paste(result_buf.sectionf("*,*,2"), 0,0,1);
	}
    }
    
    if ( flag_float_tiff == true ) {

	sio.printf("Writing '%s' ", filename_out_float[target_channel]);

	//sio.eprintf("[DEBUG]: \n");
	//result_buf.dprint();

	/* zero- and max-check */
	ptr = result_buf.array_ptr();
	if ( flat_pow < 1.0 ) {	/* pow(v,?) flat when v < 1.0 */
	    for ( i=0 ; i < result_buf.length() ; i++ ) {
		if ( ptr[i] < 1.0 ) ptr[i] = pow(ptr[i], flat_pow);
	    }
	}
    
	if ( write_float_to_tiff(result_buf,
		 icc_buf, NULL, filename_out_float[target_channel]) < 0 ) {
	    sio.eprintf("[ERROR] write_float_to_tiff() failed\n");
	    goto quit;
	}

    }
    else {

	sio.printf("Writing '%s' ", filename_out[target_channel]);
	if ( flag_dither == true ) sio.printf("using dither ...\n");
	else sio.printf("NOT using dither ...\n");

	/* zero- and max-check */
	ptr = result_buf.array_ptr();
	if ( flat_pow < 1.0 ) {	/* pow(v,?) flat when v < 1.0 */
	    for ( i=0 ; i < result_buf.length() ; i++ ) {
		if ( ptr[i] < 1.0 ) ptr[i] = pow(ptr[i], flat_pow);
		ptr[i] *= 32768;
		if ( ptr[i] <= 0 ) ptr[i] = /* 1 */ 32768.0 /* maybe bad pixels */;
		else if ( 65535.0 < ptr[i] ) ptr[i] = 65535.0;
	    }
	}
	else {			/* normal flat */
	    for ( i=0 ; i < result_buf.length() ; i++ ) {
		ptr[i] *= 32768;
		if ( ptr[i] <= 0 ) ptr[i] = /* 1 */ 32768.0 /* maybe bad pixels */;
		else if ( 65535.0 < ptr[i] ) ptr[i] = 65535.0;
	    }
	}
    
	if ( write_float_to_tiff24or48(result_buf, 0.0, 65535.0, flag_dither, 
			   icc_buf, NULL, filename_out[target_channel]) < 0 ) {
	    sio.eprintf("[ERROR] write_float_to_tiff24or48() failed\n");
	    goto quit;
	}

    }
    
    return_status = 0;
 quit:
    return return_status;
}

#include "read_tiff24or48_to_float.cc"
#include "write_float_to_tiff.cc"
#include "write_float_to_tiff24or48.cc"
