#include <tiffio.h>

#include "MT.h"

static int write_float_to_tiff48( const mdarray_float &img_buf_in,
				  double min_val, double max_val,
				  bool dither,
				  const mdarray_uchar &icc_buf_in,
				  const float camera_calibration1[],
				  const char *filename_out )
{
    stdstreamio sio;
    double range;

    /* TIFF: See http://www.libtiff.org/man/TIFFGetField.3t.html */
    TIFF *tiff_out = NULL;
    uint16 bps, byps, spp;
    uint32 width, height, icc_prof_size;
    size_t i;
    uint32_t rnd_seed = 0;
    
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

    /* set random seed */
    i = 0;
    while ( filename_out[i] != '\0' ) {
	rnd_seed += (uint32_t)(filename_out[i]) << (rnd_seed % 25);
	rnd_seed -= (uint32_t)(filename_out[i]) << (rnd_seed % 19);
	rnd_seed += (uint32_t)(filename_out[i]) << (rnd_seed % 11);
	rnd_seed -= (uint32_t)(filename_out[i]) << (rnd_seed % 5);
	i++;
    }
    init_genrand(rnd_seed);

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
    TIFFSetField(tiff_out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
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
	uint16_t *strip_buf_ptr;

	const float *r_img_in_ptr;
	const float *g_img_in_ptr;
	const float *b_img_in_ptr;

	size_t pix_offset;

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

	    if ( dither == false ) {
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
	    }
	    else {
		uint16_t v1;
		for ( j=0, jj=0 ; j < width ; j++, jj+=3 ) {
		    v = ((r_img_in_ptr[pix_offset+j] - min_val)/range) * 65535.0;
		    v1 = (uint16_t)v;
		    if ( v1 < 65535 && genrand_real2() < v - floor(v) ) v1 ++;
		    strip_buf_ptr[jj] = v1;
		}
		for ( j=0, jj=1 ; j < width ; j++, jj+=3 ) {
		    v = ((g_img_in_ptr[pix_offset+j] - min_val)/range) * 65535.0;
		    v1 = (uint16_t)v;
		    if ( v1 < 65535 && genrand_real2() < v - floor(v) ) v1 ++;
		    strip_buf_ptr[jj] = v1;
		}
		for ( j=0, jj=2 ; j < width ; j++, jj+=3 ) {
		    v = ((b_img_in_ptr[pix_offset+j] - min_val)/range) * 65535.0;
		    v1 = (uint16_t)v;
		    if ( v1 < 65535 && genrand_real2() < v - floor(v) ) v1 ++;
		    strip_buf_ptr[jj] = v1;
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
