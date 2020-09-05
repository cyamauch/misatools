#include <sli/stdstreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>
using namespace sli;

#include "read_tiff24or48_to_float.h"
#include "write_float_to_tiff24or48.h"
#include "write_float_to_tiff.h"
#include "make_output_filename.cc"
#include "icc_srgb_profile.c"

/**
 * @file   proc_images.cc
 * @brief  a command-line tool for object frame with dark, flat and sky proc.
 *         8/16-bit integer and 32-bit float images are supported.
 */

int main( int argc, char *argv[] )
{
    stdstreamio sio, f_in;

    tarray_tstring filenames_in;
    tarray_tstring darkfile_list;
    mdarray_float img_dark_buf(false);
    mdarray_float img_flat_buf(false);
    mdarray_float img_sky_buf(false);
    mdarray_float img_in_buf(false);
    mdarray_uchar icc_buf(false);
    float camera_calibration1[12];			/* for TIFF tag */
    float *raw_colors_p = camera_calibration1 + 4;		/* [1] */
    float *daylight_multipliers = camera_calibration1 + 5;	/* [3] */
    //float *camera_multipliers = camera_calibration1 + 5 + 3;	/* [4] */
    const char *filename_dark = "dark.tiff";
    const char *filename_dark_list = "dark.txt";
    const char *filename_flat[] = {"flat.float.tiff", "flat.16bit.tiff"};
    tstring filename_sky;
    int flag_use_flat = -1;
    bool flag_output_8bit = false;
    bool flag_output_16bit = false;
    bool flag_dither = true;
    bool flag_raw_rgb = false;
    double dark_factor = 1.0;
    double flat_factor = 1.0;
    double flat_idx_factor = 1.0;
    double softdark = 0.0;
    double softsky = 0.0;
    double softbias = 0.0;
    int arg_cnt;
    size_t i;
    
    int return_status = -1;

    if ( argc < 2 ) {
	sio.eprintf("Process target frames\n");
	sio.eprintf("\n");
        sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s [-8] [-16] [-t] [-d param] [-f param] [-fi param] [-s sky.tiff] img_0.tiff img_1.tiff ...\n", argv[0]);
	sio.eprintf("\n");
	sio.eprintf("-8 ... If set, output 8-bit processed images for 8-bit original images\n");
	sio.eprintf("-16 .. If set, output 16-bit processed images\n");
	sio.eprintf("-t ... If set, not using dither to output 8/16-bit images\n");
	sio.eprintf("-r ... If set, output raw RGB without applying daylight multipliers\n");
	sio.eprintf("-s param ... Set softbias value. Default is 0.0\n");
	sio.eprintf("-d param ... Set dark factor to param. Default is 1.0.\n");
	sio.eprintf("-f param ... Set flat factor to param. Default is 1.0.\n");
	sio.eprintf("-fi param ... Set flat index factor (flat ^ x) to param. Default is 1.0.\n");
	sio.eprintf("NOTE: %s is used when it exists\n",filename_dark);
	sio.eprintf("NOTE: %s or %s is used when it exists\n",
		    filename_flat[0],filename_flat[1]);
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
	else if ( argstr == "-r" ) {
	    flag_raw_rgb = true;
	    arg_cnt ++;
	}
	else if ( argstr == "-s" ) {
	    arg_cnt ++;
	    argstr = argv[arg_cnt];
	    softbias = argstr.atof();
	    arg_cnt ++;
	}
	else if ( argstr == "-d" ) {
	    arg_cnt ++;
	    argstr = argv[arg_cnt];
	    dark_factor = argstr.atof();
	    sio.printf("Using dark factor: %g\n", dark_factor);
	    arg_cnt ++;
	}
	else if ( argstr == "-f" ) {
	    arg_cnt ++;
	    argstr = argv[arg_cnt];
	    flat_factor = argstr.atof();
	    sio.printf("Using flat factor: %g\n", flat_factor);
	    arg_cnt ++;
	}
	else if ( argstr == "-fi" ) {
	    arg_cnt ++;
	    argstr = argv[arg_cnt];
	    flat_idx_factor = argstr.atof();
	    sio.printf("Using flat index (flat^idx) factor: %g\n", flat_idx_factor);
	    arg_cnt ++;
	}
	else if ( argstr == "-s" ) {
	    arg_cnt ++;
	    filename_sky = argv[arg_cnt];
	    sio.printf("Using sky frame: '%s'\n", filename_sky.cstr());
	    arg_cnt ++;
	}
	else {
	    break;
	}
    }

    filenames_in.erase(0, arg_cnt);	/* erase */
    //filenames_in.dprint();

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
	}
    }

        
    if ( f_in.open("r", filename_dark) < 0 ) {
	sio.eprintf("[ERROR] Not found: '%s'\n",filename_dark);
	goto quit;
    }
    else {
	int bytes;
	mdarray_double dark_rgb(false);
	f_in.close();
	dark_rgb.resize_1d(3);
	sio.printf("Loading '%s'\n", filename_dark);
	if ( read_tiff24or48_to_float(filename_dark, 65536.0, 
				      &img_dark_buf, NULL, &bytes, NULL) < 0 ) {
	    sio.eprintf("[ERROR] cannot load '%s'\n", filename_dark);
	    sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	    goto quit;
	}
	if ( bytes == 1 ) sio.printf("Found an 8-bit dark image\n");
	else if ( bytes == 2 ) sio.printf("Found a 16-bit dark image\n");
	else sio.printf("Found a float(32-bit) dark image\n");
	//img_dark_buf.dprint();
	img_dark_buf *= dark_factor;
	dark_rgb[0] = md_median(img_dark_buf.sectionf("*,*,0"));
	dark_rgb[1] = md_median(img_dark_buf.sectionf("*,*,1"));
	dark_rgb[2] = md_median(img_dark_buf.sectionf("*,*,2"));
	sio.printf("median dark value of r, g, b = %g, %g, %g\n",
		   dark_rgb[0],dark_rgb[1],dark_rgb[2]);
	softdark = md_median(dark_rgb);
	//sio.printf("[INFO] softdark value = %g (when 16-bit)\n", softdark);
    }

    /* check flat file */
    flag_use_flat = -1;
    if ( f_in.open("r", filename_flat[0]) < 0 ) {
	if ( f_in.open("r", filename_flat[1]) < 0 ) {
	    sio.eprintf("[NOTICE] Not found: flat.[float|16bit].tiff\n");
	}
	else flag_use_flat = 1;
    }
    else flag_use_flat = 0;

    if ( 0 <= flag_use_flat ) {
	int bytes;
	f_in.close();
	sio.printf("Loading '%s'\n", filename_flat[flag_use_flat]);
	if ( read_tiff24or48_to_float(filename_flat[flag_use_flat], 1.0,
				      &img_flat_buf, NULL, &bytes, NULL) < 0 ) {
	    sio.eprintf("[ERROR] cannot load '%s'\n",
			filename_flat[flag_use_flat]);
	    sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	    goto quit;
	}
	if ( bytes == 1 ) sio.printf("Found an 8-bit flat image\n");
	else if ( bytes == 2 ) sio.printf("Found a 16-bit flat image\n");
	else {
	    sio.printf("Found a float(32-bit) flat image\n");
	    //sio.eprintf("[DEBUG]: \n");
	    //img_flat_buf.dprint();
	}
	if ( flat_factor != 1.0 || flat_idx_factor != 1.0 ) {
	    for ( i=0 ; i < img_flat_buf.length() ; i++ ) {
		img_flat_buf[i] += 0.5;
		img_flat_buf[i] *= flat_factor;
		img_flat_buf[i] = pow(img_flat_buf[i], flat_idx_factor);
	    }
	}
	else {
	    img_flat_buf += 0.5;
	}
    }
    //sio.eprintf("[DEBUG]: \n");
    //img_flat_buf.dprint();

    /* check sky file */
    if ( 0 < filename_sky.length() ) {
	if ( f_in.open("r", filename_sky.cstr()) < 0 ) {
	    sio.eprintf("[NOTICE] Not found: '%s'\n", filename_sky.cstr());
	}
	else {
	    int bytes;
	    mdarray_double sky_rgb(false);
	    f_in.close();
	    sky_rgb.resize_1d(3);
	    sio.printf("Loading '%s'\n", filename_sky.cstr());
	    if ( read_tiff24or48_to_float(filename_sky.cstr(), 65536.0, 
					  &img_sky_buf, NULL, &bytes, NULL) < 0 ) {
		sio.eprintf("[ERROR] cannot load '%s'\n", filename_sky.cstr());
		sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
		goto quit;
	    }
	    if ( bytes == 1 ) sio.printf("Found an 8-bit sky image\n");
	    else if ( bytes == 2 ) sio.printf("Found a 16-bit sky image\n");
	    else sio.printf("Found a float(32-bit) sky image\n");
	    //
	    sky_rgb[0] = md_median(img_sky_buf.sectionf("*,*,0"));
	    sky_rgb[1] = md_median(img_sky_buf.sectionf("*,*,1"));
	    sky_rgb[2] = md_median(img_sky_buf.sectionf("*,*,2"));
	    sio.printf("median sky value of r, g, b = %g, %g, %g\n",
		       sky_rgb[0],sky_rgb[1],sky_rgb[2]);
	    softsky = md_median(sky_rgb);
	    //sio.printf("[INFO] softsky value = %g\n", softsky);
	}
    }

    for ( i=0 ; i < filenames_in.length() ; i++ ) {
	int bytes;
	tstring filename, filename_out;
	float *ptr;
	size_t j;

	filename = filenames_in[i];
	sio.printf("Loading '%s'\n", filename.cstr());
	if ( read_tiff24or48_to_float(filename.cstr(), 65536.0, 
		    &img_in_buf, &icc_buf, &bytes, camera_calibration1) < 0 ) {
	    sio.eprintf("[ERROR] cannot load '%s'\n", filename.cstr());
	    sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	    goto quit;
	}
	if ( 0 < darkfile_list.length() ) {
	    int bytes0;
	    const char *fn = darkfile_list[i % darkfile_list.length()].cstr();
	    sio.printf("Loading '%s'\n", fn);
	    if ( read_tiff24or48_to_float(fn, 65536.0, 
				  &img_dark_buf, NULL, &bytes0, NULL) < 0 ) {
		sio.eprintf("[ERROR] cannot load '%s'\n", fn);
		sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
		goto quit;
	    }
	    if ( bytes0 == 1 ) sio.printf("Found an 8-bit dark image\n");
	    else if ( bytes0 == 2 ) sio.printf("Found a 16-bit dark image\n");
	    else sio.printf("Found a float(32-bit) dark image\n");
	    img_dark_buf *= dark_factor;
	}
	img_in_buf -= img_dark_buf;
	ptr = img_in_buf.array_ptr();
	for ( j=0 ; j < img_in_buf.length() ; j++ ) {
	    if ( ptr[j] < 0 ) ptr[j] = 0.0;
	}
	if ( 0 < img_flat_buf.length() ) {
	    img_in_buf /= img_flat_buf;
	}

	/* Subtract sky */
	if ( 0 < img_sky_buf.length() ) {
	    img_in_buf -= img_sky_buf;
	}

	/* Apply daylight multipliers, if possible */
	if ( flag_raw_rgb == false && raw_colors_p[0] == 3 ) {
	    float mul_0;
	    size_t len_xy = img_in_buf.x_length() * img_in_buf.y_length();
	    mul_0 = daylight_multipliers[1];
	    if ( daylight_multipliers[0] < mul_0 ) mul_0 = daylight_multipliers[0];
	    if ( daylight_multipliers[2] < mul_0 ) mul_0 = daylight_multipliers[2];
	    if ( 0.0 < mul_0 ) {
		sio.printf("[INFO] applying daylight multipliers (%g, %g, %g)\n", 
		  daylight_multipliers[0],daylight_multipliers[1],daylight_multipliers[2]);
		for ( j=0 ; j < 3 ; j++ ) {
		    float *p = img_in_buf.array_ptr(0,0,j);
		    float mul = daylight_multipliers[j] / mul_0;
		    size_t k;
		    for ( k=0 ; k < len_xy ; k++ ) {
			p[k] *= mul;
		    }
		}
	    }
	    else {
		sio.printf("[INFO] daylight multipliers are not found\n");
	    }
	}

	/* Add softbias */
	if ( softbias != 0.0 ) {
	    sio.printf("[INFO] softbias = %g (when 16-bit)\n", softbias);
	    img_in_buf += softbias;
	}
	else {
	    sio.printf("[INFO] softbias = %g\n", softbias);
	}
	

	if ( icc_buf.length() == 0 ) {
	    icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	    icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
	}

	/* check min, max and write a processed file */
	ptr = img_in_buf.array_ptr();
	if ( flag_output_16bit == true ) {
	    make_output_filename(filename.cstr(), "proc", "16bit",
				 &filename_out);
	    for ( j=0 ; j < img_in_buf.length() ; j++ ) {
		if ( ptr[j] < 0 ) ptr[j] = 0.0;
		else if ( 65535.0 < ptr[j] ) ptr[j] = 65535.0;
	    }
	    sio.printf("Writing '%s' [16bit/ch] ", filename_out.cstr());
	    if ( flag_dither == true ) sio.printf("using dither ...\n");
	    else sio.printf("NOT using dither ...\n");
	    if ( write_float_to_tiff24or48(img_in_buf,  
			icc_buf, camera_calibration1,
			0.0, 65535.0, flag_dither, filename_out.cstr()) < 0 ) {
		sio.eprintf("[ERROR] write_float_to_tiff24or48() failed\n");
		goto quit;
	    }
	}
	else if ( bytes == 1 && flag_output_8bit == true ) {
	    make_output_filename(filename.cstr(), "proc", "8bit",
				 &filename_out);
	    for ( j=0 ; j < img_in_buf.length() ; j++ ) {
		ptr[j] /= 256.0;
		if ( ptr[j] < 0 ) ptr[j] = 0.0;
		else if ( 255.0 < ptr[j] ) ptr[j] = 255.0;
	    }
	    sio.printf("Writing '%s' [8bit/ch] ", filename_out.cstr());
	    if ( flag_dither == true ) sio.printf("using dither ...\n");
	    else sio.printf("NOT using dither ...\n");
	    if ( write_float_to_tiff24or48(img_in_buf,  
			  icc_buf, camera_calibration1,
			  0.0, 255.0, flag_dither, filename_out.cstr()) < 0 ) {
		sio.eprintf("[ERROR] write_float_to_tiff24or48() failed\n");
		goto quit;
	    }
	}
	else {	/* output float tiff */
	    make_output_filename(filename.cstr(), "proc", "float",
				 &filename_out);
	    sio.printf("Writing '%s' [32-bit_float/ch]\n", filename_out.cstr());
	    if ( write_float_to_tiff(img_in_buf, icc_buf, camera_calibration1,
				     65536.0, filename_out.cstr()) < 0 ) {
		sio.eprintf("[ERROR] write_float_to_tiff() failed\n");
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
#include "write_float_to_tiff.cc"

