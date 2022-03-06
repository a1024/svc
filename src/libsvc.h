//libsvc.h - Simple Video Codec include
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
#ifndef LIBSVC_H
#define LIBSVC_H
#ifdef __cplusplus
extern "C"
{
#endif
#ifdef __GNUC__
#define			SVC_API
//#ifdef			LIBSVC_INTERNAL_H
//#define			SVC_API		__attribute__((export))
//#else
//#define			SVC_API		__attribute__((import))
//#endif
#else
#ifdef			LIBSVC_INTERNAL_H
#define			SVC_API		__declspec(dllexport)
#else
#define			SVC_API		__declspec(dllimport)
#endif
#endif

#define			FORMATLIST	\
	FORMAT(SVC_UINT8)\
	FORMAT(SVC_UINT16)\
	FORMAT(SVC_FLOAT32)
typedef enum
{
#define			FORMAT(LABEL)	LABEL,
	FORMATLIST
#undef			FORMAT
	SVC_FORMATCOUNT,
} SrcType;
typedef struct
{
	int magic;//magic_sv01
	int w, h;
	short nchannels, depth;
	int nframes;
	short fps_num, fps_den;
	char bayer[4];
	SrcType srctype;//0: uchar, 1: ushort, 2: float
	char reserved[32];
} SVCHeader;
typedef void	*SVCContext;

SVC_API const char*		svc_get_error();//check after each API call

SVC_API SVCContext		svc_enc_start(int w, int h, short nchannels, short depth, SrcType type, int fps_num, int fps_den);
SVC_API int				svc_enc_add_frame(SVCContext handle, const void *data);//returns frame compressed size
SVC_API unsigned char*	svc_enc_finish(SVCContext handle, unsigned long long *bytesize);

SVC_API SVCContext		svc_dec_start(const unsigned char *data, unsigned long long bytesize);
SVC_API void			svc_dec_get_info(SVCContext handle, SVCHeader *info);
SVC_API void			svc_dec_get_frame(SVCContext handle, void *data, const void *guide);

SVC_API void			svc_cleanup(SVCContext handle);

typedef enum
{
	SVC_LOG_NONE,
	SVC_LOG_STATS,
	SVC_LOG_PROFILER,
} LogLevel;
SVC_API void			svc_set_loglevel(LogLevel loglevel);

#ifdef __cplusplus
}
#endif

#ifndef LIBSVC_INTERNAL_H
#include		<stdio.h>
static int		svc_check()
{
	const char *error=svc_get_error();
	if(error)
	{
		printf("%s\n", error);
		int k=0;
#ifdef __GNUC__
		scanf("%d", &k);
#else
		scanf_s("%d", &k, 1);
#endif
		return 1;
	}
	return 0;
}
static const char* svc_format2str(SrcType type)
{
	const char *a="UNKNOWN_FORMAT";
	switch(type)
	{
#define			FORMAT(LABEL)	case LABEL:a=#LABEL;break;
		FORMATLIST
#undef			FORMAT
	}
	return a;
}
#endif
#undef	FORMATLIST

#endif