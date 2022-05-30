//libsvc_internal.h - The internal libsvc include
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

#pragma once
#ifndef LIBSVC_INTERNAL_H
#define LIBSVC_INTERNAL_H
#include		"libsvc.h"
#include		"OpenCL_wrap.h"
#include		"profiler.h"
#include		<string.h>
#include		<vector>
#include		<string>

	#define		NO_OPENCL
	#define		NO_AVX2

#ifndef NO_OPENCL
	#define		ANS_CL
//	#define		ABAC_CL
#endif
#ifndef NO_AVX2
//	#define		ANS_AVX2//TODO
#endif
//	#define		ANS_SSE2//TODO
//	#define		ANS_64BIT
//	#define		ANS_32BIT
//otherwise ABAC is used

//	#define		SUBTRACT_PREV_FRAME

	#define		DEBUG_PRINT

//utility
#ifdef __linux__
#define sprintf_s snprintf
#define scanf_s scanf
#define _HUGE HUGE_VAL
typedef unsigned char byte;
#endif
#define			SIZEOF(STATIC_ARRAY)	(sizeof(STATIC_ARRAY)/sizeof(*(STATIC_ARRAY)))
#define			G_BUF_SIZE	4096
extern char		g_buf[G_BUF_SIZE];
#if 0
double			time_sec();
#endif
static void		memfill(void *dst, const void *src, size_t dstbytes, size_t srcbytes)
{
	size_t copied;
	char *d=(char*)dst;
	const char *s=(const char*)src;
	if(dstbytes<srcbytes)
	{
		memcpy(dst, src, dstbytes);
		return;
	}
	copied=srcbytes;
	memcpy(d, s, copied);
	while(copied<<1<=dstbytes)
	{
		memcpy(d+copied, d, copied);
		copied<<=1;
	}
	if(copied<dstbytes)
		memcpy(d+copied, d, dstbytes-copied);
}

//math
#if 0
long long		maximum(long long a, long long b);
long long		minimum(long long a, long long b);
long long		mod(long long x, long long n);
int				first_set_bit(unsigned long long n);
int				first_set_bit16(unsigned short n);//idx of LSB
int				floor_log2(unsigned long long n);//idx of MSB
int				ceil_log2(unsigned long long n);
int				floor_log10(double x);
double			power(double x, int y);
double			_10pow(int n);
#endif


//SVC
void			apply_intDCT_8x8x8_8bit(const unsigned char *src, unsigned short *dst, int iw, int ih, int stride);

struct			SymbolInfo
{
	unsigned short
		freq,//quantized
		cmpl_freq,
		shift,
		reserved0;
	unsigned
		CDF,
		inv_freq,
		bias,
		renorm_limit;
};
#ifndef NO_OPENCL
enum			DimInfo
{
	DIM_W0,
	DIM_H0,
	DIM_BLOCK_XCOUNT,
	DIM_BLOCK_YCOUNT,
	DIM_BLOCK_W,
	DIM_BLOCK_H,
	DIM_BYTESPERSYMBOL,
	DIM_PROB_SHIFT,
	DIM_LOGALLOCCOUNT,

	DIM_PROB_VAL,//these must be last
	DIM_PROB_V1,
	DIM_PROB_V2,
	DIM_PROB_V3,

	DIM_VAL_COUNT,
};

//OpenCL ABAC - 32-bit
//const int		abac9_block_w=8, abac9_block_h=8;	//308.87 MB/s	3127.08 MB/s	6994.14 MB/s	10.638579
//const int		abac9_block_w=16, abac9_block_h=16;	//332.08 MB/s	3547.39 MB/s	9523.92 MB/s	41.913559
//const int		abac9_block_w=32, abac9_block_h=32;	//302.60 MB/s	3031.44 MB/s	6364.22 MB/s	162.593849
const int		abac9_block_w=64, abac9_block_h=64;	//274.04 MB/s	1629.74 MB/s	2307.20 MB/s	580.312041
struct			ABAC9Context//32bit depth
{
	int iw, ih,//original image dimensions
		block_w,//must be POT
		block_h,//must be POT

		block_xcount,	//number of blocks horizontally - derived: ceil(iw/block_w)
		block_ycount,	//number of blocks vertically - derived: ceil(ih/block_h)
		padded_w,		//derived: block_w*block_xcount
		padded_h,		//derived: block_h*block_ycount
		paddedcount,	//padded image pixel count - derived: blockw*blockh*blockcountx*blockcounty
		blockplanecount,//number of blockplanes - derived: blockcountx*blockcounty*depth
		alloccount,		//size of allocated buffer for compressed data, in machine 32bit integers - derived: block_w*block_h*allowance/32 - should be POT
		logalloccount;	//log2 size of ONE such allocated block, in machine 32bit integers
	int *receiver_stats;
	unsigned char *receiver_cdata;
	CLBuffer
		buf_image0,
		buf_image_p,//if image_p==nullptr: use image0 as it doesn't need padding
		buf_dim,
		buf_stats,
		buf_cdata,
		buf_sizes;
};
int				abac9_prep(int iw, int ih, int block_w, int block_h, int encode, ABAC9Context *ctx);
int				abac9_finish(ABAC9Context *ctx);
int				abac9_encode(const void *src, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, ABAC9Context *ctx, int loud);
int				abac9_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, ABAC9Context *ctx, int loud);

//OpenCL ANS
struct			ANS9Context//32bit depth
{
	int iw, ih,//original image dimensions
		block_w,//must be POT
		block_h,//must be POT

		block_xcount,	//number of blocks horizontally - derived: ceil(iw/block_w)
		block_ycount,	//number of blocks vertically - derived: ceil(ih/block_h)
		padded_w,		//derived: block_w*block_xcount
		padded_h,		//derived: block_h*block_ycount
		paddedcount,	//padded image pixel count - derived: blockw*blockh*blockcountx*blockcounty
		blockplanecount,//number of blockplanes - derived: blockcountx*blockcounty*depth
		alloccount,		//size of allocated buffer for compressed data, in machine 32bit integers - derived: block_w*block_h*allowance/32 - should be POT
		logalloccount,	//log2 size of ONE such allocated block, in machine 32bit integers

		bytespersymbol,
		statscount;

	SymbolInfo *symbolinfo;
	unsigned char *receiver_cdata;
	int *receiver_sizes;
	unsigned char *CDF2sym;

	CLBuffer
	//	buf_CDF0,
		buf_image0,
	//	buf_image_p,//if image_p==nullptr: use image0 as it doesn't need padding
		buf_dim,
		buf_stats,
		buf_cdata,
		buf_sizes,
		buf_CDF2sym;
};
int				ans9_prep(int iw, int ih, int block_w, int block_h, int bytespersymbol, int encode, ANS9Context *ctx);
int				ans9_encode(const void *src, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, ANS9Context *ctx, int loud);
int				ans9_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, ANS9Context *ctx, int loud);
#endif

bool			set_error(const char *file, int line, const char *format, ...);
#define			ERROR(MESSAGE, ...)		set_error(__FILE__, __LINE__, MESSAGE, ##__VA_ARGS__)
const int		magic_sv01='S'|'V'<<8|'0'<<16|'1'<<24;
struct			SVCContextStruct
{
	int w, h;
	short nchannels, depth;
	int nframes;
	short fps_num, fps_den;
	char bayer[4];
	SrcType srctype;

	int frame_res, frame_samples, frame_bytesize;
	void *prev_frame;
#ifdef ANS_CL
	ANS9Context ans9_ctx;
#elif defined ABAC_CL
	ABAC9Context abac9_ctx;
#endif
	
	int frame_idx;
	const unsigned char *in_data;
	unsigned long long in_idx, in_size;

	unsigned char *out_data;
	unsigned long long out_size, out_cap;

	SVCContextStruct():
		w(0), h(0), nchannels(0), depth(0), nframes(0), fps_num(0), fps_den(0), srctype(SVC_UINT8),
		frame_res(0), frame_samples(0), frame_bytesize(0),
		prev_frame(nullptr),
		frame_idx(0), in_data(nullptr), in_idx(0), in_size(0),
		out_data(nullptr), out_size(0), out_cap(0)
	{}
	SVCContextStruct(int w, int h, short nchannels, short depth, SrcType srctype, short fps_num, short fps_den):
		w(w), h(h), nchannels(nchannels), depth(depth), nframes(0), fps_num(fps_num), fps_den(fps_den), srctype(srctype),
		frame_res(0), frame_samples(0), frame_bytesize(0),
		prev_frame(nullptr),
		frame_idx(0), in_data(nullptr), in_idx(0), in_size(0),
		out_data(nullptr), out_size(0), out_cap(0)
	{
		out_cap=sizeof(SVCHeader);
		out_data=(unsigned char*)malloc(out_cap);
		if(!out_data)
		{
			ERROR("Failed to allocate output buffer");
			return;
		}
		SVCHeader header=
		{
			magic_sv01,
			w, h,
			nchannels, depth,
			nframes,
			fps_num, fps_den,
			{0, 0, 0, 0},
			srctype,
		};
		memcpy(out_data, &header, sizeof(SVCHeader));
		out_size=out_cap;
		init_frame(true);
	}
	SVCContextStruct(const unsigned char *in_data, unsigned long long bytesize):
		w(0), h(0), nchannels(0), depth(0), nframes(0), fps_num(0), fps_den(0), srctype(SVC_UINT8),
		frame_res(0), frame_samples(0), frame_bytesize(0),
		prev_frame(nullptr),
		frame_idx(0), in_data(in_data), in_idx(0), in_size(bytesize),
		out_data(nullptr), out_size(0), out_cap(0)
	{
		if(bytesize<sizeof(SVCHeader))
		{
			ERROR("Bytesize = %lld < sizeof SVCHeader", bytesize);
			return;
		}
		auto header=(SVCHeader*)in_data;
		if(header->magic!=magic_sv01)
		{
			ERROR("Invalid magic number = 0x%08X. Expected 0x%08X", header->magic, magic_sv01);
			return;
		}
		w			=header->w;
		h			=header->h;
		nchannels	=header->nchannels;
		depth		=header->depth;
		nframes		=header->nframes;
		fps_num		=header->fps_num;
		fps_den		=header->fps_den;
		srctype		=header->srctype;
		init_frame(false);
	}
	void init_frame(int encode)
	{
		frame_res=w*h;
		frame_samples=frame_res*nchannels;
		switch(srctype)
		{
		case 0:
			frame_bytesize=frame_samples;
			break;
		case 1:
			frame_bytesize=frame_samples<<1;
			break;
		case 2:
			frame_bytesize=frame_samples<<2;
			break;
		}
		prev_frame=malloc(frame_bytesize);
		if(!prev_frame)
			ERROR("Failed to allocate frame buffer");
#ifdef ANS_CL
		int sample_size=1<<srctype;
		int block_w=abac9_block_w, block_h=abac9_block_h;
		if(block_w>w)
			block_w=w;
		if(block_h>h)
			block_h=h;
		ans9_prep(w, h, block_w, block_h, sample_size*nchannels, encode, &ans9_ctx);
#elif defined ABAC_CL
		int block_w=abac9_block_w, block_h=abac9_block_h;
		if(block_w>w)
			block_w=w;
		if(block_h>h)
			block_h=h;
		abac9_prep(w, h, block_w, block_h, encode, &abac9_ctx);
#endif
	}
};

//entropy coders

//ABAC (slow, broken)
int				abac4_encode(const void *src, int imsize, int depth, int bytestride, unsigned char *&out_data, unsigned long long &out_size, unsigned long long &out_cap, int loud);
int				abac4_decode(const unsigned char *in_data, unsigned long long &in_idx, unsigned long long in_size, void *dst, int imsize, int depth, int bytestride, int loud);//dst must be initialized to zero, so that channels can be interleaved

//32bit ANS (div-free version is broken)
int				rans4_encode(const void *src, int nsymbols, int bytespersymbol, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, int loud);//bytespersymbol: up to 16
int				rans4_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, int nsymbols, int bytespersymbol, int loud, const void *guide);

//64bit ANS (slow, broken)
int				rans6_encode(const void *src, int nsymbols, int bytespersymbol, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, int loud);
int				rans6_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, int nsymbols, int bytespersymbol, int loud, const void *guide);

//SSE2 rANS (TODO)
int				rans7_encode(const void *src, int nsymbols, int bytespersymbol, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, int loud);
int				rans7_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, int nsymbols, int bytespersymbol, int loud, const void *guide);

//AVX2 rANS (TODO)
int				rans8_encode(const void *src, int nsymbols, int bytespersymbol, unsigned char *&dst, unsigned long long &dst_size, unsigned long long &dst_cap, int loud);
int				rans8_decode(const unsigned char *src, unsigned long long &src_idx, unsigned long long src_size, void *dst, int nsymbols, int bytespersymbol, int loud, const void *guide);

#endif