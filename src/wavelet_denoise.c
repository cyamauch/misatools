/*
  This code includes a part of functions in dcraw.c.

  dcraw.c -- Dave Coffin's raw photo decoder
  Copyright 1997-2015 by Dave Coffin, dcoffin a cybercom o net

  **** GPL Version 2 ****

 */

#define FORC(cnt) for (c=0; c < cnt; c++)
#define SQR(x) ((x)*(x))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define LIM(x,min,max) MAX(min,MIN(x,max))
#define ULIM(x,y,z) ((y) < (z) ? LIM(x,y,z) : LIM(x,z,y))
#define CLIP(x) LIM((int)(x),0,65535)

static void hat_transform (float *temp, float *base, int st, int size, int sc)
{
  int i;
  for (i=0; i < sc; i++)
    temp[i] = 2*base[st*i] + base[st*(sc-i)] + base[st*(i+sc)];
  for (; i+sc < size; i++)
    temp[i] = 2*base[st*i] + base[st*(i-sc)] + base[st*(i+sc)];
  for (; i < size; i++)
    temp[i] = 2*base[st*i] + base[st*(i-sc)] + base[st*(2*size-2-(i+sc))];
}

static void wavelet_denoise( unsigned colors /* 3 for RGB color */, 
			     unsigned short iwidth, unsigned short iheight,
			     float threshold[] /* for r,g,b */,
			     float *image_io_buf,
			     float *fimg /* (size*3 + iheight + iwidth) * sizeof *fimg */
			     )
{
  float *temp, thold;
#if 0
  float mul[2], avg, diff;
#endif
  int scale=1;	/* fixed value (1) for maximus=65536 */
  int size, lev, hpass, lpass, row, col, nc, c, i;
#if 0
  int wlast, blk[2];
  unsigned short *window[4];
#endif
  static const float noise[] =
  { 0.8002,0.2735,0.1202,0.0585,0.0291,0.0152,0.0080,0.0044 };

  const unsigned filters = 0;	/* disable codes for BAYER */
  unsigned maximum = 65536;
  
  /* if (verbose) fprintf (stderr,_("Wavelet denoising...\n")); */

  while (maximum << scale < 0x10000) scale++;
  maximum <<= --scale;
#if 0
  black <<= scale;
  FORC4 cblack[c] <<= scale;
#endif
  size = iheight*iwidth;
  temp = fimg + size*3;
  if ((nc = colors) == 3 && filters) nc++;
  FORC(nc) {			/* denoise R,G1,B,G3 individually */
    float *image;
    image = image_io_buf + (iwidth * iheight) * c;
    for (i=0; i < size; i++)
      fimg[i] = 256 * sqrt(image[i] /* << scale */);
    for (hpass=lev=0; lev < 5; lev++) {
      lpass = size*((lev & 1)+1);
      for (row=0; row < iheight; row++) {
	hat_transform (temp, fimg+hpass+row*iwidth, 1, iwidth, 1 << lev);
	for (col=0; col < iwidth; col++)
	  fimg[lpass + row*iwidth + col] = temp[col] * 0.25;
      }
      for (col=0; col < iwidth; col++) {
	hat_transform (temp, fimg+lpass+col, iwidth, iheight, 1 << lev);
	for (row=0; row < iheight; row++)
	  fimg[lpass + row*iwidth + col] = temp[row] * 0.25;
      }
      thold = threshold[c] * noise[lev];
      for (i=0; i < size; i++) {
	fimg[hpass+i] -= fimg[lpass+i];
	if	(fimg[hpass+i] < -thold) fimg[hpass+i] += thold;
	else if (fimg[hpass+i] >  thold) fimg[hpass+i] -= thold;
	else	 fimg[hpass+i] = 0;
	if (hpass) fimg[i] += fimg[hpass+i];
      }
      hpass = lpass;
    }
    for (i=0; i < size; i++)
      image[i] = CLIP(SQR(fimg[i]+fimg[lpass+i])/0x10000);
  }

#if 0
  if (filters && colors == 3) {  /* pull G1 and G3 closer together */
    for (row=0; row < 2; row++) {
      mul[row] = 0.125 * pre_mul[FC(row+1,0) | 1] / pre_mul[FC(row,0) | 1];
      blk[row] = cblack[FC(row,0) | 1];
    }
    for (i=0; i < 4; i++)
      window[i] = (unsigned short *) fimg + width*i;
    for (wlast=-1, row=1; row < height-1; row++) {
      while (wlast < row+1) {
	for (wlast++, i=0; i < 4; i++)
	  window[(i+3) & 3] = window[i];
	for (col = FC(wlast,1) & 1; col < width; col+=2)
	  window[2][col] = BAYER(wlast,col);
      }
      thold = threshold[c]/512;
      for (col = (FC(row,0) & 1)+1; col < width-1; col+=2) {
	avg = ( window[0][col-1] + window[0][col+1] +
		window[2][col-1] + window[2][col+1] - blk[~row & 1]*4 )
	      * mul[row & 1] + (window[1][col] + blk[row & 1]) * 0.5;
	avg = avg < 0 ? 0 : sqrt(avg);
	diff = sqrt(BAYER(row,col)) - avg;
	if      (diff < -thold) diff += thold;
	else if (diff >  thold) diff -= thold;
	else diff = 0;
	BAYER(row,col) = CLIP(SQR(avg+diff) + 0.5);
      }
    }
  }
#endif

}


#undef CLIP
#undef ULIM
#undef LIM
#undef MAX
#undef MIN
#undef SQR
#undef FORC
