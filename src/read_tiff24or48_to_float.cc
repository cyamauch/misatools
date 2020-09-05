#include <tiffio.h>

/* this returns float array (data contents are unsigned 16-bit) */
/* this returns 1 or 2 (for 8-/16-bit) to *ret_sztype            */
static int read_tiff24or48_to_float( const char *filename_in, double scale,
    mdarray_float *ret_img_buf, int *ret_sztype, mdarray_uchar *ret_icc_buf, 
    float camera_calibration1_ret[] )
{
    stdstreamio sio;

    /* TIFF: See http://www.libtiff.org/man/TIFFGetField.3t.html */
    TIFF *tiff_in = NULL;
    uint16 bps, byps, spp, pconfig, photom, format;
    uint32 width, height, icc_prof_size = 0, camera_calibration1_size = 0;
    void *icc_prof_data = NULL;
    float *camera_calibration1 = NULL;
    tsize_t strip_size;
    size_t strip_max;

    int ret_status = -1;

    if ( filename_in == NULL ) return -1;	/* ERROR */

    tiff_in = TIFFOpen(filename_in, "r");
    if ( tiff_in == NULL ) {
	sio.eprintf("[ERROR] cannot open: %s\n", filename_in);
	goto quit;
    }

    if ( TIFFGetField(tiff_in, TIFFTAG_SAMPLEFORMAT, &format) == 0 ) {
	format = SAMPLEFORMAT_UINT;
    }

    if ( TIFFGetField(tiff_in, TIFFTAG_BITSPERSAMPLE, &bps) == 0 ) {
	sio.eprintf("[ERROR] TIFFGetField() failed [bps]\n");
	goto quit;
    }

    if ( format == SAMPLEFORMAT_IEEEFP ) {
	if ( bps != 32 ) {
	    sio.eprintf("[ERROR] unsupported SAMPLEFORMAT: %d\n",(int)format);
	    sio.eprintf("[ERROR] unsupported BITSPERSAMPLE: %d\n",(int)bps);
	    goto quit;
	}
    }
    else if ( format == SAMPLEFORMAT_UINT ) {
	if ( bps != 8 && bps != 16 ) {
	    sio.eprintf("[ERROR] unsupported SAMPLEFORMAT: %d\n",(int)format);
	    sio.eprintf("[ERROR] unsupported BITSPERSAMPLE: %d\n",(int)bps);
	    goto quit;
	}
    }
    else {
	sio.eprintf("[ERROR] unsupported SAMPLEFORMAT: %d\n",(int)format);
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

    if ( TIFFGetField(tiff_in, TIFFTAG_CAMERACALIBRATION1,
		      &camera_calibration1_size, &camera_calibration1) != 0 ) {
	if ( camera_calibration1 != NULL ) {
	    /*
	    uint32 i;
	    sio.eprintf("[INFO] camera_calibration1 = ( ");
            for ( i=0 ; i < camera_calibration1_size ; i++ ) {
		sio.eprintf("%g ", camera_calibration1[i]);
            }
            sio.eprintf(")\n");
	    */
	}
    }

    if ( TIFFGetField(tiff_in, TIFFTAG_ICCPROFILE,
		      &icc_prof_size, &icc_prof_data) != 0 ) {
	if ( ret_icc_buf != NULL ) {
	    ret_icc_buf->resize_1d(icc_prof_size);
	    ret_icc_buf->putdata(icc_prof_data, icc_prof_size);
	}
    }

    if ( format == SAMPLEFORMAT_UINT && byps == 1 ) {        /* 8-bit mode */

        mdarray_uchar strip_buf(false);
	unsigned char *strip_buf_ptr = NULL;
	float *ret_rgb_img_ptr = NULL;
	size_t pix_offset, i;
	double scl;
	
        strip_buf.resize_1d(strip_size);
	/* get line buf ptr */
	strip_buf_ptr = strip_buf.array_ptr();

	if ( ret_img_buf != NULL ) {
	    if ( /* ret_img_buf->x_length() != width ||
		 ret_img_buf->y_length() != height ||
		 ret_img_buf->z_length() != 3 */ 1 ) {
		ret_img_buf->init(false);
		ret_img_buf->resize_3d(width,height,3);
	    }
	}

	if ( scale == 65536.0 ) scl = 256.0;
	else scl = scale / 256.0;
	
	pix_offset = 0;
	for ( i=0 ; i < strip_max ; i++ ) {
	    size_t len_pix, j, jj, ch;
	    ssize_t s_len = TIFFReadEncodedStrip(tiff_in, i,
						 (void *)strip_buf_ptr,
						 strip_size);
	    if ( s_len < 0 ) {
		sio.eprintf("[ERROR] TIFFReadEncodedStrip() failed\n");
		goto quit;
	    }
	    len_pix = s_len / (byps * spp);

	    if ( ret_img_buf != NULL ) {
		for ( ch=0 ; ch < 3 ; ch++ ) {
		    /* get array ptr of each ch */
		    ret_rgb_img_ptr = ret_img_buf->array_ptr(0,0,ch);
		    for ( j=0, jj=ch ; j < len_pix ; j++, jj+=3 ) {
			ret_rgb_img_ptr[pix_offset+j] 
			    = strip_buf_ptr[jj] * scl;
		    }
		}
	    }
	    pix_offset += len_pix;
	}

    }
    else if ( format == SAMPLEFORMAT_UINT && byps == 2 ) {        /* 16-bit mode */

        mdarray_uchar strip_buf(false);
	uint16_t *strip_buf_ptr = NULL;
	float *ret_rgb_img_ptr = NULL;
	size_t pix_offset, i;
	double scl;
	
	strip_buf.resize_1d(strip_size);
	/* get line buf ptr */
	strip_buf_ptr = (uint16_t *)strip_buf.data_ptr();

	if ( ret_img_buf != NULL ) {
	    ret_img_buf->init(false);
	    ret_img_buf->resize_3d(width,height,3);
	}

	if ( scale == 65536.0 ) scl = 1.0;
	else scl = scale / 65536.0;

	pix_offset = 0;
	for ( i=0 ; i < strip_max ; i++ ) {
	    size_t len_pix, j, jj, ch;
	    ssize_t s_len = TIFFReadEncodedStrip(tiff_in, i,
						 (void *)strip_buf_ptr,
						 strip_size);
	    if ( s_len < 0 ) {
		sio.eprintf("[ERROR] TIFFReadEncodedStrip() failed\n");
		goto quit;
	    }
	    len_pix = s_len / (byps * spp);

	    if ( ret_img_buf != NULL ) {
		for ( ch=0 ; ch < 3 ; ch++ ) {
		    /* get array ptr of each ch */
		    ret_rgb_img_ptr = ret_img_buf->array_ptr(0,0,ch);
		    for ( j=0, jj=ch ; j < len_pix ; j++, jj+=3 ) {
			ret_rgb_img_ptr[pix_offset+j]
			    = strip_buf_ptr[jj] * scl;
		    }
		}
	    }
	    pix_offset += len_pix;
	}

    }
    else {	/* 32-bit float */

        mdarray_uchar strip_buf(false);
	float *strip_buf_ptr = NULL;
	float *ret_rgb_img_ptr = NULL;
	size_t pix_offset, i;

	strip_buf.resize_1d(strip_size);
	/* get line buf ptr */
	strip_buf_ptr = (float *)strip_buf.data_ptr();

	if ( ret_img_buf != NULL ) {
	    ret_img_buf->init(false);
	    ret_img_buf->resize_3d(width,height,3);
	}

	pix_offset = 0;
	for ( i=0 ; i < strip_max ; i++ ) {
	    size_t len_pix, j, jj, ch;
	    ssize_t s_len = TIFFReadEncodedStrip(tiff_in, i,
						 (void *)strip_buf_ptr,
						 strip_size);
	    if ( s_len < 0 ) {
		sio.eprintf("[ERROR] TIFFReadEncodedStrip() failed\n");
		goto quit;
	    }
	    len_pix = s_len / (byps * spp);

	    if ( ret_img_buf != NULL ) {
		for ( ch=0 ; ch < 3 ; ch++ ) {
		    /* get array ptr of each ch */
		    ret_rgb_img_ptr = ret_img_buf->array_ptr(0,0,ch);
		    for ( j=0, jj=ch ; j < len_pix ; j++, jj+=3 ) {
			ret_rgb_img_ptr[pix_offset+j]
			    = strip_buf_ptr[jj] * scale;
		    }
		}
	    }
	    pix_offset += len_pix;
	}

    }
    
    if ( ret_sztype != NULL ) {
	*ret_sztype = byps;
	if ( format == SAMPLEFORMAT_IEEEFP ) *ret_sztype *= -1;
    }
    if ( camera_calibration1_ret != NULL ) {
	uint32 i;
	if ( camera_calibration1 == NULL ) camera_calibration1_size = 0;
        for ( i=0 ; i < 12 ; i++ ) {
	    if ( i < camera_calibration1_size ) {
		camera_calibration1_ret[i] = camera_calibration1[i];
            }
	    else {
		if ( 5 <= i && i <= 10 ) camera_calibration1_ret[i] = 1.0;
		else camera_calibration1_ret[i] = 0.0;
	    }
	}
    }

    ret_status = 0;
 quit:
    if ( tiff_in != NULL ) {
	TIFFClose(tiff_in);
    }

    return ret_status;
}

