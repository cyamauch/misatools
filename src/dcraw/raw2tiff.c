/*
 * raw2tiff.c
 *
 * Read raw image file using dcraw and convert bayer to raw RGB TIFF
 * without interpolation.
 *
 * GPL V2
 * (C) 2015-2024  Chisato Yamauchi
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <limits.h>
#include <tiffio.h>

#include "icc_srgb_profile.c"
#include "adjust_crop_prms.c"
#include "s_byteswap2.c"
#include "s_byteswap4.c"

const size_t FITS_FILE_RECORD_UNIT = 2880;
const size_t FITS_HEADER_RECORD_UNIT = 80;

const int Matrix_NONE = 0;
const int Matrix_RG_GB = 1;
const int Matrix_GR_BG = 2;

#if 0
static int check_uint_string( const char *str_in )
{
    if ( strspn(str_in,"0123456789") != strlen(str_in) ) return -1;
    else return 0;
}
#endif

/**
 * @brief  Safety version of strncpy().
 * @param  dest       Pointer to the destination array
 * @param  size_dest  Buffer size of destination array
 * @param  src        C string to be copied
 * @param  n          Maximum number of characters to be copied from source
 * @return  destination is returned.
 */
inline static char *safe_strncpy( char *dest, size_t size_dest, 
                                  const char *src, size_t n )
{
    n ++;
    if ( n < size_dest ) size_dest = n;
    if ( 0 < size_dest ) {
        const size_t m = size_dest - 1;
        strncpy(dest,src,m);
        dest[m] = '\0';
    }
    return dest;
}

/* Test specified file exists and returns file type [0:RAW, 1:FITS, 2:TIFF] */
static int check_a_file( const char *filename )
{
    int return_status = -1;	/* file type */
    FILE *fp = NULL;
    char suffix_case[PATH_MAX];
    size_t i, dot_idx = 0;

    char line[8];

    if ( filename == NULL ) goto quit;
    if ( strlen(filename) == 0 ) goto quit;

    for ( i=0 ; filename[i] != '\0' ; i++ ) {
	if ( filename[i] == '.' ) dot_idx = i;
    }
    if ( filename[dot_idx] == '.' ) {
	for ( i=0 ; i < PATH_MAX ; i++ ) {
	    suffix_case[i] = filename[dot_idx + i];
	    if ( suffix_case[i] == '\0' ) break;
	    if ( 'A' <= suffix_case[i] && suffix_case[i] <= 'Z' ) {
		suffix_case[i] += 0x20;		/* to lower case */
	    }
	}
	if ( strcmp(suffix_case, ".tif") == 0 ||
	     strcmp(suffix_case, ".tiff") == 0 ) {
	    return_status = 2;		/* type = TIFF */
	    goto quit;
	}
    }

    fp = fopen(filename,"r");
    if ( fp == NULL ) goto quit;

    return_status = 0;			/* type = RAW */
    if ( fgets(line,8,fp) != NULL ) {
	if ( strncmp(line,"SIMPLE",6) == 0 ) {
	    return_status = 1;		/* type = FITS */
	}
    }

 quit:
    if ( fp != NULL ) fclose(fp);
    return return_status;
}

static int get_fits_key_val( const char *line /* an 80-char string */,
			     char key_str[], size_t n_key_str, /* return */
			     char val_str[], size_t n_val_str )
{
    int return_status = -1;

    if ( ('a' <= line[0] && line[0] <= 'z') ||
	 ('A' <= line[0] && line[0] <= 'Z') || line[0] == '_' ) {
	size_t i, n0=0, n1=0, n2=0, n3=0;
	for ( i=0 ; i < FITS_HEADER_RECORD_UNIT ; i++ ) {
	    if ( n0 == 0 ) {
		if ( line[i] == ' ' || line[i] == '=' ) n0=i;  /* key length */
	    }
	    if ( n1 == 0 ) {
		if ( line[i] == '=' ) n1 = i;
	    }
	    else {
		if ( n2 == 0 ) {
		    if ( line[i] != ' ' ) {
			n2 = i;	/* begins val */
			n3 = n2;
		    }
		}
		else {
		    if ( n3 <= n2 ) {
			if ( line[n2] == 39  /* "'" */  ) {
			    if ( line[i] == 39 ) n3 = i+1;
			}
			else {
			    if ( line[i] == ' ' ) n3 = i;
			}
		    }
		    else {
			break;
		    }
		}
	    }
	}
	
	safe_strncpy(key_str,n_key_str,line+0,n0);
	safe_strncpy(val_str,n_val_str,line+n2,n3-n2);
	
	return_status = 0;
    }

    return return_status;
}

static int get_matrix_type_and_info_fits(
	const char *filename_in, int matrix_type_in,
	float basic_info5[], float daylight_mul[], float camera_mul[],
	long fits_size_info[],   /* hdr-blk-len, bitpix, width, height, nz */
	float fits_pix_info[]	   /* bzero, bscale */ )
{
    int return_status = -1;
    char line[FITS_HEADER_RECORD_UNIT + 1];
    char key[64];
    char val[64];
    FILE *fp = NULL;
    size_t n_header_rec;
    int matrix_type = Matrix_NONE;

    const char *str_bitpix    = "BITPIX";
    const char *str_naxis     = "NAXIS";
    const char *str_naxis1    = "NAXIS1";
    const char *str_naxis2    = "NAXIS2";
    const char *str_naxis3    = "NAXIS3";
    const char *str_bzero     = "BZERO";
    const char *str_bscale    = "BSCALE";
    const char *str_bayerpat  = "BAYERPAT";
    const char *str_end       = "END";

    fp = fopen(filename_in,"r");
    if ( fp == NULL ) goto quit;

    basic_info5[0] = 0.0;	/* ISO */
    basic_info5[1] = 0.0;	/* Shutter */
    basic_info5[2] = 0.0;	/* F */
    basic_info5[3] = 0.0;	/* f */
    basic_info5[4] = 0.0;	/* raw_colors */
    daylight_mul[0] = 0.0;
    daylight_mul[1] = 0.0;
    daylight_mul[2] = 0.0;
    camera_mul[0] = 0.0;
    camera_mul[1] = 0.0;
    camera_mul[2] = 0.0;
    camera_mul[3] = 0.0;

    fits_size_info[0] = 0;
    fits_size_info[1] = 0;
    fits_size_info[2] = 0;
    fits_size_info[3] = 0;
    fits_size_info[4] = 1;	/* n of z */
    fits_pix_info[0] = 0.0;
    fits_pix_info[1] = 1.0;	/* bscale */
    
    n_header_rec = 0;
    while ( fgets(line,FITS_HEADER_RECORD_UNIT + 1,fp) != NULL ) {
	if ( get_fits_key_val(line,key,64,val,64) < 0 ) break;
	else n_header_rec ++;
	/* printf("key=[%s] val=[%s]\n",key,val); */
	/* */
	if ( strcmp(key, str_bitpix) == 0 ) fits_size_info[1] = atol(val);
	else if ( strcmp(key, str_naxis1) == 0 ) fits_size_info[2] = atol(val);
	else if ( strcmp(key, str_naxis2) == 0 ) fits_size_info[3] = atol(val);
	else if ( strcmp(key, str_naxis3) == 0 ) fits_size_info[4] = atol(val);
	else if ( strcmp(key, str_bzero) == 0 ) fits_pix_info[0] = atof(val);
	else if ( strcmp(key, str_bscale) == 0 ) fits_pix_info[1] = atof(val);
	else if ( strcmp(key, str_bayerpat) == 0 ) {
	    if ( strncmp(val+1,"RGGB",4)==0 ) matrix_type = Matrix_RG_GB;
	    else if ( strncmp(val+1,"GRBG",4)==0 ) matrix_type = Matrix_GR_BG;
	}
	else if ( strcmp(key, str_naxis) == 0 ) {
	    if ( atoi(val) < 2 || 3 < atoi(val) ) goto quit;	/* ERROR */
	}
	else if ( strcmp(key, str_end) == 0 ) break;
    }
    if ( 0 < n_header_rec ) {
	size_t sz_header_used = FITS_HEADER_RECORD_UNIT * n_header_rec;
	fits_size_info[0] = 1 + (sz_header_used - 1) / FITS_FILE_RECORD_UNIT;
    }
    
    if ( matrix_type_in == Matrix_NONE ) {
	return_status = matrix_type;
    }
    else {
	return_status = matrix_type_in;
    }
 quit:
    if ( fp != NULL ) pclose(fp);
    return return_status;
}

/* Get matrix type using dcraw */
static int get_matrix_type_and_info_raw(
	const char *filename_in, int matrix_type_in,
	float basic_info5[], float daylight_mul[], float camera_mul[],
	long fits_size_info[],   /* hdr-blk-len, bitpix, width, height, nz */
	float fits_pix_info[]	   /* bzero, bscale */ )
{
    int return_status = -1;
    const char *str_iso_speed = "ISO speed: ";
    const char *str_shutter = "Shutter: ";
    const char *str_shutter_1 = "Shutter: 1/";
    const char *str_aperture = "Aperture: ";
    const char *str_focal_length = "Focal length: ";
    const char *str_raw_colors = "Raw colors: ";
    const char *str_fpat_rg_gb = "Filter pattern: RG/GB\n";
    const char *str_fpat_gr_bg = "Filter pattern: GR/BG\n";
    const char *str_daylight_mul = "Daylight multipliers: ";
    const char *str_camera_mul = "Camera multipliers: ";

    char cmd[32 + PATH_MAX];
    char line[256];
    FILE *pp = NULL;
    int matrix_type = Matrix_NONE;
    
    snprintf(cmd, 32 + PATH_MAX, "dcraw -i -v %s", filename_in);
    pp = popen(cmd, "r");
    if ( pp == NULL ) goto quit;			/* ERROR */

    basic_info5[0] = 0.0;	/* ISO */
    basic_info5[1] = 0.0;	/* Shutter */
    basic_info5[2] = 0.0;	/* F */
    basic_info5[3] = 0.0;	/* f */
    basic_info5[4] = 0.0;	/* raw_colors */
    daylight_mul[0] = 0.0;
    daylight_mul[1] = 0.0;
    daylight_mul[2] = 0.0;
    camera_mul[0] = 0.0;
    camera_mul[1] = 0.0;
    camera_mul[2] = 0.0;
    camera_mul[3] = 0.0;

    fits_size_info[0] = 0;
    fits_size_info[1] = 0;
    fits_size_info[2] = 0;
    fits_size_info[3] = 0;
    fits_size_info[4] = 0;
    fits_pix_info[0] = 0.0;
    fits_pix_info[1] = 0.0;

    while ( fgets(line,256,pp) != NULL ) {
	if ( strcmp(line,str_fpat_rg_gb) == 0 ) matrix_type = Matrix_RG_GB;
	else if ( strcmp(line,str_fpat_gr_bg) == 0 ) matrix_type = Matrix_GR_BG;
	else if ( strncmp(line,str_iso_speed,strlen(str_iso_speed)) == 0 ) {
	    sscanf(line + strlen(str_iso_speed), "%f", basic_info5+0);
	}
	else if ( strncmp(line,str_shutter_1,strlen(str_shutter_1)) == 0 ) {
	    sscanf(line + strlen(str_shutter_1), "%f", basic_info5+1);
	    if ( basic_info5[1] != 0.0 ) {
		basic_info5[1] = 1.0 / basic_info5[1];
	    }
	}
	else if ( strncmp(line,str_shutter,strlen(str_shutter)) == 0 ) {
	    sscanf(line + strlen(str_shutter), "%f", basic_info5+1);
	}
	else if ( strncmp(line,str_aperture,strlen(str_aperture)) == 0 ) {
	    const char *p1 = line + strlen(str_aperture);
	    if ( (p1[0] == 'f' || p1[0] == 'F') && p1[1] == '/' ) {
		sscanf(p1 + 2, "%f", basic_info5+2);
	    }
	}
	else if ( strncmp(line,str_focal_length,strlen(str_focal_length)) == 0 ) {
	    sscanf(line + strlen(str_focal_length), "%f", basic_info5+3);
	}
	else if ( strncmp(line,str_raw_colors,strlen(str_raw_colors)) == 0 ) {
	    sscanf(line + strlen(str_raw_colors), "%f", basic_info5+4);
	}
	else if ( strncmp(line,str_daylight_mul,strlen(str_daylight_mul)) == 0 ) {
	    sscanf(line + strlen(str_daylight_mul), "%f %f %f",
		   daylight_mul + 0, daylight_mul + 1, daylight_mul + 2);
	}
	else if ( strncmp(line,str_camera_mul,strlen(str_camera_mul)) == 0 ) {
	    sscanf(line + strlen(str_camera_mul), "%f %f %f %f",
		   camera_mul + 0, camera_mul + 1, camera_mul + 2, camera_mul + 3);
	}
    }
    
    if ( matrix_type_in == Matrix_NONE ) {
	return_status = matrix_type;
    }
    else {
	return_status = matrix_type_in;
    }
 quit:
    if ( pp != NULL ) pclose(pp);
    return return_status;
}

/* Get matrix type for TIFF */
static int get_matrix_type_and_info_tiff(
	const char *filename_in, int matrix_type_in,
	float basic_info5[], float daylight_mul[], float camera_mul[],
	long fits_size_info[],   /* hdr-blk-len, bitpix, width, height, nz */
	float fits_pix_info[]	   /* bzero, bscale */ )
{
    int return_status = -1;

    basic_info5[0] = 0.0;	/* ISO */
    basic_info5[1] = 0.0;	/* Shutter */
    basic_info5[2] = 0.0;	/* F */
    basic_info5[3] = 0.0;	/* f */
    basic_info5[4] = 0.0;	/* raw_colors */
    daylight_mul[0] = 0.0;
    daylight_mul[1] = 0.0;
    daylight_mul[2] = 0.0;
    camera_mul[0] = 0.0;
    camera_mul[1] = 0.0;
    camera_mul[2] = 0.0;
    camera_mul[3] = 0.0;

    fits_size_info[0] = 0;
    fits_size_info[1] = 0;
    fits_size_info[2] = 0;
    fits_size_info[3] = 0;
    fits_size_info[4] = 0;
    fits_pix_info[0] = 0.0;
    fits_pix_info[1] = 0.0;

    if ( matrix_type_in == Matrix_NONE ) {
	fprintf(stderr,"[INFO] Assuming RG/GB type matrix.\n");
	return_status = Matrix_RG_GB;	/* default */
    }
    else {
	return_status = matrix_type_in;
    }
 quit:
    return return_status;
}

/* Get matrix type and camera info */
static int get_matrix_type_and_info(
	const char *filename_in, int matrix_type_in,
	float basic_info5[], float daylight_mul[], float camera_mul[],
	long fits_size_info[],   /* hdr-blk-len, bitpix, width, height, nz */
	float fits_pix_info[]	   /* bzero, bscale */ )
{
    int return_status = -1;
    int file_type;

    file_type = check_a_file(filename_in);

    if ( file_type < 0 ) goto quit;	/* ERROR */
    else if ( file_type == 0 ) {
	/* RAW: using dcraw */
	return_status = get_matrix_type_and_info_raw(
				     filename_in, matrix_type_in,
				     basic_info5, daylight_mul, camera_mul,
				     fits_size_info, fits_pix_info );
    }
    else if ( file_type == 1 ) {
	/* FITS */
	return_status = get_matrix_type_and_info_fits(
				     filename_in, matrix_type_in,
				     basic_info5, daylight_mul, camera_mul,
				     fits_size_info, fits_pix_info );
    }
    else {
	/* TIFF */
	return_status = get_matrix_type_and_info_tiff(
				     filename_in, matrix_type_in,
				     basic_info5, daylight_mul, camera_mul,
				     fits_size_info, fits_pix_info );
    }
    
 quit:
    return return_status;
}

static int get_crop_prms( const char *opt, long prms[] )
{
    int return_status = -1;
    size_t off, i;
    long val[4];

    if ( opt == NULL ) goto quit;
    
    off = 0;
    for ( i=0 ; ; i++ ) {
        size_t spn;
        spn = strspn(opt + off,"0123456789");
	if ( spn == 0 ) val[i] = 0;
	else val[i] = atol(opt + off);
	if ( val[i] < 0 ) {
	    fprintf(stderr,"[ERROR] Invalid -c option\n");
	    goto quit;
	}
	off += spn;
	if ( i == 3 ) break;
	if ( opt[off] == '\0' ) break;
	if ( opt[off] != ',' ) {
	    fprintf(stderr,"[ERROR] Invalid -c option\n");
	    goto quit;
	}
	off ++;
    }
    if ( i == 0 ) {
        prms[0] = -1;
	prms[1] = -1;
	prms[2] = val[0];
	prms[3] = 0;
    }
    else if ( i == 1 ) {
        prms[0] = -1;
	prms[1] = -1;
	prms[2] = val[0];
	prms[3] = val[1];
    }
    else if ( i == 2 ) {
        prms[0] = val[0];
	prms[1] = -1;
	prms[2] = val[1];
	prms[3] = val[2];
    }
    else {
        prms[0] = val[0];
	prms[1] = val[1];
	prms[2] = val[2];
	prms[3] = val[3];
    }

    return_status = 0;
 quit:
    return return_status;
}

static int create_tiff_filename( const char *filename_in,
				 char *filename_out_buf, size_t buf_len )
{
    int return_status = -1;
    size_t i, dot_idx = 0;
    char basename[PATH_MAX] = {'\0'};
    char suffix_case[PATH_MAX] = {'\0'};

    if ( filename_in == NULL ) goto quit;
    
    for ( i = 0 ; i < PATH_MAX ; i++ ) {
	basename[i] = filename_in[i];
        if ( basename[i] == '\0' ) break;
        if ( filename_in[i] == '.' ) dot_idx = i;
    }
    if ( basename[dot_idx] == '.' ) {
	basename[dot_idx] = '\0';
	for ( i=0 ; i < PATH_MAX ; i++ ) {
	    suffix_case[i] = filename_in[dot_idx + i];
	    if ( suffix_case[i] == '\0' ) break;
	    if ( 'A' <= suffix_case[i] && suffix_case[i] <= 'Z' ) {
		suffix_case[i] += 0x20;		/* to lower case */
	    }
	}
    }

    if ( strcmp(suffix_case, ".tiff") == 0 ) {
	snprintf(filename_out_buf, buf_len, "%s%s", basename, ".tif");
    }
    else {
	snprintf(filename_out_buf, buf_len, "%s%s", basename, ".tiff");
    }

    return_status = 0;
 quit:
    return return_status;
}

/*
 *  e.g., 6000x4000 bayer RGB image sensor => 3000x2000 RGB output
 */
static int bayer_to_half( int bayer_matrix,	       /* bayer type       */
			  int bayer_direct,            /* not interp if non0 */
			  const int bit_used[],
			  FILE *pp,		       /* input stream     */
			  const unsigned char *in_img_buf, /* input img buf */
			  TIFF *tiff_out,	       /* output TIFF inst */
			  uint32 width, uint32 height, /* w/h of output    */
			  uint16 byps, uint16 spp,     /* 2 and 3          */
			  uint32 x_out, uint32 y_out,  /* actual crop area */
			  uint32 width_out, uint32 height_out,
			  float fits_pix_info[], bool byte_swap )
{
    const float fits_bzero = fits_pix_info[0];
    const float fits_bscale = fits_pix_info[1];
    
    int return_status = -1;

    uint32 i;
    unsigned char *strip_buf_in = NULL;
    uint16 *strip_buf_out = NULL;

    uint32 shift_for_scale[3];
    uint32 shift_for_scale_r;
    uint32 shift_for_scale_g;
    uint32 shift_for_scale_b;

    for ( i=0 ; i < 3 ; i++ ) {
	if ( 0 < bit_used[i] && bit_used[i] < 16 )
	    shift_for_scale[i] = 16 - bit_used[i];
	else
	    shift_for_scale[i] = 0;
    }
    shift_for_scale_r = shift_for_scale[0];
    shift_for_scale_g = shift_for_scale[1];
    shift_for_scale_b = shift_for_scale[2];

    if ( 1 < bayer_direct ) bayer_matrix = Matrix_NONE;   /* Monochrome Mode */
    
    if ( byps != 2 ) {
        fprintf(stderr,"[ERROR] 'byps' should be 2\n");
	goto quit;
    }
    if ( spp != 3 ) {
        fprintf(stderr,"[ERROR] 'spp' should be 3\n");
	goto quit;
    }
   
    strip_buf_in = (unsigned char *)malloc(sizeof(uint16) * 2 * width * 2);
    if ( strip_buf_in == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
	goto quit;
    }

    strip_buf_out = (uint16 *)malloc(byps * spp * width_out);
    if ( strip_buf_out == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
	goto quit;
    }

    /* */
    for ( i=0 ; i < height ; i ++ ) {
        const size_t n_elem_to_read = 2 * width * 2;
        uint32 j, j_in, j_out;
	uint32 v_r, v_g, v_b, v_mono;
	if ( pp != NULL ) {
	    if ( fread(strip_buf_in, 1, sizeof(uint16) * n_elem_to_read, pp)
		 != sizeof(uint16) * n_elem_to_read ) {
		fprintf(stderr,"[ERROR] fread() failed\n");
		goto quit;
	    }
	} else if ( in_img_buf != NULL ) {
	    memcpy(strip_buf_in,
		   in_img_buf + sizeof(uint16) * n_elem_to_read * i,
		   sizeof(uint16) * n_elem_to_read);
	}
	if ( byte_swap == true ) s_byteswap2(strip_buf_in, n_elem_to_read);
	if ( 0.0 < fits_bscale ) {
	    size_t ix;
	    const int16 *pr = (const int16 *)strip_buf_in;
	    uint16 *pw = (uint16 *)strip_buf_in;
	    for ( ix=0 ; ix < n_elem_to_read ; ix++ ) {
		float val = 0.5 + fits_bzero + fits_bscale * pr[ix];
		if ( val < 0.0 ) pw[ix] = (uint16)0;
		else if ( 65535.0 < val ) pw[ix] = (uint16)65535;
		else pw[ix] = (uint16)val;
	    }
	}
	if ( byte_swap == true ) s_byteswap2(strip_buf_in, n_elem_to_read);
	if ( y_out <= i && i < y_out + height_out ) {
	    if ( bayer_matrix == Matrix_GR_BG ) {
	        j_in = 4 * (0 + x_out);
		for ( j=0, j_out=0 ; j < width_out ; j++ ) {
		    v_g = strip_buf_in[j_in];  j_in ++;
		    v_g <<= 8;
		    v_g |= strip_buf_in[j_in]; j_in ++;
		    v_r = strip_buf_in[j_in];  j_in ++;
		    v_r <<= 8;
		    v_r |= strip_buf_in[j_in]; j_in ++;
		    /* */
		    v_r <<= shift_for_scale_r;
		    if ( 65535 < v_r ) v_r = 65535;
		    v_g <<= shift_for_scale_g;
		    if ( 65535 < v_g ) v_g = 65535;
		    /**/
		    strip_buf_out[j_out] = v_r;  j_out ++;
		    strip_buf_out[j_out] = v_g;  j_out ++;
		    j_out ++;
		}
		j_in = 4 * (width + x_out);
		for ( j=0, j_out=0 ; j < width_out ; j++ ) {
		    v_b = strip_buf_in[j_in];  j_in ++;
		    v_b <<= 8;
		    v_b |= strip_buf_in[j_in]; j_in ++;
		    v_g = strip_buf_in[j_in];  j_in ++;
		    v_g <<= 8;
		    v_g |= strip_buf_in[j_in]; j_in ++;
		    /* */
		    v_b <<= shift_for_scale_b;
		    if ( 65535 < v_b ) v_b = 65535;
		    v_g <<= shift_for_scale_g;
		    if ( 65535 < v_g ) v_g = 65535;
		    /**/
		    j_out ++;
		    v_g += strip_buf_out[j_out];
		    v_g >>= 1;
		    strip_buf_out[j_out] = v_g;  j_out ++;
		    strip_buf_out[j_out] = v_b;  j_out ++;
		}
	    }
	    else if ( bayer_matrix == Matrix_RG_GB ) {	      /* popular one */
	        j_in = 4 * (0 + x_out);
		for ( j=0, j_out=0 ; j < width_out ; j++ ) {
		    v_r = strip_buf_in[j_in];  j_in ++;
		    v_r <<= 8;
		    v_r |= strip_buf_in[j_in]; j_in ++;
		    v_g = strip_buf_in[j_in];  j_in ++;
		    v_g <<= 8;
		    v_g |= strip_buf_in[j_in]; j_in ++;
		    /* */
		    v_r <<= shift_for_scale_r;
		    if ( 65535 < v_r ) v_r = 65535;
		    v_g <<= shift_for_scale_g;
		    if ( 65535 < v_g ) v_g = 65535;
		    /**/
		    strip_buf_out[j_out] = v_r;  j_out ++;
		    strip_buf_out[j_out] = v_g;  j_out ++;
		    j_out ++;
		}
		j_in = 4 * (width + x_out);
		for ( j=0, j_out=0 ; j < width_out ; j++ ) {
		    v_g = strip_buf_in[j_in];  j_in ++;
		    v_g <<= 8;
		    v_g |= strip_buf_in[j_in]; j_in ++;
		    v_b = strip_buf_in[j_in];  j_in ++;
		    v_b <<= 8;
		    v_b |= strip_buf_in[j_in]; j_in ++;
		    /* */
		    v_b <<= shift_for_scale_b;
		    if ( 65535 < v_b ) v_b = 65535;
		    v_g <<= shift_for_scale_g;
		    if ( 65535 < v_g ) v_g = 65535;
		    /* */
		    j_out ++;
		    v_g += strip_buf_out[j_out];
		    v_g >>= 1;
		    strip_buf_out[j_out] = v_g;  j_out ++;
		    strip_buf_out[j_out] = v_b;  j_out ++;
		}
	    }
	    else {		   	      /* NOT bayer (Monochrome mode) */
	        j_in = 4 * (0 + x_out);
		for ( j=0, j_out=0 ; j < width_out ; j++ ) {
		    v_r = strip_buf_in[j_in];  j_in ++;
		    v_r <<= 8;
		    v_r |= strip_buf_in[j_in]; j_in ++;
		    v_g = strip_buf_in[j_in];  j_in ++;
		    v_g <<= 8;
		    v_g |= strip_buf_in[j_in]; j_in ++;
		    /**/
		    strip_buf_out[j_out] = v_r;  j_out ++;
		    strip_buf_out[j_out] = v_g;  j_out ++;
		    j_out ++;
		}
		j_in = 4 * (width + x_out);
		for ( j=0, j_out=0 ; j < width_out ; j++ ) {
		    v_g = strip_buf_in[j_in];  j_in ++;
		    v_g <<= 8;
		    v_g |= strip_buf_in[j_in]; j_in ++;
		    v_b = strip_buf_in[j_in];  j_in ++;
		    v_b <<= 8;
		    v_b |= strip_buf_in[j_in]; j_in ++;
		    /* */
		    v_mono = strip_buf_out[j_out];
		    v_mono += strip_buf_out[j_out + 1];
		    v_mono += v_g + v_b;
		    v_r = v_mono;
		    v_r <<= shift_for_scale_r;
		    if ( 65535 < v_r ) v_r = 65535;
		    v_g = v_mono;
		    v_g <<= shift_for_scale_g;
		    if ( 65535 < v_g ) v_g = 65535;
		    v_b = v_mono;
		    v_b <<= shift_for_scale_b;
		    if ( 65535 < v_b ) v_b = 65535;
		    /* */
		    strip_buf_out[j_out] = v_r;  j_out ++;
		    strip_buf_out[j_out] = v_g;  j_out ++;
		    strip_buf_out[j_out] = v_b;  j_out ++;
		}
	    }
	    /* */
	    if ( TIFFWriteEncodedStrip(tiff_out, i - y_out, strip_buf_out,
				       byps * spp * width_out) == 0 ) {
	        fprintf(stderr,"[ERROR] TIFFWriteEncodedStrip() failed\n");
		goto quit;
	    }
	}
    }

    return_status = 0;
 quit:
    if ( strip_buf_out != NULL ) free(strip_buf_out);
    if ( strip_buf_in != NULL ) free(strip_buf_in);

    return return_status;
}

/*
 *  e.g., 6000x4000 bayer RGB image sensor => 6000x4000 RGB output
 */
static int bayer_to_full( int bayer_matrix,	       /* bayer type       */
			  int bayer_direct,            /* not interp if non0 */
			  const int bit_used[],
			  FILE *pp,		       /* input stream     */
			  const unsigned char *in_img_buf,
			  TIFF *tiff_out,	       /* output TIFF inst */
			  uint32 width, uint32 height, /* w/h of output    */
			  uint16 byps, uint16 spp,     /* 2 and 3          */
			  uint32 x_out, uint32 y_out,  /* actual crop area */
			  uint32 width_out, uint32 height_out,
			  float fits_pix_info[], bool byte_swap )
{
    const float fits_bzero = fits_pix_info[0];
    const float fits_bscale = fits_pix_info[1];
    
    int return_status = -1;

    uint32 i, num_i, line_cnt_out;
    unsigned char *strip_buf_in = NULL;		/* to read 2 sensor lines  */
    float *stripe_buf = NULL;			/* work buffer for 4 lines */
    uint16 *strip_buf_out = NULL;		/* buffer for final output */

    uint32 stripe0unit_width = spp * (1+width+1);
    uint32 idx_stripe = 0;
    
    float *stripe0ptr[4] = {NULL,NULL,NULL,NULL}; /* fixed ptr of stripe_buf */
    float *stripe_ptr[4] = {NULL,NULL,NULL,NULL}; /* rotational ptr of stripe_buf */

    float interp_0_25 = 0.25;
    float interp_0_5 = 0.5;

    uint32 shift_for_scale[3];
    uint32 shift_for_scale_r;
    uint32 shift_for_scale_g;
    uint32 shift_for_scale_b;

    for ( i=0 ; i < 3 ; i++ ) {
	if ( 0 < bit_used[i] && bit_used[i] < 16 )
	    shift_for_scale[i] = 16 - bit_used[i];
	else
	    shift_for_scale[i] = 0;
    }
    shift_for_scale_r = shift_for_scale[0];
    shift_for_scale_g = shift_for_scale[1];
    shift_for_scale_b = shift_for_scale[2];

    if ( 1 < bayer_direct ) bayer_matrix = Matrix_NONE;   /* Monochrome Mode */

    if ( byps != 2 ) {
        fprintf(stderr,"[ERROR] 'byps' should be 2\n");
	goto quit;
    }
    if ( spp != 3 ) {
        fprintf(stderr,"[ERROR] 'spp' should be 3\n");
	goto quit;
    }

    if ( bayer_direct != 0 ) {
        interp_0_25 = 0.0;
	interp_0_5 = 0.0;
    }
    
    strip_buf_in = (unsigned char *)malloc(sizeof(uint16) * width * 2);
    if ( strip_buf_in == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
	goto quit;
    }

    stripe_buf = (float *)malloc(sizeof(float) * stripe0unit_width * 4);
    if ( stripe_buf == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
	goto quit;
    }
    memset(stripe_buf, 0, sizeof(float) * stripe0unit_width * 4);
    stripe0ptr[0] = stripe_buf + spp*1 /* offset for interpolation */;
    stripe0ptr[1] = stripe0ptr[0] + stripe0unit_width;
    stripe0ptr[2] = stripe0ptr[1] + stripe0unit_width;
    stripe0ptr[3] = stripe0ptr[2] + stripe0unit_width;

    /* e.g. 1st time is ...
    stripe_ptr[0] = stripe0ptr[0];
    stripe_ptr[1] = stripe0ptr[1];
    stripe_ptr[2] = stripe0ptr[2];
    stripe_ptr[3] = stripe0ptr[3];
            2nd time is ...
    stripe_ptr[0] = stripe0ptr[2];
    stripe_ptr[1] = stripe0ptr[3];
    stripe_ptr[2] = stripe0ptr[0];	// should be reset 
    stripe_ptr[3] = stripe0ptr[1];	// should be reset 
    */
    /* stripe_ptr[1] and [2] should be used for reading raw direct data */
    stripe_ptr[0] = stripe0ptr[(idx_stripe+0) % 4];
    stripe_ptr[1] = stripe0ptr[(idx_stripe+1) % 4];
    stripe_ptr[2] = stripe0ptr[(idx_stripe+2) % 4];
    stripe_ptr[3] = stripe0ptr[(idx_stripe+3) % 4];
    memset(stripe_ptr[2] - spp*1, 0, sizeof(float) * stripe0unit_width * 2);
    idx_stripe += 2;
    
    strip_buf_out = (uint16 *)malloc(byps * spp * width_out);
    if ( strip_buf_out == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
    	goto quit;
    }

    /* */
    line_cnt_out = 0;
    num_i = height / 2;
    for ( i=0 ; i < num_i + 1 ; i ++ ) {
        const size_t n_elem_to_read = width * 2; /* 2 lines */
        uint32 j, num_j, j_in;
        long j_out;		/* should be signed integer! */
	uint32 v_r, v_g, v_b, v_mono;
	num_j = width / 2;
	if ( i < num_i ) {
	    if ( pp != NULL ) {
		if ( fread(strip_buf_in, 1, sizeof(uint16) * n_elem_to_read, pp)
		     != sizeof(uint16) * n_elem_to_read ) {
		    fprintf(stderr,"[ERROR] fread() failed\n");
		    goto quit;
		}
	    }
	    else if ( in_img_buf != NULL ) {
		memcpy(strip_buf_in,
		       in_img_buf + sizeof(uint16) * n_elem_to_read * i,
		       sizeof(uint16) * n_elem_to_read);
	    }
	    if ( byte_swap == true ) s_byteswap2(strip_buf_in, n_elem_to_read);
	    if ( 0.0 < fits_bscale ) {
		size_t ix;
		const int16 *pr = (const int16 *)strip_buf_in;
		uint16 *pw = (uint16 *)strip_buf_in;
		for ( ix=0 ; ix < n_elem_to_read ; ix++ ) {
		    float val = 0.5 + fits_bzero + fits_bscale * pr[ix];
		    if ( val < 0.0 ) pw[ix] = (uint16)0;
		    else if ( 65535.0 < val ) pw[ix] = (uint16)65535;
		    else pw[ix] = (uint16)val;
		}
	    }
	    if ( byte_swap == true ) s_byteswap2(strip_buf_in, n_elem_to_read);
	    if ( bayer_matrix == Matrix_GR_BG ) {
	        /*** upper line ***/
	        j_in = sizeof(uint16) * 0;
		for ( j=0, j_out=0 ; j < num_j ; j++ ) {
		  v_g = strip_buf_in[j_in];  j_in ++;
		  v_g <<= 8;
		  v_g |= strip_buf_in[j_in]; j_in ++;
		  v_r = strip_buf_in[j_in];  j_in ++;
		  v_r <<= 8;
		  v_r |= strip_buf_in[j_in]; j_in ++;
		  /* */
		  v_r <<= shift_for_scale_r;
		  v_g <<= shift_for_scale_g;
		  /**/
		                               j_out ++; /* left RGB pixel */
		  stripe_ptr[0][j_out]     += v_g * interp_0_25;
		  stripe_ptr[1][j_out-spp] += v_g * interp_0_25;
		  stripe_ptr[1][j_out]     += v_g;
		  stripe_ptr[1][j_out+spp] += v_g * interp_0_25;
		  stripe_ptr[2][j_out]     += v_g * interp_0_25;
		                               j_out ++;
					       j_out ++;
		  stripe_ptr[0][j_out-spp] += v_r * interp_0_25;
		  stripe_ptr[0][j_out]     += v_r * interp_0_5;
		  stripe_ptr[0][j_out+spp] += v_r * interp_0_25;
		  stripe_ptr[1][j_out-spp] += v_r * interp_0_5;
		  stripe_ptr[1][j_out]     += v_r;
		  stripe_ptr[1][j_out+spp] += v_r * interp_0_5;
		  stripe_ptr[2][j_out-spp] += v_r * interp_0_25;
		  stripe_ptr[2][j_out]     += v_r * interp_0_5;
		  stripe_ptr[2][j_out+spp] += v_r * interp_0_25;
		                               j_out ++; /* right RGB pixel */
					       j_out ++;
					       j_out ++;
#if 0
		  stripe_ptr[1][j_out] = 0;    j_out ++; /* left RGB pixel */
		  stripe_ptr[1][j_out] = v_g;  j_out ++;
		  stripe_ptr[1][j_out] = 0;    j_out ++;
		  stripe_ptr[1][j_out] = v_r;  j_out ++; /* right RGB pixel */
		  stripe_ptr[1][j_out] = 0;    j_out ++;
		  stripe_ptr[1][j_out] = 0;    j_out ++;
#endif
		}
		/*** lower line ***/
		j_in = sizeof(uint16) * width;
		for ( j=0, j_out=0 ; j < num_j ; j++ ) {
		  v_b = strip_buf_in[j_in];  j_in ++;
		  v_b <<= 8;
		  v_b |= strip_buf_in[j_in]; j_in ++;
		  v_g = strip_buf_in[j_in];  j_in ++;
		  v_g <<= 8;
		  v_g |= strip_buf_in[j_in]; j_in ++;
		  /* */
		  v_b <<= shift_for_scale_b;
		  v_g <<= shift_for_scale_g;
		  /**/
		                               j_out ++; /* left RGB pixel */
		                               j_out ++;
		  stripe_ptr[1][j_out-spp] += v_b * interp_0_25;
		  stripe_ptr[1][j_out]     += v_b * interp_0_5;
		  stripe_ptr[1][j_out+spp] += v_b * interp_0_25;
		  stripe_ptr[2][j_out-spp] += v_b * interp_0_5;
		  stripe_ptr[2][j_out]     += v_b;
		  stripe_ptr[2][j_out+spp] += v_b * interp_0_5;
		  stripe_ptr[3][j_out-spp] += v_b * interp_0_25;
		  stripe_ptr[3][j_out]     += v_b * interp_0_5;
		  stripe_ptr[3][j_out+spp] += v_b * interp_0_25;
		                               j_out ++;
					       j_out ++; /* right RGB pixel */
		  stripe_ptr[1][j_out]     += v_g * interp_0_25;
		  stripe_ptr[2][j_out-spp] += v_g * interp_0_25;
		  stripe_ptr[2][j_out]     += v_g;
		  stripe_ptr[2][j_out+spp] += v_g * interp_0_25;
		  stripe_ptr[3][j_out]     += v_g * interp_0_25;
		                               j_out ++;
		                               j_out ++;
#if 0
		  stripe_ptr[2][j_out] = 0;    j_out ++; /* left RGB pixel */
		  stripe_ptr[2][j_out] = 0;    j_out ++;
		  stripe_ptr[2][j_out] = v_b;  j_out ++;
		  stripe_ptr[2][j_out] = 0;    j_out ++; /* right RGB pixel */
		  stripe_ptr[2][j_out] = v_g;  j_out ++;
		  stripe_ptr[2][j_out] = 0;    j_out ++;
#endif
		}
	    }
	    else if ( bayer_matrix == Matrix_RG_GB ) {	      /* popular one */
	        /*** upper line ***/
	        j_in = sizeof(uint16) * 0;
	        for ( j=0, j_out=0 ; j < num_j ; j++ ) {
		  v_r = strip_buf_in[j_in];  j_in ++;
		  v_r <<= 8;
		  v_r |= strip_buf_in[j_in]; j_in ++;
		  v_g = strip_buf_in[j_in];  j_in ++;
		  v_g <<= 8;
		  v_g |= strip_buf_in[j_in]; j_in ++;
		  /* */
		  v_r <<= shift_for_scale_r;
		  v_g <<= shift_for_scale_g;
		  /*
		   * R: (+ 1 0.5 0.5 0.5 0.5 0.25 0.25 0.25 0.25) (* 0.25 0.5)
		   */
		  stripe_ptr[0][j_out-spp] += v_r * interp_0_25;
		  stripe_ptr[0][j_out]     += v_r * interp_0_5;
		  stripe_ptr[0][j_out+spp] += v_r * interp_0_25;
		  stripe_ptr[1][j_out-spp] += v_r * interp_0_5;
		  stripe_ptr[1][j_out]     += v_r;
		  stripe_ptr[1][j_out+spp] += v_r * interp_0_5;
		  stripe_ptr[2][j_out-spp] += v_r * interp_0_25;
		  stripe_ptr[2][j_out]     += v_r * interp_0_5;
		  stripe_ptr[2][j_out+spp] += v_r * interp_0_25;
		                                j_out ++; /* left RGB pixel */
		                                j_out ++;
						j_out ++;

						j_out ++; /* right RGB pixel */
		  stripe_ptr[0][j_out]     += v_g * interp_0_25;
		  stripe_ptr[1][j_out-spp] += v_g * interp_0_25;
		  stripe_ptr[1][j_out]     += v_g;
		  stripe_ptr[1][j_out+spp] += v_g * interp_0_25;
		  stripe_ptr[2][j_out]     += v_g * interp_0_25;
		                                j_out ++;
	                                        j_out ++;
#if 0
		  stripe_ptr[1][j_out] = v_r;  j_out ++; /* left RGB pixel */
		  stripe_ptr[1][j_out] = 0;    j_out ++;
		  stripe_ptr[1][j_out] = 0;    j_out ++;
		  stripe_ptr[1][j_out] = 0;    j_out ++; /* right RGB pixel */
		  stripe_ptr[1][j_out] = v_g;  j_out ++;
		  stripe_ptr[1][j_out] = 0;    j_out ++;
#endif
		}
		/*** lower line ***/
		j_in = sizeof(uint16) * width;
		for ( j=0, j_out=0 ; j < num_j ; j++ ) {
		  v_g = strip_buf_in[j_in];  j_in ++;
		  v_g <<= 8;
		  v_g |= strip_buf_in[j_in]; j_in ++;
		  v_b = strip_buf_in[j_in];  j_in ++;
		  v_b <<= 8;
		  v_b |= strip_buf_in[j_in]; j_in ++;
		  /* */
		  v_b <<= shift_for_scale_b;
		  v_g <<= shift_for_scale_g;
		  /**/
		                               j_out ++; /* left RGB pixel */
		  stripe_ptr[1][j_out]     += v_g * interp_0_25;
		  stripe_ptr[2][j_out-spp] += v_g * interp_0_25;
		  stripe_ptr[2][j_out]     += v_g;
		  stripe_ptr[2][j_out+spp] += v_g * interp_0_25;
		  stripe_ptr[3][j_out]     += v_g * interp_0_25;
		                               j_out ++;
		                               j_out ++;
		                               j_out ++; /* right RGB pixel */
		                               j_out ++;
		  stripe_ptr[1][j_out-spp] += v_b * interp_0_25;
		  stripe_ptr[1][j_out]     += v_b * interp_0_5;
		  stripe_ptr[1][j_out+spp] += v_b * interp_0_25;
		  stripe_ptr[2][j_out-spp] += v_b * interp_0_5;
		  stripe_ptr[2][j_out]     += v_b;
		  stripe_ptr[2][j_out+spp] += v_b * interp_0_5;
		  stripe_ptr[3][j_out-spp] += v_b * interp_0_25;
		  stripe_ptr[3][j_out]     += v_b * interp_0_5;
		  stripe_ptr[3][j_out+spp] += v_b * interp_0_25;
                                               j_out ++;
#if 0
		  stripe_ptr[2][j_out] = 0;    j_out ++; /* left RGB pixel */
		  stripe_ptr[2][j_out] = v_g;  j_out ++;
		  stripe_ptr[2][j_out] = 0;    j_out ++;
		  stripe_ptr[2][j_out] = 0;    j_out ++; /* right RGB pixel */
		  stripe_ptr[2][j_out] = 0;    j_out ++;
		  stripe_ptr[2][j_out] = v_b;  j_out ++;
#endif
		}
	    }
	    else {		   	      /* NOT bayer (Monochrome mode) */
	        /*** upper line ***/
	        j_in = sizeof(uint16) * 0;
	        for ( j=0, j_out=0 ; j < num_j ; j++ ) {
		  v_r = strip_buf_in[j_in];  j_in ++;
		  v_r <<= 8;
		  v_r |= strip_buf_in[j_in]; j_in ++;
		  v_g = strip_buf_in[j_in];  j_in ++;
		  v_g <<= 8;
		  v_g |= strip_buf_in[j_in]; j_in ++;
		  /* */
		  v_mono = v_r;  v_mono <<= shift_for_scale_r;
		  stripe_ptr[1][j_out] = v_mono;  j_out ++;
		  v_mono = v_r;  v_mono <<= shift_for_scale_g;
		  stripe_ptr[1][j_out] = v_mono;  j_out ++;
		  v_mono = v_r;  v_mono <<= shift_for_scale_b;
		  stripe_ptr[1][j_out] = v_mono;  j_out ++;
		  /* */
		  v_mono = v_g;  v_mono <<= shift_for_scale_r;
		  stripe_ptr[1][j_out] = v_mono;  j_out ++;
		  v_mono = v_g;  v_mono <<= shift_for_scale_g;
		  stripe_ptr[1][j_out] = v_mono;  j_out ++;
		  v_mono = v_g;  v_mono <<= shift_for_scale_b;
		  stripe_ptr[1][j_out] = v_mono;  j_out ++;
		}
		/*** lower line ***/
		j_in = sizeof(uint16) * width;
		for ( j=0, j_out=0 ; j < num_j ; j++ ) {
		  v_g = strip_buf_in[j_in];  j_in ++;
		  v_g <<= 8;
		  v_g |= strip_buf_in[j_in]; j_in ++;
		  v_b = strip_buf_in[j_in];  j_in ++;
		  v_b <<= 8;
		  v_b |= strip_buf_in[j_in]; j_in ++;
		  /* */
		  v_mono = v_g;  v_mono <<= shift_for_scale_r;
		  stripe_ptr[2][j_out] = v_mono;  j_out ++;
		  v_mono = v_g;  v_mono <<= shift_for_scale_g;
		  stripe_ptr[2][j_out] = v_mono;  j_out ++;
		  v_mono = v_g;  v_mono <<= shift_for_scale_b;
		  stripe_ptr[2][j_out] = v_mono;  j_out ++;
		  /* */
		  v_mono = v_b;  v_mono <<= shift_for_scale_r;
		  stripe_ptr[2][j_out] = v_mono;  j_out ++;
		  v_mono = v_b;  v_mono <<= shift_for_scale_g;
		  stripe_ptr[2][j_out] = v_mono;  j_out ++;
		  v_mono = v_b;  v_mono <<= shift_for_scale_b;
		  stripe_ptr[2][j_out] = v_mono;  j_out ++;
		}
	    }
	}
	
	/*
	  y_out=0:
	     1 <= 0   not output
	     0 <= 0   output
	     1 <= 2   output
	     0 <= 2   output
	  y_out=1:
	     2 <= 0   not output
	     1 <= 0   not output
	     2 <= 2   output
	     1 <= 2   output
	  y_out=2:
	     3 <= 0   not output
	     2 <= 0   not output
	     3 <= 2   not output
	     2 <= 2   output
	 */
	if ( y_out + 1 <= i*2 && line_cnt_out < height_out ) {
	    //fprintf(stderr,"[DEBUG] line_cnt_out = %d\n",(int)line_cnt_out);
	    for ( j=0 ; j < spp * width_out ; j++ ) {
	        float v = stripe_ptr[0][spp * x_out + j] + 0.5;
		if ( 65535.0 < v ) strip_buf_out[j] = 65535;
		else strip_buf_out[j] = (uint16)v;
	    }
	    if ( TIFFWriteEncodedStrip(tiff_out, line_cnt_out,
				       strip_buf_out,
				       byps * spp * width_out) == 0 ) {
	        fprintf(stderr,"[ERROR] TIFFWriteEncodedStrip() failed\n");
		goto quit;
	    }
	    line_cnt_out ++;
	}	
	if ( y_out <= i*2 && line_cnt_out < height_out ) {
	    //fprintf(stderr,"[DEBUG] line_cnt_out = %d\n",(int)line_cnt_out);
	    for ( j=0 ; j < spp * width_out ; j++ ) {
	        float v = stripe_ptr[1][spp * x_out + j] + 0.5;
		if ( 65535.0 < v ) strip_buf_out[j] = 65535;
		else strip_buf_out[j] = (uint16)v;
	    }
	    if ( TIFFWriteEncodedStrip(tiff_out, line_cnt_out,
				       strip_buf_out,
				       byps * spp * width_out) == 0 ) {
	        fprintf(stderr,"[ERROR] TIFFWriteEncodedStrip() failed\n");
		goto quit;
	    }
	    line_cnt_out ++;
	}	

	/* buffer rotation ... */
	stripe_ptr[0] = stripe0ptr[(idx_stripe+0) % 4];
	stripe_ptr[1] = stripe0ptr[(idx_stripe+1) % 4];
	stripe_ptr[2] = stripe0ptr[(idx_stripe+2) % 4];
	stripe_ptr[3] = stripe0ptr[(idx_stripe+3) % 4];
	memset(stripe_ptr[2] - spp*1, 0, sizeof(float) * stripe0unit_width * 2);
	idx_stripe += 2;

    }

    return_status = 0;
 quit:
    if ( strip_buf_out != NULL ) free(strip_buf_out);
    if ( stripe_buf != NULL ) free(stripe_buf);
    if ( strip_buf_in != NULL ) free(strip_buf_in);

    return return_status;
}

/*
 *  e.g., 6000x4000 FITS RGB cube => 6000x4000 RGB tiff output
 */
static int rgbcube_to_full( const int bit_used[],
		       FILE *pp,		       /* input stream     */
		       TIFF *tiff_out,	       /* output TIFF inst */
		       uint32 width, uint32 height, /* w/h of output    */
		       uint16 byps, uint16 spp,     /* 2 and 3          */
		       uint32 x_out, uint32 y_out,  /* actual crop area */
		       uint32 width_out, uint32 height_out,
		       int fits_bitpix, float fits_pix_info[], bool byte_swap )
{
    const float fits_bzero = fits_pix_info[0];
    const float fits_bscale = fits_pix_info[1];
    
    int return_status = -1;

    uint32 i;
    size_t sz_cube = 0;
    unsigned char *strip_buf_tmp = NULL;	/* to read 1 lines  */
    unsigned char *cube_buf_in = NULL;		/* to read all  */
    size_t off_cube_buf_in;
    unsigned char *strip_buf_out = NULL;	/* buffer for final output */
    uint16 *strip_buf_out_uint16_ptr;
    float *strip_buf_out_float_ptr;
    
    if ( byps == 2 && fits_bitpix == 16) {
	sz_cube = sizeof(uint16);
    }
    else if ( byps == 4 && fits_bitpix == -32 ) {
	sz_cube = sizeof(float);
    }
    else {
        fprintf(stderr,"[ERROR] Invalid 'byps': %d\n",(int)byps);
	goto quit;
    }

    if ( spp != 3 ) {
        fprintf(stderr,"[ERROR] 'spp' should be 3\n");
	goto quit;
    }

    strip_buf_tmp = (unsigned char *)malloc(sz_cube * width * 1);
    if ( strip_buf_tmp == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
	goto quit;
    }

    /* allocate buffer for RGB cube */
    cube_buf_in	= (unsigned char *)malloc(sz_cube * width * height_out * 3);
    if ( cube_buf_in == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
	goto quit;
    }

    strip_buf_out = (unsigned char *)malloc(byps * spp * width_out);
    if ( strip_buf_out == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
    	goto quit;
    }
    strip_buf_out_uint16_ptr = (uint16 *)strip_buf_out;
    strip_buf_out_float_ptr = (float *)strip_buf_out;

    off_cube_buf_in = 0;
    for ( i=0 ; i < 3 ; i++ ) {
        const size_t n_elem_to_read = width;
	uint32 j;

	for ( j=0 ; j < y_out ; j++ ) {				/* skip */
	    if ( fread(strip_buf_tmp, 1, sz_cube * n_elem_to_read, pp)
		 != sz_cube * n_elem_to_read ) {
	        fprintf(stderr,"[ERROR] fread() failed\n");
		goto quit;
	    }
	}

	for ( ; j < y_out + height_out ; j++ ) {     /* read y-cropping area */
	    if ( fread(cube_buf_in + off_cube_buf_in, 1,
		       sz_cube * n_elem_to_read, pp)
		 != sz_cube * n_elem_to_read ) {
	        fprintf(stderr,"[ERROR] fread() failed\n");
		goto quit;
	    }
	    if ( byps == 2 ) {
		if ( byte_swap == true ) {
		    s_byteswap2(cube_buf_in + off_cube_buf_in, n_elem_to_read);
		}
		if ( 0.0 < fits_bscale ) {
		    size_t ix;
		    const int16 *pr
			= (const int16 *)(cube_buf_in + off_cube_buf_in);
		    uint16 *pw = (uint16 *)(cube_buf_in + off_cube_buf_in);
		    for ( ix=0 ; ix < n_elem_to_read ; ix++ ) {
			float val = 0.5 + fits_bzero + fits_bscale * pr[ix];
			if ( val < 0.0 ) pw[ix] = (uint16)0;
			else if ( 65535.0 < val ) pw[ix] = (uint16)65535;
			else pw[ix] = (uint16)val;
		    }
		}
	    }
	    else if ( byps == 4 ) {
		if ( byte_swap == true ) {
		    s_byteswap4(cube_buf_in + off_cube_buf_in, n_elem_to_read);
		}
		if ( 0.0 < fits_bscale ) {
		    size_t ix;
		    const float *pr
			= (const float *)(cube_buf_in + off_cube_buf_in);
		    float *pw = (float *)(cube_buf_in + off_cube_buf_in);
		    for ( ix=0 ; ix < n_elem_to_read ; ix++ ) {
			pw[ix] = 0.5 + fits_bzero + fits_bscale * pr[ix];
		    }
		}
	    }
	    off_cube_buf_in += sz_cube * n_elem_to_read;
	}

	if ( i == 2 ) break;
	
	for ( ; j < height ; j++ ) {				/* skip */
	    if ( fread(strip_buf_tmp, 1, sz_cube * n_elem_to_read, pp)
		 != sz_cube * n_elem_to_read ) {
	        fprintf(stderr,"[ERROR] fread() failed\n");
		goto quit;
	    }
	}

    }
    
    /* */
    if ( byps == 2 && fits_bitpix == 16 ) {
	uint32 line_cnt_out;
	const uint16 *img_buf_uint16_ptr[3];
	uint32 shift_for_scale[3];
	for ( i=0 ; i < 3 ; i++ ) {
	    if ( 0 < bit_used[i] && bit_used[i] < 16 )
		shift_for_scale[i] = 16 - bit_used[i];
	    else
		shift_for_scale[i] = 0;
	}

	img_buf_uint16_ptr[0] = (const uint16 *)cube_buf_in;
	img_buf_uint16_ptr[1] = img_buf_uint16_ptr[0] + (width * height_out);
	img_buf_uint16_ptr[2] = img_buf_uint16_ptr[1] + (width * height_out);
	
	line_cnt_out = 0;	/* out */
	for ( i=0 ; i < height_out ; i++ ) {
	    size_t j;

	    for ( j=0 ; j < 3 ; j++ ) {
		size_t ix;
		uint32 k;
		ix = j;
		for ( k=0 ; k < width_out ; k++ ) {
		    strip_buf_out_uint16_ptr[ix] = img_buf_uint16_ptr[j][x_out + k];
		    strip_buf_out_uint16_ptr[ix] <<= shift_for_scale[j];
		    ix += 3;				/* out */
		}
		img_buf_uint16_ptr[j] += width;		/* in */
	    }

	    if ( TIFFWriteEncodedStrip(tiff_out, line_cnt_out,
				   strip_buf_out_uint16_ptr,
				   byps * spp * width_out) == 0 ) {
		fprintf(stderr,"[ERROR] TIFFWriteEncodedStrip() failed\n");
		goto quit;
	    }
	    line_cnt_out ++;
	}
    }
    else if ( byps == 4 && fits_bitpix == -32 ) {
	uint32 line_cnt_out;
	const float *img_buf_float_ptr[3];
	int flag_limit16 = 0;
	float v_min=0.0, v_max=0.0, v_off=0.0, v_scale=1.0;

	/* for now */
	for ( i=0 ; i < 3 ; i++ ) {
	    if ( 0 < bit_used[i] && bit_used[i] <= 16 ) flag_limit16 = 1;
	}

	img_buf_float_ptr[0] = (const float *)cube_buf_in;
	img_buf_float_ptr[1] = img_buf_float_ptr[0] + (width * height_out);
	img_buf_float_ptr[2] = img_buf_float_ptr[1] + (width * height_out);

	if ( flag_limit16 != 0 ) {
	    v_min = img_buf_float_ptr[0][x_out + 0];
	    v_max = img_buf_float_ptr[0][x_out + 0];
	    for ( i=0 ; i < height_out ; i++ ) {
		size_t j;
		for ( j=0 ; j < 3 ; j++ ) {
		    uint32 k;
		    for ( k=0 ; k < width_out ; k++ ) {
			float v = img_buf_float_ptr[j][x_out + k];
			if ( v < v_min ) v_min = v;
			if ( v_max < v ) v_max = v;
		    }
		    img_buf_float_ptr[j] += width;		/* in */
		}
	    }
	    v_off   = 0.0 - v_min;
	    v_scale = 1.0 / (v_max - v_min);
	    fprintf(stderr,"[INFO] v_off=%g v_scale=%g\n", v_off, v_scale);
	}

	img_buf_float_ptr[0] = (const float *)cube_buf_in;
	img_buf_float_ptr[1] = img_buf_float_ptr[0] + (width * height_out);
	img_buf_float_ptr[2] = img_buf_float_ptr[1] + (width * height_out);

	line_cnt_out = 0;	/* out */
	for ( i=0 ; i < height_out ; i++ ) {
	    size_t j;

	    for ( j=0 ; j < 3 ; j++ ) {
		size_t ix;
		uint32 k;
		ix = j;
		for ( k=0 ; k < width_out ; k++ ) {
		    float v = img_buf_float_ptr[j][x_out + k];
		    v += v_off;
		    v *= v_scale;
		    strip_buf_out_float_ptr[ix] = v;
		    ix += 3;				/* out */
		}
		img_buf_float_ptr[j] += width;		/* in */
	    }

	    if ( TIFFWriteEncodedStrip(tiff_out, line_cnt_out,
				   strip_buf_out_float_ptr,
				   byps * spp * width_out) == 0 ) {
		fprintf(stderr,"[ERROR] TIFFWriteEncodedStrip() failed\n");
		goto quit;
	    }
	    line_cnt_out ++;
	}
	
    }
    

    return_status = 0;
 quit:
    if ( strip_buf_out != NULL ) free(strip_buf_out);
    if ( cube_buf_in != NULL ) free(cube_buf_in);
    if ( strip_buf_tmp != NULL ) free(strip_buf_tmp);

    return return_status;
}

/*
 * main function to convert RAW/FITS to tiff 16-bit
 */
static int raw_to_tiff( const char *filename_in,
			int matrix_type_in,
			const char *deadpix_file,
			const int bit_used[],
			const long crop_prms[], /* for output image */
			bool half_size_image,   /* true: half-size */
			int bayer_direct        /* non-0: do not solve bayer */)
{
    int return_status = -1;

    const long test_long_1 = 1;
    bool fits_byte_swap = 
	( ((const char *)(&test_long_1))[0] != 0 ? true : false );
    
    const char *const bayer_str[3] = {"NONE","RG-GB","GR-BG"};

    char filename_out[PATH_MAX] = {'\0'};
    char *dcraw_cmd = NULL;
    unsigned char *tiff_in_img_buf = NULL;
    size_t dcraw_cmd_len;
    int bayer_matrix;
    int _width, _height, maxval;
    uint16 bps, byps, spp;
    uint32 width, height, icc_prof_size;
    uint32 x_out, y_out, width_out, height_out;	/* actual crop area */
    char line_buf[256];
    int input_file_type = -1;		/* 0:raw 1:fits 2:tiff */
    FILE *pp = NULL;
    TIFF *tiff_in = NULL;
    TIFF *tiff_out = NULL;
    float camera_calibration1[12];				/* for TIFF tag */
    float *camera_basic_info = camera_calibration1 + 0;		/* [5] */
    float *daylight_multipliers = camera_calibration1 + 5;	/* [3] */
    float *camera_multipliers = camera_calibration1   + 5 + 3;	/* [4] */

    long fits_size_info[5]   /* header-blk-length, bitpix, width, height, nz */
		= {0,0,0,0,0};
    float fits_pix_info[2]	/* bzero, bscale */
		= {0.0, 0.0};

    input_file_type = check_a_file(filename_in);
    if ( input_file_type < 0 ) {
        fprintf(stderr,"[ERROR] not found: %s\n",filename_in);
	goto quit;	/* ERROR */
    }

    /* get basic info from RAW/FITS file */
    bayer_matrix = get_matrix_type_and_info(
				   filename_in, matrix_type_in,
				   camera_basic_info,
				   daylight_multipliers, camera_multipliers,
				   fits_size_info, fits_pix_info);

    if ( input_file_type == 1 /* fits */ ) {
	if ( fits_size_info[0] <= 0 /* header-blk-length */ ) {
	    fprintf(stderr,"[ERROR] invalid FITS\n");
	    goto quit;	/* ERROR */
	}
    }
    
    if ( create_tiff_filename( filename_in, filename_out, PATH_MAX ) < 0 ) {
        fprintf(stderr,"[ERROR] create_tiff_filename() failed\n");
	goto quit;	/* ERROR */
    }

    if ( input_file_type == 1 /* fits */ ) {
	/* FITS */
	char fits_rec[FITS_FILE_RECORD_UNIT];
	long i;

	if ( fits_size_info[2] <= 0 || fits_size_info[3] <= 0 ) {
	    fprintf(stderr,"[ERROR] FITS WIDTH is invalid\n");
	    goto quit;	/* ERROR */
	}

	_width = fits_size_info[2];
	_height = fits_size_info[3];

	/* for TIFF output */
	if ( fits_size_info[1] == 16 ) {
	    bps = 16;
	    printf("[INFO] Used bit of R,G,B = [%d,%d,%d]."
		   "  Bayer type = [%s]\n",
		   bit_used[0],bit_used[1],bit_used[2],
		   bayer_str[bayer_matrix]);
	}
	else if ( fits_size_info[1] == -32 &&
		  2 < fits_size_info[4] ) {		/* NAXIS3 == 3 */
	    bps = 32;
	}
	else {
	    fprintf(stderr,"[ERROR] Invalid FITS BITPIX: %ld\n",
		    fits_size_info[1]);
	    goto quit;	/* ERROR */
	}

	printf("[INFO] FITS BITPIX = [%d].\n",(int)(fits_size_info[1]));

	pp = fopen(filename_in, "r");
	if ( pp == NULL ) {
	    fprintf(stderr,"[ERROR] cannot open: %s\n", filename_in);
	    goto quit;	/* ERROR */
	}

	/* Skip header part */
	for ( i=0 ; i < fits_size_info[0] ; i++ ) {
	    if ( fread(fits_rec, 1, FITS_FILE_RECORD_UNIT, pp)
		 != FITS_FILE_RECORD_UNIT ) {
		fprintf(stderr,"[ERROR] fread() failed\n");
		goto quit;
	    }
	}
    }

    else if ( input_file_type == 2 /* tiff */ ) {
	/* TIFF */
	uint16 pconfig, photom, format;
	uint32 tif_width, tif_height;
	tsize_t strip_size;
	size_t strip_max, i;

	fits_byte_swap = false;

	tiff_in = TIFFOpen(filename_in, "r");
	if ( tiff_in == NULL ) {
	    fprintf(stderr,"[ERROR] cannot open: %s\n", filename_in);
	    goto quit;
	}

	if ( TIFFGetField(tiff_in, TIFFTAG_SAMPLEFORMAT, &format) == 0 ) {
	    format = SAMPLEFORMAT_UINT;
	}
	if ( format != SAMPLEFORMAT_UINT ) {
	    fprintf(stderr,"[ERROR] unsupported SAMPLEFORMAT: %d\n",(int)format);
	    goto quit;
	}

	if ( TIFFGetField(tiff_in, TIFFTAG_BITSPERSAMPLE, &bps) == 0 ) {
	    fprintf(stderr,"[ERROR] TIFFGetField() failed [bps]\n");
	    goto quit;
	}
	if ( bps != 16 ) {
	    fprintf(stderr,"[ERROR] unsupported BITSPERSAMPLE: %d\n",(int)bps);
	    goto quit;
	}

	if ( TIFFGetField(tiff_in, TIFFTAG_SAMPLESPERPIXEL, &spp) == 0 ) {
	    fprintf(stderr,"[ERROR] TIFFGetField() failed [spp]\n");
	    goto quit;
	}
	if ( spp != 1 ) {
	    fprintf(stderr,"[ERROR] unsupported SAMPLESPERPIXEL: %d\n",(int)spp);
	    goto quit;
	}

	strip_size = TIFFStripSize(tiff_in);		/* in bytes */
	strip_max = TIFFNumberOfStrips(tiff_in);

	if ( TIFFGetField(tiff_in, TIFFTAG_IMAGEWIDTH, &tif_width) == 0 ) {
	    fprintf(stderr,"[ERROR] TIFFGetField() failed [width]\n");
	    goto quit;
	}
	if ( TIFFGetField(tiff_in, TIFFTAG_IMAGELENGTH, &tif_height) == 0 ) {
	    fprintf(stderr,"[ERROR] TIFFGetField() failed [height]\n");
	    goto quit;
	}
	if ( TIFFGetField(tiff_in, TIFFTAG_PLANARCONFIG, &pconfig) == 0 ) {
	    fprintf(stderr,"[ERROR] TIFFGetField() failed [pconfig]\n");
	    goto quit;
	}
	if ( pconfig != PLANARCONFIG_CONTIG ) {
	    fprintf(stderr,"[ERROR] Unsupported PLANARCONFIG value\n");
	    goto quit;
	}
	if ( TIFFGetField(tiff_in, TIFFTAG_PHOTOMETRIC, &photom) == 0 ) {
	    fprintf(stderr,"[ERROR] TIFFGetField() failed [photom]\n");
	    goto quit;
	}
	if ( photom != PHOTOMETRIC_MINISBLACK ) {
	    fprintf(stderr,"[ERROR] Unsupported PHOTOMETRIC value: %d\n",(int)photom);
	    goto quit;
	}

	tiff_in_img_buf = (unsigned char *)malloc(strip_size * strip_max);
	if ( tiff_in_img_buf == NULL ) {
	    fprintf(stderr,"[ERROR] malloc() failed\n");
	}

	for ( i=0 ; i < strip_max ; i++ ) {
	    unsigned char *tiff_strip_ptr = tiff_in_img_buf + strip_size * i;
	    ssize_t s_len = TIFFReadEncodedStrip(tiff_in, i,
						 (void *)tiff_strip_ptr,
						 strip_size);
	    if ( s_len < 0 ) {
		fprintf(stderr,"[ERROR] TIFFReadEncodedStrip() failed\n");
		goto quit;
	    }
	    s_byteswap2(tiff_strip_ptr, strip_size / sizeof(uint16) );
	}
	/*
	fprintf(stderr,"[DEBUG] [%d][%d][%d][%d] [%d][%d][%d][%d]\n",
		(int)tiff_in_img_buf[0],(int)tiff_in_img_buf[1],
		(int)tiff_in_img_buf[2],(int)tiff_in_img_buf[3],
		(int)tiff_in_img_buf[4],(int)tiff_in_img_buf[5],
		(int)tiff_in_img_buf[6],(int)tiff_in_img_buf[7]);
	*/

	_width = tif_width;
	_height = tif_height;

	if ( _width % 2 != 0 ) {
	    fprintf(stderr,"[ERROR] Invalid _width value\n");
	    goto quit;
	}

	if ( _height % 2 != 0 ) {
	    fprintf(stderr,"[ERROR] Invalid _height value\n");
	    goto quit;
	}

    }

    else {
	/* RAW */

	fits_byte_swap = false;
	
	/* for TIFF output */
	bps = 16;

	printf("[INFO] Used bit of R,G,B = [%d,%d,%d]."
	       "  Bayer type = [%s]\n",
	       bit_used[0],bit_used[1],bit_used[2],
	       bayer_str[bayer_matrix]);

	dcraw_cmd_len = 256;
	if ( deadpix_file != NULL ) dcraw_cmd_len += strlen(deadpix_file);
	dcraw_cmd_len += strlen(filename_in);
	dcraw_cmd = (char *)malloc(dcraw_cmd_len);
	if ( dcraw_cmd == NULL ) {
	    fprintf(stderr,"[ERROR] malloc() failed\n");
	    goto quit;
	}
    
	if ( deadpix_file != NULL && 0 < strlen(deadpix_file) ) {
	    if ( check_a_file(deadpix_file) < 0 ) {
		fprintf(stderr,"[ERROR] not found %s\n", deadpix_file);
		goto quit;
	    }
	    snprintf(dcraw_cmd, dcraw_cmd_len,
		     "dcraw -c -D -4 -j -t 0 -P %s %s",
		     /* "dcraw -c -d -4 -j -t 0 -b 0.5 -P %s %s", */
		     deadpix_file, filename_in);
	}
	else {
	    snprintf(dcraw_cmd, dcraw_cmd_len,
		     "dcraw -c -D -4 -j -t 0 %s",
		     /* "dcraw -c -d -4 -j -t 0 -b 0.5 %s", */
		     filename_in);
	}

	//printf("crop: %ld %ld %ld %ld\n",
	//	   crop_prms[0],crop_prms[1],crop_prms[2],crop_prms[3]);
	//printf("%s\n", dcraw_cmd);
	printf("Converting: %s => %s\n", filename_in, filename_out);

	fprintf(stderr,"[INFO] camera_calibration1 = (%g %g %g %g %g %g %g %g %g %g %g %g)\n",
		camera_calibration1[0],camera_calibration1[1],camera_calibration1[2],camera_calibration1[3],
		camera_calibration1[4],camera_calibration1[5],camera_calibration1[6],camera_calibration1[7],
		camera_calibration1[8],camera_calibration1[9],camera_calibration1[10],camera_calibration1[11]);

	/* Start reading PGM */
    
	pp = popen(dcraw_cmd, "r");
	if ( pp == NULL ) {
	    fprintf(stderr,"[ERROR] popen() failed\n");
	    goto quit;			/* ERROR */
	}
	
	fgets(line_buf, 256, pp);
	if ( strcmp(line_buf,"P5\n") != 0 ) {
	    fprintf(stderr,"[ERROR] Line #1 of input data is not 'P5'\n");
	    goto quit;
	}

	fgets(line_buf, 256, pp);
	if ( sscanf(line_buf,"%d %d",&_width,&_height) != 2 ) {
	    fprintf(stderr,"[ERROR] Line #2 of input data is not valid\n");
	    goto quit;
	}
	if ( _width % 2 != 0 ) {
	    fprintf(stderr,"[ERROR] Invalid _width value\n");
	    goto quit;
	}

	if ( _height % 2 != 0 ) {
	    fprintf(stderr,"[ERROR] Invalid _height value\n");
	    goto quit;
	}
    
	fgets(line_buf, 256, pp);
	maxval = atoi(line_buf);
	if ( maxval != 65535 ) {
	    fprintf(stderr,"[ERROR] Line #3 of input data is not 65535\n");
	    goto quit;
	}

    }

    /* for TIFF output */
    byps = (bps + 7) / 8;
    spp = 3;

    if ( input_file_type == 1 /* fits */ &&
	 2 < fits_size_info[4] ) {		/* NAXIS3 == 3 */
	width = _width;
	height = _height;
    }
    else if ( half_size_image == false ) {	/* full size image */
	width = _width;
	height = _height;
    }
    else {					/* half size image */
	width = _width / 2;
	height = _height / 2;
    }

    /* Adjust cropping parameters */
    
    adjust_crop_prms( width, height, crop_prms,
		      &x_out, &y_out, &width_out, &height_out );

    printf("[INFO] actual crop (x,y,w,h) = %d %d %d %d\n",
    	   (int)x_out,(int)y_out,(int)width_out,(int)height_out);
    
    /* Preparing of TIFF output */
    
    tiff_out = TIFFOpen(filename_out, "w");
    if ( tiff_out == NULL ) {
        fprintf(stderr,"[ERROR] TIFFOpen() failed\n");
	goto quit;
    }

    TIFFSetField(tiff_out, TIFFTAG_IMAGEWIDTH, width_out);
    TIFFSetField(tiff_out, TIFFTAG_IMAGELENGTH, height_out);

    TIFFSetField(tiff_out, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    //TIFFSetField(tiff_out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(tiff_out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiff_out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tiff_out, TIFFTAG_BITSPERSAMPLE, bps);
    TIFFSetField(tiff_out, TIFFTAG_SAMPLESPERPIXEL, spp);
    TIFFSetField(tiff_out, TIFFTAG_ROWSPERSTRIP, (uint32)1);

    if ( input_file_type == 0 /* raw */ ) {
	TIFFSetField(tiff_out, TIFFTAG_CAMERACALIBRATION1,
		     12, camera_calibration1);
    }
    
    icc_prof_size = sizeof(Icc_srgb_profile);
    TIFFSetField(tiff_out, TIFFTAG_ICCPROFILE,
		 icc_prof_size, Icc_srgb_profile);

    if ( input_file_type == 1 /* fits */ &&
	 2 < fits_size_info[4] ) {		/* NAXIS3 == 3 */
	/* RGB cube */
	if ( fits_size_info[1] == -32 ) {	/* BITPIX */
	    TIFFSetField(tiff_out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
	}
	else {
	    TIFFSetField(tiff_out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
	}
        if ( rgbcube_to_full( bit_used,
		     pp, tiff_out,
		     width, height,
		     byps, spp,
		     x_out, y_out, width_out, height_out,
		     fits_size_info[1], fits_pix_info, fits_byte_swap ) < 0 ) {
	    fprintf(stderr,"[ERROR] rgbcube_to_full() failed\n");
	    goto quit;
	}
    }
    else if ( half_size_image == false ) {	/* full size image */
	TIFFSetField(tiff_out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        if ( bayer_to_full( bayer_matrix, bayer_direct,
			    bit_used,
			    pp, tiff_in_img_buf, tiff_out,
			    width, height,
			    byps, spp,
			    x_out, y_out, width_out, height_out,
			    fits_pix_info, fits_byte_swap ) < 0 ) {
	  fprintf(stderr,"[ERROR] bayer_to_full() failed\n");
	  goto quit;
	}
    }
    else {					/* half size image */
	TIFFSetField(tiff_out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        if ( bayer_to_half( bayer_matrix, bayer_direct,
			    bit_used,
			    pp, tiff_in_img_buf, tiff_out,
			    width, height,
			    byps, spp,
			    x_out, y_out, width_out, height_out,
			    fits_pix_info, fits_byte_swap ) < 0 ) {
	  fprintf(stderr,"[ERROR] bayer_to_half() failed\n");
	  goto quit;
	}
    }

    return_status = 0;
 quit:
    if ( tiff_in_img_buf != NULL ) free(tiff_in_img_buf);
    if ( tiff_in != NULL ) TIFFClose(tiff_in);
    if ( tiff_out != NULL ) TIFFClose(tiff_out);
    if ( pp != NULL ) pclose(pp);
    if ( dcraw_cmd != NULL ) free(dcraw_cmd);
    
    return return_status;
}

int main( int argc, char *argv[] )
{
    int return_status = -1;

    size_t arg_cnt, i;
    int bit_used[3] = {14,14,14};
    bool half_size_image = false;
    int bayer_direct = 0;
    long crop_prms[4] = {-1,-1,-1,-1};
    char deadpix_file[PATH_MAX] = {'\0'};
    int matrix_type = Matrix_NONE;
    
    if ( argc < 3 ) {
	fprintf(stderr,"Read and demosaic 16-bit RAW/FITS/TIFF with bayer filter and convert RGB TIFF.\n");
	fprintf(stderr,"Convert of FITS with 16-bit int and 32-bit float RGB cube is also supported.\n");
	fprintf(stderr,"\n");
        fprintf(stderr,"[USAGE]\n");
        fprintf(stderr,"$ %s [-h] [-d] [-m] [-c param] [-P file] bit_used rawfile1 rawfile2 ...\n",argv[0]);
        fprintf(stderr,"$ %s [-h] [-d] [-m] [-c param] [-P file] bit_used_R,bit_used_G,bit_used_B rawfile1 ...\n",argv[0]);
	fprintf(stderr,"\n");
	fprintf(stderr,"-h ... Output half size images (only for bayer input)\n");
	fprintf(stderr,"-d ... Output Color images without bayer demosaic (ignored with -h option)\n");
	fprintf(stderr,"-m ... Output Monochrome images without bayer demosaic\n");
	fprintf(stderr,"-c [x,y,]width,height ... Crop images. Center when x and y are omitted.\n");
	fprintf(stderr,"                          Cropping will be performd after resizing by -h arg.\n");
	fprintf(stderr,"-rggb ... Set 'RGGB' matrix type\n");
	fprintf(stderr,"-grbg ... Set 'GRBG' matrix type\n");
	fprintf(stderr,"-P file ... Fix the dead pixels listed in this file (RAW only)\n");
	fprintf(stderr,"bit_used ... 9 to 16.  Set 14 for lossless 14-bit data\n");
	fprintf(stderr,"             Set 16 to rescale values [0.0...1.0] for float FITS RGB cube.\n");
	fprintf(stderr,"             Set 32 for float FITS RGB cube without rescaling.\n");
        fprintf(stderr,"\n");
        fprintf(stderr,"[EXAMPLE]\n");
        fprintf(stderr,"$ %s 13,14,13 DSC_0001.NEF DSC_0002.NEF DSC_0003.NEF\n", argv[0]);
        fprintf(stderr,"$ %s 16 ASIImg_frame0001.fit ASIImg_frame0002.fit\n", argv[0]);
        fprintf(stderr,"$ %s 16 M74_32bit_float_RGB-cube.fits\n", argv[0]);
        fprintf(stderr,"$ %s 16 Saturn_00001.tif\n", argv[0]);
        fprintf(stderr,"\n");
        fprintf(stderr,"Note that output='foo.tiff' for input='foo.tif' and\n");
	fprintf(stderr,"          output='foo.tif' for input='foo.tiff'.\n");
	goto quit;
    }

    arg_cnt = 1;
    
    while ( arg_cnt < argc ) {
        if ( strcmp(argv[arg_cnt],"-h") == 0 ) {
	    half_size_image = true;
	    arg_cnt ++;
	}
        else if ( strcmp(argv[arg_cnt],"-d") == 0 ) {
	    bayer_direct = 1;
	    arg_cnt ++;
	}
        else if ( strcmp(argv[arg_cnt],"-m") == 0 ) {
	    bayer_direct = 2;
	    arg_cnt ++;
	}
	else if ( strcmp(argv[arg_cnt],"-rggb") == 0 ) {
	    matrix_type = Matrix_RG_GB;
	    arg_cnt ++;
	}
	else if ( strcmp(argv[arg_cnt],"-grbg") == 0 ) {
	    matrix_type = Matrix_GR_BG;
	    arg_cnt ++;
	}
	else if ( strcmp(argv[arg_cnt],"-c") == 0 ) {
	    arg_cnt ++;
	    if ( argv[arg_cnt] != NULL ) {
	        if ( get_crop_prms(argv[arg_cnt], crop_prms) < 0 ) {
		    fprintf(stderr,"[ERROR] get_crop_prms() failed\n");
		    goto quit;
		}
		arg_cnt ++;
	    }
	}
        else if ( strcmp(argv[arg_cnt],"-P") == 0 ) {
	    arg_cnt ++;
	    if ( argv[arg_cnt] != NULL ) {
	        snprintf(deadpix_file, PATH_MAX, "%s", argv[arg_cnt]);
		arg_cnt ++;
	    }
	}
	else break;
    }

    if ( arg_cnt < argc ) {
	char tmpbuf[64];
        size_t spn, off = 0;
	for ( i=0 ; i < 3 /* RGB */ ; i++ ) {
	    spn = strspn(argv[arg_cnt] + off,"0123456789");
	    if ( spn <= 0 ) {
		fprintf(stderr,"[ERROR] Invalid arg: '%s'\n",argv[arg_cnt]);
		goto quit;
	    }
	    safe_strncpy(tmpbuf,64, argv[arg_cnt] + off,spn);
	    bit_used[i] = atoi(tmpbuf);
	    if ( bit_used[i] < 9 ) bit_used[i] = 16;
	    else if ( 16 < bit_used[i] ) bit_used[i] = 0;
	    if ( argv[arg_cnt][off + spn] == ',' ) {
		off += spn + 1;
	    }
	}
	arg_cnt ++;
    }

    while ( arg_cnt < argc ) {
        if ( raw_to_tiff( argv[arg_cnt], matrix_type,
			  deadpix_file, bit_used, crop_prms,
			  half_size_image, bayer_direct ) < 0 ) {
	    fprintf(stderr,"[ERROR] raw_to_tiff() failed\n");
	    goto quit;
	}
        arg_cnt ++;
    }

    return_status = 0;
 quit:
    return return_status;
}
