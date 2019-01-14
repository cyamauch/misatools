#include <tiffio.h>

/* this returns uchar or float array */
static int read_tiff24or48_separate_buffer( const char *filename_in,
	mdarray *ret_img_r_buf, mdarray *ret_img_g_buf, mdarray *ret_img_b_buf,
        mdarray_uchar *ret_icc_buf )
{
    stdstreamio sio;

    /* TIFF: See http://www.libtiff.org/man/TIFFGetField.3t.html */
    TIFF *tiff_in = NULL;
    uint16 bps, byps, spp, pconfig, photom;
    uint32 width, height, icc_prof_size = 0;
    void *icc_prof_data = NULL;
    tsize_t strip_size;
    size_t strip_max;

    int ret_status = -1;

    if ( filename_in == NULL ) return -1;	/* ERROR */

    tiff_in = TIFFOpen(filename_in, "r");
    if ( tiff_in == NULL ) {
	sio.eprintf("[ERROR] cannot open: %s\n", filename_in);
	goto quit;
    }

    if ( TIFFGetField(tiff_in, TIFFTAG_BITSPERSAMPLE, &bps) == 0 ) {
	sio.eprintf("[ERROR] TIFFGetField() failed [bps]\n");
	goto quit;
    }
    if ( bps != 8 && bps != 16 ) {
	sio.eprintf("[ERROR] unsupported BITSPERSAMPLE: %d\n",(int)bps);
	goto quit;
    }
    byps = (bps + 7) / 8;

    if ( TIFFGetField(tiff_in, TIFFTAG_SAMPLESPERPIXEL, &spp) == 0 ) {
	sio.eprintf("[ERROR] TIFFGetField() failed [spp]\n");
	goto quit;
    }
    if ( spp != 3 ) {
	sio.eprintf("[ERROR] unsupported SAMPLESPERPIXEL: %d\n",(int)spp);
	goto quit;
    }

    strip_size = TIFFStripSize(tiff_in);		/* in bytes */
    strip_max = TIFFNumberOfStrips(tiff_in);

    if ( TIFFGetField(tiff_in, TIFFTAG_IMAGEWIDTH, &width) == 0 ) {
	sio.eprintf("[ERROR] TIFFGetField() failed [width]\n");
	goto quit;
    }
    if ( TIFFGetField(tiff_in, TIFFTAG_IMAGELENGTH, &height) == 0 ) {
	sio.eprintf("[ERROR] TIFFGetField() failed [height]\n");
	goto quit;
    }
    if ( TIFFGetField(tiff_in, TIFFTAG_PLANARCONFIG, &pconfig) == 0 ) {
	sio.eprintf("[ERROR] TIFFGetField() failed [pconfig]\n");
	goto quit;
    }
    if ( pconfig != PLANARCONFIG_CONTIG ) {
	sio.eprintf("[ERROR] Unsupported PLANARCONFIG value\n");
	goto quit;
    }
    if ( TIFFGetField(tiff_in, TIFFTAG_PHOTOMETRIC, &photom) == 0 ) {
	sio.eprintf("[ERROR] TIFFGetField() failed [photom]\n");
	goto quit;
    }
    if ( photom != PHOTOMETRIC_RGB ) {
	sio.eprintf("[ERROR] Unsupported PHOTOMETRIC value\n");
	goto quit;
    }
    if ( TIFFGetField(tiff_in, TIFFTAG_ICCPROFILE,
		      &icc_prof_size, &icc_prof_data) != 0 ) {
	if ( ret_icc_buf != NULL ) {
	    ret_icc_buf->resize_1d(icc_prof_size);
	    ret_icc_buf->putdata(icc_prof_data, icc_prof_size);
	}
    }

    if ( byps == 1 ) {        /* 8-bit mode */

        mdarray_uchar strip_buf(false);
	unsigned char *strip_buf_ptr = NULL;
	unsigned char *ret_r_img_ptr = NULL;
	unsigned char *ret_g_img_ptr = NULL;
	unsigned char *ret_b_img_ptr = NULL;
	size_t pix_offset, i;
	
        strip_buf.resize_1d(strip_size);
	/* get line buf ptr */
	strip_buf_ptr = strip_buf.array_ptr();

	if ( ret_img_r_buf != NULL ) {
	    ret_img_r_buf->init(UCHAR_ZT, false);
	    ret_img_r_buf->resize_2d(width,height);
	    /* get array ptr of each ch */
	    ret_r_img_ptr = (unsigned char *)ret_img_r_buf->data_ptr();
	}
	if ( ret_img_g_buf != NULL ) {
	    ret_img_g_buf->init(UCHAR_ZT, false);
	    ret_img_g_buf->resize_2d(width,height);
	    /* get array ptr of each ch */
	    ret_g_img_ptr = (unsigned char *)ret_img_g_buf->data_ptr();
	}
	if ( ret_img_b_buf != NULL ) {
	    ret_img_b_buf->init(UCHAR_ZT, false);
	    ret_img_b_buf->resize_2d(width,height);
	    /* get array ptr of each ch */
	    ret_b_img_ptr = (unsigned char *)ret_img_b_buf->data_ptr();
	}
    
	pix_offset = 0;
	for ( i=0 ; i < strip_max ; i++ ) {
	    size_t len_pix, j, jj;
	    ssize_t s_len = TIFFReadEncodedStrip(tiff_in, i,
						 (void *)strip_buf_ptr,
						 strip_size);
	    if ( s_len < 0 ) {
		sio.eprintf("[ERROR] TIFFReadEncodedStrip() failed\n");
		goto quit;
	    }
	    len_pix = s_len / (byps * spp);

	    if ( ret_r_img_ptr != NULL ) {
		for ( j=0, jj=0 ; j < len_pix ; j++, jj+=3 ) {
		    ret_r_img_ptr[pix_offset+j] = strip_buf_ptr[jj];
		}
	    }
	    if ( ret_g_img_ptr != NULL ) {
		for ( j=0, jj=1 ; j < len_pix ; j++, jj+=3 ) {
		    ret_g_img_ptr[pix_offset+j] = strip_buf_ptr[jj];
		}
	    }
	    if ( ret_b_img_ptr != NULL ) {
		for ( j=0, jj=2 ; j < len_pix ; j++, jj+=3 ) {
		    ret_b_img_ptr[pix_offset+j] = strip_buf_ptr[jj];
		}
	    }
	    pix_offset += len_pix;
	}

    }
    else {        /* 16-bit mode */

        mdarray_uchar strip_buf(false);
	uint16_t *strip_buf_ptr = NULL;
	float *ret_r_img_ptr = NULL;
	float *ret_g_img_ptr = NULL;
	float *ret_b_img_ptr = NULL;
	size_t pix_offset, i;

	strip_buf.resize_1d(strip_size);
	/* get line buf ptr */
	strip_buf_ptr = (uint16_t *)strip_buf.data_ptr();

	if ( ret_img_r_buf != NULL ) {
	    ret_img_r_buf->init(FLOAT_ZT, false);
	    ret_img_r_buf->resize_2d(width,height);
	    /* get array ptr of each ch */
	    ret_r_img_ptr = (float *)ret_img_r_buf->data_ptr();
	}
	if ( ret_img_g_buf != NULL ) {
	    ret_img_g_buf->init(FLOAT_ZT, false);
	    ret_img_g_buf->resize_2d(width,height);
	    /* get array ptr of each ch */
	    ret_g_img_ptr = (float *)ret_img_g_buf->data_ptr();
	}
	if ( ret_img_b_buf != NULL ) {
	    ret_img_b_buf->init(FLOAT_ZT, false);
	    ret_img_b_buf->resize_2d(width,height);
	    /* get array ptr of each ch */
	    ret_b_img_ptr = (float *)ret_img_b_buf->data_ptr();
	}

	pix_offset = 0;
	for ( i=0 ; i < strip_max ; i++ ) {
	    size_t len_pix, j, jj;
	    ssize_t s_len = TIFFReadEncodedStrip(tiff_in, i,
						 (void *)strip_buf_ptr,
						 strip_size);
	    if ( s_len < 0 ) {
		sio.eprintf("[ERROR] TIFFReadEncodedStrip() failed\n");
		goto quit;
	    }
	    len_pix = s_len / (byps * spp);

	    if ( ret_r_img_ptr != NULL ) {
		for ( j=0, jj=0 ; j < len_pix ; j++, jj+=3 ) {
		    ret_r_img_ptr[pix_offset+j] = strip_buf_ptr[jj];
		}
	    }
	    if ( ret_g_img_ptr != NULL ) {
		for ( j=0, jj=1 ; j < len_pix ; j++, jj+=3 ) {
		    ret_g_img_ptr[pix_offset+j] = strip_buf_ptr[jj];
		}
	    }
	    if ( ret_b_img_ptr != NULL ) {
		for ( j=0, jj=2 ; j < len_pix ; j++, jj+=3 ) {
		    ret_b_img_ptr[pix_offset+j] = strip_buf_ptr[jj];
		}
	    }
	    pix_offset += len_pix;
	}

    }

   
    ret_status = 0;
 quit:
    if ( tiff_in != NULL ) {
	TIFFClose(tiff_in);
    }

    return ret_status;
}

