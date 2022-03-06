//cl_kernels.h - The OpenCL kernels, implementing entropy coders on the GPU
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

#ifndef __OPEN_CL__
#include<math.h>
#include<stdio.h>
#define __kernel
#define __global
#define __constant
#define get_global_id(...)	(__VA_ARGS__)
#define max(...)			(__VA_ARGS__)
#define clamp(...)			(__VA_ARGS__)
#endif

//	#define		ANS_PRINT_WRITES	idx==0
//	#define		ANS_PRINT_READS		idx==0&&kc==0

//	#define		PRINT_BOUNDS
//	#define		PRINT_ENC_STATE		idx==0
//	#define		PRINT_EMIT			idx==0
//	#define		PRINT_DEC_STATE		idx==0&&kp==0
//	#define		PRINT_READS			idx==0&&kp==0

typedef enum
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
} DimInfo;
__kernel void zeromem		(__global int *dst)
{
	int idx=get_global_id(0);
	dst[idx]=0;
}

//worksize = padded_w*padded_h
__kernel void pad_const		(__global int *src, __global int *dst, __constant int *dim)
{
	int idx=get_global_id(0);
	int iw=dim[DIM_W0],
		ih=dim[DIM_H0],
		block_xcount=dim[DIM_BLOCK_XCOUNT],
		block_w=dim[DIM_BLOCK_W],
		padded_w=block_xcount*block_w;
	int kx=idx%padded_w, ky=idx/padded_w;
	if(kx>=iw)
		kx=iw-1;
	if(ky>=ih)
		ky=ih-1;
	dst[idx]=src[iw*ky+kx];
}

//worksize = iw*ih
__kernel void unpad			(__global int *dst, __global int *src, __constant int *dim)
{
	int idx=get_global_id(0);
	int iw=dim[DIM_W0],
		ih=dim[DIM_H0],
		block_xcount=dim[DIM_BLOCK_XCOUNT],
		block_w=dim[DIM_BLOCK_W],
		padded_w=block_xcount*block_w;
	int kx=idx%iw, ky=idx/iw;
	dst[idx]=src[padded_w*ky+kx];
}


//OpenCL ABAC
#define		ABAC_PROB_BITS	12
#define		ABAC_PROB_HALF	(1<<(ABAC_PROB_BITS-1))
#define		ABAC_STATE_BITS	16
#define		ABAC_STATE_ONES	((1<<ABAC_STATE_BITS)-1)
#define		ABAC_EMIT_LIMIT	0x100

//worksize = block_ycount*block_xcount = blockplanecount/32
//first call zeromem on stats
__kernel void abac_enc2D32_prep(__global int *src, __constant int *dim, __global int *out_stats)
{
	int idx=get_global_id(0);//block number
	//if(!idx)
	//{
	//	for(int k=0;k<DIM_VAL_COUNT;++k)
	//		printf("dim[%d]: %d", k, dim[k]);
	//}
	int block_xcount=dim[DIM_BLOCK_XCOUNT],
		block_ycount=dim[DIM_BLOCK_YCOUNT],
		block_w=dim[DIM_BLOCK_W],
		block_h=dim[DIM_BLOCK_H],
		blocksize=block_w*block_h, nblocks=block_xcount*block_ycount, iw=block_xcount*block_w;
	int kbx=idx%block_xcount, kby=idx/block_xcount;
	int y1=block_w*kby, y2=y1+block_w,
		x1=block_h*kbx, x2=x1+block_h;
	
#ifdef PRINT_BOUNDS
	printf("prep %d (%d,%d)->(%d,%d)", idx, x1, y1, x2, y2);//
#endif
	__global int *stats=out_stats+((block_xcount*kby+kbx)<<5);
	//printf("(%d, %d)->(%d, %d)", x1, y1, x2, y2);
	for(int ky=y1;ky<y2;++ky)
	{
		int yoffset=iw*ky;
		for(int kx=x1;kx<x2;++kx)
		{
			int sample=src[yoffset+kx];
			int s0=sample;//
			for(int kp=0;kp<32;++kp)
			{
				stats[kp]+=sample&1;
				sample>>=1;
			}
			//if(!idx)
			//	printf("sample 0x%08X, bit 31: %d", s0, stats[31]);
		}
	}
	for(int kp=0;kp<32;++kp)
	{
		int s0=stats[kp];//
		stats[kp]-=stats[kp]==blocksize;
		stats[kp]>>=dim[DIM_PROB_SHIFT];
		//stats[kp]^=3;
		//if(!idx)//
		//	printf("kp %d: p0=%d->%d", kp, s0, stats[kp]);//
	}
}

//worksize = block_ycount*block_xcount*32 = blockplanecount
//first call zeromem on dst
//need to join output buffers on CPU afterwards
__kernel void abac_enc2D32(__global int *src, __constant int *dim, __global int *in_stats, __global int *dst, __global int *sizes)
{
	int block_xcount=dim[DIM_BLOCK_XCOUNT],
		block_ycount=dim[DIM_BLOCK_YCOUNT],
		block_w=dim[DIM_BLOCK_W],
		block_h=dim[DIM_BLOCK_H],
		nblocks=block_xcount*block_ycount, iw=block_xcount*block_w;
	int idx=get_global_id(0);
	int kp=idx&31, kbx=(idx>>5)%block_xcount, kby=(idx>>5)/block_xcount;
	int y1=block_w*kby, y2=y1+block_w,
		x1=block_h*kbx, x2=x1+block_h;

	int bp_idx=(block_xcount*kby+kbx)<<5|kp;
	__global int *cdata=dst+(bp_idx<<dim[DIM_LOGALLOCCOUNT]);
	int bit_idx=0, bitsizelimit=1<<(dim[DIM_LOGALLOCCOUNT]+5);
#define EMIT_BYTE(BYTE)		if(bit_idx<bitsizelimit)cdata[bit_idx>>5]|=(BYTE)<<(bit_idx&31); bit_idx+=8

#ifdef PRINT_BOUNDS
	printf("enc %d (%d,%d)->(%d,%d)", idx, x1, y1, x2, y2);//
#endif
	int prob=dim[DIM_PROB_VAL+in_stats[bp_idx]];//12-bit prob/memory
	int prob_correct=ABAC_PROB_HALF;
	unsigned short start=0, range=ABAC_STATE_ONES;//16-bit state
	for(int ky=y1;ky<y2;++ky)
	{
		int yoffset=iw*ky;
		for(int kx=x1;kx<x2;++kx)
		{
			int bit=src[yoffset+kx]>>kp&1;
			if(range<3)
			{
				EMIT_BYTE(start>>8&0xFF);//big endian
				EMIT_BYTE(start&0xFF);
				start=0, range=ABAC_STATE_ONES;//because 1=0.9999...
			}
			int p0=prob-ABAC_PROB_HALF;
			p0=p0*prob_correct>>ABAC_PROB_BITS;
			p0=p0*prob_correct>>ABAC_PROB_BITS;
			p0+=ABAC_PROB_HALF;
			p0=clamp(p0, 1, 0xFFE);
			unsigned r2=(unsigned)range*p0>>ABAC_PROB_BITS;
			r2+=(r2==0)-(r2==range);
#ifdef PRINT_ENC_STATE
			if(PRINT_ENC_STATE)
				printf("kp %d (%d,%d) prob %03X p0 %03X %04X+%04X mid %04X", kp, kx, ky, prob, p0, start, range, r2);
#endif

			int correct=bit^(p0>=ABAC_PROB_HALF);
			prob=!bit<<(ABAC_PROB_BITS-1)|prob>>1;
			prob_correct=correct<<(ABAC_PROB_BITS-1)|prob_correct>>1;

			if(bit)//update state
			{
				++r2;
				start+=r2, range-=r2;
			}
			else
				range=r2-1;

			while((start^(start+range))<ABAC_EMIT_LIMIT)//most significant byte has stabilized			zpaq 1.10
			{
				EMIT_BYTE(start>>8);
#ifdef PRINT_EMIT
				if(PRINT_EMIT)//
					printf("kp %d (%d,%d) emit %02X start %04X->%04X idx %d", kp, kx, ky, start>>8, start, start<<8, bit_idx);//
#endif
				start<<=8;
				range=range<<8|0xFF;
			}
		}
	}
	EMIT_BYTE(start>>8&0xFF);//big endian
	EMIT_BYTE(start&0xFF);
#ifdef PRINT_EMIT
	if(PRINT_EMIT)
		printf("kp %d finish %04X idx %d", kp, start, bit_idx);
#endif
#undef EMIT_BYTE
	//printf("kp %d: bix_idx %d start %04X", kp, bit_idx, start);//
	sizes[bp_idx]=bit_idx>>3;
}
typedef struct
{
	unsigned short
		prob,
		prob_correct,
		code,//16-bit state
		start,
		range;
	unsigned bit_idx;
} ABACDecContext;

//worksize = block_ycount*block_xcount = blockplanecount/32
//first call zeromem on dst
__kernel void abac_dec2D32(__global int *dst, __constant int *dim, __global int *in_stats, __global int *src, __global int *indices)
{
	int block_xcount=dim[DIM_BLOCK_XCOUNT],
		block_ycount=dim[DIM_BLOCK_YCOUNT],
		block_w=dim[DIM_BLOCK_W],
		block_h=dim[DIM_BLOCK_H],
		nblocks=block_xcount*block_ycount, iw=block_xcount*block_w;
	int idx=get_global_id(0);
	int kbx=idx%block_xcount, kby=idx/block_xcount;
	int y1=block_w*kby, y2=y1+block_w,
		x1=block_h*kbx, x2=x1+block_h;
	//if(idx==0)//
	//{
	//	__global unsigned char *buffer=(__global unsigned char*)dst;
	//	for(int k=0;k<20;++k)//
	//	{
	//		int k2=k<<2;
	//		printf("[%d] 0x%08X  %02X-%02X-%02X-%02X", k, dst[k2], buffer[k2], buffer[k2|1], buffer[k2|2], buffer[k2|3]);
	//	}
	//}
	
	int bp_idx=(block_xcount*kby+kbx)<<5;
	ABACDecContext ctx[32];
#ifdef PRINT_READS
#define READ_BYTE()		ctx[kp].code=ctx[kp].code<<8|(src[ctx[kp].bit_idx>>5]>>(ctx[kp].bit_idx&31)&0xFF), ctx[kp].bit_idx+=8;	\
	if(PRINT_READS)	\
	{	\
		int bit_idx=ctx[kp].bit_idx-8;	\
		printf("read %04X idx %d src[%d]>>%d", ctx[kp].code, ctx[kp].bit_idx, bit_idx>>5, bit_idx&31);	\
	}
#else
#define READ_BYTE()		ctx[kp].code=ctx[kp].code<<8|(src[ctx[kp].bit_idx>>5]>>(ctx[kp].bit_idx&31)&0xFF), ctx[kp].bit_idx+=8
#endif
	for(int kp=0;kp<32;++kp)
	{
		ctx[kp].prob=dim[DIM_PROB_VAL+in_stats[bp_idx|kp]];//12-bit prob/memory
		ctx[kp].prob_correct=ABAC_PROB_HALF;
		ctx[kp].code=0;
		ctx[kp].start=0;
		ctx[kp].range=ABAC_STATE_ONES;
		ctx[kp].bit_idx=indices[bp_idx|kp];
		READ_BYTE();
		READ_BYTE();
	}
	for(int kp=0;kp<32;++kp)
	{
		if(ctx[kp].bit_idx!=-1)//TODO: decode bypass
		{
			for(int ky=y1;ky<y2;++ky)
			{
				int yoffset=iw*ky;
				for(int kx=x1;kx<x2;++kx)
				{
					if(ctx[kp].range<3)
					{
						READ_BYTE();
						READ_BYTE();
						ctx[kp].start=0, ctx[kp].range=ABAC_STATE_ONES;//because 1=0.9999...
					}
					int p0=ctx[kp].prob-ABAC_PROB_HALF;
					p0=p0*ctx[kp].prob_correct>>ABAC_PROB_BITS;
					p0=p0*ctx[kp].prob_correct>>ABAC_PROB_BITS;
					p0+=ABAC_PROB_HALF;
					p0=clamp(p0, 1, 0xFFE);
					unsigned short r2=(unsigned)ctx[kp].range*p0>>ABAC_PROB_BITS;
					r2+=(r2==0)-(r2==ctx[kp].range);
					unsigned middle=ctx[kp].start+r2;
					int bit=ctx[kp].code>middle;
#ifdef PRINT_DEC_STATE
					if(PRINT_DEC_STATE)
						printf("kp %d (%d,%d) prob %03X p0 %03X %04X+%04X mid %04X code %04X b %d", kp, kx, ky, (int)ctx[kp].prob, p0, (int)ctx[kp].start, (int)ctx[kp].range, (int)r2, ctx[kp].code, bit);
#endif
			
					int correct=bit^(p0>=ABAC_PROB_HALF);
					ctx[kp].prob=!bit<<(ABAC_PROB_BITS-1)|ctx[kp].prob>>1;
					ctx[kp].prob_correct=correct<<(ABAC_PROB_BITS-1)|ctx[kp].prob_correct>>1;
					if(bit)
					{
						++r2;
						ctx[kp].start+=r2, ctx[kp].range-=r2;
					}
					else
						ctx[kp].range=r2-1;
					dst[yoffset+kx]|=bit<<kp;
					while((ctx[kp].start^(ctx[kp].start+ctx[kp].range))<ABAC_EMIT_LIMIT)//shift-out identical bytes			zpaq 1.10
					{
						READ_BYTE();
						ctx[kp].start<<=8;
						ctx[kp].range=(ctx[kp].range<<8|0xFF)&ABAC_STATE_ONES;
					}
				}
			}
		}
	}
#undef READ_BYTE
}


//OpenCL ANS

typedef struct
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
} SymbolInfo;

//worksize = bytespersymbol*256
//__kernel void ans_calc_histogram(__global unsigned char *src, __global int *histogram)
//{
//}

//worksize = bytespersymbol*256
__kernel void ans_enc_prep2(__global unsigned short *CDF, __constant int *dim, __global SymbolInfo *out_stats)
{
	int iw=dim[DIM_W0],
		ih=dim[DIM_H0],
		block_xcount=dim[DIM_BLOCK_XCOUNT],
		block_ycount=dim[DIM_BLOCK_YCOUNT],
		block_w=dim[DIM_BLOCK_W],
		block_h=dim[DIM_BLOCK_H],
		bytespersymbol=dim[DIM_BYTESPERSYMBOL];
	int idx=get_global_id(0);
	int symbol=idx&255, kc=idx>>8;
	__global SymbolInfo *info=out_stats+idx;
	int prev_CDF=0;
	if(symbol>0)
		prev_CDF=CDF[idx-1];
	info->CDF=CDF[idx];
	info->freq=info->CDF-prev_CDF;
	info->cmpl_freq=~info->freq;
	info->shift=0;//
	info->reserved0=0;
	info->inv_freq=0;//
	info->bias=0;//
	info->renorm_limit=info->freq<<(32-16);
}

//worksize = bytespersymbol*256
__kernel void ans_dec_prep2(__global unsigned short *CDF, __constant int *dim, __global SymbolInfo *out_stats, __global unsigned char *in_CDF2sym)
{
	int iw=dim[DIM_W0],
		ih=dim[DIM_H0],
		block_xcount=dim[DIM_BLOCK_XCOUNT],
		block_ycount=dim[DIM_BLOCK_YCOUNT],
		block_w=dim[DIM_BLOCK_W],
		block_h=dim[DIM_BLOCK_H],
		bytespersymbol=dim[DIM_BYTESPERSYMBOL];
	int idx=get_global_id(0);
	int symbol=idx&255, kc=idx>>8;
	__global SymbolInfo *info=out_stats+idx;
	int prev_CDF=0;
	if(symbol>0)
		prev_CDF=CDF[idx-1];
	info->CDF=CDF[idx];
	info->freq=info->CDF-prev_CDF;
	info->cmpl_freq=~info->freq;
	info->shift=0;//
	info->reserved0=0;
	info->inv_freq=0;//
	info->bias=0;//
	info->renorm_limit=info->freq<<(32-16);

	__global unsigned char *CDF2sym=in_CDF2sym+(kc<<16);
	for(int k=prev_CDF;k<info->CDF;++k)//X slow
		CDF2sym[k]=symbol;
}

//worksize = block_ycount*block_xcount*bytespersymbol
__kernel void ans_enc2D32(__global unsigned char *src, __constant int *dim, __global SymbolInfo *in_stats, __global int *dst, __global int *sizes)
{
	int iw=dim[DIM_W0],
		ih=dim[DIM_H0],
		block_xcount=dim[DIM_BLOCK_XCOUNT],
		block_ycount=dim[DIM_BLOCK_YCOUNT],
		block_w=dim[DIM_BLOCK_W],
		block_h=dim[DIM_BLOCK_H],
		bytespersymbol=dim[DIM_BYTESPERSYMBOL];
	int idx=get_global_id(0);
	int kc=idx%bytespersymbol, kbx=idx/bytespersymbol%block_xcount, kby=idx/(bytespersymbol*block_xcount);
	int y1=block_w*kby, y2=y1+block_w,
		x1=block_h*kbx, x2=x1+block_h;
	if(x2>=iw)
		x2=iw-1;
	if(y2>=ih)
		y2=ih-1;
	int block_idx=bytespersymbol*(block_xcount*kby+kbx)+kc;
	__global unsigned short *cdata=(__global unsigned short*)(dst+(block_idx<<dim[DIM_LOGALLOCCOUNT]));
	int bit_idx=0, bitsizelimit=1<<(dim[DIM_LOGALLOCCOUNT]+5);
#define EMIT_UINT16(U16)		if(bit_idx<bitsizelimit)cdata[bit_idx>>4]=U16; bit_idx+=16

	__global SymbolInfo *stats=in_stats+(kc<<8);
	unsigned state=0x10000;
	int bypass=0;
again:
	for(int ky=y1;ky<y2;++ky)
	{
		int yoffset=iw*ky;
		for(int kx=bytespersymbol*(yoffset+x1)+kc, xend=bytespersymbol*(yoffset+x2)+kc;kx<x2;kx+=bytespersymbol)
		{
			unsigned char symbol=src[kx];
			__global SymbolInfo *info=stats+(symbol&-bypass);//fetch
			if(state>info->renorm_limit)//renormalize
			{
				EMIT_UINT16((unsigned short)state);
#ifdef ANS_PRINT_WRITES
				if(ANS_PRINT_WRITES)
					printf("kc %d (%d,%d) x %08X->%08X idx %d", kc, kx, ky, state, state>>16, bit_idx);//
#endif
				state>>=16;
			}
			if(info->freq)//update
				state=(state/info->freq<<16)+state%info->freq+(bypass?symbol<<8:info->CDF);
		}
	}
	EMIT_UINT16((unsigned short)state);
	EMIT_UINT16((unsigned short)(state>>16));
#ifdef ANS_PRINT_WRITES
	if(ANS_PRINT_WRITES)
		printf("kc %d finish %08X idx %d", kc, state, bit_idx);
#endif
	if(!bypass)
	{
		int size0=block_w*block_h<<3;
		bypass=bit_idx>size0;
		if(bypass)
		{
			bit_idx=0;
			state=0x10000;
			stats=in_stats+(bytespersymbol<<8);
			goto again;
		}
		sizes[block_idx]=bit_idx>>4;
	}
#undef EMIT_UINT16
}

//worksize = block_ycount*block_xcount*bytespersymbol
__kernel void ans_dec2D32(__global unsigned char *dst, __constant int *dim, __global SymbolInfo *in_stats, __global int *src, __global int *sizes, __global unsigned char *in_CDF2sym)
{
	int iw=dim[DIM_W0],
		ih=dim[DIM_H0],
		block_xcount=dim[DIM_BLOCK_XCOUNT],
		block_ycount=dim[DIM_BLOCK_YCOUNT],
		block_w=dim[DIM_BLOCK_W],
		block_h=dim[DIM_BLOCK_H],
		bytespersymbol=dim[DIM_BYTESPERSYMBOL];
	int idx=get_global_id(0);
	int kc=idx%bytespersymbol, kbx=idx/bytespersymbol%block_xcount, kby=idx/(bytespersymbol*block_xcount);
	int y1=block_w*kby, y2=y1+block_w,
		x1=block_h*kbx, x2=x1+block_h;
	if(x2>=iw)
		x2=iw-1;
	if(y2>=ih)
		y2=ih-1;
	int bp_idx=bytespersymbol*(block_xcount*kby+kbx)+kc;
	//for(int kc=0;kc<32;++kc)
	//{
	int bit_idx=sizes[bp_idx]<<3;
	int bypass=bit_idx<0;
	if(bypass)
		bit_idx=-bit_idx;
	__global SymbolInfo *stats=in_stats+((bypass?bytespersymbol:kc)<<8);
	__global unsigned char *CDF2sym=in_CDF2sym+((bypass?bytespersymbol:kc)<<16);
	unsigned state=0;
#ifdef PRINT_READS
#define READ_UINT16()		state=state<<16|(src[bit_idx>>5]>>(bit_idx&31)&0xFF), bit_idx+=16;	\
	if(PRINT_READS)	\
	{	\
		int bit_idx=ctx[kp].bit_idx-8;	\
		printf("read %04X idx %d src[%d]>>%d", ctx[kp].code, ctx[kp].bit_idx, bit_idx>>5, bit_idx&31);	\
	}
#else
#define READ_UINT16()		state=state<<16|(src[bit_idx>>5]>>(bit_idx&31)&0xFFFF), bit_idx+=16
#endif
	READ_UINT16();
	READ_UINT16();
#ifdef ANS_PRINT_READS
	if(ANS_PRINT_READS)
		printf("read x %08X idx %d", state, bit_idx);
#endif
	//if(bit_idx<0)//decode bypass
	//{
	//	bit_idx=-bit_idx;
	//	for(int ky=y1;ky<y2;++ky)
	//	{
	//		int yoffset=iw*ky;
	//		for(int kx=bytespersymbol*(yoffset+x1)+kc, xend=bytespersymbol*(yoffset+x2)+kc;kx<x2;kx+=bytespersymbol)
	//		{
	//			dst[kx]=src[bit_idx>>5]>>(bit_idx&31);
	//			bit_idx+=8;
	//		}
	//	}
	//}
	//else
	//{
	for(int ky=y1;ky<y2;++ky)
	{
		int yoffset=iw*ky;
		for(int kx=bytespersymbol*(yoffset+x1)+kc, xend=bytespersymbol*(yoffset+x2)+kc;kx<x2;kx+=bytespersymbol)
		{
			unsigned short c=(unsigned short)state;
			unsigned char symbol=CDF2sym[c];
			dst[kx]=symbol;
			__global SymbolInfo *info=stats+(symbol&-bypass);
			state=info->freq*(state>>16)+c-(bypass?symbol<<8:info->CDF);
			if(state<0x10000)
			{
				READ_UINT16();
#ifdef ANS_PRINT_READS
				if(ANS_PRINT_READS)
					printf("kc %d (%d,%d) read x %08X idx %d", kc, kx, ky, state, bit_idx);
#endif
			}
		}
	}
	//}
#undef READ_UINT16
	//}
}

