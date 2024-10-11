#include <sli/stdstreamio.h>

#include "image_funcs.h"

using namespace sli;

/*
 * scale an image (3d-cube).
 */
int scale_image( int scale, mdarray *img_io )
{
    int ret_value = -1;
    stdstreamio sio;
    size_t org_width = img_io->x_length();
    size_t org_height = img_io->y_length();
    size_t width, height;
    size_t i,j,k;

    if ( scale == 1 ) {
	ret_value = 0;
	goto quit;
    }
    else if ( scale < 1 ) {
	sio.eprintf("[ERROR] Too small scale: %d\n",scale);
	goto quit;
    }
    else if ( 32000.0 < (double)scale * org_width ||
	      32000.0 < (double)scale * org_height ) {
	sio.eprintf("[ERROR] Too large scale: %d\n",scale);
	goto quit;
    }

    width = scale * org_width;
    height = scale * org_height;

    img_io->resize_3d(width, height, img_io->z_length());

    for ( i=0 ; i < img_io->z_length() ; i++ ) {	/* z */
	const size_t bytes = img_io->bytes();

	/* copy x-direction */
	for ( j=0 ; j < org_height; j++ ) {		/* y */
	    unsigned char *ptr;
	    size_t k_w;
	    ptr = (unsigned char *)img_io->data_ptr(0,j,i);
	    k_w = width;
	    for ( k=org_width ; 0 < k ; ) {
		size_t l, m;
		k--;
		k_w -= scale;
		for ( l=scale ; 0 < l ; ) {
		    l--;
		    for ( m=bytes ; 0 < m ; ) {
			m--;
			ptr[(k_w+l) * bytes + m] = ptr[k * bytes + m];
		    }
		}
	    }
	}

	/* copy y-direction */
	for ( j=org_height ; 0 < j ; ) {		/* y */
	    const unsigned char *ptr;
	    j--;
	    ptr = (const unsigned char *)img_io->data_ptr(0,j,i);
	    for ( k=scale ; 0 < k ; ) {
		k--;
		img_io->putdata( ptr, bytes * width,
				 0, j*scale + k, i );
	    }
	}

    }

    ret_value = 0;
 quit:
    return ret_value;

}
