#include <tiffio.h>

static int write_float_to_tiff48( const mdarray_float &img_buf_in,
				  double min_val, double max_val,
				  const mdarray_uchar &icc_buf_in,
				  const char *filename_out )
{
    stdstreamio sio;
    double range;

    /* TIFF: See http://www.libtiff.org/man/TIFFGetField.3t.html */
    TIFF *tiff_out = NULL;
    uint16 bps, byps, spp;
    uint32 width, height, icc_prof_size;
    
    int ret_status = -1;
    
    if ( filename_out == NULL ) return 0;

    if ( max_val <= min_val ) {
	max_val = md_max(img_buf_in);
	min_val = md_min(img_buf_in);
    }
    range = max_val - min_val;

    bps = 16;
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

    {
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
	    double v;

	    for ( j=0, jj=0 ; j < width ; j++, jj+=3 ) {
		v = ((r_img_in_ptr[pix_offset+j] - min_val)/range) * 65535.0;
		strip_buf_ptr[jj] = (uint16_t)(v + 0.5);
	    }
	    for ( j=0, jj=1 ; j < width ; j++, jj+=3 ) {
		v = ((g_img_in_ptr[pix_offset+j] - min_val)/range) * 65535.0;
		strip_buf_ptr[jj] = (uint16_t)(v + 0.5);
	    }
	    for ( j=0, jj=2 ; j < width ; j++, jj+=3 ) {
		v = ((b_img_in_ptr[pix_offset+j] - min_val)/range) * 65535.0;
		strip_buf_ptr[jj] = (uint16_t)(v + 0.5);
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
