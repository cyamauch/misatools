#include <tiffio.h>

static int write_tiff24or48( const mdarray &img_buf_in,
			     const mdarray_uchar &icc_buf_in,
			     const char *filename_out )
{
    stdstreamio sio;

    /* TIFF: See http://www.libtiff.org/man/TIFFGetField.3t.html */
    TIFF *tiff_out = NULL;
    uint16 bps, byps, spp;
    uint32 width, height, icc_prof_size;
    
    int ret_status = -1;

    if ( filename_out == NULL ) return 0;

    if ( img_buf_in.size_type() == UCHAR_ZT ) bps = 8;
    else if ( img_buf_in.size_type() == FLOAT_ZT ) bps = 16;
    else {
        sio.eprintf("[ERROR] unexpected array type\n");
	goto quit;
    }
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
    TIFFSetField(tiff_out, TIFFTAG_ROWSPERSTRIP, (uint32)1);
    
    if ( 0 < icc_buf_in.length() ) {
	icc_prof_size = icc_buf_in.length();
	TIFFSetField(tiff_out, TIFFTAG_ICCPROFILE,
		     icc_prof_size, icc_buf_in.data_ptr());
    }
    
    /* write image data */
    if ( img_buf_in.size_type() == UCHAR_ZT ) {

	mdarray_uchar strip_buf(false);
	unsigned char *strip_buf_ptr;
	
	const unsigned char *r_img_in_ptr;
	const unsigned char *g_img_in_ptr;
	const unsigned char *b_img_in_ptr;

	size_t pix_offset, i;

	strip_buf.resize_1d(byps * spp * width);
	strip_buf_ptr = strip_buf.array_ptr();

	/* get ptr of each ch */
	r_img_in_ptr = (const unsigned char *)img_buf_in.data_ptr_cs(0,0,0);
	g_img_in_ptr = (const unsigned char *)img_buf_in.data_ptr_cs(0,0,1);
	b_img_in_ptr = (const unsigned char *)img_buf_in.data_ptr_cs(0,0,2);

	pix_offset = 0;
	for ( i=0 ; i < height ; i++ ) {
	    size_t j, jj;

	    for ( j=0, jj=0 ; j < width ; j++, jj+=3 ) {
		strip_buf_ptr[jj] = r_img_in_ptr[pix_offset+j];
	    }
	    for ( j=0, jj=1 ; j < width ; j++, jj+=3 ) {
		strip_buf_ptr[jj] = g_img_in_ptr[pix_offset+j];
	    }
	    for ( j=0, jj=2 ; j < width ; j++, jj+=3 ) {
		strip_buf_ptr[jj] = b_img_in_ptr[pix_offset+j];
	    }
	    
	    if ( TIFFWriteEncodedStrip(tiff_out, i, strip_buf_ptr,
				       width * byps * spp) == 0 ) {
		sio.eprintf("[ERROR] TIFFWriteEncodedStrip() failed\n");
		goto quit;
	    }

	    pix_offset += width;
	}

    }
    else if ( img_buf_in.size_type() == FLOAT_ZT ) {

	mdarray_uchar strip_buf(false);
	uint16_t *strip_buf_ptr;

	const float *r_img_in_ptr;
	const float *g_img_in_ptr;
	const float *b_img_in_ptr;

	size_t pix_offset, i;

	strip_buf.resize_1d(byps * spp * width);
	strip_buf_ptr = (uint16_t *)strip_buf.data_ptr();

	/* get ptr of each ch */
	r_img_in_ptr = (const float *)img_buf_in.data_ptr_cs(0,0,0);
	g_img_in_ptr = (const float *)img_buf_in.data_ptr_cs(0,0,1);
	b_img_in_ptr = (const float *)img_buf_in.data_ptr_cs(0,0,2);

	pix_offset = 0;
	for ( i=0 ; i < height ; i++ ) {
	    size_t j, jj;

	    for ( j=0, jj=0 ; j < width ; j++, jj+=3 ) {
		strip_buf_ptr[jj] = r_img_in_ptr[pix_offset+j];
	    }
	    for ( j=0, jj=1 ; j < width ; j++, jj+=3 ) {
		strip_buf_ptr[jj] = g_img_in_ptr[pix_offset+j];
	    }
	    for ( j=0, jj=2 ; j < width ; j++, jj+=3 ) {
		strip_buf_ptr[jj] = b_img_in_ptr[pix_offset+j];
	    }
	    
	    if ( TIFFWriteEncodedStrip(tiff_out, i, strip_buf_ptr,
				       width * byps * spp) == 0 ) {
		sio.eprintf("[ERROR] TIFFWriteEncodedStrip() failed\n");
		goto quit;
	    }

	    pix_offset += width;
	}
	
    }
    else {
	goto quit;
    }
    
   
    ret_status = 0;
 quit:
    if ( tiff_out != NULL ) {
	TIFFClose(tiff_out);
    }

    return ret_status;
}
