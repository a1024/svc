#include		"libsvc_internal.h"
#include		<tmmintrin.h>
inline void		integerDCT8x8_step(__m128i *data)
{
	//https://stackoverflow.com/questions/18621167/dct-using-integer-only
	//https://fgiesen.wordpress.com/2013/11/04/bink-2-2-integer-dct-design-part-1/
	//stage 1
	__m128i a[8];
	a[0]=_mm_add_epi16(data[0], data[7]);
	a[1]=_mm_add_epi16(data[1], data[6]);
	a[2]=_mm_add_epi16(data[2], data[5]);
	a[3]=_mm_add_epi16(data[3], data[4]);
	a[4]=_mm_sub_epi16(data[0], data[7]);
	a[5]=_mm_sub_epi16(data[1], data[6]);
	a[6]=_mm_sub_epi16(data[2], data[5]);
	a[7]=_mm_sub_epi16(data[3], data[4]);
	
	//even stage 2
	__m128i b[8];
	b[0]=_mm_add_epi16(a[0], a[3]);
	b[1]=_mm_add_epi16(a[1], a[2]);
	b[2]=_mm_sub_epi16(a[0], a[3]);
	b[3]=_mm_sub_epi16(a[1], a[2]);
	
	//even stage 3
	__m128i c[8];
	c[0]=_mm_add_epi16(b[0], b[1]);
	c[1]=_mm_sub_epi16(b[0], b[1]);
	c[2]=_mm_add_epi16(_mm_add_epi16(b[2], _mm_srai_epi16(b[2], 2)), _mm_srai_epi16(b[3], 1));
	c[3]=_mm_sub_epi16(_mm_sub_epi16(_mm_srai_epi16(b[2], 1), b[3]), _mm_srai_epi16(b[3], 2));

	//odd stage 2
	__m128i a4_4=_mm_srai_epi16(a[4], 2), a7_4=_mm_srai_epi16(a[7], 2);
	b[4]=_mm_add_epi16(_mm_add_epi16(a7_4, a[4]), _mm_sub_epi16(a4_4, _mm_srai_epi16(a[4], 4)));
	b[7]=_mm_add_epi16(_mm_sub_epi16(a4_4, a[7]), _mm_sub_epi16(_mm_srai_epi16(a[7], 4), a7_4));
	b[5]=_mm_sub_epi16(_mm_add_epi16(a[5], a[6]), _mm_add_epi16(_mm_srai_epi16(a[6], 2), _mm_srai_epi16(a[6], 4)));
	b[6]=_mm_add_epi16(_mm_sub_epi16(a[6], a[5]), _mm_add_epi16(_mm_srai_epi16(a[5], 2), _mm_srai_epi16(a[5], 4)));

	//odd stage 3
	c[4]=_mm_add_epi16(b[4], b[5]);
	c[5]=_mm_sub_epi16(b[4], b[5]);
	c[6]=_mm_add_epi16(b[6], b[7]);
	c[7]=_mm_sub_epi16(b[6], b[7]);

	//stage 4
	data[0]=c[0];
	data[1]=c[4];
	data[2]=c[2];
	data[3]=_mm_sub_epi16(c[5], c[7]);
	data[4]=c[1];
	data[5]=_mm_add_epi16(c[5], c[7]);
	data[6]=c[3];
	data[7]=c[6];
	
	//odd stage 4
	//__m128i d4, d5, d6, d7;
	//d4=c[4];
	//d5=_mm_add_epi16(c[5], c[7]);
	//d6=_mm_sub_epi16(c[5], c[7]);
	//d7=c[6];
	//
	//permute/output
	//o[0]=c[0], o[1]=d4, o[2]=c[2], o[3]=d6, o[4]=c[1], o[5]=d5, o[6]=c[3], o[7]=d7;
}
inline void		transpose8x8(__m128i *data)
{
	//https://stackoverflow.com/questions/2517584/transpose-for-8-registers-of-16-bit-elements-on-sse2-ssse3
	__m128i a[8], b[8];
	a[0]=_mm_unpacklo_epi16(data[0], data[1]);
	a[1]=_mm_unpacklo_epi16(data[2], data[3]);
	a[2]=_mm_unpacklo_epi16(data[4], data[5]);
	a[3]=_mm_unpacklo_epi16(data[6], data[7]);
	a[4]=_mm_unpackhi_epi16(data[0], data[1]);
	a[5]=_mm_unpackhi_epi16(data[2], data[3]);
	a[6]=_mm_unpackhi_epi16(data[4], data[5]);
	a[7]=_mm_unpackhi_epi16(data[6], data[7]);

	b[0]=_mm_unpacklo_epi32(a[0], a[1]);
	b[1]=_mm_unpackhi_epi32(a[0], a[1]);
	b[2]=_mm_unpacklo_epi32(a[2], a[3]);
	b[3]=_mm_unpackhi_epi32(a[2], a[3]);
	b[4]=_mm_unpacklo_epi32(a[4], a[5]);
	b[5]=_mm_unpackhi_epi32(a[4], a[5]);
	b[6]=_mm_unpacklo_epi32(a[6], a[7]);
	b[7]=_mm_unpackhi_epi32(a[6], a[7]);

	data[0]=_mm_unpacklo_epi64(b[0], b[2]);
	data[1]=_mm_unpackhi_epi64(b[0], b[2]);
	data[2]=_mm_unpacklo_epi64(b[1], b[3]);
	data[3]=_mm_unpackhi_epi64(b[1], b[3]);
	data[4]=_mm_unpacklo_epi64(b[4], b[6]);
	data[5]=_mm_unpackhi_epi64(b[4], b[6]);
	data[6]=_mm_unpacklo_epi64(b[5], b[7]);
	data[7]=_mm_unpackhi_epi64(b[5], b[7]);
}

//src: 8 frames of iw x ih pixels
//dst: 8 frames of iw x ih shorts (single channel)
//iw & ih are divisible by 8
void			apply_intDCT_8x8x8_8bit(const unsigned char *src, unsigned short *dst, int iw, int ih, int stride)
{
	int str2=stride<<1,
		str3=str2+stride,
		str4=str2<<1,
		str5=str4+stride,
		str6=str3<<1,
		str7=str6+stride,
		str8=stride<<3;
	int w2=iw<<1, w3=w2+iw, w4=w2<<1, w5=w4+iw, w6=w3<<1, w7=w6+iw;
	for(int ky=0, kd=0;ky<ih;ky+=8)
	{
		auto srcrow=src+iw*ky;
		for(int kx=0, ks=0;kx<iw;kx+=8, ks+=str8)
		{
			int ks2=ks;
			__m128i data[8];
			data[0]=_mm_set_epi16(srcrow[ks2], srcrow[ks2+stride], srcrow[ks2+str2], srcrow[ks2+str3], srcrow[ks2+str4], srcrow[ks2+str5], srcrow[ks2+str6], srcrow[ks2+str7]);
			ks2+=iw;
			data[1]=_mm_set_epi16(srcrow[ks2], srcrow[ks2+stride], srcrow[ks2+str2], srcrow[ks2+str3], srcrow[ks2+str4], srcrow[ks2+str5], srcrow[ks2+str6], srcrow[ks2+str7]);
			ks2+=iw;
			data[2]=_mm_set_epi16(srcrow[ks2], srcrow[ks2+stride], srcrow[ks2+str2], srcrow[ks2+str3], srcrow[ks2+str4], srcrow[ks2+str5], srcrow[ks2+str6], srcrow[ks2+str7]);
			ks2+=iw;
			data[3]=_mm_set_epi16(srcrow[ks2], srcrow[ks2+stride], srcrow[ks2+str2], srcrow[ks2+str3], srcrow[ks2+str4], srcrow[ks2+str5], srcrow[ks2+str6], srcrow[ks2+str7]);
			ks2+=iw;
			data[4]=_mm_set_epi16(srcrow[ks2], srcrow[ks2+stride], srcrow[ks2+str2], srcrow[ks2+str3], srcrow[ks2+str4], srcrow[ks2+str5], srcrow[ks2+str6], srcrow[ks2+str7]);
			ks2+=iw;
			data[5]=_mm_set_epi16(srcrow[ks2], srcrow[ks2+stride], srcrow[ks2+str2], srcrow[ks2+str3], srcrow[ks2+str4], srcrow[ks2+str5], srcrow[ks2+str6], srcrow[ks2+str7]);
			ks2+=iw;
			data[6]=_mm_set_epi16(srcrow[ks2], srcrow[ks2+stride], srcrow[ks2+str2], srcrow[ks2+str3], srcrow[ks2+str4], srcrow[ks2+str5], srcrow[ks2+str6], srcrow[ks2+str7]);
			ks2+=iw;
			data[7]=_mm_set_epi16(srcrow[ks2], srcrow[ks2+stride], srcrow[ks2+str2], srcrow[ks2+str3], srcrow[ks2+str4], srcrow[ks2+str5], srcrow[ks2+str6], srcrow[ks2+str7]);
			
			integerDCT8x8_step(data);
			transpose8x8(data);
			integerDCT8x8_step(data);
			transpose8x8(data);

			_mm_storeu_si128((__m128i*)(dst+kd), data[0]), kd+=8;
			_mm_storeu_si128((__m128i*)(dst+kd), data[1]), kd+=8;
			_mm_storeu_si128((__m128i*)(dst+kd), data[2]), kd+=8;
			_mm_storeu_si128((__m128i*)(dst+kd), data[3]), kd+=8;
			_mm_storeu_si128((__m128i*)(dst+kd), data[4]), kd+=8;
			_mm_storeu_si128((__m128i*)(dst+kd), data[5]), kd+=8;
			_mm_storeu_si128((__m128i*)(dst+kd), data[6]), kd+=8;
			_mm_storeu_si128((__m128i*)(dst+kd), data[7]), kd+=8;
		}
	}
}
void			apply_inv_intDCT_8x8x8_8bit(const unsigned short *dst, unsigned char *src, int iw, int ih, int stride)
{
}