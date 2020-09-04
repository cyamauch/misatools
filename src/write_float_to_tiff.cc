#include <tiffio.h>

/* write tiff of 32-bit float data */
static int write_float_to_tiff( const mdarray_float &img_buf_in,
				const mdarray_uchar &icc_buf_in,
				const float camera_calibration1[],
				double scale,
				const char *filename_out )
{
    stdstreamio sio;

    /* TIFF: See http://www.libtiff.org/man/TIFFGetField.3t.html */
    TIFF *tiff_out = NULL;
    uint16 bps, byps, spp;
    uint32 width, height, icc_prof_size;
    size_t i;
    
    int ret_status = -1;
    
    if ( filename_out == NULL ) return 0;

    bps = 32;
    byps = (bps + 7) / 8;
    spp = 3;

    width = img_buf_in.x_length();
    height = img_buf_in.y_length();

    tiff_out = TIFFOpen(filename_out, "w");
    if ( tiff_out == NULL ) {
	sio.eprintf("[ERROR] TIFFOpen() failed\n");
	goto quit;
    }

    TIFFSetField(tiff_out, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tiff_out, TIFFTAG_IMAGELENGTH, height);

    TIFFSetField(tiff_out, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    //TIFFSetField(tiff_out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(tiff_out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiff_out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tiff_out, TIFFTAG_BITSPERSAMPLE, bps);
    TIFFSetField(tiff_out, TIFFTAG_SAMPLESPERPIXEL, spp);
    TIFFSetField(tiff_out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
    TIFFSetField(tiff_out, TIFFTAG_ROWSPERSTRIP, (uint32)1);

    if ( camera_calibration1 != NULL ) {
	TIFFSetField(tiff_out, TIFFTAG_CAMERACALIBRATION1, 12, camera_calibration1);
    }
    
    if ( 0 < icc_buf_in.length() ) {
	icc_prof_size = icc_buf_in.length();
	TIFFSetField(tiff_out, TIFFTAG_ICCPROFILE,
		     icc_prof_size, icc_buf_in.data_ptr());
    }

    {
	mdarray_uchar strip_buf(false);
	float *strip_buf_ptr;

	const float *rgb_img_in_ptr;

	size_t pix_offset;

	strip_buf.resize_1d(byps * spp * width);
	strip_buf_ptr = (float *)strip_buf.data_ptr();

	pix_offset = 0;
	for ( i=0 ; i < height ; i++ ) {
	    size_t j, jj, ch;

	    for ( ch=0 ; ch < 3 ; ch++ ) {
		/* get ptr of each ch */
		rgb_img_in_ptr = (const float *)img_buf_in.data_ptr_cs(0,0,ch);
		if ( scale == 1.0 ) {
		    for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
			strip_buf_ptr[jj] = rgb_img_in_ptr[pix_offset+j];
		    }
		}
		else {
		    for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
			strip_buf_ptr[jj] =
			    rgb_img_in_ptr[pix_offset+j] / scale;
		    }
		}
	    }

	    if ( TIFFWriteEncodedStrip(tiff_out, i, strip_buf_ptr,
				       width * byps * spp) == 0 ) {
		sio.eprintf("[ERROR] TIFFWriteEncodedStrip() failed\n");
		goto quit;
	    }

	    pix_offset += width;
	}
    }
    
    ret_status = 0;
 quit:
    if ( tiff_out != NULL ) {
	TIFFClose(tiff_out);
    }

    return ret_status;
}
