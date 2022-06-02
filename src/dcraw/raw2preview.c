/*
 * raw2preview.c
 *
 * Read raw image file using dcraw and create RGB TIFF for preview.
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

#include "icc_srgb_profile.c"
#include "adjust_crop_prms.c"

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

static int get_gamma_prms( const char *opt, double prms[] )
{
    int return_status = -1;
    size_t off, i;
    double val[2];

    if ( opt == NULL ) goto quit;
    
    off = 0;
    for ( i=0 ; ; i++ ) {
        size_t spn;
        spn = strspn(opt + off,"0123456789.");
	if ( spn == 0 ) val[i] = 0;
	else val[i] = atof(opt + off);
	if ( val[i] <= 0 ) {
	    fprintf(stderr,"[ERROR] Invalid -g option\n");
	    goto quit;
	}
	off += spn;
	if ( i == 1 ) break;
	if ( opt[off] == '\0' ) break;
	if ( opt[off] != ',' ) {
	    fprintf(stderr,"[ERROR] Invalid -g option\n");
	    goto quit;
	}
	off ++;
    }
    if ( i == 0 ) {
        prms[0] = val[0];
	prms[1] = 4.5;
    }
    else {
        prms[0] = val[0];
	prms[1] = val[1];
    }

    return_status = 0;
 quit:
    return return_status;
}

static int create_preview_filename( const char *filename_in,
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
    snprintf(filename_out_buf, buf_len, "%s%s", basename, ".preview.tiff");

    return_status = 0;
 quit:
    return return_status;
}

static int raw_to_preview( const char *filename_in,
			   const char *deadpix_file,
			   const long crop_prms[],
			   const double brightness,
			   const double gamma_prms[] )
{
    int return_status = -1;
    
    char filename_out[PATH_MAX] = {'\0'};
    char *dcraw_cmd = NULL;
    size_t dcraw_cmd_len;
    int _width, _height, maxval;
    uint16 bps, byps, spp;
    uint32 width, height, icc_prof_size, i;
    uint32 x_out, y_out, width_out, height_out;	/* actual crop area */
    char line_buf[256];
    FILE *pp = NULL;
    unsigned char *strip_buf_in = NULL;
    unsigned char *strip_buf_out = NULL;
    TIFF *tiff_out = NULL;

    if ( check_a_file(filename_in) < 0 ) {
        fprintf(stderr,"[ERROR] not found: %s\n",filename_in);
	goto quit;	/* ERROR */
    }

    if ( create_preview_filename( filename_in, filename_out, PATH_MAX ) < 0 ) {
        fprintf(stderr,"[ERROR] create_preview_filename() failed\n");
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
		 "dcraw -c -h -j -t 0 -b %f -g %f %f -P %s %s", brightness,
		 gamma_prms[0], gamma_prms[1], deadpix_file, filename_in);
    }
    else {
	snprintf(dcraw_cmd, dcraw_cmd_len,
		 "dcraw -c -h -j -t 0 -b %f -g %f %f %s", brightness,
		 gamma_prms[0], gamma_prms[1], filename_in);
    }

    //printf("crop: %ld %ld %ld %ld\n",
    //	   crop_prms[0],crop_prms[1],crop_prms[2],crop_prms[3]);
    //printf("%s\n", dcraw_cmd);
    printf("Converting: %s => %s\n", filename_in, filename_out);

    /* Start reading PBM */
    
    pp = popen(dcraw_cmd, "r");
    if ( pp == NULL ) {
        fprintf(stderr,"[ERROR] popen() failed\n");
	goto quit;			/* ERROR */
    }
    
    fgets(line_buf, 256, pp);
    if ( strcmp(line_buf,"P6\n") != 0 ) {
        fprintf(stderr,"[ERROR] Line #1 of input data is not 'P5'\n");
	goto quit;
    }

    fgets(line_buf, 256, pp);
    if ( sscanf(line_buf,"%d %d",&_width,&_height) != 2 ) {
        fprintf(stderr,"[ERROR] Line #2 of input data is not valid\n");
	goto quit;
    }
    
    width = _width;
    height = _height;

    fgets(line_buf, 256, pp);
    maxval = atoi(line_buf);
    if ( maxval != 255 ) {
        fprintf(stderr,"[ERROR] Line #3 of input data is not 255\n");
	goto quit;
    }

    strip_buf_in = (unsigned char *)malloc(3 * width);
    if ( strip_buf_in == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
	goto quit;
    }

    /* Adjust cropping parameters */

    adjust_crop_prms( width, height, crop_prms,
		      &x_out, &y_out, &width_out, &height_out );

    //printf("actual crop = %d %d %d %d\n",
    //	   (int)x_out,(int)y_out,(int)width_out,(int)height_out);
    
    /* Preparing of TIFF output */
    
    bps = 8;
    byps = (bps + 7) / 8;
    spp = 3;

    strip_buf_out = (unsigned char *)malloc(byps * spp * width_out);
    if ( strip_buf_out == NULL ) {
        fprintf(stderr,"[ERROR] malloc() failed\n");
	goto quit;
    }

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

    for ( i=0 ; i < height ; i ++ ) {
        const size_t n_to_read = 3 * width;
        uint32 j, j_in, j_out;
	if ( fread(strip_buf_in, 1, n_to_read, pp) != n_to_read ) {
	    fprintf(stderr,"[ERROR] fread() failed\n");
	    goto quit;
	}
	if ( y_out <= i && i < y_out + height_out ) {
	    j_in = 3 * x_out;
	    for ( j=0, j_out=0 ; j < width_out ; j++ ) {
		strip_buf_out[j_out] = strip_buf_in[j_in];
		j_in ++;  j_out ++;
		strip_buf_out[j_out] = strip_buf_in[j_in];
		j_in ++;  j_out ++;
		strip_buf_out[j_out] = strip_buf_in[j_in];
		j_in ++;  j_out ++;
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
    if ( tiff_out != NULL ) {
	TIFFClose(tiff_out);
    }
    if ( strip_buf_out != NULL ) free(strip_buf_out);
    if ( strip_buf_in != NULL ) free(strip_buf_in);
    if ( pp != NULL ) pclose(pp);
    if ( dcraw_cmd != NULL ) free(dcraw_cmd);
    
    return return_status;
}

int main( int argc, char *argv[] )
{
    int return_status = -1;

    size_t arg_cnt;
    long crop_prms[4] = {-1,-1,-1,-1};
    double gamma_prms[2] = {2.222, 4.5};
    double brightness = 1.0;
    char deadpix_file[PATH_MAX] = {'\0'};
    
    if ( argc < 2 ) {
	fprintf(stderr,"Read raw image using dcraw and create RGB TIFF for preview.\n");
	fprintf(stderr,"\n");
        fprintf(stderr,"[USAGE]\n");
        fprintf(stderr,"$ %s [-P file] [-c param] [-g param] rawfile1 rawfile2 ...\n",argv[0]);
	fprintf(stderr,"\n");
	fprintf(stderr,"-P file ... Fix the dead pixels listed in this file\n");
	fprintf(stderr,"-c [x,y,]width,height ... Crop images. Center when x and y are omitted\n");
	fprintf(stderr,"-b brightness ... Adjust brightness (default = 1.0)\n");
	fprintf(stderr,"-g gamma_p[,gamma_ts] ... Set gamma curve (default = 2.222,4.5)\n");
	goto quit;
    }

    arg_cnt = 1;
    
    while ( arg_cnt < argc ) {
        if ( strcmp(argv[arg_cnt],"-c") == 0 ) {
	    arg_cnt ++;
	    if ( argv[arg_cnt] != NULL ) {
	        if ( get_crop_prms(argv[arg_cnt], crop_prms) < 0 ) {
		    fprintf(stderr,"[ERROR] get_crop_prms() failed\n");
		    goto quit;
		}
		arg_cnt ++;
	    }
	}
        else if ( strcmp(argv[arg_cnt],"-b") == 0 ) {
	    arg_cnt ++;
	    if ( argv[arg_cnt] != NULL ) {
	        brightness = atof(argv[arg_cnt]);
		arg_cnt ++;
	    }
	}
        else if ( strcmp(argv[arg_cnt],"-g") == 0 ) {
	    arg_cnt ++;
	    if ( argv[arg_cnt] != NULL ) {
	        if ( get_gamma_prms(argv[arg_cnt], gamma_prms) < 0 ) {
		    fprintf(stderr,"[ERROR] get_gamma_prms() failed\n");
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

    while ( arg_cnt < argc ) {
        if ( raw_to_preview(argv[arg_cnt],
		    deadpix_file, crop_prms, brightness, gamma_prms) < 0 ) {
	    fprintf(stderr,"[ERROR] raw_to_preview() failed\n");
	    goto quit;
	}
        arg_cnt ++;
    }

    return_status = 0;
 quit:
    return return_status;
}
