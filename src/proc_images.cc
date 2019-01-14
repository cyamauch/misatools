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
    const char *filename_dark = "dark.tiff";
    const char *filename_dark_list = "dark.txt";
    const char *filename_flat = "flat.tiff";
    tstring filename_sky;
    bool flag_output_8bit = false;
    double dark_factor = 1.0;
    double softdark = 0;
    double softsky = 0;
    int arg_cnt;
    size_t i;
    
    int return_status = -1;

    if ( argc < 2 ) {
	sio.eprintf("Process target frames\n");
	sio.eprintf("\n");
        sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s [-8] [-d param] [-s sky.tiff] img_0.tiff img_1.tiff ...\n", argv[0]);
	sio.eprintf("\n");
	sio.eprintf("-8 ... If set, output 8-bit processed images for 8-bit original images\n");
	sio.eprintf("-d param ... Set dark factor to param. Default is 1.0.\n");
	sio.eprintf("NOTE: %s is used when it exists\n",filename_dark);
	sio.eprintf("NOTE: %s is used when it exists\n",filename_flat);
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
	else if ( argstr == "-d" ) {
	    arg_cnt ++;
	    argstr = argv[arg_cnt];
	    dark_factor = argstr.atof();
	    sio.printf("Using dark factor: %g\n", dark_factor);
	    arg_cnt ++;
	}
	else if ( argstr == "-s" ) {
	    arg_cnt ++;
	    filename_sky = argv[arg_cnt];
	    sio.printf("Using sky frame: %s\n", filename_sky.cstr());
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
	sio.eprintf("[ERROR] Not found: %s\n",filename_dark);
	goto quit;
    }
    else {
	size_t bytes;
	mdarray_double dark_rgb(false);
	f_in.close();
	dark_rgb.resize_1d(3);
	sio.printf("Loading %s\n", filename_dark);
	if ( read_tiff24or48_to_float(filename_dark,
				      &img_dark_buf, NULL, &bytes) < 0 ) {
	    sio.eprintf("[ERROR] cannot load '%s'\n", filename_dark);
	    sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	    goto quit;
	}
	if ( bytes == 1 ) sio.printf("Found an 8-bit dark image\n");
	else sio.printf("Found a 16-bit dark image\n");
	//img_dark_buf.dprint();
	img_dark_buf *= dark_factor;
	dark_rgb[0] = md_median(img_dark_buf.sectionf("*,*,0"));
	dark_rgb[1] = md_median(img_dark_buf.sectionf("*,*,1"));
	dark_rgb[2] = md_median(img_dark_buf.sectionf("*,*,2"));
	sio.printf("median dark value of r, g, b = %g, %g, %g\n",
		   dark_rgb[0],dark_rgb[1],dark_rgb[2]);
	softdark = md_median(dark_rgb);
	sio.printf("Softdark value = %g (when 16-bit)\n", softdark);
    }

    /* check flat file */
    if ( f_in.open("r", "flat.tiff") < 0 ) {
	sio.eprintf("[NOTICE] Not found: flat.tiff\n");
    }
    else {
	size_t bytes;
	f_in.close();
	sio.printf("Loading %s\n", filename_flat);
	if ( read_tiff24or48_to_float(filename_flat,
				      &img_flat_buf, NULL, &bytes) < 0 ) {
	    sio.eprintf("[ERROR] cannot load '%s'\n", filename_flat);
	    sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	    goto quit;
	}
	if ( bytes == 1 ) sio.printf("Found an 8-bit flat image\n");
	else sio.printf("Found a 16-bit flat image\n");
	img_flat_buf *= (1.0/32768.0);
    }
    //img_flat_buf.dprint();

    /* check sky file */
    if ( 0 < filename_sky.length() ) {
	if ( f_in.open("r", filename_sky.cstr()) < 0 ) {
	    sio.eprintf("[NOTICE] Not found: %s\n", filename_sky.cstr());
	}
	else {
	    size_t bytes;
	    mdarray_double sky_rgb(false);
	    f_in.close();
	    sky_rgb.resize_1d(3);
	    sio.printf("Loading %s\n", filename_sky.cstr());
	    if ( read_tiff24or48_to_float(filename_sky.cstr(),
					  &img_sky_buf, NULL, &bytes) < 0 ) {
		sio.eprintf("[ERROR] cannot load '%s'\n", filename_sky.cstr());
		sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
		goto quit;
	    }
	    if ( bytes == 1 ) sio.printf("Found an 8-bit sky image\n");
	    else sio.printf("Found a 16-bit sky image\n");
	    //
	    sky_rgb[0] = md_median(img_sky_buf.sectionf("*,*,0"));
	    sky_rgb[1] = md_median(img_sky_buf.sectionf("*,*,1"));
	    sky_rgb[2] = md_median(img_sky_buf.sectionf("*,*,2"));
	    sio.printf("median sky value of r, g, b = %g, %g, %g\n",
		       sky_rgb[0],sky_rgb[1],sky_rgb[2]);
	    softsky = md_median(sky_rgb);
	    sio.printf("Softsky value = %g\n", softsky);
	}
    }

    for ( i=0 ; i < filenames_in.length() ; i++ ) {
	size_t bytes;
	tstring filename, filename_out;
	float *ptr;
	size_t j;

	filename = filenames_in[i];
	sio.printf("Loading %s\n", filename.cstr());
	if ( read_tiff24or48_to_float(filename.cstr(),
				      &img_in_buf, &icc_buf, &bytes) < 0 ) {
	    sio.eprintf("[ERROR] cannot load '%s'\n", filename.cstr());
	    sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	    goto quit;
	}
	if ( 0 < darkfile_list.length() ) {
	    const char *fn = darkfile_list[i % darkfile_list.length()].cstr();
	    sio.printf("Loading %s\n", fn);
	    if ( read_tiff24or48_to_float(fn,
					  &img_dark_buf, NULL, &bytes) < 0 ) {
		sio.eprintf("[ERROR] cannot load '%s'\n", fn);
		sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
		goto quit;
	    }
	    if ( bytes == 1 ) sio.printf("Found an 8-bit dark image\n");
	    else sio.printf("Found a 16-bit dark image\n");
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
	img_in_buf += softdark;

	/* Subtract sky */
	if ( 0 < img_sky_buf.length() ) {
	    img_in_buf -= img_sky_buf;
	    img_in_buf += softsky;
	}

	/* create new filename */
	if ( bytes == 1 ) {
	    make_output_filename(filename.cstr(), "proc", "8bit",
				 &filename_out);
	}
	else {
	    make_output_filename(filename.cstr(), "proc", "16bit",
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
	    sio.printf("Writing %s [8bit/ch] ...\n", filename_out.cstr());
	    if ( write_float_to_tiff24or48(img_in_buf, 0.0, 255.0, icc_buf,
					   filename_out.cstr()) < 0 ) {
		sio.eprintf("[ERROR] write_float_to_tiff24or48() failed\n");
		goto quit;
	    }
	}
	else {
	    for ( j=0 ; j < img_in_buf.length() ; j++ ) {
		if ( ptr[j] < 0 ) ptr[j] = 0.0;
		else if ( 65535.0 < ptr[j] ) ptr[j] = 65535.0;
	    }
	    sio.printf("Writing %s [16bit/ch] ...\n", filename_out.cstr());
	    if ( write_float_to_tiff24or48(img_in_buf, 0.0, 65535.0, icc_buf,
					   filename_out.cstr()) < 0 ) {
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

