/*
 * raw2tiff.c
 *
 * Read raw image file using dcraw and convert bayer to raw RGB TIFF
 * without interpolation.
 *
 * GPL V2
 * (C) 2015, 2016  Chisato Yamauchi
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <tiffio.h>

#include "../icc_srgb_profile.c"
#include "adjust_crop_prms.c"

const int Matrix_RG_GB = 0;
const int Matrix_GR_BG = 1;

#if 0
static int check_uint_string( const char *str_in )
{
    if ( strspn(str_in,"0123456789") != strlen(str_in) ) return -1;
    else return 0;
}
#endif

/* Test specified file exists or not */
static int check_a_file( const char *filename )
{
    FILE *fp;
    if ( filename == NULL ) return -1;
    if ( strlen(filename) == 0 ) return -1;
    fp = fopen(filename,"r");
    if ( fp == NULL ) return -1;
    fclose(fp);
    return 0;
}

/* Get matrix type using dcraw */
static int get_matrix_type( const char *filename_in )
{
    int return_status = -1;
    const char *str_pat_rg_gb = "Filter pattern: RG/GB\n";
    const char *str_pat_gr_bg = "Filter pattern: GR/BG\n";
    char cmd[32 + PATH_MAX];
    char line[256];
    FILE *pp = NULL;
    int matrix_type = -1;
    
    if ( check_a_file(filename_in) < 0 ) goto quit;	/* ERROR */

    snprintf(cmd, 32 + PATH_MAX, "dcraw -i -v %s", filename_in);
    pp = popen(cmd, "r");
    if ( pp == NULL ) goto quit;			/* ERROR */

    while ( fgets(line,256,pp) != NULL ) {
	if ( strcmp(line,str_pat_rg_gb) == 0 ) matrix_type = Matrix_RG_GB;
	if ( strcmp(line,str_pat_gr_bg) == 0 ) matrix_type = Matrix_GR_BG;
    }
    
    return_status = matrix_type;
 quit:
    if ( pp != NULL ) pclose(pp);
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

    if ( filename_in == NULL ) goto quit;
    
    for ( i = 0 ; filename_in[i] != '\0' ; i++ ) {
        if ( PATH_MAX <= i ) break;
	basename[i] = filename_in[i];
        if ( filename_in[i] == '.' ) dot_idx = i;
    }
    if ( basename[dot_idx] == '.' ) basename[dot_idx] = '\0';
    snprintf(filename_out_buf, buf_len, "%s.tiff", basename);

    return_status = 0;
 quit:
    return return_status;
}

/*
 *  e.g., 6000x4000 bayer RGB image sensor => 3000x2000 RGB output
 */
static int bayer_to_half( int bayer_matrix,	       /* bayer type       */
			  const uint32 shift_for_scale[],
			  FILE *pp,		       /* input stream     */
			  TIFF *tiff_out,	       /* output TIFF inst */
			  uint32 width, uint32 height, /* w/h of output    */
			  uint16 byps, uint16 spp,     /* 2 and 3          */
			  uint32 x_out, uint32 y_out,  /* actual crop area */
			  uint32 width_out, uint32 height_out )
{
    const uint32 shift_for_scale_r = shift_for_scale[0];
    const uint32 shift_for_scale_g = shift_for_scale[1];
    const uint32 shift_for_scale_b = shift_for_scale[2];

    int return_status = -1;

    uint32 i;
    unsigned char *strip_buf_in = NULL;
    uint16 *strip_buf_out = NULL;

   
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


    for ( i=0 ; i < height ; i ++ ) {
        const size_t n_to_read = sizeof(uint16) * 2 * width * 2;
        uint32 j, j_in, j_out;
	uint32 v_r, v_g, v_b;
	if ( fread(strip_buf_in, 1, n_to_read, pp) != n_to_read ) {
	    fprintf(stderr,"[ERROR] fread() failed\n");
	    goto quit;
	}
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
	    else {	/* default:  RG/GB */
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
		    /**/
		    j_out ++;
		    v_g += strip_buf_out[j_out];
		    v_g >>= 1;
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
			  const uint32 shift_for_scale[],
			  FILE *pp,		       /* input stream     */
			  TIFF *tiff_out,	       /* output TIFF inst */
			  uint32 width, uint32 height, /* w/h of output    */
			  uint16 byps, uint16 spp,     /* 2 and 3          */
			  uint32 x_out, uint32 y_out,  /* actual crop area */
			  uint32 width_out, uint32 height_out )
{
    const uint32 shift_for_scale_r = shift_for_scale[0];
    const uint32 shift_for_scale_g = shift_for_scale[1];
    const uint32 shift_for_scale_b = shift_for_scale[2];

    int return_status = -1;

    uint32 i, num_i, line_cnt_out;
    unsigned char *strip_buf_in = NULL;		/* to read 2 sensor lines  */
    uint16 *stripe_buf = NULL;			/* work buffer for 4 lines */
    //    uint16 *strip_buf_out = NULL;		/* buffer for output       */

    uint32 stripe0unit_width = spp * (1+width+1);
    uint32 idx_stripe = 0;
    
    uint16 *stripe0ptr[4] = {NULL,NULL,NULL,NULL}; /* fixed ptr of stripe_buf */
    uint16 *stripe_ptr[4] = {NULL,NULL,NULL,NULL}; /* rotational ptr of stripe_buf */

    double interp_0_25 = 0.25;
    double interp_0_5 = 0.5;

    if ( bayer_direct != 0 ) {
        interp_0_25 = 0.0;
	interp_0_5 = 0.0;
    }
    
    strip_buf_in = (unsigned char *)malloc(sizeof(uint16) * width * 2);
    if ( strip_buf_in == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
	goto quit;
    }

    stripe_buf = (uint16 *)malloc(byps * stripe0unit_width * 4);
    if ( stripe_buf == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
	goto quit;
    }
    memset(stripe_buf, 0, byps * stripe0unit_width * 4);
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
    memset(stripe_ptr[2] - spp*1, 0, byps * stripe0unit_width * 2);
    idx_stripe += 2;
    
    //strip_buf_out = (uint16 *)malloc(byps * spp * width_out);
    //if ( strip_buf_out == NULL ) {
    //    fprintf(stderr,"[ERROR] malloc() failed\n");
    //	goto quit;
    //}

    /* */
    line_cnt_out = 0;
    num_i = height / 2;
    for ( i=0 ; i < num_i + 1 ; i ++ ) {
        const size_t n_to_read = sizeof(uint16) * width * 2; /* 2 lines */
        uint32 j, num_j, j_in, j_out;
	uint32 v_r, v_g, v_b;
	num_j = width / 2;
	if ( i < num_i ) {
	    if ( fread(strip_buf_in, 1, n_to_read, pp) != n_to_read ) {
	        fprintf(stderr,"[ERROR] fread() failed\n");
		goto quit;
	    }
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
		  if ( 65535 < v_r ) v_r = 65535;
		  v_g <<= shift_for_scale_g;
		  if ( 65535 < v_g ) v_g = 65535;
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
		  if ( 65535 < v_b ) v_b = 65535;
		  v_g <<= shift_for_scale_g;
		  if ( 65535 < v_g ) v_g = 65535;
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
	    else {	/* default:  RG/GB */
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
		  if ( 65535 < v_r ) v_r = 65535;
		  v_g <<= shift_for_scale_g;
		  if ( 65535 < v_g ) v_g = 65535;
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
		  if ( 65535 < v_b ) v_b = 65535;
		  v_g <<= shift_for_scale_g;
		  if ( 65535 < v_g ) v_g = 65535;
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
	    if ( TIFFWriteEncodedStrip(tiff_out, line_cnt_out,
				       stripe_ptr[0] + spp * x_out,
				       byps * spp * width_out) == 0 ) {
	        fprintf(stderr,"[ERROR] TIFFWriteEncodedStrip() failed\n");
		goto quit;
	    }
	    line_cnt_out ++;
	}	
	if ( y_out <= i*2 && line_cnt_out < height_out ) {
	    //fprintf(stderr,"[DEBUG] line_cnt_out = %d\n",(int)line_cnt_out);
	    if ( TIFFWriteEncodedStrip(tiff_out, line_cnt_out,
				       stripe_ptr[1] + spp * x_out,
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
	memset(stripe_ptr[2] - spp*1, 0, byps * stripe0unit_width * 2);
	idx_stripe += 2;

    }

    return_status = 0;
 quit:
    //if ( strip_buf_out != NULL ) free(strip_buf_out);
    if ( stripe_buf != NULL ) free(stripe_buf);
    if ( strip_buf_in != NULL ) free(strip_buf_in);

    return return_status;
}

/*
 * main function to convert raw to tiff 16-bit
 */
static int raw_to_tiff( const char *filename_in,
			const char *deadpix_file,
			const int bit_used[],
			const long crop_prms[], /* for output image        */
			int full_size_image,    /* 0:half-size 1:full-size */
			int bayer_direct        /* 1:do not solve bayer    */)
{
    int return_status = -1;
    
    char filename_out[PATH_MAX] = {'\0'};
    char *dcraw_cmd = NULL;
    size_t dcraw_cmd_len;
    int bayer_matrix;
    int _width, _height, maxval;
    uint16 bps, byps, spp;
    uint32 width, height, icc_prof_size;
    uint32 x_out, y_out, width_out, height_out;	/* actual crop area */
    char line_buf[256];
    FILE *pp = NULL;
    TIFF *tiff_out = NULL;
    const uint32 shift_for_scale[3] = { 16 - bit_used[0],
					16 - bit_used[1],
					16 - bit_used[2] };

    if ( check_a_file(filename_in) < 0 ) {
        fprintf(stderr,"[ERROR] not found: %s\n",filename_in);
	goto quit;	/* ERROR */
    }

    bayer_matrix = get_matrix_type(filename_in);
    
    if ( create_tiff_filename( filename_in, filename_out, PATH_MAX ) < 0 ) {
        fprintf(stderr,"[ERROR] create_tiff_filename() failed\n");
	goto quit;	/* ERROR */
    }

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
		 /* "dcraw -c -D -4 -j -t 0 -P %s %s", */
		 "dcraw -c -d -4 -j -t 0 -b 0.5 -P %s %s",
		 deadpix_file, filename_in);
    }
    else {
	snprintf(dcraw_cmd, dcraw_cmd_len,
		 /* "dcraw -c -D -4 -j -t 0 %s", */
		 "dcraw -c -d -4 -j -t 0 -b 0.5 %s",
		 filename_in);
    }

    //printf("crop: %ld %ld %ld %ld\n",
    //	   crop_prms[0],crop_prms[1],crop_prms[2],crop_prms[3]);
    //printf("%s\n", dcraw_cmd);
    printf("Converting: %s => %s\n", filename_in, filename_out);

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

    if ( full_size_image != 0 ) {	/* full size image */
        width = _width;
	height = _height;
    }
    else {				/* half size image */
        width = _width / 2;
	height = _height / 2;
    }

    /* Adjust cropping parameters */
    
    adjust_crop_prms( width, height, crop_prms,
		      &x_out, &y_out, &width_out, &height_out );

    printf("[INFO] actual crop (x,y,w,h) = %d %d %d %d\n",
    	   (int)x_out,(int)y_out,(int)width_out,(int)height_out);
    
    /* Preparing of TIFF output */
    
    bps = 16;
    byps = (bps + 7) / 8;
    spp = 3;

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
    
    icc_prof_size = sizeof(Icc_srgb_profile);
    TIFFSetField(tiff_out, TIFFTAG_ICCPROFILE,
		 icc_prof_size, Icc_srgb_profile);

    if ( full_size_image != 0 ) {	/* full size image */
        if ( bayer_to_full( bayer_matrix, bayer_direct,
			    shift_for_scale,
			    pp, tiff_out,
			    width, height,
			    byps, spp,
			    x_out, y_out, width_out, height_out ) < 0 ) {
	  fprintf(stderr,"[ERROR] bayer_to_full() failed\n");
	  goto quit;
	}
    }
    else {				/* half size image */
        if ( bayer_to_half( bayer_matrix, shift_for_scale,
			    pp, tiff_out,
			    width, height,
			    byps, spp,
			    x_out, y_out, width_out, height_out ) < 0 ) {
	  fprintf(stderr,"[ERROR] bayer_to_half() failed\n");
	  goto quit;
	}
    }

    return_status = 0;
 quit:
    if ( tiff_out != NULL ) {
	TIFFClose(tiff_out);
    }
    if ( pp != NULL ) pclose(pp);
    if ( dcraw_cmd != NULL ) free(dcraw_cmd);
    
    return return_status;
}

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

int main( int argc, char *argv[] )
{
    int return_status = -1;

    size_t arg_cnt, i;
    int bit_used[3] = {14,14,14};
    int full_size_image = 0;
    int bayer_direct = 0;
    long crop_prms[4] = {-1,-1,-1,-1};
    char deadpix_file[PATH_MAX] = {'\0'};
    
    if ( argc < 3 ) {
	fprintf(stderr,"Read raw image using dcraw and convert bayer to RGB TIFF without interpolation.\n");
	fprintf(stderr,"\n");
        fprintf(stderr,"[USAGE]\n");
        fprintf(stderr,"$ %s [-f] [-d] [-P file] [-c param] bit_used rawfile1 rawfile2 ...\n",argv[0]);
        fprintf(stderr,"$ %s [-f] [-d] [-P file] [-c param] bit_used_R,bit_used_G,bit_used_B rawfile1 ...\n",argv[0]);
	fprintf(stderr,"\n");
	fprintf(stderr,"-f ... Output full size images\n");
	fprintf(stderr,"-d ... Do not interpolate bayer (with -f option)\n");
	fprintf(stderr,"-P file ... Fix the dead pixels listed in this file\n");
	fprintf(stderr,"-c [x,y,]width,height ... Crop images. Center when x and y are omitted\n");
	fprintf(stderr,"bit_used ... 9 to 16.  Set 14 for lossless 14-bit data\n");
	goto quit;
    }

    arg_cnt = 1;
    
    while ( arg_cnt < argc ) {
        if ( strcmp(argv[arg_cnt],"-f") == 0 ) {
	    full_size_image = 1;
	    arg_cnt ++;
	}
        else if ( strcmp(argv[arg_cnt],"-d") == 0 ) {
	    bayer_direct = 1;
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
	    if ( bit_used[i] < 9 || 16 < bit_used[i] ) bit_used[i] = 16;
	    if ( argv[arg_cnt][off + spn] == ',' ) {
		off += spn + 1;
	    }
	}
	arg_cnt ++;
    }

    printf("Used bit of R,G,B = %d,%d,%d\n",
	   bit_used[0],bit_used[1],bit_used[2]);
    
    while ( arg_cnt < argc ) {
        if ( raw_to_tiff( argv[arg_cnt],
			  deadpix_file, bit_used, crop_prms,
			  full_size_image, bayer_direct ) < 0 ) {
	    fprintf(stderr,"[ERROR] raw_to_tiff() failed\n");
	    goto quit;
	}
        arg_cnt ++;
    }

    return_status = 0;
 quit:
    return return_status;
}
