
#define USE_OPTIMIZED_BYTE_SWAP 1

#include "../test_simd.h"

#ifdef _SSE2_IS_OK
#include <emmintrin.h>

/* Minimum byte length of buffer read or written by SSE2. */
/* NOTE: Do not set values less than 16.                  */
#define _SSE2_MIN_NBYTES 64

#endif

/**
 * @brief  SIMD命令(SSE2)を使った4バイト値のバイトオーダの高速変換
 */
inline static void s_byteswap4( void *buf, size_t len_elements )
{
    unsigned char *d_ptr = (unsigned char *)buf;
    unsigned char tmp;

#ifdef USE_OPTIMIZED_BYTE_SWAP
#ifdef _SSE2_IS_OK
    if ( _SSE2_MIN_NBYTES <= 4 * len_elements ) {
	bool is_aligned = ( ((size_t)d_ptr & 0x03) == 0 );
	if ( is_aligned == true ) {			/* align if possible */
	    while ( ((size_t)d_ptr & 0x0f) != 0 ) {
		len_elements --;
		tmp = d_ptr[0];  d_ptr[0] = d_ptr[3];  d_ptr[3] = tmp;
		tmp = d_ptr[1];  d_ptr[1] = d_ptr[2];  d_ptr[2] = tmp;
		d_ptr += 4;
	    }
	}
	/* NOTE: Do not use _mm_stream...() that causes slow down.      */
	/*       Using load+store improves slightly, but u+u is enough. */
	while ( 4 <= len_elements ) {
	    len_elements -= 4;
	    __m128i r0 = _mm_loadu_si128((__m128i *)d_ptr);
	    /* 0123 => 2301 */
	    r0 = _mm_shufflelo_epi16(r0, _MM_SHUFFLE(2,3,0,1));
	    r0 = _mm_shufflehi_epi16(r0, _MM_SHUFFLE(2,3,0,1));
	    __m128i r1 = r0;
	    /* 1-byte shift */
	    r0 = _mm_srli_epi16(r0, 8);
	    r1 = _mm_slli_epi16(r1, 8);
	    /* or */
	    r0 = _mm_or_si128(r0, r1);
	    _mm_storeu_si128((__m128i *)d_ptr, r0);
	    d_ptr += 4 * 4;
	}
    }
#else
    while ( 4 <= len_elements ) {
	len_elements -= 4;
	tmp = d_ptr[0];  d_ptr[0] = d_ptr[3];  d_ptr[3] = tmp;
	tmp = d_ptr[1];  d_ptr[1] = d_ptr[2];  d_ptr[2] = tmp;
	tmp = d_ptr[4];  d_ptr[4] = d_ptr[7];  d_ptr[7] = tmp;
	tmp = d_ptr[5];  d_ptr[5] = d_ptr[6];  d_ptr[6] = tmp;
	tmp = d_ptr[8];  d_ptr[8] = d_ptr[11];  d_ptr[11] = tmp;
	tmp = d_ptr[9];  d_ptr[9] = d_ptr[10];  d_ptr[10] = tmp;
	tmp = d_ptr[12];  d_ptr[12] = d_ptr[15];  d_ptr[15] = tmp;
	tmp = d_ptr[13];  d_ptr[13] = d_ptr[14];  d_ptr[14] = tmp;
	d_ptr += 4 * 4;
    }
#endif	/* _SSE2_IS_OK */
#endif	/* USE_OPTIMIZED_BYTE_SWAP */

    while ( 0 < len_elements ) {
	len_elements --;
	tmp = d_ptr[0];  d_ptr[0] = d_ptr[3];  d_ptr[3] = tmp;
	tmp = d_ptr[1];  d_ptr[1] = d_ptr[2];  d_ptr[2] = tmp;
	d_ptr += 4;
    }

    return;
}
