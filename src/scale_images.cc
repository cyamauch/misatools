#include <sli/stdstreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>

#include "tiff_funcs.h"
#include "image_funcs.h"

using namespace sli;

/**
 * @file   scale_images.cc
 * @brief  a command-line tool for scaling/binning images.
 *         8/16-bit integer and 32-bit float images are supported.
 */

int main( int argc, char *argv[] )
{
    stdstreamio sio, f_in;

    tarray_tstring filenames_in;
    mdarray img_in_buf(false);
    mdarray_uchar icc_buf(false);
    float camera_calibration1[12];			/* for TIFF tag */
    int scaling_factor = 2;
    int arg_cnt;
    size_t i;

    int return_status = -1;

    if ( argc < 2 ) {
	sio.eprintf("Perform scaling or binning\n");
	sio.eprintf("\n");
        sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s [-s scaling_factor] img_0.tiff img_1.tiff ...\n", argv[0]);
	sio.eprintf("\n");
	sio.eprintf("-s param ... Set scaling/binning factor. Default is 2.\n");
	sio.eprintf("             Set negative values for binning\n");
	goto quit;
    }

    filenames_in = argv;

    arg_cnt = 1;

    while ( arg_cnt < argc ) {
	tstring argstr;
	argstr = argv[arg_cnt];
	if ( argstr == "-s" ) {
	    arg_cnt ++;
	    argstr = argv[arg_cnt];
	    scaling_factor = argstr.atoi();
	    arg_cnt ++;
	}
	else {
	    break;
	}
    }

    if ( -2 < scaling_factor && scaling_factor < 2 ) {
	sio.eprintf("[ERROR] Invalid scaling_factor: %d\n", scaling_factor);
	goto quit;
    }

    filenames_in.erase(0, arg_cnt);	/* erase */
    //filenames_in.dprint();

    for ( i=0 ; i < filenames_in.length() ; i++ ) {
	int sztype;
	tstring filename, filename_out, ex_str;
	const char *type_str;

	filename = filenames_in[i];
	sio.printf("Loading '%s'\n", filename.cstr());
	if ( load_tiff(filename.cstr(), 
		   &img_in_buf, &sztype, &icc_buf, camera_calibration1) < 0 ) {
	    sio.eprintf("[ERROR] cannot load '%s'\n", filename.cstr());
	    sio.eprintf("[ERROR] load_tiff() failed\n");
	    goto quit;
	}

	if ( sztype == 1 ) type_str = "8bit";
	else if ( sztype == 2 ) type_str = "16bit";
	else type_str = "float";

	if ( scaling_factor < 0 ) {
	    ex_str.printf("%dx_binning", 0 - scaling_factor);
	}
	else {
	    ex_str.printf("%dx", scaling_factor);
	}

	make_tiff_filename(filename.cstr(), ex_str.cstr(), type_str,
			   &filename_out);

	if ( scaling_factor < 0 ) {
	    if ( bin_image( 0 - scaling_factor, &img_in_buf, sztype ) < 0 ) {
		sio.eprintf("[ERROR] faild bin_image()\n");
		goto quit;
	    }
	}
	else {
	    if ( scale_image( scaling_factor, &img_in_buf ) < 0 ) {
		sio.eprintf("[ERROR] faild scale_image()\n");
		goto quit;
	    }
	}

	sio.printf("Writing '%s'\n", filename_out.cstr());
	if ( save_tiff(img_in_buf, sztype, icc_buf, camera_calibration1,
		       filename_out.cstr()) < 0 ) {
	    sio.eprintf("[ERROR] save_tiff() failed\n");
	    goto quit;
	}

    }

    return_status = 0;
 quit:
    return return_status;
}
