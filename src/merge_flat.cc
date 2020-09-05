#include <sli/stdstreamio.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>
using namespace sli;

#include "read_tiff24or48_to_float.h"
#include "write_float_to_tiff24or48.h"
#include "write_float_to_tiff.h"
#include "icc_srgb_profile.c"

int main( int argc, char *argv[] )
{
    stdstreamio sio;

    mdarray_float img_flat_r_buf(false);
    mdarray_float img_flat_g_buf(false);
    mdarray_float img_flat_b_buf(false);
    mdarray_uchar icc_buf(false);
    const char *filename_in;
    const char *filename_out = "flat.16bit.tiff";
    const char *filename_out_float = "flat.float.tiff";
    int bytes_r=0, bytes_g=0, bytes_b=0;

    int return_status = -1;

    if ( argc < 4 ) {
	sio.eprintf("Merge R,G and B master flat frames\n");
	sio.eprintf("\n");
        sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s flat_r.tiff flat_g.tiff flat_b.tiff\n",
		    argv[0]);
	sio.eprintf("NOTE: name of output file is %s or %s\n",
		    filename_out_float, filename_out);
	goto quit;
    }
    
    filename_in = argv[1];
    sio.printf("Loading %s\n", filename_in);
    if ( read_tiff24or48_to_float(filename_in, 1.0,
			  &img_flat_r_buf, &icc_buf, &bytes_r, NULL) < 0 ) {
	sio.eprintf("[ERROR] cannot load '%s'\n", filename_in);
	sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	goto quit;
    }
    
    filename_in = argv[2];
    sio.printf("Loading %s\n", filename_in);
    if ( read_tiff24or48_to_float(filename_in, 1.0,
			  &img_flat_g_buf, &icc_buf, &bytes_g, NULL) < 0 ) {
	sio.eprintf("[ERROR] cannot load '%s'\n", filename_in);
	sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	goto quit;
    }

    filename_in = argv[3];
    sio.printf("Loading %s\n", filename_in);
    if ( read_tiff24or48_to_float(filename_in, 1.0,
			  &img_flat_b_buf, &icc_buf, &bytes_r, NULL) < 0 ) {
	sio.eprintf("[ERROR] cannot load '%s'\n", filename_in);
	sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	goto quit;
    }

    /* merge */
    img_flat_g_buf.paste(img_flat_r_buf.sectionf("*,*,0"),0,0,0);
    img_flat_g_buf.paste(img_flat_b_buf.sectionf("*,*,2"),0,0,2);

    if ( icc_buf.length() == 0 ) {
	icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
    }

    if ( bytes_r < 0 || bytes_g < 0 || bytes_b < 0 ) {
	sio.printf("Writing %s ...\n", filename_out_float);

	if ( write_float_to_tiff(img_flat_g_buf, icc_buf, NULL,
				 1.0, filename_out_float) < 0 ) {
	    sio.eprintf("[ERROR] write_float_to_tiff() failed\n");
	    goto quit;
	}
    }
    else {
	sio.printf("Writing %s ...\n", filename_out);

	img_flat_g_buf *= 65536.0;

	if ( write_float_to_tiff24or48(img_flat_g_buf, icc_buf, NULL, 
				     0.0, 65535.0, false, filename_out) < 0 ) {
	    sio.eprintf("[ERROR] write_float_to_tiff24or48() failed\n");
	    goto quit;
	}
    }
    
    return_status = 0;
 quit:
    return return_status;
}

#include "read_tiff24or48_to_float.cc"
#include "write_float_to_tiff24or48.cc"
#include "write_float_to_tiff.cc"
