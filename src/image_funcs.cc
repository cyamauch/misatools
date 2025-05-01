#include <sli/stdstreamio.h>
#include <math.h>

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


/*
 * bin an image (3d-cube).
 */
int bin_image( int factor, mdarray *img_io, int sztype )
{
    int ret_value = -1;
    stdstreamio sio;
    const double f2 = 1.0 / ((double)factor * factor);
    const size_t org_width = img_io->x_length();
    size_t new_len_x = img_io->x_length();
    size_t new_len_y = img_io->y_length();
    size_t new_len_z = img_io->z_length();

    if ( factor == 1 ) {
	ret_value = 0;
	goto quit;
    }
    else if ( factor < 1 ) {
	sio.eprintf("[ERROR] Too small factor: %d\n",factor);
	goto quit;
    }
    new_len_x /= factor;
    new_len_y /= factor;

    if ( sztype == 1 && img_io->size_type() == UCHAR_ZT ) {
	size_t i;
	for ( i=0 ; i < new_len_z ; i++ ) {
	    unsigned char *p0 = (unsigned char *)img_io->data_ptr(0,0,i);
	    size_t j;
	    for ( j=0 ; j < new_len_y ; j++ ) {
		unsigned char *p_out = p0 + org_width * j;
		size_t k;
		for ( k=0 ; k < new_len_x ; k++ ) {
		    size_t ix,iy;
		    const unsigned char *p_in =
			p0 + (org_width*j*factor) + (k*factor) ;
		    double v = 0;
		    for ( iy=0 ; iy < (size_t)factor ; iy++ ) {
			for ( ix=0 ; ix < (size_t)factor ; ix++ ) {
			    v += p_in[ix];
			}
			p_in += org_width;
		    }
		    v = floor(v * f2 + 0.5);
		    if ( 255 < v ) v = 255;
		    p_out[k] = (unsigned char)v;
		}
	    }
	}
    }
    else if ( (sztype == 2||sztype == -4) &&
	      img_io->size_type() == FLOAT_ZT ) {
	size_t i;
	for ( i=0 ; i < new_len_z ; i++ ) {
	    float *p0 = (float *)img_io->data_ptr(0,0,i);
	    size_t j;
	    for ( j=0 ; j < new_len_y ; j++ ) {
		float *p_out = p0 + org_width * j;
		size_t k;
		for ( k=0 ; k < new_len_x ; k++ ) {
		    size_t ix,iy;
		    const float *p_in =
			p0 + (org_width*j*factor) + (k*factor) ;
		    double v = 0;
		    for ( iy=0 ; iy < (size_t)factor ; iy++ ) {
			for ( ix=0 ; ix < (size_t)factor ; ix++ ) {
			    v += p_in[ix];
			}
			p_in += org_width;
		    }
		    if ( sztype == 2 ) {
			v = floor(v * f2 + 0.5);
			if ( 65535 < v ) v = 65535;
		    }
		    else {
			v *= f2;
		    }
		    p_out[k] = (float)v;
		}
	    }
	}
    }
    else {
	sio.eprintf("[ERROR] Unsupported: sztype=%d size_type=%d\n",
		    sztype, (int)(img_io->size_type()) );
	goto quit;
    }

    img_io->resize_3d(new_len_x, new_len_y, img_io->z_length());

    ret_value = 0;
 quit:
    return ret_value;
}
