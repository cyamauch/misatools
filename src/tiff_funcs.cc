#include "tiff_funcs.h"

#include <sli/stdstreamio.h>
#include <sli/mdarray_statistics.h>
using namespace sli;

#include <tiffio.h>
#include "MT.h"

/* test suffix of filename and try opening file with readonly */
bool test_tiff_file( const char *file )
{
    stdstreamio f_in;
    tstring filename = file;
    ssize_t len_file = (ssize_t)filename.length();
    bool is_tiff_name = false;
    bool ret_value = false;

    if ( file == NULL ) goto quit;
    
    if ( filename.rfind(".tif") + 4 == len_file ) is_tiff_name = true;
    else if ( filename.rfind(".tiff") + 5 == len_file ) is_tiff_name = true;
    else if ( filename.rfind(".TIF") + 4 == len_file ) is_tiff_name = true;
    else if ( filename.rfind(".TIFF") + 5 == len_file ) is_tiff_name = true;

    if ( is_tiff_name == true ) {
	if ( f_in.open("r", file) == 0 ) {
	    f_in.close();
	    ret_value =  true;
	}
    }

 quit:
    return ret_value;
}

int make_tiff_filename( const char *_filename_in,
		 const char *appended_str,
		 const char *bit_str,	/* "8bit" , "16bit" or "float" */
		 tstring *filename_out )
{
    const char *str_8[4] = {".8bit.","_8bit.",".8bit_","_8bit_"};
    const char *str_16[4] = {".16bit.","_16bit.",".16bit_","_16bit_"};
    const char *str_fl[4] = {".float.","_float.",".float_","_float_"};
    stdstreamio sio;
    tstring str_dot_appended_dot;
    tstring filename_in, bit_str_in;
    ssize_t pos_dot, ix_8, ix_16, ix_fl, ix_right_most, ix_appended, ix;
    size_t i;
    bool flag_need_bit_str = true;

    bit_str_in.assign(bit_str);
    filename_in.assign(_filename_in);
    pos_dot = filename_in.rfind('.');
    if ( 0 <= pos_dot ) filename_in.copy(0, pos_dot, filename_out);
    else filename_in.copy(filename_out);

    ix_right_most = -1;
    ix_8 = -1;
    for ( i=0 ; i < 4 ; i++ ) {
	ix = filename_in.rfind(str_8[i]);
	if ( ix_8 < ix ) ix_8 = ix;
	if ( ix_right_most < ix ) ix_right_most = ix;
    }

    ix_16 = -1;
    for ( i=0 ; i < 4 ; i++ ) {
	ix = filename_in.rfind(str_16[i]);
	if ( ix_16 < ix ) ix_16 = ix;
	if ( ix_right_most < ix ) ix_right_most = ix;
    }

    ix_fl = -1;
    for ( i=0 ; i < 4 ; i++ ) {
	ix = filename_in.rfind(str_fl[i]);
	if ( ix_fl < ix ) ix_fl = ix;
	if ( ix_right_most < ix ) ix_right_most = ix;
    }

    if ( appended_str != NULL ) {
	str_dot_appended_dot.printf(".%s.", appended_str);
	ix_appended = filename_in.rfind(str_dot_appended_dot.cstr());
    }
    else {
	ix_appended = -1;
    }

    if ( bit_str_in == "8bit" ) {
	if ( 0 <= ix_8 && ix_8 == ix_right_most ) flag_need_bit_str = false;
    }
    else if ( bit_str_in == "16bit" ) {
	if ( 0 <= ix_16 && ix_16 == ix_right_most ) flag_need_bit_str = false;
    }
    else if ( bit_str_in == "float" ) {
	if ( 0 <= ix_fl && ix_fl == ix_right_most ) flag_need_bit_str = false;
    }
    else {
	sio.eprintf("[WARNING] unexpected bit_str: '%s'\n", bit_str);
    }

    if ( appended_str != NULL && ix_appended < 0 ) {
	if ( flag_need_bit_str == true ) {
	    filename_out->appendf(".%s.%s.tiff",appended_str,bit_str);
	}
	else {
	    filename_out->appendf(".%s.tiff",appended_str);
	}
    }
    else {
	if ( flag_need_bit_str == true ) {
	    filename_out->appendf(".%s.tiff",bit_str);
	}
	else {
	    filename_out->append(".tiff");
	}
    }
    
    return 0;
}

/* this returns uchar or float array */
int load_tiff( const char *filename_in,
	    mdarray *ret_img_buf, int *ret_sztype, mdarray_uchar *ret_icc_buf,
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
    if ( spp != 3 && spp != 1 ) {
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
    if ( (spp == 3 && photom != PHOTOMETRIC_RGB) ||
	 (spp == 1 && photom != PHOTOMETRIC_MINISBLACK) ) {
	sio.eprintf("[ERROR] Unsupported PHOTOMETRIC value: %d\n",(int)photom);
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
	unsigned char *ret_rgb_img_ptr = NULL;
	size_t pix_offset, i;
	
        strip_buf.resize_1d(strip_size);
	/* get line buf ptr */
	strip_buf_ptr = strip_buf.array_ptr();

	if ( ret_img_buf != NULL ) {
	    ret_img_buf->init(UCHAR_ZT, false);
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
		    ret_rgb_img_ptr =
			(unsigned char *)ret_img_buf->data_ptr(0,0,ch);
		    if ( spp == 3 ) jj = ch;
		    else jj = 0;
		    for ( j=0 ; j < len_pix ; j++, jj+=spp ) {
			ret_rgb_img_ptr[pix_offset+j] = strip_buf_ptr[jj];
		    }
		}
	    }
	    pix_offset += len_pix;
	}
    }
    else if ( format == SAMPLEFORMAT_UINT && byps == 2 ) {    /* 16-bit mode */

        mdarray_uchar strip_buf(false);
	uint16_t *strip_buf_ptr = NULL;
	float *ret_rgb_img_ptr = NULL;
	size_t pix_offset, i;

	strip_buf.resize_1d(strip_size);
	/* get line buf ptr */
	strip_buf_ptr = (uint16_t *)strip_buf.data_ptr();

	if ( ret_img_buf != NULL ) {
	    ret_img_buf->init(FLOAT_ZT, false);
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
		    ret_rgb_img_ptr = (float *)ret_img_buf->data_ptr(0,0,ch);
		    if ( spp == 3 ) jj = ch;
		    else jj = 0;
		    for ( j=0 ; j < len_pix ; j++, jj+=spp ) {
			ret_rgb_img_ptr[pix_offset+j] = strip_buf_ptr[jj];
		    }
		}
	    }
	    pix_offset += len_pix;
	}
    }
    else if ( format == SAMPLEFORMAT_IEEEFP && byps == 4 ) {    /* float */

        mdarray_uchar strip_buf(false);
	float *strip_buf_ptr = NULL;
	float *ret_rgb_img_ptr = NULL;
	size_t pix_offset, i;

	strip_buf.resize_1d(strip_size);
	/* get line buf ptr */
	strip_buf_ptr = (float *)strip_buf.data_ptr();

	if ( ret_img_buf != NULL ) {
	    ret_img_buf->init(FLOAT_ZT, false);
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
		    ret_rgb_img_ptr = (float *)ret_img_buf->data_ptr(0,0,ch);
		    if ( spp == 3 ) jj = ch;
		    else jj = 0;
		    for ( j=0 ; j < len_pix ; j++, jj+=spp ) {
			ret_rgb_img_ptr[pix_offset+j] = strip_buf_ptr[jj];
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

/* this returns float array (data contents are unsigned 16-bit) */
/* this returns 1, 2(for 8-/16-bit) or -4(for 32-bit float) to *ret_sztype */
int load_tiff_into_float( const char *filename_in, double scale,
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
    if ( spp != 3 && spp != 1 ) {
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
    if ( (spp == 3 && photom != PHOTOMETRIC_RGB) ||
	 (spp == 1 && photom != PHOTOMETRIC_MINISBLACK) ) {
	sio.eprintf("[ERROR] Unsupported PHOTOMETRIC value: %d\n",(int)photom);
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
		    if ( spp == 3 ) jj = ch;
		    else jj = 0;
		    for ( j=0 ; j < len_pix ; j++, jj+=spp ) {
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
		    if ( spp == 3 ) jj = ch;
		    else jj = 0;
		    for ( j=0 ; j < len_pix ; j++, jj+=spp ) {
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
		    if ( spp == 3 ) jj = ch;
		    else jj = 0;
		    for ( j=0 ; j < len_pix ; j++, jj+=spp ) {
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


/* this returns uchar or float array */
int load_tiff_into_separate_buffer( const char *filename_in,
	mdarray *ret_img_r_buf, mdarray *ret_img_g_buf, mdarray *ret_img_b_buf,
	int *ret_sztype, mdarray_uchar *ret_icc_buf,
	float camera_calibration1_ret[] )
{
    stdstreamio sio;
    mdarray *ret_img_rgb_buf[3] = {ret_img_r_buf,ret_img_g_buf,ret_img_b_buf};
    
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
	unsigned char *ret_rgb_img_ptr[3] = {NULL,NULL,NULL};
	size_t pix_offset, i, ch;
	
        strip_buf.resize_1d(strip_size);
	/* get line buf ptr */
	strip_buf_ptr = strip_buf.array_ptr();

	for ( ch=0 ; ch < 3 ; ch++ ) {
	    if ( ret_img_rgb_buf[ch] != NULL ) {
		ret_img_rgb_buf[ch]->init(UCHAR_ZT, false);
		ret_img_rgb_buf[ch]->resize_2d(width,height);
		/* get array ptr of each ch */
		ret_rgb_img_ptr[ch]
		    = (unsigned char *)ret_img_rgb_buf[ch]->data_ptr();
	    }
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

	    for ( ch=0 ; ch < 3 ; ch++ ) {
		if ( ret_rgb_img_ptr[ch] != NULL ) {
		    for ( j=0, jj=ch ; j < len_pix ; j++, jj+=3 ) {
			ret_rgb_img_ptr[ch][pix_offset+j] = strip_buf_ptr[jj];
		    }
		}
	    }
	    pix_offset += len_pix;
	}
    }
    else if ( format == SAMPLEFORMAT_UINT && byps == 2 ) {    /* 16-bit mode */

        mdarray_uchar strip_buf(false);
	uint16_t *strip_buf_ptr = NULL;
	float *ret_rgb_img_ptr[3] = {NULL,NULL,NULL};
	size_t pix_offset, i, ch;

	strip_buf.resize_1d(strip_size);
	/* get line buf ptr */
	strip_buf_ptr = (uint16_t *)strip_buf.data_ptr();

	for ( ch=0 ; ch < 3 ; ch++ ) {
	    if ( ret_img_rgb_buf[ch] != NULL ) {
		ret_img_rgb_buf[ch]->init(FLOAT_ZT, false);
		ret_img_rgb_buf[ch]->resize_2d(width,height);
		/* get array ptr of each ch */
		ret_rgb_img_ptr[ch] = (float *)ret_img_rgb_buf[ch]->data_ptr();
	    }
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

	    for ( ch=0 ; ch < 3 ; ch++ ) {
		if ( ret_rgb_img_ptr[ch] != NULL ) {
		    for ( j=0, jj=ch ; j < len_pix ; j++, jj+=3 ) {
			ret_rgb_img_ptr[ch][pix_offset+j] = strip_buf_ptr[jj];
		    }
		}
	    }
	    pix_offset += len_pix;
	}
    }
    else if ( format == SAMPLEFORMAT_IEEEFP && byps == 4 ) {    /* float */

        mdarray_uchar strip_buf(false);
	float *strip_buf_ptr = NULL;
	float *ret_rgb_img_ptr[3] = {NULL,NULL,NULL};
	size_t pix_offset, i, ch;

	strip_buf.resize_1d(strip_size);
	/* get line buf ptr */
	strip_buf_ptr = (float *)strip_buf.data_ptr();

	for ( ch=0 ; ch < 3 ; ch++ ) {
	    if ( ret_img_rgb_buf[ch] != NULL ) {
		ret_img_rgb_buf[ch]->init(FLOAT_ZT, false);
		ret_img_rgb_buf[ch]->resize_2d(width,height);
		/* get array ptr of each ch */
		ret_rgb_img_ptr[ch] = (float *)ret_img_rgb_buf[ch]->data_ptr();
	    }
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

	    for ( ch=0 ; ch < 3 ; ch++ ) {
		if ( ret_rgb_img_ptr[ch] != NULL ) {
		    for ( j=0, jj=ch ; j < len_pix ; j++, jj+=3 ) {
			ret_rgb_img_ptr[ch][pix_offset+j] = strip_buf_ptr[jj];
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

/* write 8-bit/16-bit tiff data */
int save_tiff( const mdarray &img_buf_in, int sztype,
	       const mdarray_uchar &icc_buf_in,
	       const float camera_calibration1[],	/* [12] */
	       const char *filename_out )
{
    stdstreamio sio;

    /* TIFF: See http://www.libtiff.org/man/TIFFGetField.3t.html */
    TIFF *tiff_out = NULL;
    uint16 bps, byps, spp;
    uint32 width, height, icc_prof_size;
    
    int ret_status = -1;

    if ( filename_out == NULL ) return 0;

    if ( sztype == 1 && img_buf_in.size_type() == UCHAR_ZT ) bps = 8;
    else if ( sztype == 2 && img_buf_in.size_type() == FLOAT_ZT ) bps = 16;
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
    
    /* write image data */
    if ( img_buf_in.size_type() == UCHAR_ZT ) {

	mdarray_uchar strip_buf(false);
	unsigned char *strip_buf_ptr;
	
	const unsigned char *rgb_img_in_ptr;

	size_t pix_offset, i;

	strip_buf.resize_1d(byps * spp * width);
	strip_buf_ptr = strip_buf.array_ptr();

	pix_offset = 0;
	for ( i=0 ; i < height ; i++ ) {
	    size_t j, jj, ch;
	    for ( ch=0 ; ch < 3 ; ch++ ) {
		/* get ptr of each ch */
		rgb_img_in_ptr =
		    (const unsigned char *)img_buf_in.data_ptr_cs(0,0,ch);
		for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
		    strip_buf_ptr[jj] = rgb_img_in_ptr[pix_offset+j];
		}
		if ( TIFFWriteEncodedStrip(tiff_out, i, strip_buf_ptr,
					   width * byps * spp) == 0 ) {
		    sio.eprintf("[ERROR] TIFFWriteEncodedStrip() failed\n");
		    goto quit;
		}
	    }
	    pix_offset += width;
	}

    }
    else if ( img_buf_in.size_type() == FLOAT_ZT ) {

	mdarray_uchar strip_buf(false);
	uint16_t *strip_buf_ptr;

	const float *rgb_img_in_ptr;

	size_t pix_offset, i;

	strip_buf.resize_1d(byps * spp * width);
	strip_buf_ptr = (uint16_t *)strip_buf.data_ptr();

	pix_offset = 0;
	for ( i=0 ; i < height ; i++ ) {
	    size_t j, jj, ch;

	    for ( ch=0 ; ch < 3 ; ch++ ) {
		/* get ptr of each ch */
		rgb_img_in_ptr = (const float *)img_buf_in.data_ptr_cs(0,0,ch);

		for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
		    double v = rgb_img_in_ptr[pix_offset+j];
		    strip_buf_ptr[jj] = (uint16_t)(v + 0.5);
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

/* write tiff of 32-bit float data */
int save_float_to_tiff( const mdarray &img_buf_in,
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

    if ( img_buf_in.size_type() != FLOAT_ZT ) {
	sio.eprintf("[ERROR] write_float_to_tiff() only support FLOAT_ZT\n");
	goto quit;
    }
    
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

int save_float_to_tiff48( const mdarray &img_buf_in,
			  const mdarray_uchar &icc_buf_in,
			  const float camera_calibration1[],
			  double min_val, double max_val,
			  bool dither,
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
    
    if ( img_buf_in.size_type() != FLOAT_ZT ) {
	sio.eprintf("[ERROR] save_float_to_tiff48() only support FLOAT_ZT\n");
	goto quit;
    }

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

	const float *rgb_img_in_ptr;

	size_t pix_offset;

	strip_buf.resize_1d(byps * spp * width);
	strip_buf_ptr = (uint16_t *)strip_buf.data_ptr();

	pix_offset = 0;
	for ( i=0 ; i < height ; i++ ) {
	    size_t j, jj, ch;
	    double v;

	    for ( ch=0 ; ch < 3 ; ch++ ) {
		/* get ptr of each ch */
		rgb_img_in_ptr = (const float *)img_buf_in.data_ptr_cs(0,0,ch);

		if ( dither == false ) {
		    if ( min_val == 0.0 && max_val == 65535.0 ) {
			for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
			    v = rgb_img_in_ptr[pix_offset+j];
			    strip_buf_ptr[jj] = (uint16_t)(v + 0.5);
			}
		    }
		    else {
			for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
			    v = ((rgb_img_in_ptr[pix_offset+j] - min_val)/range) * 65535.0;
			    strip_buf_ptr[jj] = (uint16_t)(v + 0.5);
			}
		    }
		}
		else {
		    uint16_t v1;
		    for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
			v = ((rgb_img_in_ptr[pix_offset+j] - min_val)/range) * 65535.0;
			v1 = (uint16_t)v;
			if ( v1 < 65535 && genrand_real2() < v - floor(v) ) v1 ++;
			strip_buf_ptr[jj] = v1;
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

int save_float_to_tiff24or48( const mdarray_float &img_buf_in,
			      const mdarray_uchar &icc_buf_in,
			      const float camera_calibration1[],     /* [12] */
			      double min_val, double max_val,
			      bool dither,
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

    if ( 255.0 < range ) bps = 16;
    else bps = 8;
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

    //sio.eprintf("seed=%u\n", (unsigned int)rnd_seed);

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

    if ( byps == 2 ) {			/* 16-bit */

	mdarray_uchar strip_buf(false);
	uint16_t *strip_buf_ptr;

	const float *rgb_img_in_ptr;

	size_t pix_offset;

	strip_buf.resize_1d(byps * spp * width);
	strip_buf_ptr = (uint16_t *)strip_buf.data_ptr();

	pix_offset = 0;
	for ( i=0 ; i < height ; i++ ) {
	    size_t j, jj, ch;
	    double v;

	    for ( ch=0 ; ch < 3 ; ch++ ) {
		/* get ptr of each ch */
		rgb_img_in_ptr = (const float *)img_buf_in.data_ptr_cs(0,0,ch);

		if ( dither == false ) {
		    if ( min_val == 0.0 && max_val == 65535.0 ) {
			for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
			    v = rgb_img_in_ptr[pix_offset+j];
			    strip_buf_ptr[jj] = (uint16_t)(v + 0.5);
			}
		    }
		    else {
			for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
			    v = ((rgb_img_in_ptr[pix_offset+j] - min_val)/range) * 65535.0;
			    strip_buf_ptr[jj] = (uint16_t)(v + 0.5);
			}
		    }
		} else {
		    uint16_t v1;
		    for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
			v = ((rgb_img_in_ptr[pix_offset+j] - min_val)/range) * 65535.0;
			v1 = (uint16_t)v;
			if ( v1 < 65535 && genrand_real2() < v - floor(v) ) v1 ++;
			strip_buf_ptr[jj] = v1;
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
    else if ( byps == 1 ) {		/* 8-bit */

	mdarray_uchar strip_buf(false);
	unsigned char *strip_buf_ptr;

	const float *rgb_img_in_ptr;

	size_t pix_offset, i;

	strip_buf.resize_1d(byps * spp * width);
	strip_buf_ptr = (unsigned char *)strip_buf.data_ptr();


	pix_offset = 0;
	for ( i=0 ; i < height ; i++ ) {
	    size_t j, jj, ch;
	    double v;

	    for ( ch=0 ; ch < 3 ; ch++ ) {
		/* get ptr of each ch */
		rgb_img_in_ptr = (const float *)img_buf_in.data_ptr_cs(0,0,ch);
	    
		if ( dither == false ) {
		    if ( min_val == 0.0 && max_val == 255.0 ) {
			for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
			    v = rgb_img_in_ptr[pix_offset+j];
			    strip_buf_ptr[jj] = (unsigned char)(v + 0.5);
			}
		    }
		    else {
			for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
			    v = ((rgb_img_in_ptr[pix_offset+j] - min_val)/range) * 255.0;
			    strip_buf_ptr[jj] = (unsigned char)(v + 0.5);
			}
		    }
		}
		else {
		    unsigned char v1;
		    for ( j=0, jj=ch ; j < width ; j++, jj+=3 ) {
			v = ((rgb_img_in_ptr[pix_offset+j] - min_val)/range) * 255.0;
			v1 = (unsigned char)v;
			if ( v1 < 255 && genrand_real2() < v - floor(v) ) v1 ++;
			strip_buf_ptr[jj] = v1;
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
