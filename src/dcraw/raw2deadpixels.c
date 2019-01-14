/*
 * raw2deadpixels.c
 *
 * Output candidate of dead pixels info that can be used by dcraw.
 * Flat images can be used.
 *
 * GPL V2
 * (C) 2015, 2016  Chisato Yamauchi
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

const int Matrix_RG_GB = 0;
const int Matrix_GR_BG = 1;

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

/* Create a filename for dead pixels */
static int create_deadpixels_filename( char *buf, size_t buf_len )
{
    size_t i;
    for ( i=0 ; ; i++ ) {
        snprintf(buf,buf_len,"deadpixels_%zu.txt",i);
	if ( check_a_file(buf) < 0 ) break;
    }
    return 0;
}

#if 0
static int check_uint_string( const char *str_in )
{
    if ( strspn(str_in,"0123456789") != strlen(str_in) ) return -1;
    else return 0;
}
#endif

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

/* 
   Read raw bayer pixel data to uint16_t buffer.
   NOTE: *dest_buf_p must be NULL at 1st use 
 */
static int read_bayer_to_uint16( const char *filename_in,
	     uint16_t **dest_buf_p, size_t *raw_width_p, size_t *raw_height_p )
{
    int return_status = -1;
    char cmd[32 + PATH_MAX];
    char line_buf[256];
    FILE *pp = NULL;
    unsigned char *strip_buf_in = NULL;
    size_t nbytes_strip_buf_in;
    int _raw_width, _raw_height, maxval;
    size_t raw_width, raw_height, i;
    void *tmp_ptr;
    
    if ( check_a_file(filename_in) < 0 ) goto quit;	/* ERROR */

    snprintf(cmd, 32 + PATH_MAX, "dcraw -c -D -4 -j -t 0 %s", filename_in);
    pp = popen(cmd, "r");
    if ( pp == NULL ) goto quit;			/* ERROR */

    fgets(line_buf, 256, pp);
    if ( strcmp(line_buf,"P5\n") != 0 ) {
        fprintf(stderr,"[ERROR] Line #1 of input data is not 'P5'\n");
	goto quit;
    }

    fgets(line_buf, 256, pp);
    if ( sscanf(line_buf,"%d %d",&_raw_width,&_raw_height) != 2 ) {
        fprintf(stderr,"[ERROR] Line #2 of input data is not valid\n");
	goto quit;
    }
    if ( _raw_width % 2 != 0 ) {
        fprintf(stderr,"[ERROR] Invalid _raw_width value\n");
	goto quit;
    }

    if ( _raw_height % 2 != 0 ) {
        fprintf(stderr,"[ERROR] Invalid _raw_height value\n");
	goto quit;
    }
    raw_width = _raw_width;
    raw_height = _raw_height;
    
    fgets(line_buf, 256, pp);
    maxval = atoi(line_buf);
    if ( maxval != 65535 ) {
        fprintf(stderr,"[ERROR] Line #3 of input data is not 65535\n");
	goto quit;
    }

    tmp_ptr = realloc(*dest_buf_p,
		      sizeof(**dest_buf_p) * raw_width * raw_height);
    if ( tmp_ptr == NULL ) {
        fprintf(stderr,"[ERROR] realloc() failed\n");
	goto quit;
    }
    *dest_buf_p = (uint16_t *)tmp_ptr;
    *raw_width_p = raw_width;
    *raw_height_p = raw_height;

    nbytes_strip_buf_in = sizeof(**dest_buf_p) * raw_width;
    strip_buf_in = malloc(nbytes_strip_buf_in);
    if ( strip_buf_in == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
	goto quit;
    }

    for ( i=0 ; i < raw_height ; i++ ) {
	uint16_t *dest_p = (*dest_buf_p) + (raw_width * i);
	uint16_t v;
        size_t j, j_in;
	if ( fread(strip_buf_in, 1, nbytes_strip_buf_in, pp)
	     != nbytes_strip_buf_in ) {
	    fprintf(stderr,"[ERROR] fread() failed\n");
	    goto quit;
	}
	j_in = 0;
	for ( j=0 ; j < raw_width ; j++ ) {
	    v = strip_buf_in[j_in];  j_in ++;
	    v <<= 8;
	    v |= strip_buf_in[j_in];  j_in ++;
	    dest_p[j] = v;
	}
    }
    
    return_status = 0;
 quit:
    if ( strip_buf_in != NULL ) free(strip_buf_in);
    if ( pp != NULL ) pclose(pp);
    return return_status;
}

typedef struct _float_stat {
    size_t index;
    float value;
} float_stat;

static int compar_fnc(const void *_a, const void *_b)
{
    float a = ((const float_stat *)_a)->value;
    float b = ((const float_stat *)_b)->value;
    if ( a < b ) return -1;
    if ( b < a ) return 1;
    else return 0;
}

void stack_to_float_stat_buf( const uint16_t *bayer_buf,
			    int matrix_idx /* 0-3 */,
			    size_t width, size_t height, float_stat *stat_buf )
{
    size_t bayer_off = 0, dest_off = 0;
    size_t i;
    
    bayer_off += (matrix_idx % 2);
    if ( 2 <= matrix_idx ) bayer_off += (2 * width);
    
    for ( i=0 ; i < height ; i++ ) {
        size_t j, jj;
	jj = 0;
	for ( j=0 ; j < width ; j++ ) {
	    stat_buf[dest_off + j].value += bayer_buf[bayer_off + jj];
	    jj += 2;
	}
	dest_off += width;
	bayer_off += 4 * width;
    }
}

void report_lowest( const float_stat *stat_buf,
		    size_t width, size_t height, int bayer_idx /* 0-3 */,
		    size_t n_report, FILE *f_out )
{
    size_t n_pix = width * height;
    size_t i;
    
    if ( 0 < n_report ) {
        fprintf(f_out,"# Lowest pixels\r\n");
	for ( i=0 ; i < n_pix ; i++ ) {
	    size_t x,y;
	    if ( n_report <= i ) break;
	    x = (stat_buf[i].index % width) * 2;
	    y = (stat_buf[i].index / width) * 2;
	    x += (bayer_idx % 2);
	    if ( 2 <= bayer_idx ) y ++;
	    fprintf(f_out,"%zu %zu %g\r\n", x, y, stat_buf[i].value);
	}
    }

    return;
}

void report_highest( const float_stat *stat_buf,
		     size_t width, size_t height, int bayer_idx /* 0-3 */,
		     size_t n_report, FILE *f_out )
{
    size_t n_pix = width * height;
    size_t i;
    
    if ( 0 < n_report ) {
        fprintf(f_out,"# Highest pixels\r\n");
	for ( i=0 ; i < n_pix ; i++ ) {
	    size_t x,y, ii;
	    if ( n_report <= i ) break;
	    ii = n_pix - 1 - i;
	    x = (stat_buf[ii].index % width) * 2;
	    y = (stat_buf[ii].index / width) * 2;
	    x += (bayer_idx % 2);
	    if ( 2 <= bayer_idx ) y ++;
	    fprintf(f_out,"%zu %zu %g\r\n", x, y, stat_buf[ii].value);
	}
    }
    
    return;
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
    
    FILE *f_out = NULL;
    int matrix_type = 0;
    uint16_t *bayer_buf = NULL;
    float_stat *stat_buf = NULL;
    size_t bayer_width = 0, bayer_height = 0;
    size_t width = 0, height = 0;
    size_t n_pix_list_min[3];
    size_t n_pix_list_max[3];
    int rgb_matrix[2][4] = { { 0,1, 1,2 }, /* rgb_matrix[0] RG GB */  
			     { 1,0, 2,1 }  /* rgb_matrix[1] GR BG */ };
    const char *rgb_name[3] = {"Red","Green","Blue"};
    char deadpixels_filename[PATH_MAX];
    int arg_cnt_file_begin;
    size_t arg_cnt, i;
    
    if ( argc < 4 ) {
	fprintf(stderr,"Output candidate of dead pixels info that can be used by dcraw.\n");
	fprintf(stderr,"Flat images can be used.\n");
	fprintf(stderr,"\n");
        fprintf(stderr,"[USAGE]\n");
        fprintf(stderr,"$ %s n_min n_max rawfile1 rawfile2 ...\n",argv[0]);
        fprintf(stderr,"$ %s n_min_R,n_min_G,n_min_B n_max_R,n_max_G,n_max_B rawfile1 rawfile2 ...\n",argv[0]);
	fprintf(stderr,"\n");
	fprintf(stderr," n_min_R ... number of lowest pixels[R] to be reported\n");
	fprintf(stderr," n_min_G ... number of lowest pixels[G] to be reported\n");
	fprintf(stderr," n_min_B ... number of lowest pixels[B] to be reported\n");
	fprintf(stderr," n_max_R ... number of highest pixels[R] to be reported\n");
	fprintf(stderr," n_max_G ... number of highest pixels[G] to be reported\n");
	fprintf(stderr," n_max_B ... number of highest pixels[B] to be reported\n");
	goto quit;
    }

    arg_cnt = 1;

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
	    n_pix_list_min[i] = atol(tmpbuf);
	    if ( argv[arg_cnt][off + spn] == ',' ) {
		off += spn + 1;
	    }
	}
	arg_cnt ++;
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
	    n_pix_list_max[i] = atol(tmpbuf);
	    if ( argv[arg_cnt][off + spn] == ',' ) {
		off += spn + 1;
	    }
	}
	arg_cnt ++;
    }

    printf("Number of lowest pixels of R,G,B = %zu,%zu,%zu\n",
	   n_pix_list_min[0],n_pix_list_min[1],n_pix_list_min[2]);
    printf("Number of highest pixels of R,G,B = %zu,%zu,%zu\n",
	   n_pix_list_max[0],n_pix_list_max[1],n_pix_list_max[2]);
    
    create_deadpixels_filename(deadpixels_filename,PATH_MAX);
    f_out = fopen(deadpixels_filename,"w");
    if ( f_out == NULL ) {
        fprintf(stderr,"[ERROR] fopen() failed\n");
	goto quit;
    }
    printf("Output file is '%s'\n", deadpixels_filename);
    
    fprintf(f_out,
	    "# Number of lowest pixels reported (R,G,B): %zu,%zu,%zu\r\n",
	    n_pix_list_min[0],n_pix_list_min[1],n_pix_list_min[2]);
    fprintf(f_out,
	    "# Number of highest pixels reported (R,G,B): %zu,%zu,%zu\r\n",
	    n_pix_list_max[0],n_pix_list_max[1],n_pix_list_max[2]);

    arg_cnt_file_begin = arg_cnt;
    if ( (matrix_type=get_matrix_type(argv[arg_cnt_file_begin])) < 0 ) {
        fprintf(stderr,"[ERROR] Unsupported Matrix type\n");
	goto quit;
    }
    fprintf(f_out,"# Matrix type = %d\r\n", matrix_type);

    for ( i=0 ; i < 4 /* matrix_idx */ ; i++ ) {
        int rgb_ch = rgb_matrix[matrix_type][i];
	if ( 0 < n_pix_list_min[rgb_ch] || 0 < n_pix_list_max[rgb_ch] ) {
	    size_t j;
	    printf("Starting channel %s\n",rgb_name[rgb_ch]);
	    fprintf(f_out, "# Candidate of dead pixels (%s channel)\r\n",
		   rgb_name[rgb_ch]);
	    for ( j=arg_cnt_file_begin ; j < argc ; j++ ) {
	        printf(" Loading ... %s\n",argv[j]);
	        if ( read_bayer_to_uint16(argv[j],
				    &bayer_buf, &bayer_width, &bayer_height) < 0 ) {
		    fprintf(stderr,"[ERROR] read_bayer_to_uint16() failed\n");
		    goto quit;
		}
		if ( width == 0 ) {
		    if ( 0 < bayer_width && 0 < bayer_height ) {
			width = bayer_width / 2;
			height = bayer_height / 2;
			/* */
			stat_buf = malloc(sizeof(*stat_buf) * width * height);
			if ( stat_buf == NULL ) {
			    fprintf(stderr,"[ERROR] malloc() failed\n");
			    goto quit;
			}
		    }
		    else {
		        fprintf(stderr,"[ERROR] Invalid width or height\n");
			goto quit;
		    }
		}
		else {
		    if ( width != bayer_width / 2 ||
			 height != bayer_height / 2 ) {
		        fprintf(stderr,"[ERROR] Invalid width or height\n");
			goto quit;
		    }
		}
		if ( j == arg_cnt_file_begin ) {
		    size_t k;
		    for ( k=0 ; k < width * height ; k++ ) {
		        stat_buf[k].index = k;
			stat_buf[k].value = 0;
		    }
		}
		stack_to_float_stat_buf( bayer_buf, i, width, height, stat_buf );
		if ( j + 1 == argc ) {
		    size_t k;
		    double mean, sum = 0, sum2 = 0, npix = width * height;
		    for ( k=0 ; k < width * height ; k++ ) {
		      stat_buf[k].value /= (float)(argc - arg_cnt_file_begin);
		      sum += stat_buf[k].value;
		      sum2 += stat_buf[k].value * stat_buf[k].value;
		    }
		    mean = sum / npix;
		    fprintf(f_out,"# mean = %g\r\n", mean);
		    fprintf(f_out,"# stddev = %g\r\n",
		       sqrt( (sum2 - 2*mean*sum + mean*mean*npix)/(npix-1) ) );
		}
	    }	/* end of loop for files */
	    /* sort */
	    qsort(stat_buf, width*height, sizeof(*stat_buf), &compar_fnc);
	    /* test */
	    //printf("%g %g\n", stat_buf[0].value, stat_buf[1].value);
	    //printf("%g %g\n", stat_buf[width*height-1].value, stat_buf[width*height-2].value);
	      //printf("%g %g %g\n", stat_buf[width+0], stat_buf[width+1], stat_buf[width+2]);
	    report_lowest( stat_buf, width, height, i, n_pix_list_min[rgb_ch],
			   f_out );
	    report_highest( stat_buf, width, height, i, n_pix_list_max[rgb_ch],
			    f_out );
	}
    }
    
    
    return_status = 0;
 quit:
    if ( f_out != NULL ) fclose(f_out);
    if ( stat_buf != NULL ) free(stat_buf);
    if ( bayer_buf != NULL ) free(bayer_buf);

    return return_status;
}
