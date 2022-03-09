//ac2.cpp - Entropy coders implementation, CPU-side
//Copyright (C) 2022  Ayman Wagih Mohsen
//
//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program.  If not, see <https://www.gnu.org/licenses/>.

#define			PROFILER_IMPLEMENTATION
#include		"profiler.h"
#include		"libsvc_internal.h"
#include		<stdio.h>
#include		<stdlib.h>
#include		<string.h>
#include		<math.h>
#include		<tmmintrin.h>
//#include		<intrin.h>
#ifdef __GNUC__
#include		<x86intrin.h>
#endif
#include		<algorithm>

//	#define		ANS_CL_EMUATE_RENDER2GLTEXTURE

//	#define		ANS_ENC_DIV_FREE
//	#define		ANS_PRINT_STATE2
//	#define		ANS_PRINT_STATE//X
//	#define		ANS_PRINT_HISTOGRAM
//	#define		ANS_CHECK_STATE

	#define		AC_MEASURE_PREDICTION

int				floor_log2(unsigned long long n)
{
	int logn=0;
	int sh=(n>=1ULL<<32)<<5;logn+=sh, n>>=sh;
		sh=(n>=1<<16)<<4;	logn+=sh, n>>=sh;
		sh=(n>=1<< 8)<<3;	logn+=sh, n>>=sh;
		sh=(n>=1<< 4)<<2;	logn+=sh, n>>=sh;
		sh=(n>=1<< 2)<<1;	logn+=sh, n>>=sh;
		sh= n>=1<< 1;		logn+=sh;
	return logn;
}
int				ceil_log2(unsigned long long n)
{
	int l2=floor_log2(n);
	l2+=(1ULL<<l2)<n;
	return l2;
}

//error handling
bool			set_error(const char *file, int line, const char *msg, ...);
#define			MY_ASSERT(SUCCESS, MESSAGE, ...)	((SUCCESS)!=0||set_error(__FILE__, __LINE__, MESSAGE, ##__VA_ARGS__))
#define			FAIL(REASON, ...)					return set_error(__FILE__, __LINE__, REASON, ##__VA_ARGS__)

void			pause()
{
	int k=0;
	scanf_s("%d", &k, 1);
}

typedef unsigned long long u64;
const int		magic_ac04='A'|'C'<<8|'0'<<16|'4'<<24,//ABAC
				magic_ac05='A'|'C'<<8|'0'<<16|'5'<<24,//ANS
				magic_ac06='A'|'C'<<8|'0'<<16|'6'<<24,//64bit ANS
				magic_ac07='A'|'C'<<8|'0'<<16|'7'<<24,//SSE2 ANS
				magic_ac08='A'|'C'<<8|'0'<<16|'8'<<24,//AVX2 ANS

				magic_ac09='A'|'C'<<8|'0'<<16|'9'<<24,//OpenCL ABAC
				magic_an09='A'|'N'<<8|'0'<<16|'9'<<24;//OpenCL ANS
//struct		ABAC4Header
//{
//	int magic;//ac04
//	int sizes[];
//};
inline int		clamp(int lo, int x, int hi)
{
	if(x<lo)
		x=lo;
	if(x>hi)
		x=hi;
	return x;
}
inline bool		emit_byte(unsigned char *&out_data, unsigned long long &out_size, unsigned long long &out_cap, unsigned char b)
{
	if(out_size>=out_cap)
	{
		auto newcap=out_cap?out_cap<<1:1;
		auto ptr=(unsigned char*)realloc(out_data, newcap);
		if(!ptr)
			FAIL("Realloc returned nullptr");
		out_data=ptr, out_cap=newcap;
	}
	out_data[out_size]=b;
	++out_size;
	return true;
}
inline bool		emit_pad(unsigned char *&out_data, const unsigned long long &out_size, unsigned long long &out_cap, int size)
{
	while(out_size+size>=out_cap)
	{
		auto newcap=out_cap?out_cap<<1:1;
		auto ptr=(unsigned char*)realloc(out_data, newcap);
		if(!ptr)
			FAIL("Realloc returned nullptr");
		out_data=ptr, out_cap=newcap;
	}
	memset(out_data+out_size, 0, size);
	return true;
}
inline void		store_int_le(unsigned char *base, unsigned long long &offset, int i)
{
	auto p=(unsigned char*)&i;
	base[offset  ]=p[0];
	base[offset+1]=p[1];
	base[offset+2]=p[2];
	base[offset+3]=p[3];
	offset+=4;
}
inline int		load_int_le(const unsigned char *buffer)
{
	int i=0;
	auto p=(unsigned char*)&i;
	p[0]=buffer[0];
	p[1]=buffer[1];
	p[2]=buffer[2];
	p[3]=buffer[3];
	return i;
}
inline int		load_int_be(const unsigned char *buffer)
{
	int i=0;
	auto p=(unsigned char*)&i;
	p[0]=buffer[3];
	p[1]=buffer[2];
	p[2]=buffer[1];
	p[3]=buffer[0];
	return i;
}

	#define		LOG_WINDOW_SIZE		16	//[2, 16]	do not change
	#define		LOG_CONFBOOST		14
	#define		ABAC2_CONF_MSB_RELATION

const double	boost_power=4, min_conf=0.55;
const int		window_size=1<<LOG_WINDOW_SIZE, prob_mask=window_size-1, prob_max=window_size-2, prob_init=(1<<(LOG_WINDOW_SIZE-1))-1;
int				abac4_encode(const void *src, int imsize, int depth, int bytestride, unsigned char *&out_data, unsigned long long &out_size, unsigned long long &out_cap, int loud)
{
	if(!src||!imsize||!depth||!bytestride)
		FAIL("abac4_encode(src=%p, imsize=%d, depth=%d, stride=%d)", src, imsize, depth, bytestride);
	auto buffer=(const unsigned char*)src;
	auto t1=__rdtsc();

	u64 out_start=out_size, out_idx_sizes=out_start+sizeof(int), out_idx_conf=out_idx_sizes+depth*sizeof(int);
	int headercount=1+(depth<<1);
	if(!emit_pad(out_data, out_size, out_cap, headercount*sizeof(int)))
		return false;
	store_int_le(out_data, out_size, magic_ac04);
	out_size=out_idx_conf+depth*sizeof(int);
	
#ifdef AC_MEASURE_PREDICTION
	u64 hitnum=0, hitden=0;//prediction efficiency
#endif

	//std::vector<std::string> planes(depth);
	for(int kp=depth-1;kp>=0;--kp)//bit-plane loop		encode MSB first
	{
		u64 out_planestart=out_size;
		int bit_offset=kp>>3, bit_shift=kp&7;
		int bit_offset2=(kp+1)>>3, bit_shift2=(kp+1)&7;
		//auto &plane=planes[depth-1-kp];
		int prob=0x8000, prob_correct=0x8000;//cheap weighted average predictor

		u64 hitcount=1;

		for(int kb=0, kb2=0;kb<imsize;++kb, kb2+=bytestride)//analyze bitplane
		{
			int bit=buffer[kb2+bit_offset]>>bit_shift&1;
			int p0=((long long)(prob-0x8000)*prob_correct>>16);
			p0+=0x8000;
			//int p0=0x8000+(long long)(prob-0x8000)*hitcount/(kb+1);
			p0=clamp(1, p0, prob_max);
			int correct=bit^(p0>=0x8000);
			//if(kp==1)
			//	printf("%d", bit);//actual bits
			//	printf("%d", p0<0x8000);//predicted bits
			//	printf("%d", !correct);//prediction error
			hitcount+=correct;
			prob=!bit<<15|prob>>1;
			prob_correct=correct<<15|prob_correct>>1;
		}
		PROF(ENC_ANALYZE_PLANE);
		u64 offset=out_idx_conf+kp*sizeof(int);
		store_int_le(out_data, offset, (int)hitcount);
		//out_data[out_idx_conf+kp]=(int)hitcount;//X assigns single byte
		//out_data[out_idx_conf+depth-1-kp]=(int)hitcount;

		if(hitcount<imsize*min_conf)//incompressible, bypass
		{
			emit_pad(out_data, out_size, out_cap, (imsize+7)>>3);
			auto plane=out_data+out_size;
			//plane.resize((imsize+7)>>3, 0);
			for(int kb=0, kb2=0, b=0;kb<imsize;++kb, kb2+=bytestride)
			{
				int byte_idx=kb>>3, bit_idx=kb&7;
				int bit=buffer[kb2+bit_offset]>>bit_shift&1;
			//	int bit=buffer[kb]>>kp&1;
				plane[byte_idx]|=bit<<bit_idx;
			}
			PROF(ENC_BYPASS_PLANE);
		}
		else
		{
			int hitratio_sure=int(0x10000*pow((double)hitcount/imsize, 1/boost_power)), hitratio_notsure=int(0x10000*pow((double)hitcount/imsize, boost_power));
			int hitratio_delta=hitratio_sure-hitratio_notsure;
		//	int hitratio_sure=int(0x10000*cbrt((double)hitcount/imsize)), hitratio_notsure=int(0x10000*(double)hitcount*hitcount*hitcount/((double)imsize*imsize*imsize));
		//	int hitratio_sure=int(0x10000*sqrt((double)hitcount/imsize)), hitratio_notsure=int(0x10000*(double)hitcount*hitcount/((double)imsize*imsize));
			hitcount=(hitcount<<16)/imsize;

			//hitcount=unsigned(((u64)hitcount<<16)/imsize);
			//hitcount=abac2_normalize16(hitcount, logimsize);
			//hitcount*=invimsize;

			prob_correct=prob=0x8000;

#ifdef ABAC2_CONF_MSB_RELATION
			int prevbit0=0;
#endif
			
			emit_pad(out_data, out_size, out_cap, imsize>>8);
			//plane.reserve(imsize>>8);
			unsigned start=0;
			u64 range=0xFFFFFFFF;
			for(int kb=0, kb2=0;kb<imsize;kb2+=bytestride)//bit-pixel loop		http://mattmahoney.net/dc/dce.html#Section_32
			{
				int bit=buffer[kb2+bit_offset]>>bit_shift&1;
			//	int bit=buffer[kb]>>kp&1;
#ifdef ABAC2_CONF_MSB_RELATION
				int prevbit=buffer[kb2+bit_offset2]>>bit_shift2&1;
			//	int prevbit=buffer[kb]>>(kp+1)&1;
#endif
				
				if(range<3)
				{
					//emit_pad(out_data, out_size, out_cap, 4);
					//memcpy(out_data+out_size, &start, 4), out_size+=4;

					emit_byte(out_data, out_size, out_cap, start>>24);//big endian
					emit_byte(out_data, out_size, out_cap, start>>16&0xFF);
					emit_byte(out_data, out_size, out_cap, start>>8&0xFF);
					emit_byte(out_data, out_size, out_cap, start&0xFF);

					//plane.push_back(start>>24);
					//plane.push_back(start>>16&0xFF);
					//plane.push_back(start>>8&0xFF);
					//plane.push_back(start&0xFF);
					start=0, range=0xFFFFFFFF;//because 1=0.9999...
				}
				
				int p0=prob-0x8000;
				p0=p0*prob_correct>>16;
				p0=p0*prob_correct>>16;
				int sure=-(prevbit==prevbit0);
				p0=p0*(hitratio_notsure+(hitratio_delta&sure))>>16;
				//p0=p0*(prevbit==prevbit0?hitratio_sure:hitratio_notsure)>>16;
				//p0=(long long)p0*hitcount>>16;
				p0+=0x8000;
				//if(prevbit!=prevbit0)
				//	p0=0x8000;
				//	p0=0xFFFF-p0;

				//int p0=0x8000+((long long)(prob-0x8000)*(prevbit==prevbit0?hitratio_sure:hitratio_notsure)>>16);

				//int p0=(long long)(prob-0x8000)*sqrthitcount>>16;
				//if(prevbit==prevbit0)
				//	p0=(long long)p0*hitcount>>16;
				//p0+=0x8000;

				//int confboost=prevbit==prevbit0;
				//confboost-=!confboost;
				//confboost<<=LOG_CONFBOOST;
				//int p0=0x8000+((long long)(prob-0x8000)*(hitcount+confboost)>>16);

			//	int p0=0x8000+(int)((prob-0x8000)*(prevbit==prevbit0?sqrt((double)test_conf[kp]/imsize):(double)test_conf[kp]*test_conf[kp]/((double)imsize*imsize)));
			//	int p0=prevbit==prevbit0?prob:0x8000;
			//	int p0=0x8000+(long long)(prob-0x8000)*test_conf[kp]/imsize;
			//	int p0=0x8000+(long long)(prob-0x8000)*hitcount/(kb+1);
				p0=clamp(1, p0, prob_max);
				unsigned r2=(unsigned)(range*p0>>16);
				r2+=(r2==0)-(r2==range);
#ifdef DEBUG_ABAC2
				if(kp==examined_plane&&kb>=examined_start&&kb<examined_end)
					printf("%6d %6d %d %08X+%08X %08X %08X\n", kp, kb, bit, start, (int)range, r2, start+r2);
#endif

				int correct=bit^(p0>=0x8000);
			//	hitcount+=correct;
				prob=!bit<<15|prob>>1;
				prob_correct=correct<<15|prob_correct>>1;
#ifdef ABAC2_CONF_MSB_RELATION
				prevbit0=prevbit;
#endif
#ifdef AC_MEASURE_PREDICTION
				hitnum+=correct, ++hitden;
#endif
				auto start0=start;
				if(bit)
				{
					++r2;
					start+=r2, range-=r2;
				}
				//	start=middle+1;
				else
					range=r2-1;
				//	end=middle-1;
				if(start<start0)//
				{
					FAIL("AC OVERFLOW: start = %08X -> %08X, r2 = %08X", start0, start, r2);
					//printf("OVERFLOW\nstart = %08X -> %08X, r2 = %08X", start0, start, r2);
					//int k=0;
					//scanf_s("%d", &k);
				}
				++kb;
				
				while((start^(start+(unsigned)range))<0x1000000)//most significant byte has stabilized			zpaq 1.10
				{
#ifdef DEBUG_ABAC2
					if(kp==examined_plane&&kb>=examined_start&&kb<examined_end)
						printf("range 0x%08X byte-out 0x%02X\n", (int)range, start>>24);
#endif
					emit_byte(out_data, out_size, out_cap, start>>24);
					//plane.push_back(start>>24);
					start<<=8;
					range=range<<8|0xFF;
				}
			}
			emit_byte(out_data, out_size, out_cap, start>>24);//big endian
			emit_byte(out_data, out_size, out_cap, start>>16&0xFF);
			emit_byte(out_data, out_size, out_cap, start>>8&0xFF);
			emit_byte(out_data, out_size, out_cap, start&0xFF);

			//plane.push_back(start>>24&0xFF);//big-endian
			//plane.push_back(start>>16&0xFF);
			//plane.push_back(start>>8&0xFF);
			//plane.push_back(start&0xFF);
			PROF(ENC_AC);
		}
		if(loud)
		{
			int c=load_int_le(out_data+out_idx_conf+kp*sizeof(int));
			printf("bit %d: conf = %6d / %6d = %lf%%\n", kp, c, imsize, 100.*c/imsize);
		}
		//	printf("bit %d: conf = %6d / %6d = %lf%%\n", kp, hitcount, imsize, 100.*hitcount/imsize);
		offset=out_idx_sizes+kp*sizeof(int);
		store_int_le(out_data, offset, (int)(out_size-out_planestart));
		//out_data[out_idx_sizes+kp]=out_size-out_planestart;
		//out_data[out_idx_sizes+depth-1-kp]=out_size-out_planestart;
	}
	auto t2=__rdtsc();
	//out_data.clear();
	//for(int k=0;k<depth;++k)
	//	out_sizes[k]=(int)planes[k].size();
	//for(int k=0;k<depth;++k)
	//{
	//	auto &plane=planes[k];
	//	out_data.insert(out_data.end(), plane.begin(), plane.end());
	//}
	//auto t3=__rdtsc();

	if(loud)
	{
		int original_bitsize=imsize*depth, compressed_bitsize=(int)(out_size-out_start)<<3;
		printf("AC encode:  %lld cycles, %lf c/byte\n", t2-t1, (double)(t2-t1)/(original_bitsize>>3));
		printf("Size: %d -> %d, ratio: %lf, %lf bpp\n", original_bitsize>>3, compressed_bitsize>>3, (double)original_bitsize/compressed_bitsize, (double)compressed_bitsize/imsize);
#ifdef AC_MEASURE_PREDICTION
		printf("Predicted: %6lld / %6lld = %lf%%\n", hitnum, hitden, 100.*hitnum/hitden);
#endif
		printf("Bit\tbytes\tratio,\tbytes/bitplane = %d\n", imsize>>3);
		for(int k=0;k<depth;++k)
		{
			int size=load_int_le(out_data+out_idx_sizes+k*sizeof(int));
			printf("%2d\t%5d\t%lf\n", depth-1-k, size, (double)imsize/(size<<3));
		}
		
		printf("Preview:\n");
		int kprint=out_size-out_start<200?(int)(out_size-out_start):200;
		for(int k=0;k<kprint;++k)
			printf("%02X-", out_data[out_start+k]&0xFF);
		printf("\n");
	}
	return true;
}
int				abac4_decode(const unsigned char *in_data, unsigned long long &in_idx, unsigned long long in_size, void *dst, int imsize, int depth, int bytestride, int loud)
{
	auto buffer=(unsigned char*)dst;
	if(!in_data||!imsize||!depth||!bytestride)
		FAIL("abac4_decode(data=%p, imsize=%d, depth=%d, stride=%d)", in_data, imsize, depth, bytestride);
	auto t1=__rdtsc();
	//memset(buffer, 0, imsize*bytestride);

	int headercount=1+(depth<<1);
	if(in_idx+headercount*sizeof(int)>=in_size)
		FAIL("Missing information: idx=%lld, size=%lld", in_idx, in_size);
	int magic=load_int_le(in_data+in_idx);
	if(magic!=magic_ac04)
		FAIL("Invalid magic number 0x%08X, expected 0x%08X", magic, magic_ac04);
	auto sizes=in_data+in_idx+sizeof(int), conf=sizes+depth*sizeof(int), data=conf+depth*sizeof(int);
	in_idx+=headercount*sizeof(int);
	
	int cusize=0;
	for(int kp=depth-1;kp>=0;--kp)//bit-plane loop
	{
		int bit_offset=kp>>3, bit_shift=kp&7;
		int bit_offset2=(kp+1)>>3, bit_shift2=(kp+1)&7;
		int ncodes=load_int_le(sizes+kp*sizeof(int));
	//	int ncodes=load_int_le(sizes+(depth-1-kp)*sizeof(int));
		auto plane=data+cusize;
		
		int prob=0x8000, prob_correct=0x8000;
#if 1
		u64 hitcount=load_int_le(conf+kp*sizeof(int));
	//	u64 hitcount=load_int_le(conf+(depth-1-kp)*sizeof(int));
		if(hitcount<imsize*min_conf)
		{
			for(int kb=0, kb2=0, b=0;kb<imsize;++kb, kb2+=bytestride)
			{
				int byte_idx=kb>>3, bit_idx=kb&7;
				int bit=plane[byte_idx]>>bit_idx&1;
				buffer[kb2+bit_offset]|=bit<<bit_shift;
			//	buffer[kb]|=bit<<kp;
			}
			cusize+=ncodes;
			PROF(DEC_BYPASS_PLANE);
			continue;
		}
#ifdef ABAC2_CONF_MSB_RELATION
		int prevbit0=0;
#endif
		int hitratio_sure=int(0x10000*pow((double)hitcount/imsize, 1/boost_power)), hitratio_notsure=int(0x10000*pow((double)hitcount/imsize, boost_power));
		int hitratio_delta=hitratio_sure-hitratio_notsure;
		//int hitratio_sure=int(0x10000*cbrt((double)hitcount/imsize)), hitratio_notsure=int(0x10000*(double)hitcount*hitcount*hitcount/((double)imsize*imsize*imsize));
		//int hitratio_sure=int(0x10000*sqrt((double)hitcount/imsize)), hitratio_notsure=int(0x10000*(double)hitcount*hitcount/((double)imsize*imsize));
		hitcount=(hitcount<<16)/imsize;
		//hitcount=unsigned(((u64)hitcount<<16)/imsize);
		//hitcount=abac2_normalize16(hitcount, logimsize);
		//hitcount*=invimsize;
#endif

		unsigned start=0;
		u64 range=0xFFFFFFFF;
		unsigned code=load_int_be(plane);
		for(int kc=4, kb=0, kb2=0;kb<imsize;kb2+=bytestride)//bit-pixel loop
		{
			if(range<3)
			{
				code=load_int_be(plane+kc);
				kc+=4;
				start=0, range=0xFFFFFFFF;//because 1=0.9999...
			}
#ifdef ABAC2_CONF_MSB_RELATION
			int prevbit=0;
			if(kp+1<depth)
				prevbit=buffer[kb2+bit_offset2]>>bit_shift2&1;
		//	int prevbit=buffer[kb]>>(kp+1)&1;
#endif
			int p0=prob-0x8000;
			p0=p0*prob_correct>>16;
			p0=p0*prob_correct>>16;
			int sure=-(prevbit==prevbit0);
			p0=p0*(hitratio_notsure+(hitratio_delta&sure))>>16;
			//p0=p0*(prevbit==prevbit0?hitratio_sure:hitratio_notsure)>>16;
			//p0=(long long)p0*hitcount>>16;
			p0+=0x8000;
			//if(prevbit!=prevbit0)
			//	p0=0x8000;
			//	p0=0xFFFF-p0;

			//int p0=0x8000+((long long)(prob-0x8000)*(prevbit==prevbit0?hitratio_sure:hitratio_notsure)>>16);

			//int p0=(long long)(prob-0x8000)*sqrthitcount>>16;
			//if(prevbit==prevbit0)
			//	p0=(long long)p0*hitcount>>16;
			//p0+=0x8000;

			//int confboost=prevbit==prevbit0;
			//confboost-=!confboost;
			//confboost<<=LOG_CONFBOOST;
			//int p0=0x8000+((long long)(prob-0x8000)*(hitcount+confboost)>>16);

		//	int p0=0x8000+(int)((prob-0x8000)*(prevbit==prevbit0?sqrt((double)test_conf[kp]/imsize):(double)test_conf[kp]*test_conf[kp]/((double)imsize*imsize)));
		//	int p0=prevbit==prevbit0?prob:0x8000;
		//	int p0=0x8000+(long long)(prob-0x8000)*test_conf[kp]/imsize;
		//	int p0=0x8000+(long long)(prob-0x8000)*hitcount/(kb+1);
			p0=clamp(1, p0, prob_max);
			unsigned r2=(unsigned)(range*p0>>16);
			r2+=(r2==0)-(r2==range);
			unsigned middle=start+r2;
			int bit=code>middle;
#ifdef DEBUG_ABAC2
			if(kp==examined_plane&&kb>=examined_start&&kb<examined_end)
				printf("%6d %6d %d %08X+%08X %08X %08X %08X\n", kp, kb, bit, start, (int)range, r2, middle, code);
#endif
			
			int correct=bit^(p0>=0x8000);
		//	hitcount+=correct;
			prob=!bit<<15|prob>>1;
			prob_correct=correct<<15|prob_correct>>1;
#ifdef ABAC2_CONF_MSB_RELATION
			prevbit0=prevbit;
#endif
			
			if(bit)
			{
				++r2;
				start+=r2, range-=r2;
			}
			//	start=middle+1;
			else
				range=r2-1;
			//	end=middle-1;
			
			buffer[kb2+bit_offset]|=bit<<bit_shift;
		//	buffer[kb]|=bit<<kp;
			++kb;
			
			while((start^(start+(unsigned)range))<0x1000000)//shift-out identical bytes			zpaq 1.10
			{
#ifdef DEBUG_ABAC2
				if(kp==examined_plane&&kb>=examined_start&&kb<examined_end)
					printf("range 0x%08X byte-out 0x%02X\n", (int)range, code>>24);
#endif
				code=code<<8|(unsigned char)plane[kc];
				++kc;
				start<<=8;
				range=range<<8|0xFF;
			}
		}
		cusize+=ncodes;
		PROF(DEC_AC);
	}
	in_idx+=cusize;
	auto t2=__rdtsc();

	if(loud)
	{
		printf("AC decode:  %lld cycles, %lf c/byte\n", t2-t1, (double)(t2-t1)/(imsize*depth>>3));
	}
	return true;
}


typedef unsigned RANS_state;
const int
	ANS_PROB_BITS=16,//CHANNEL DEPTH <= 15
	ANS_L=(1<<ANS_PROB_BITS),//X
	ANS_DEPTH=8,
	ANS_NLEVELS=1<<ANS_DEPTH;
struct			SortedHistInfo
{
	int idx,//symbol
		freq,//original freq
		qfreq;//quantized freq
	SortedHistInfo():idx(0), freq(0), qfreq(0){}
};
int				ans_calc_histogram(const unsigned char *buffer, int nsymbols, int bytestride, unsigned short *histogram, int prob_bits, int integrate)
{
	int prob_sum=1<<prob_bits;
	//MY_ASSERT(ANS_NLEVELS<prob_sum, "Channel depth %d >= PROB_BITS %d", ANS_NLEVELS, prob_sum);//what if ANS_NLEVELS = 2^N-1 ?
	if(!nsymbols)
	{
		memset(histogram, 0, ANS_NLEVELS*sizeof(*histogram));
		FAIL("Symbol count is zero");
	}
	SortedHistInfo h[ANS_NLEVELS];
	for(int k=0;k<ANS_NLEVELS;++k)
		h[k].idx=k;
	int bytesize=nsymbols*bytestride;
	PROF(HISTOGRAM_INIT);
	for(int k=0;k<bytesize;k+=bytestride)//this loop takes 73% of encode time
		++h[buffer[k]].freq;
	//	++h[buffer[k]>>bit0&mask].freq;
	PROF(HISTOGRAM_LOOKUP);
	if(nsymbols!=prob_sum)
	{
		const int prob_max=prob_sum-1;
	//	const int prob_max=prob_sum-2;
		for(int k=0;k<ANS_NLEVELS;++k)
			h[k].qfreq=((long long)h[k].freq<<ANS_PROB_BITS)/nsymbols;

		std::sort(h, h+ANS_NLEVELS, [](SortedHistInfo const &a, SortedHistInfo const &b)
		{
			return a.freq<b.freq;
		});
		int idx=0;
		for(;idx<ANS_NLEVELS&&!h[idx].freq;++idx);
		for(;idx<ANS_NLEVELS&&!h[idx].qfreq;++idx)
			++h[idx].qfreq;
		for(idx=ANS_NLEVELS-1;idx>=0&&h[idx].qfreq>=prob_max;--idx);
		for(++idx;idx<ANS_NLEVELS;++idx)
			h[idx].qfreq=prob_max;

		int error=-prob_sum;//too much -> +ve error & vice versa
		for(int k=0;k<ANS_NLEVELS;++k)
			error+=h[k].qfreq;
		if(error>0)
		{
			while(error)
			{
				for(int k=0;k<ANS_NLEVELS&&error;++k)
				{
					int dec=h[k].qfreq>1;
					h[k].qfreq-=dec, error-=dec;
				}
			}
		}
		else
		{
			while(error)
			{
				for(int k=ANS_NLEVELS-1;k>=0&&error;--k)
				{
					int inc=h[k].qfreq<prob_max;
					h[k].qfreq+=inc, error+=inc;
				}
			}
		}
		if(error)
			FAIL("Internal error: histogram adds up to %d != %d", prob_sum+error, prob_sum);
		std::sort(h, h+ANS_NLEVELS, [](SortedHistInfo const &a, SortedHistInfo const &b)
		{
			return a.idx<b.idx;
		});
	}
	int sum=0;
	for(int k=0;k<ANS_NLEVELS;++k)
	{
		if(h[k].qfreq>0xFFFF)
			FAIL("Internal error: symbol %d has probability %d", k, h[k].qfreq);
		histogram[k]=integrate?sum:h[k].qfreq;
		sum+=h[k].qfreq;
	}
	if(sum!=ANS_L)
		FAIL("Internal error: CDF ends with 0x%08X, should end with 0x%08X", sum, ANS_L);
	return true;
}
bool			rans4_prep(const void *hist_ptr, int bytespersymbol, SymbolInfo *&info, unsigned char *&CDF2sym, int loud)
{
	int tempsize=bytespersymbol*(ANS_NLEVELS*sizeof(SymbolInfo)+ANS_L);
	info=(SymbolInfo*)malloc(tempsize);
	if(!info)
		FAIL("Failed to allocate temp buffer");
	CDF2sym=(unsigned char*)info+bytespersymbol*ANS_NLEVELS*sizeof(SymbolInfo);
	for(int kc=0;kc<bytespersymbol;++kc)
	{
		auto c_histogram=(const unsigned short*)hist_ptr+(kc<<ANS_DEPTH);
		auto c_info=info+(kc<<ANS_DEPTH);
		auto c_CDF2sym=CDF2sym+(kc<<ANS_PROB_BITS);
		int sum=0;
		for(int k=0;k<ANS_NLEVELS;++k)
		{
			auto &si=c_info[k];
			si.freq=c_histogram[k];
			si.cmpl_freq=~si.freq;
			si.CDF=sum;
			si.reserved0=0;

			if(si.freq<2)//0 freq: don't care, 1 freq:		//Ryg's fast rANS encoder
			{
				si.shift=0;
				si.inv_freq=0xFFFFFFFF;
				si.bias=si.CDF+ANS_L-1;
			}
			else
			{
				si.shift=ceil_log2(c_histogram[k])-1;
				si.inv_freq=(unsigned)(((0x100000000<<si.shift)+c_histogram[k]-1)/c_histogram[k]);
				si.bias=si.CDF;
			}
			//if(si.freq)//doesn't work?
			//{
			//	si.shift=0;//
			//	si.inv_freq=(unsigned)(0xFFFFFFFF/c_histogram[k]);
			//}
			//else
			//{
			//	si.shift=0;
			//	si.inv_freq=0xFFFFFFFF;
			//}
			//si.CDF=sum;

			si.renorm_limit=si.freq<<(32-ANS_PROB_BITS);

			if(CDF2sym&&k)
			{
				for(int k2=c_info[k-1].CDF;k2<(int)si.CDF;++k2)
					c_CDF2sym[k2]=k-1;
			}
			sum+=si.freq;
		}
		if(CDF2sym)
		{
			for(int k2=c_info[ANS_NLEVELS-1].CDF;k2<ANS_L;++k2)
				c_CDF2sym[k2]=ANS_NLEVELS-1;
		}
		if(sum!=ANS_L)
			FAIL("histogram sum = %d != %d", sum, ANS_L);
		if(loud)
		{
#ifdef ANS_PRINT_HISTOGRAM
			static int printed=0;
			if(printed<1)
			{
				printf("s\tf\tCDF\n");
				for(int k=0;k<ANS_NLEVELS;++k)
				{
					auto &si=c_info[k];
					if(c_histogram[k])
						printf("%3d\t%5d = %04X\t%04X\n", k, c_histogram[k], c_histogram[k], si.CDF);
				}
				++printed;
			}
#endif
		}
	}
	//	if(!calc_hist_derivaties((const unsigned short*)hist_ptr+kc*ANS_NLEVELS, info+(kc<<ANS_DEPTH), CDF2sym+ANS_L*kc, loud))
	//		return false;
	return true;
}
inline bool		emit_short_le(unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, unsigned short i)
{
	if(dst_size>=dst_cap)
	{
		auto newcap=dst_cap<<1;
		newcap+=!newcap<<1;
		auto ptr=(unsigned char*)realloc(dst, newcap);
		if(!ptr)
			FAIL("Realloc returned nullptr");
		dst=ptr, dst_cap=newcap;
	}
	*(unsigned short*)(dst+dst_size)=i;//assume all encoded data is 2 byte-aligned
	dst_size+=2;
	return true;
}

inline void		rans_encode_start(RANS_state &state){state=ANS_L;}
inline bool		rans_encode(const unsigned char *&srcptr, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, RANS_state &state, const SymbolInfo *info)//goes forward in src & dst
{
	auto s=*srcptr;
	++srcptr;
	auto &si=info[s];
	PROF(FETCH);

	if(!si.freq)
		FAIL("Symbol 0x%02X has zero frequency", s);

	if(state>=si.renorm_limit)//renormalize
//	if(state>=(unsigned)(si.freq<<(32-ANS_PROB_BITS)))
	{
		if(!emit_short_le(dst, dst_size, dst_cap, (unsigned short)state))
			return false;
		state>>=16;
	}
	PROF(RENORM);
#ifdef ANS_PRINT_STATE2
	printf("enc: 0x%08X = 0x%08X+(0x%08X*0x%08X>>(32+%d))*0x%04X+0x%08X\n", state+(((long long)state*si.inv_freq>>32)>>si.shift)*si.cmpl_freq+si.bias, state, state, si.inv_freq, si.shift, si.cmpl_freq, si.bias);
#endif
#ifdef ANS_ENC_DIV_FREE
	state+=(((long long)state*si.inv_freq>>32)>>si.shift)*si.cmpl_freq+si.bias;//Ryg's division-free rANS encoder	https://github.com/rygorous/ryg_rans/blob/master/rans_byte.h
#else
	state=(state/si.freq<<ANS_PROB_BITS)+state%si.freq+si.CDF;
	
//	lldiv_t result=lldiv(state, si.freq);//because unsigned
//	state=((result.quot<<ANS_PROB_BITS)|result.rem)+si.CDF;
#endif
	PROF(UPDATE);
	return true;
}
inline bool		rans_encode_finish(unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, RANS_state &state)
{
	if(!emit_short_le(dst, dst_size, dst_cap, (unsigned short)state))
		return false;
	if(!emit_short_le(dst, dst_size, dst_cap, (unsigned short)(state>>16)))
		return false;
	//dst.push_back((unsigned short)state);
	//dst.push_back((unsigned short)(state>>16));
	return true;
}

inline bool		rans_decode_start(const unsigned char *srcbuf, const unsigned char *&srcptr, unsigned char *dstbuf, unsigned char *&dstptr, int csize, int framebytes, RANS_state *state, int bytespersymbol)
{
	if(srcbuf)
		srcptr=srcbuf+csize;
	if(dstbuf)
		dstptr=dstbuf+framebytes;

	if(srcbuf+bytespersymbol*4>srcptr)
		FAIL("Not enuogh data to initialize state");
	for(int kc=bytespersymbol-1;kc>=0;--kc)
	{
		srcptr-=2, state[kc]=*(const unsigned short*)srcptr;
		srcptr-=2, state[kc]=state[kc]<<16|*(const unsigned short*)srcptr;
	}
	return true;
}
inline bool		rans_decode(const unsigned char *&srcptr, unsigned char *&dstptr, RANS_state &state, const SymbolInfo *info, const unsigned char *CDF2sym)//goes backwards in src & dst
{
	auto c=(unsigned short)state;
//	int c=state&(ANS_L-1);
	auto s=CDF2sym[c];
	auto &si=info[s];
	if(!si.freq)
		FAIL("Symbol 0x%02X has zero frequency", s);
	
	--dstptr;
	*dstptr=s;
	PROF(FETCH);
#ifdef ANS_PRINT_STATE2
	printf("dec: 0x%08X = 0x%04X*(0x%08X>>%d)+0x%04X-0x%08X\n", si.freq*(state>>ANS_PROB_BITS)+c-si.CDF, (int)si.freq, state, ANS_PROB_BITS, c, si.CDF);
#endif
	state=si.freq*(state>>ANS_PROB_BITS)+c-si.CDF;
	PROF(UPDATE);

	if(state<ANS_L)
		srcptr-=2, state=state<<16|*(const unsigned short*)srcptr;
	PROF(RENORM);
	return true;
}

int				rans4_encode(const void *src, int nsymbols, int bytespersymbol, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, int loud)//bytespersymbol: up to 16
{
	PROF(WASTE);
	auto buffer=(const unsigned char*)src;
	auto dst_start=dst_size;
	int headersize=8+bytespersymbol*ANS_NLEVELS*sizeof(short);
	emit_pad(dst, dst_size, dst_cap, headersize);
	dst_size+=headersize;
	auto temp=dst_start;
	store_int_le(dst, temp, magic_ac05);
	for(int kc=0;kc<bytespersymbol;++kc)
		if(!ans_calc_histogram(buffer+kc, nsymbols, bytespersymbol, (unsigned short*)(dst+dst_start+8+kc*(ANS_NLEVELS*sizeof(short))), 16, false))
			return false;

	//printf("idx = %lld\n", dst_start+8);//
	SymbolInfo *info=nullptr;
	unsigned char *CDF2sym=nullptr;
	if(!rans4_prep(dst+dst_start+8, bytespersymbol, info, CDF2sym, loud))
		return false;

	RANS_state state[16]={};
	rans_encode_start(state[0]);
	for(int kc=1;kc<bytespersymbol;++kc)
		state[kc]=state[0];
	int framebytes=nsymbols*bytespersymbol;
	auto srcptr=buffer;
	PROF(PREP);
	for(int ks=0;ks<framebytes;++ks)
	{
		if(srcptr>=(const unsigned char*)src+framebytes)
			FAIL("Out of bounds");
		int kc=ks%bytespersymbol;
#ifdef ANS_PRINT_STATE
		unsigned s0=state[kc];//
#endif
		if(!rans_encode(srcptr, dst, dst_size, dst_cap, state[kc], info+(kc<<ANS_DEPTH)))
		{
			free(info);
			return false;
		}
#ifdef ANS_PRINT_STATE
		printf("kc %d s=%02X x=%08X->%08X\n", kc, srcptr[-1]&0xFF, s0, state[kc]);//
#endif
	}
	for(int kc=0;kc<bytespersymbol;++kc)
	{
		if(!rans_encode_finish(dst, dst_size, dst_cap, state[kc]))
		{
			free(info);
			return false;
		}
	}
	int csize=(int)(dst_size-dst_start);
	dst_start+=4;
	store_int_le(dst, dst_start, csize);
	//printf("\nenc csize=%d\n", csize);//
	free(info);
	return true;
}
int				rans4_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, int nsymbols, int bytespersymbol, int loud, const void *guide)
{
	PROF(WASTE);
	int tag=load_int_le(src+src_idx);
	if(tag!=magic_ac05)
		FAIL("Lost at %lld: found 0x%08X, magic = 0x%08X", src_idx, tag, magic_ac05);
	auto hist=(unsigned short*)(src+src_idx+8);
	
	//printf("idx = %lld\n", src_idx+8);//
	SymbolInfo *info=nullptr;
	unsigned char *CDF2sym=nullptr;
	if(!rans4_prep(hist, bytespersymbol, info, CDF2sym, loud))
		return false;

	RANS_state state[16]={};
	const unsigned char *srcptr=nullptr;
	unsigned char *dstptr=nullptr;
	int headersize=8+bytespersymbol*ANS_NLEVELS*sizeof(short);
	auto src_start=src+src_idx+headersize;
	int csize=load_int_le(src+src_idx+4);
	//printf("\ndec csize=%d\n", csize);//
	int framebytes=nsymbols*bytespersymbol;
	if(!rans_decode_start(src_start, srcptr, (unsigned char*)dst, dstptr, csize-headersize, framebytes, state, bytespersymbol))
		return false;
	PROF(PREP);
	for(int ks=framebytes-1;ks>=0;--ks)
	{
		if(dstptr<dst)
			FAIL("dstptr is out of bounds: ks=%d, frame = %d bytes,\ndstptr=%p, start=%p", ks, framebytes, dstptr, dst);
		int kc=ks%bytespersymbol;
#ifdef ANS_PRINT_STATE
		unsigned s0=state[kc];//
#endif
		if(!rans_decode(srcptr, dstptr, state[kc], info+(kc<<ANS_DEPTH), CDF2sym+(kc<<ANS_PROB_BITS)))
		{
			free(info);
			return false;
		}
		if(srcptr<src_start)
			FAIL("srcptr < start: ks=%d, frame = %d bytes, s = 0x%02X,\nsrcptr=%p, start=%p", ks, framebytes, *dstptr, srcptr, src_start);
#ifdef ANS_PRINT_STATE
		printf("kc %d s=%02X x=%08X->%08X\n", kc, *dstptr&0xFF, s0, state[kc]);//
#endif
		if(guide&&((unsigned char*)guide)[ks]!=*dstptr)
			FAIL("Decode error at byte %d/%d: decoded 0x%02X != original 0x%02X", ks, framebytes, ((unsigned char*)guide)[ks]&0xFF, *dstptr&0xFF);
	}

	src_idx+=csize;
	free(info);
	return true;
}

//64-bit rANS
struct			SymbolInfo64
{
	unsigned short
		freq,//quantized
		cmpl_freq,
		shift,
		reserved0;
	unsigned
		CDF,
		bias;
	unsigned long long
		renorm_limit,
		inv_freq;
};
bool			rans6_prep(const void *hist_ptr, int bytespersymbol, SymbolInfo64 *&info, unsigned char *&CDF2sym, int loud)
{
	int tempsize=bytespersymbol*(ANS_NLEVELS*sizeof(SymbolInfo64)+ANS_L);
	info=(SymbolInfo64*)malloc(tempsize);
	if(!info)
		FAIL("Failed to allocate temp buffer");
	CDF2sym=(unsigned char*)info+bytespersymbol*ANS_NLEVELS*sizeof(SymbolInfo64);
	for(int kc=0;kc<bytespersymbol;++kc)
	{
		auto c_histogram=(const unsigned short*)hist_ptr+(kc<<ANS_DEPTH);
		auto c_info=info+(kc<<ANS_DEPTH);
		auto c_CDF2sym=CDF2sym+(kc<<ANS_PROB_BITS);
		int sum=0;
		for(int k=0;k<ANS_NLEVELS;++k)
		{
			auto &si=c_info[k];
			si.freq=c_histogram[k];
			si.cmpl_freq=~si.freq;
			si.CDF=sum;
			si.reserved0=0;

			if(si.freq<2)//0 freq: don't care, 1 freq:		//Ryg's fast rANS encoder
			{
				si.shift=0;
				si.inv_freq=0xFFFFFFFFFFFFFFFF;
				si.bias=si.CDF+ANS_L-1;
			}
			else
			{
				si.shift=ceil_log2(c_histogram[k])-1;
				u64 temp=0x100000000<<si.shift;
				si.inv_freq=(temp/si.freq<<32)+((temp%si.freq<<32)+si.freq-1)/si.freq;
				si.bias=si.CDF;
			}

			si.renorm_limit=(u64)si.freq<<(64-ANS_PROB_BITS);

			if(CDF2sym&&k)
			{
				for(int k2=c_info[k-1].CDF;k2<(int)si.CDF;++k2)
					c_CDF2sym[k2]=k-1;
			}
			sum+=si.freq;
		}
		if(CDF2sym)
		{
			for(int k2=c_info[ANS_NLEVELS-1].CDF;k2<ANS_L;++k2)
				c_CDF2sym[k2]=ANS_NLEVELS-1;
		}
		if(sum!=ANS_L)
			FAIL("histogram sum = %d != %d", sum, ANS_L);
		if(loud)
		{
#ifdef ANS_PRINT_HISTOGRAM
			static int printed=0;
			if(printed<1)
			{
				printf("s\tf\tCDF\n");
				for(int k=0;k<ANS_NLEVELS;++k)
				{
					auto &si=c_info[k];
					if(c_histogram[k])
						printf("%3d\t%5d = %04X\t%04X\n", k, c_histogram[k], c_histogram[k], si.CDF);
				}
				++printed;
			}
#endif
		}
	}
	return true;
}
inline bool		emit_int_le(unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, unsigned i)//aligned by 2 bytes
{
	if(dst_size>=dst_cap)
	{
		auto newcap=dst_cap<<1;
		newcap+=!newcap<<1;
		auto ptr=(unsigned char*)realloc(dst, newcap);
		if(!ptr)
			FAIL("Realloc returned nullptr");
		dst=ptr, dst_cap=newcap;
	}
	auto p=(unsigned short*)&i;
	*(unsigned short*)(dst+dst_size  )=p[0];
	*(unsigned short*)(dst+dst_size+2)=p[1];
	dst_size+=4;
	return true;
}
int				rans6_encode(const void *src, int nsymbols, int bytespersymbol, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, int loud)
{
	PROF(WASTE);
	auto buffer=(const unsigned char*)src;
	auto dst_start=dst_size;
	int headersize=8+bytespersymbol*ANS_NLEVELS*sizeof(short);
	emit_pad(dst, dst_size, dst_cap, headersize);
	dst_size+=headersize;
	auto temp=dst_start;
	store_int_le(dst, temp, magic_ac06);
	for(int kc=0;kc<bytespersymbol;++kc)
		if(!ans_calc_histogram(buffer+kc, nsymbols, bytespersymbol, (unsigned short*)(dst+dst_start+8+kc*(ANS_NLEVELS*sizeof(short))), 16, false))
			return false;

	//printf("idx = %lld\n", dst_start+8);//
	SymbolInfo64 *info=nullptr;
	unsigned char *CDF2sym=nullptr;
	if(!rans6_prep(dst+dst_start+8, bytespersymbol, info, CDF2sym, loud))
		return false;

	unsigned long long state[16]={};
	for(int kc=0;kc<bytespersymbol;++kc)
		state[kc]=0x100000000;
	int framebytes=nsymbols*bytespersymbol;
	auto srcptr=buffer;
	PROF(PREP);
	for(int ks=0;ks<framebytes;++ks)
	{
		//if(srcptr>=(const unsigned char*)src+framebytes)
		//	FAIL("srcptr is out of bounds");
		int kc=ks%bytespersymbol;

		auto &x=state[kc];
		auto s=*srcptr;
		++srcptr;
		auto &si=info[kc<<ANS_DEPTH|s];
		PROF(FETCH);

		if(!si.freq)
			FAIL("Symbol 0x%02X has zero frequency", s);

		if(x>=si.renorm_limit)//renormalize
	//	if(x>=(unsigned)(si.freq<<(32-ANS_PROB_BITS)))
		{
			if(!emit_int_le(dst, dst_size, dst_cap, (unsigned)x))
			{
				free(info);
				return false;
			}
			x>>=32;
		}
		PROF(RENORM);
#ifdef ANS_PRINT_STATE2
		printf("enc: 0x%016X = 0x%016X+(0x%016X*0x%016X>>(32+%d))*0x%04X+0x%08X\n", x+(((long long)x*si.inv_freq>>32)>>si.shift)*si.cmpl_freq+si.bias, x, x, si.inv_freq, si.shift, si.cmpl_freq, si.bias);
#endif
#ifdef ANS_ENC_DIV_FREE
#ifdef __GNUC__
		unsigned long long q=(unsigned long long)((unsigned __int128)x*si.inv_freq)>>64;
#else
		unsigned long long q=__umulh(x, si.inv_freq);
#endif
		x+=(q>>si.shift)*si.cmpl_freq+si.bias;//Ryg's division-free rANS encoder	https://github.com/rygorous/ryg_rans/blob/master/rans_byte.h
#else
		x=(x/si.freq<<ANS_PROB_BITS)+x%si.freq+si.CDF;
	
	//	lldiv_t result=lldiv(x, si.freq);//because unsigned
	//	x=((result.quot<<ANS_PROB_BITS)|result.rem)+si.CDF;
#endif
		PROF(UPDATE);
	}
	for(int kc=0;kc<bytespersymbol;++kc)
	{
		if(!emit_int_le(dst, dst_size, dst_cap, (unsigned)state[kc]))
		{
			free(info);
			return false;
		}
		if(!emit_int_le(dst, dst_size, dst_cap, (unsigned)(state[kc]>>32)))
		{
			free(info);
			return false;
		}
	}
	int csize=(int)(dst_size-dst_start);
	dst_start+=4;
	store_int_le(dst, dst_start, csize);
	//printf("\nenc csize=%d\n", csize);//
	free(info);
	return true;
}
int				rans6_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, int nsymbols, int bytespersymbol, int loud, const void *guide)
{
	PROF(WASTE);
	int tag=load_int_le(src+src_idx);
	if(tag!=magic_ac06)
		FAIL("Lost at %lld: found 0x%08X, magic = 0x%08X", src_idx, tag, magic_ac05);
	auto hist=(unsigned short*)(src+src_idx+8);
	
	//printf("idx = %lld\n", src_idx+8);//
	SymbolInfo64 *info=nullptr;
	unsigned char *CDF2sym=nullptr;
	if(!rans6_prep(hist, bytespersymbol, info, CDF2sym, loud))
		return false;

	RANS_state state[16]={};
	const unsigned char *srcptr=nullptr;
	unsigned char *dstptr=nullptr;
	int headersize=8+bytespersymbol*ANS_NLEVELS*sizeof(short);
	auto src_start=src+src_idx+headersize;
	int csize=load_int_le(src+src_idx+4);
	//printf("\ndec csize=%d\n", csize);//
	int framebytes=nsymbols*bytespersymbol;

	if(src_start)
		srcptr=src_start+csize;
	if(dst)
		dstptr=(unsigned char*)dst+framebytes;

	if(src_start+(bytespersymbol<<3)>srcptr)
		FAIL("Not enough data to initialize state");
	for(int kc=bytespersymbol-1;kc>=0;--kc)
	{
		srcptr-=2, state[kc]=*(const unsigned short*)srcptr;
		srcptr-=2, state[kc]=state[kc]<<16|*(const unsigned short*)srcptr;
		srcptr-=2, state[kc]=state[kc]<<16|*(const unsigned short*)srcptr;
		srcptr-=2, state[kc]=state[kc]<<16|*(const unsigned short*)srcptr;
	}

	PROF(PREP);
	for(int ks=framebytes-1;ks>=0;--ks)
	{
		//if(dstptr<dst)
		//	FAIL("dstptr is out of bounds: ks=%d, frame = %d bytes,\ndstptr=%p, start=%p", ks, framebytes, dstptr, dst);
		int kc=ks%bytespersymbol;

		auto &x=state[kc];
		auto c=(unsigned short)x;
		auto s=CDF2sym[kc<<ANS_PROB_BITS|c];
		auto &si=info[kc<<ANS_DEPTH|s];
		if(!si.freq)
		{
			free(info);
			FAIL("Symbol 0x%02X has zero frequency", s);
		}
		--dstptr;
		*dstptr=s;
		PROF(FETCH);
#ifdef ANS_PRINT_STATE2
		printf("dec: 0x%016X = 0x%04X*(0x%016X>>%d)+0x%04X-0x%08X\n", si.freq*(state>>ANS_PROB_BITS)+c-si.CDF, (int)si.freq, state, ANS_PROB_BITS, c, si.CDF);
#endif
		x=si.freq*(x>>ANS_PROB_BITS)+c-si.CDF;
		PROF(UPDATE);

		if(x<(1LL<<48))
			srcptr-=2, x=x<<16|*(const unsigned short*)srcptr;
		PROF(RENORM);

		if(srcptr<src_start)
		{
			free(info);
			FAIL("srcptr < start: ks=%d, frame = %d bytes, s = 0x%02X,\nsrcptr=%p, start=%p", ks, framebytes, *dstptr, srcptr, src_start);
		}
		if(guide&&((unsigned char*)guide)[ks]!=*dstptr)
		{
			free(info);
			FAIL("Decode error at byte %d/%d: decoded 0x%02X != original 0x%02X", ks, framebytes, *dstptr&0xFF, ((unsigned char*)guide)[ks]&0xFF);
		}
	}

	src_idx+=csize;
	free(info);
	return true;
}

//SSE2 rANS
const unsigned	ANS7_L=1<<16,
				ANS7_PROB_BITS=14, ANS7_M=1<<ANS7_PROB_BITS,//14-bit histogram	TODO: make calc_histogram take parameter prob_bits
				ANS7_NLEVELS=256;
union RansWordSlot
{
	unsigned u32;
	struct
	{
		unsigned short freq, bias;
	};
};
struct RansWordTables
{
	RansWordSlot slots[ANS7_M];
	unsigned char slot2sym[ANS7_M];
};
int				rans7_encode(const void *src, int nsymbols, int bytespersymbol, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, int loud)
{
	PROF(WASTE);
	auto buffer=(const unsigned char*)src;
	auto dst_start=dst_size;
	int headersize=8+bytespersymbol*ANS_NLEVELS*sizeof(short);//14-bit histogram
	emit_pad(dst, dst_size, dst_cap, headersize);
	dst_size+=headersize;
	auto temp=dst_start;
	store_int_le(dst, temp, magic_ac07);
	for(int kc=0;kc<bytespersymbol;++kc)
		if(!ans_calc_histogram(buffer+kc, nsymbols, bytespersymbol, (unsigned short*)(dst+dst_start+8+kc*(ANS_NLEVELS*sizeof(short))), ANS7_PROB_BITS, false))
			return false;
	return false;//TODO
}
int				rans7_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, int nsymbols, int bytespersymbol, int loud, const void *guide)
{
	PROF(WASTE);
	int tag=load_int_le(src+src_idx);
	if(tag!=magic_ac05)
		FAIL("Lost at %lld: found 0x%08X, magic = 0x%08X", src_idx, tag, magic_ac05);
	auto hist=(unsigned short*)(src+src_idx+8);
	return false;//TODO
}

//AVX2 rANS
int				rans8_encode(const void *src, int nsymbols, int bytespersymbol, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, int loud)
{
	return false;//TODO
}
int				rans8_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, int nsymbols, int bytespersymbol, int loud, const void *guide)
{
	return false;//TODO
}

//OpenCL ABAC - 32-bit only
const char		cl_kernels[]="cl_kernels.h";
const int		ABAC9_DEPTH=32,
				ABAC9_ALLOWANCE=2;//ratio of allocated buffer size to original image size
void			print_clmem_as_floats(CLBuffer mem)
{
	auto buffer=mem.read();
	int size=(int)mem.size();
	for(int k=0;k<size;++k)
		printf("[%d] 0x%08X = %f\n", k, (int&)buffer[k], buffer[k]);
	printf("\n");
	delete[] buffer;
}
void			print_clmem_as_ints(CLBuffer mem)
{
	auto buffer=(int*)mem.read();
	int size=(int)mem.size();
	for(int k=0;k<size;++k)
		printf("[%d] 0x%08X = %d\n", k, buffer[k], buffer[k]);
	printf("\n");
	delete[] buffer;
}
void			print_clmem_as_bytes(CLBuffer mem, int count=0, int offset=0)
{
	unsigned char *buffer=nullptr;
	if(count)
	{
		buffer=(unsigned char*)mem.read_sub(offset, count);
		count*=sizeof(int);
	}
	else
	{
		buffer=(unsigned char*)mem.read();
		count=(int)mem.size()*sizeof(int);
	}
	for(int k=0;k<count;++k)
		printf("%02X-", buffer[k]);
	printf("\n");
	delete[] buffer;
}
int				abac9_prep(int iw, int ih, int block_w, int block_h, int encode, ABAC9Context *ctx)
{
	if(!ocl_init(cl_kernels, false))
		return false;
	ctx->iw=iw;
	ctx->ih=ih;
	ctx->block_w=block_w;
	ctx->block_h=block_h;

	//derived attributes
	ctx->block_xcount=(iw+block_w-1)/block_w;
	ctx->block_ycount=(ih+block_h-1)/block_h;
	ctx->padded_w=block_w*ctx->block_xcount;
	ctx->padded_h=block_h*ctx->block_ycount;
	int blocksize=ctx->block_w*ctx->block_h,//must be POT
		blockcount=ctx->block_xcount*ctx->block_ycount;
	ctx->paddedcount=blocksize*blockcount;
	ctx->blockplanecount=blockcount*ABAC9_DEPTH;//32-bit depth
	ctx->logalloccount=floor_log2(blocksize/32*ABAC9_ALLOWANCE);//log2(64*64/32*allowance_ratio), allowance_ratio=2
	ctx->alloccount=ctx->blockplanecount<<ctx->logalloccount;
	
	ctx->receiver_stats=(int*)malloc(ctx->blockplanecount*sizeof(int)*2);
	if(encode)
		ctx->receiver_cdata=(unsigned char*)malloc(ctx->alloccount*sizeof(int));
	else
		ctx->receiver_cdata=nullptr;

	ctx->buf_image0.create(ctx->iw*ctx->ih, BUFFER_READ_WRITE);
	if(iw!=ctx->padded_w||ih!=ctx->padded_h)
		ctx->buf_image_p.create(ctx->paddedcount, BUFFER_READ_WRITE);
	else
		ctx->buf_image_p.handle=nullptr;
	ctx->buf_dim.create(DIM_VAL_COUNT, BUFFER_READ_ONLY);
	ctx->buf_stats.create(ctx->blockplanecount, BUFFER_READ_WRITE);
	ctx->buf_cdata.create(ctx->alloccount, BUFFER_READ_WRITE);
	ctx->buf_sizes.create(ctx->blockplanecount, BUFFER_READ_WRITE);
	
	int shift=floor_log2(blocksize)-2;
	if(shift<0)
		shift=0;
	int dim[]=
	{
		ctx->iw,
		ctx->ih,
		ctx->block_xcount,
		ctx->block_ycount,
		ctx->block_w,
		ctx->block_h,
		4,
		shift,
		ctx->logalloccount,
		
		0xE00,//P(0)	prep counts ones, while codec needs P(0)
		0xA00,
		0x600,
		0x200,

		//0x180,//very low
		//0x580,//below half
		//0x980,//above half
		//0xD80,//very high
	};
	ctx->buf_dim.write(dim);
	ocl_sync();//because src is on the stack

	//printf("Init:\n");
	//print_clmem_as_ints(ctx->buf_dim);

	//{
	//	auto temp=(int*)ctx->buf_dim.read();
	//	printf("Dim:\n");
	//	for(int k=0;k<DIM_VAL_COUNT;++k)
	//		printf("  %d\n", temp[k]);
	//	printf("\n");
	//	delete[] temp;
	//}
	return true;
}
int				abac9_finish(ABAC9Context *ctx)
{
	ctx->buf_image0.release();
	ctx->buf_image_p.release();
	ctx->buf_dim.release();
	ctx->buf_stats.release();
	ctx->buf_cdata.release();
	ctx->buf_sizes.release();
	return true;
}
inline bool		emit_pad_uninitialized(unsigned char *&out_data, const unsigned long long &out_size, unsigned long long &out_cap, int bytesize)
{
	while(out_size+bytesize>=out_cap)
	{
		auto newcap=out_cap?out_cap<<1:1;
		auto ptr=(unsigned char*)realloc(out_data, newcap);
		if(!ptr)
			FAIL("Realloc returned nullptr");
		out_data=ptr, out_cap=newcap;
	}
	return true;
}
int				abac9_encode(const void *src, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, ABAC9Context *ctx, int loud)
{
	if(!src||!dst||!ctx)
		FAIL("abac9_enc: invalid args");
	PROF(WASTE);
	auto t1=__rdtsc();
	auto dst_start=dst_size;
	int headersize=4+ctx->blockplanecount*sizeof(short);
	emit_pad_uninitialized(dst, dst_size, dst_cap, headersize);
	store_int_le(dst, dst_start, magic_ac09);
	dst_size+=headersize;
	PROF(PREP);
	
	CLBuffer args[5]={}, padded_image=nullptr;
	ctx->buf_image0.write(src);
	if(ctx->buf_image_p.handle)
	{
		ocl_sync();
		args[0]=ctx->buf_image0;
		args[1]=ctx->buf_image_p;
		args[2]=ctx->buf_dim;
		ocl_call_kernel(OCL_pad_const, ctx->padded_w*ctx->padded_h, args, 3);
		padded_image=ctx->buf_image_p;
	}
	else
		padded_image=ctx->buf_image0;
/*	int padded_w=ctx->block_w*ctx->block_xcount,
		padded_h=ctx->block_h*ctx->block_ycount;
	if(ctx->iw==padded_w)
	{
		if(ctx->ih==padded_h)
		{
		}
		else
		{
		}
	}
	else
	{
		if(ctx->ih==padded_h)
		{
		}
		else
		{
		}
	}//*/
	//ocl_call_kernel(OCL_zeromem, ctx->blockplanecount, &ctx->buf_image, 1);
	//ocl_sync();
	//if(ctx->iw==padded_w)
	//{
	//	if(ctx->ih==padded_h)
	//		ctx->buf_image.write(src);
	//	else
	//		ctx->buf_image.write_sub();
	//}
	//else
	//{
	//	for(int ky=0;ky<ctx->ih;++ky)
	//		ctx->buf_image.write_sub((const int*)src+ctx->iw*ky, ctx->block_w*ctx->block_xcount*ky, ctx->iw);
	//}
	ocl_call_kernel(OCL_zeromem, ctx->blockplanecount, &ctx->buf_stats, 1);
	ocl_call_kernel(OCL_zeromem, ctx->alloccount, &ctx->buf_cdata, 1);
	ocl_call_kernel(OCL_zeromem, ctx->blockplanecount, &ctx->buf_sizes, 1);
	ocl_sync();
	PROF(INITIALIZE);
	//printf("GPU image:\n");//
	//print_clmem_as_ints(ctx->buf_image);//
	//printf("GPU dim:\n");//
	//print_clmem_as_ints(ctx->buf_dim);//
	//printf("GPU dim:\n");//
	//print_clmem_as_ints(ctx->buf_stats);//

	args[0]=padded_image;
	args[1]=ctx->buf_dim;
	args[2]=ctx->buf_stats;
	//pause();
	ocl_call_kernel(OCL_abac_enc2D32_prep, ctx->blockplanecount/32, args, 3);
	ocl_sync();
	//pause();
	PROF(ENCODE_PREP);
	//return true;

	args[3]=ctx->buf_cdata;
	args[4]=ctx->buf_sizes;
	ocl_call_kernel(OCL_abac_enc2D32, ctx->blockplanecount, args, 5);
	ocl_sync();
	PROF(ENCODE);
	auto
		stats=ctx->receiver_stats,
		sizes=ctx->receiver_stats+ctx->blockplanecount;
	ctx->buf_stats.read_nonblocking(stats);
	ctx->buf_sizes.read_nonblocking(sizes);
	ctx->buf_cdata.read_nonblocking(ctx->receiver_cdata);
	ocl_sync();
	PROF(READ);
	//printf("stats:\n");//
	//print_clmem_as_ints(ctx->buf_stats);//
	//printf("sizes:\n");//
	//print_clmem_as_ints(ctx->buf_sizes);//
	//printf("cdata:\n");//
	//print_clmem_as_ints(ctx->buf_cdata);//

	int logallocbytes=ctx->logalloccount+floor_log2(sizeof(int));
	int bypass_size=(ctx->block_w*ctx->block_h+7)>>3;
	for(int k=0;k<ctx->blockplanecount;++k)
	{
		if(sizes[k]<bypass_size)
		{
			emit_pad_uninitialized(dst, dst_size, dst_cap, sizes[k]);
			memcpy(dst+dst_size, ctx->receiver_cdata+(k<<logallocbytes), sizes[k]);
			dst_size+=sizes[k];
		}
		else//bypass
		{
			pause();
			int kp=k&31, kbx=(k>>5)%ctx->block_xcount, kby=(k>>5)/ctx->block_xcount;
			int y1=ctx->block_ycount*kby, y2=y1+ctx->block_ycount,
				x1=ctx->block_xcount*kbx, x2=x1+ctx->block_xcount;
			x1*=ctx->block_w, x2*=ctx->block_w;
			y1*=ctx->block_h, y2*=ctx->block_h;
			emit_pad_uninitialized(dst, dst_size, dst_cap, bypass_size);
			auto buffer=dst+dst_size;
			for(int kb=0, ky=y1;ky<y2;++ky)
			{
				auto row=(const int*)src+ctx->iw*ky;
				if(ky<ctx->ih)
				{
					for(int kx=x1;kx<x2;++kx)
						if(kx<ctx->iw)
							buffer[kp>>3]|=(row[kx]>>kp&1)<<(kp&7);
				}
			}
			dst_size+=bypass_size;
		}
	}
	const int depth=32;
	int csizes[depth]={};//
	auto dst_stats=(unsigned short*)(dst+dst_start);
	for(int k=0;k<ctx->blockplanecount;++k)
	{
		if(sizes[k]<bypass_size)
		{
			dst_stats[k]=sizes[k]<<2|stats[k];//bytesize<<2|prob_2bit
			csizes[k&31]+=sizes[k];
		}
		else
		{
			dst_stats[k]=0x8000;//bypass
			csizes[k&31]+=bypass_size;
		}
	}
	auto t2=__rdtsc();
	PROF(PACK);
	if(loud)
	{
		int imsize=ctx->iw*ctx->ih;
		int original_bitsize=imsize*depth, compressed_bitsize=(int)(dst_size-dst_start)<<3;
		printf("AC_CL encode:  %lld cycles, %lf c/byte\n", t2-t1, (double)(t2-t1)/(original_bitsize>>3));
		printf("Size: %d -> %d bytes, ratio: %lf, %lf bpp\n", original_bitsize>>3, compressed_bitsize>>3, (double)original_bitsize/compressed_bitsize, (double)compressed_bitsize/imsize);

		printf("Bit\tsize\tratio,\tbytes/bitplane = %d\n", imsize>>3);
		for(int k=0;k<depth;++k)
			printf("%2d\t%5d\t%lf\n", k, csizes[k], (double)imsize/(csizes[k]<<3));
		
		printf("Preview:\n");
		int kprint=(int)(dst_size-dst_start);
		if(kprint>200)
			kprint=200;
		for(int k=0;k<kprint;++k)
			printf("%02X-", dst[dst_start+k]&0xFF);
		printf("\n");
	}
	return true;
}
int				abac9_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, ABAC9Context *ctx, int loud)
{
	if(!src||!dst||!ctx)
		FAIL("abac9_dec: invalid args");
	PROF(WASTE);
	auto t1=__rdtsc();

	int magic=load_int_le(src+src_idx);
	if(magic!=magic_ac09)
		FAIL("Invalid magic number 0x%08X, expected 0x%08X", magic, magic_ac09);

	int headersize=4+ctx->blockplanecount*sizeof(short);
	auto src_stats=(const unsigned short*)(src+src_idx+4);
	auto src_cdata=src+src_idx+headersize;
	auto
		stats=ctx->receiver_stats,
		sizes=ctx->receiver_stats+ctx->blockplanecount;
	int byteoffset=0;
	for(int k=0;k<ctx->blockplanecount;++k)
	{
		if(src_stats[k]==0x8000)//bypass
		{
			stats[k]=-1;
			sizes[k]=-1;
		}
		else
		{
			stats[k]=src_stats[k]&3;
			sizes[k]=byteoffset<<3;//bit_idx
			byteoffset+=src_stats[k]>>2;
		}
	}
	src_idx+=headersize+byteoffset;
	int icount=(byteoffset+3)>>2;
	PROF(PREP);
	
	CLBuffer padded_image=ctx->buf_image_p.handle?ctx->buf_image_p:ctx->buf_image0;
	ocl_call_kernel(OCL_zeromem, ctx->iw*ctx->ih, &padded_image, 1);
	ctx->buf_stats.write(stats);
	ctx->buf_cdata.write_sub(src_cdata, 0, icount);
	ctx->buf_sizes.write(sizes);
	ocl_sync();
	//printf("cdata:\n");
	//print_clmem_as_bytes(ctx->buf_cdata, icount, 0);//yes
	PROF(INITIALIZE);

	CLBuffer args[5]={};
	args[0]=padded_image;
	args[1]=ctx->buf_dim;
	args[2]=ctx->buf_stats;
	args[3]=ctx->buf_cdata;
	args[4]=ctx->buf_sizes;
	ocl_call_kernel(OCL_abac_dec2D32, ctx->blockplanecount/32, args, 5);//TODO: bypass
	ocl_sync();
	PROF(DECODE);

	if(ctx->buf_image_p.handle)
	{
		args[0]=ctx->buf_image0;
		args[1]=ctx->buf_image_p;
		args[2]=ctx->buf_dim;
		ocl_call_kernel(OCL_unpad, ctx->iw*ctx->ih, args, 3);
	}
	ctx->buf_image0.read(dst);
	PROF(READ);
	//printf("image:\n");
	//print_clmem_as_ints(ctx->buf_image);

	auto t2=__rdtsc();
	if(loud)
	{
		const int depth=32;
		int imsize=ctx->iw*ctx->ih;
		printf("AC_CL decode:  %lld cycles, %lf c/byte\n", t2-t1, (double)(t2-t1)/(imsize*depth>>3));
	}
	return true;
}

//OpenCL ANS
int				ans9_prep(int iw, int ih, int block_w, int block_h, int bytespersymbol, int encode, ANS9Context *ctx)
{
	if(!ocl_init(cl_kernels, false))
		return false;
	ctx->iw=iw;
	ctx->ih=ih;
	ctx->block_w=block_w;
	ctx->block_h=block_h;
	ctx->bytespersymbol=bytespersymbol;

	//derived attributes
	ctx->block_xcount=(iw+block_w-1)/block_w;
	ctx->block_ycount=(ih+block_h-1)/block_h;
	ctx->padded_w=block_w*ctx->block_xcount;
	ctx->padded_h=block_h*ctx->block_ycount;
	int blocksize=ctx->block_w*ctx->block_h,//must be POT
		blockcount=ctx->block_xcount*ctx->block_ycount;
	ctx->paddedcount=blocksize*blockcount;
	ctx->blockplanecount=blockcount*bytespersymbol;
	ctx->logalloccount=floor_log2(blocksize/sizeof(int)*2);//log2 of icount of buffer allocated for compressed blockplane
	ctx->alloccount=ctx->blockplanecount<<ctx->logalloccount;
	ctx->statscount=(ctx->bytespersymbol*256+1)*sizeof(SymbolInfo)/sizeof(int);
	//ctx->statscount=(bytespersymbol+1)*256*sizeof(SymbolInfo)/sizeof(int);
	
	ctx->symbolinfo=(SymbolInfo*)malloc(ctx->statscount*sizeof(int));
	ctx->receiver_sizes=(int*)malloc(ctx->blockplanecount*sizeof(int));
	if(encode)
	{
		ctx->receiver_cdata=(unsigned char*)malloc(ctx->alloccount*sizeof(int));
		ctx->CDF2sym=nullptr;
	}
	else
	{
		ctx->receiver_cdata=nullptr;
		ctx->CDF2sym=(unsigned char*)malloc((bytespersymbol+1)<<16);
	}

	//ctx->buf_CDF0.create(bytespersymbol<<8, BUFFER_READ_WRITE);
	ctx->buf_image0.create(ctx->iw*ctx->ih, BUFFER_READ_WRITE);
	//if(iw!=ctx->padded_w||ih!=ctx->padded_h)
	//	ctx->buf_image_p.create(ctx->paddedcount, BUFFER_READ_WRITE);
	//else
	//	ctx->buf_image_p.handle=nullptr;
	ctx->buf_dim.create(DIM_VAL_COUNT, BUFFER_READ_ONLY);
	ctx->buf_stats.create(ctx->statscount, BUFFER_READ_WRITE);
	ctx->buf_cdata.create(ctx->alloccount, BUFFER_READ_WRITE);
	ctx->buf_sizes.create(ctx->blockplanecount, BUFFER_READ_WRITE);
	if(encode)
		ctx->buf_CDF2sym.handle=nullptr;
	else
		ctx->buf_CDF2sym.create(((bytespersymbol+1)<<16)/sizeof(int), BUFFER_READ_WRITE);
	
	int shift=floor_log2(blocksize)-2;
	if(shift<0)
		shift=0;
	int dim[DIM_VAL_COUNT]=
	{
		ctx->iw,
		ctx->ih,
		ctx->block_xcount,
		ctx->block_ycount,
		ctx->block_w,
		ctx->block_h,
		bytespersymbol,
		shift,
		ctx->logalloccount,
		
		//0xE00,//P(0)	prep counts ones, while codec needs P(0)
		//0xA00,
		//0x600,
		//0x200,
	};
	ctx->buf_dim.write(dim);
	ocl_sync();//because src is on the stack
	return true;
}
bool			ans9_prep2(const void *hist_ptr, int bytespersymbol, SymbolInfo *info, unsigned char *CDF2sym, int loud)
{
	int tempsize=bytespersymbol*(ANS_NLEVELS*sizeof(SymbolInfo)+ANS_L);
	//info=(SymbolInfo*)malloc(tempsize);
	//if(!info)
	//	FAIL("Failed to allocate temp buffer");
	//CDF2sym=(unsigned char*)info+bytespersymbol*ANS_NLEVELS*sizeof(SymbolInfo);
	for(int kc=0;kc<bytespersymbol+1;++kc)
	{
		auto c_histogram=(const unsigned short*)hist_ptr+(kc<<ANS_DEPTH);
		auto c_info=info+(kc<<ANS_DEPTH);
		auto c_CDF2sym=CDF2sym?CDF2sym+(kc<<ANS_PROB_BITS):nullptr;
		int sum=0;
		for(int k=0;k<ANS_NLEVELS;++k)
		{
			auto &si=c_info[k];
			si.freq=kc<bytespersymbol?c_histogram[k]:0x10000/ANS_NLEVELS;
			si.cmpl_freq=~si.freq;
			si.CDF=sum;
			si.reserved0=0;

			if(si.freq<2)//0 freq: don't care, 1 freq:		//Ryg's fast rANS encoder
			{
				si.shift=0;
				si.inv_freq=0xFFFFFFFF;
				si.bias=si.CDF+ANS_L-1;
			}
			else
			{
				si.shift=ceil_log2(si.freq)-1;
				si.inv_freq=(unsigned)(((0x100000000<<si.shift)+si.freq-1)/si.freq);
				si.bias=si.CDF;
			}

			si.renorm_limit=si.freq<<(32-ANS_PROB_BITS);

			if(CDF2sym&&k)
			{
				int dstsize=si.CDF-c_info[k-1].CDF;
				if(dstsize>0)
					memset(c_CDF2sym+c_info[k-1].CDF, k-1, dstsize);

				//int src=k-1;
				//memfill(c_CDF2sym+c_info[k-1].CDF, &src, si.CDF-c_info[k-1].CDF, sizeof(char));//sic

				//for(int k2=c_info[k-1].CDF;k2<(int)si.CDF;++k2)
				//	c_CDF2sym[k2]=k-1;
			}
			if(kc>=bytespersymbol)
				break;
			sum+=si.freq;

			//si.freq=kc<<8|k;//
		}
		if(kc<bytespersymbol&&CDF2sym)
		{
			int dstsize=ANS_L-c_info[ANS_NLEVELS-1].CDF;
			if(dstsize>0)
				memset(c_CDF2sym+c_info[ANS_NLEVELS-1].CDF, ANS_NLEVELS-1, dstsize);
			//for(int k2=c_info[ANS_NLEVELS-1].CDF;k2<ANS_L;++k2)
			//	c_CDF2sym[k2]=ANS_NLEVELS-1;
		}
		if(kc<bytespersymbol&&sum!=ANS_L)
			FAIL("histogram sum = %d != %d", sum, ANS_L);
		if(loud)
		{
#ifdef ANS_PRINT_HISTOGRAM
			static int printed=0;
			if(printed<1)
			{
				printf("s\tf\tCDF\n");
				for(int k=0;k<ANS_NLEVELS;++k)
				{
					auto &si=c_info[k];
					if(c_histogram[k])
						printf("%3d\t%5d = %04X\t%04X\n", k, c_histogram[k], c_histogram[k], si.CDF);
				}
				++printed;
			}
#endif
		}
	}
	return true;
}
int				ans9_encode(const void *src, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, ANS9Context *ctx, int loud)
{
	if(!src||!dst||!ctx)
		FAIL("ans9_enc: invalid args");
	PROF(WASTE);
	auto t1=__rdtsc();
	auto dst_start=dst_size, dst_s2=dst_size;
	int sizes_offset=4+ctx->bytespersymbol*256*sizeof(short),
		headersize=sizes_offset+ctx->blockplanecount*sizeof(short);
	emit_pad_uninitialized(dst, dst_size, dst_cap, headersize);
	store_int_le(dst, dst_s2, magic_an09);
	auto buffer=(const unsigned char*)src;
	for(int kc=0;kc<ctx->bytespersymbol;++kc)
		if(!ans_calc_histogram(buffer+kc, ctx->iw*ctx->ih, ctx->bytespersymbol, (unsigned short*)(dst+dst_s2+kc*(ANS_NLEVELS*sizeof(short))), 16, false))
			return false;
	PROF(HISTOGRAM);
	if(!ans9_prep2(dst+dst_s2, ctx->bytespersymbol, ctx->symbolinfo, ctx->CDF2sym, loud))
		return false;
	//ctx->buf_CDF0.write(dst+dst_s2);
	//ocl_sync();
	dst_s2=dst_size+sizes_offset;
	dst_size+=headersize;
	PROF(PREP);
	
	CLBuffer args[5]={};
	//args[0]=ctx->buf_CDF0;
	//args[1]=ctx->buf_dim;
	//args[2]=ctx->buf_stats;
	//ocl_call_kernel(OCL_ans_enc_prep2, ctx->bytespersymbol<<8, args, 3);
	ctx->buf_image0.write(src);

	//if(ctx->buf_image_p.handle)
	//{
	//	ocl_sync();
	//	PROF(SEND_FRAME);
	//	args[0]=ctx->buf_image0;
	//	args[1]=ctx->buf_image_p;
	//	args[2]=ctx->buf_dim;
	//	ocl_call_kernel(OCL_pad_const, ctx->padded_w*ctx->padded_h, args, 3);
	//	padded_image=ctx->buf_image_p;
	//}
	//else
	//	padded_image=ctx->buf_image0;
	//ctx->buf_stats.write_sub(ctx->symbolinfo, 0, (ctx->bytespersymbol*256+1)*sizeof(SymbolInfo)/sizeof(int));
	ctx->buf_stats.write(ctx->symbolinfo);
	//ocl_call_kernel(OCL_zeromem, ctx->alloccount, &ctx->buf_cdata, 1);
	ocl_sync();
	PROF(INITIALIZE_DATA);
	//printf("GPU image:\n");//
	//print_clmem_as_ints(ctx->buf_image);//
	//printf("GPU dim:\n");//
	//print_clmem_as_ints(ctx->buf_dim);//
	//printf("GPU dim:\n");//
	//print_clmem_as_ints(ctx->buf_stats);//

	args[0]=ctx->buf_image0;
	args[1]=ctx->buf_dim;
	args[2]=ctx->buf_stats;
	args[3]=ctx->buf_cdata;
	args[4]=ctx->buf_sizes;
	ocl_call_kernel(OCL_ans_enc2D32, ctx->blockplanecount, args, 5);
	ocl_sync();
	PROF(ENCODE);
	ctx->buf_sizes.read_nonblocking(ctx->receiver_sizes);
	ctx->buf_cdata.read_nonblocking(ctx->receiver_cdata);
	ocl_sync();
	PROF(READ);
	//printf("stats:\n");//
	//print_clmem_as_ints(ctx->buf_stats);//
	//printf("sizes:\n");//
	//print_clmem_as_ints(ctx->buf_sizes);//
	//printf("cdata:\n");//
	//print_clmem_as_ints(ctx->buf_cdata);//

	int logallocbytes=ctx->logalloccount+floor_log2(sizeof(int));
	//int bypass_size=ctx->block_w*ctx->block_h;
	int bypass_count=0;
	for(int k=0;k<ctx->blockplanecount;++k)//pack
	{
		bypass_count+=ctx->receiver_sizes[k]<0;
		int bytesize=abs(ctx->receiver_sizes[k])*sizeof(short);
		emit_pad_uninitialized(dst, dst_size, dst_cap, bytesize);
		memcpy(dst+dst_size, ctx->receiver_cdata+(k<<logallocbytes), bytesize);
		dst_size+=bytesize;
	/*	if(bytesize<bypass_size)
		{
			emit_pad_uninitialized(dst, dst_size, dst_cap, bytesize);
			memcpy(dst+dst_size, ctx->receiver_cdata+(k<<logallocbytes), bytesize);
			dst_size+=bytesize;
		}
		else//bypass on CPU?
		{
			++bypass_count;
			int kp=k&31, kbx=(k>>5)%ctx->block_xcount, kby=(k>>5)/ctx->block_xcount;
			int y1=ctx->block_ycount*kby, y2=y1+ctx->block_ycount,
				x1=ctx->block_xcount*kbx, x2=x1+ctx->block_xcount;
			x1*=ctx->block_w, x2*=ctx->block_w;
			y1*=ctx->block_h, y2*=ctx->block_h;
			emit_pad_uninitialized(dst, dst_size, dst_cap, bypass_size);
			auto buffer=dst+dst_size;
			for(int kb=0, ky=y1;ky<y2;++ky)
			{
				auto row=(const int*)src+ctx->iw*ky;
				if(ky<ctx->ih)
				{
					for(int kx=x1;kx<x2;++kx)
						if(kx<ctx->iw)
							buffer[kp>>3]|=(row[kx]>>kp&1)<<(kp&7);
				}
			}
			dst_size+=bypass_size;
		}//*/
	}
	int csizes[16]={};//bytespersymbol <= 16
	auto dst_sizes=(unsigned short*)(dst+dst_s2);
	for(int k=0;k<ctx->blockplanecount;++k)
	{
		dst_sizes[k]=ctx->receiver_sizes[k];//short-count, -ve means bypass statistics were used
		csizes[k%ctx->bytespersymbol]+=abs(ctx->receiver_sizes[k])*sizeof(short);
	}
	auto t2=__rdtsc();
	PROF(PACK);
	if(loud)
	{
		int imsize=ctx->iw*ctx->ih;
		int original_bitsize=imsize*ctx->bytespersymbol<<3, compressed_bitsize=(int)(dst_size-dst_start)<<3;
		printf("ANS_CL encode:  %lld cycles, %lf c/byte\n", t2-t1, (double)(t2-t1)/(original_bitsize>>3));
		printf("Size: %d -> %d bytes, ratio: %lf, %lf bpp, bypass %d/%d\n", original_bitsize>>3, compressed_bitsize>>3, (double)original_bitsize/compressed_bitsize, (double)compressed_bitsize/imsize, bypass_count, ctx->blockplanecount);

		printf("Ch\tsize\tratio,\tbytes/channel = %d\n", imsize);
		for(int k=0;k<ctx->bytespersymbol;++k)
			printf("%2d\t%5d\t%lf\n", k, csizes[k], (double)imsize/csizes[k]);
		
		printf("Preview:\n");
		int kprint=compressed_bitsize>>3;
		//const int bytelimit=500;
		//if(kprint>bytelimit)
		//	kprint=bytelimit;
		for(int k=0;k<kprint;++k)
			printf("%02X-", dst[dst_start+k]&0xFF);
		printf("\n");
	}
	return true;
}
int				ans9_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, ANS9Context *ctx, int loud)
{
	if(!src||!dst||!ctx)
		FAIL("ans9_dec: invalid args");
	PROF(WASTE);
	auto t1=__rdtsc();

	int magic=load_int_le(src+src_idx);
	if(magic!=magic_an09)
		FAIL("Invalid magic number 0x%08X, expected 0x%08X", magic, magic_ac09);
	
	if(!ans9_prep2((const unsigned short*)(src+src_idx+4), ctx->bytespersymbol, ctx->symbolinfo, ctx->CDF2sym, loud))
		return false;
	int sizes_offset=4+ctx->bytespersymbol*256*sizeof(short),
		headersize=sizes_offset+ctx->blockplanecount*sizeof(short);
	auto src_sizes=(const short*)(src+src_idx+sizes_offset);
	auto src_cdata=src+src_idx+headersize;
	
	//ctx->buf_CDF0.write(src+src_idx+4);
	//ocl_sync();
	//int bypass_size=ctx->block_w*ctx->block_h;
	int byteoffset=0;
	for(int k=0;k<ctx->blockplanecount;++k)
	{
		int bytesize=abs(src_sizes[k])*sizeof(short);
		byteoffset+=bytesize;
		if(src_sizes[k]<0)//bypass
			ctx->receiver_sizes[k]=-(byteoffset>>1);
		else
			ctx->receiver_sizes[k]=byteoffset>>1;//points at the end of range
	}
	src_idx+=headersize+byteoffset;
	int icount=(byteoffset+3)>>2;
	PROF(PREP);
	
	CLBuffer args[6]={};
	//args[0]=ctx->buf_CDF0;
	//args[1]=ctx->buf_dim;
	//args[2]=ctx->buf_stats;
	//args[3]=ctx->buf_CDF2sym;
	//ocl_call_kernel(OCL_ans_dec_prep2, ctx->bytespersymbol<<8, args, 4);

	//ctx->buf_stats.write_sub(ctx->symbolinfo, 0, (ctx->bytespersymbol*256+1)*sizeof(SymbolInfo)/sizeof(int));
	ctx->buf_stats.write(ctx->symbolinfo);
	ocl_sync();
	PROF(INITIALIZE_STATS);
	ctx->buf_cdata.write_sub(src_cdata, 0, icount);
	ocl_sync();
	PROF(INITIALIZE_DATA);
	ctx->buf_sizes.write(ctx->receiver_sizes);
	ocl_sync();
	PROF(INITIALIZE_SIZES);
	ctx->buf_CDF2sym.write(ctx->CDF2sym);
	ocl_sync();
	//printf("cdata:\n");
	//print_clmem_as_bytes(ctx->buf_cdata, icount, 0);//yes
	PROF(INITIALIZE_ICDF);

	args[0]=ctx->buf_image0;
	args[1]=ctx->buf_dim;
	args[2]=ctx->buf_stats;
	args[3]=ctx->buf_cdata;
	args[4]=ctx->buf_sizes;
	args[5]=ctx->buf_CDF2sym;
	ocl_call_kernel(OCL_ans_dec2D32, ctx->block_xcount*ctx->block_ycount*ctx->bytespersymbol, args, 6);
	ocl_sync();
	PROF(DECODE);

#ifndef ANS_CL_EMUATE_RENDER2GLTEXTURE
	ctx->buf_image0.read(dst);
#endif
	PROF(READ);
	//printf("image:\n");
	//print_clmem_as_ints(ctx->buf_image);

	auto t2=__rdtsc();
	//prof_cycles[PROF_DELTA]=0;//
	prof_end();//
	PROF_INIT();
	if(loud)
	{
		int imsize=ctx->iw*ctx->ih;
		printf("ANS_CL decode:  %lld cycles, %lf c/byte\n", t2-t1, (double)(t2-t1)/(imsize*ctx->bytespersymbol));
	}
	return true;
}