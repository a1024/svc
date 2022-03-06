//libsvc.cpp - The Simple Video Codec implementation
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

#include		"libsvc_internal.h"
#include		<stdio.h>
#include		<stdarg.h>
int				loud=SVC_LOG_NONE;
std::vector<std::string> status;
//int			error_count2=0;
SVCContextStruct *context=nullptr;
char			g_buf[G_BUF_SIZE]={};
#ifdef __GNUC__
#define vsprintf_s vsnprintf
#endif
bool			set_error(const char *file, int line, const char *format, ...)
{
	int printed=0;
	printed+=sprintf_s(g_buf+printed, G_BUF_SIZE-printed, "[%d] ", (int)status.size());
	if(format)
	{
		va_list args;
		va_start(args, format);
		printed+=vsprintf_s(g_buf+printed, G_BUF_SIZE-printed, format, args);
		va_end(args);
	}
	else
		sprintf_s(g_buf+printed, G_BUF_SIZE-printed, "Unknown error");

	status.push_back(g_buf);
	//if(context)
	//	context->status.push_back(g_buf);
	//++error_count2;
	return false;
}

const char*		svc_get_error()
{
	if(!status.size())
		return nullptr;
	auto &str=status[0];
	memcpy(g_buf, str.c_str(), str.size());
	status.erase(status.begin());
	return g_buf;
	//if(context&&context->status.size())
	//{
	//	auto &str=context->status[0];
	//	memcpy(g_buf, str.c_str(), str.size());
	//	context->status.erase(context->status.begin());
	//	return g_buf;
	//}
	//return nullptr;
}

void			add_buffers(void *dst, const void *b1, const void *b2, int nsamples, SrcType type)//dst=b1+b2
{
	switch(type)
	{
	case SVC_UINT8:
		{
			auto s1=(const unsigned char*)b1, s2=(const unsigned char*)b2;
			auto d1=(unsigned char*)dst;
			for(int k=0;k<nsamples;++k)
				d1[k]=s1[k]+s2[k];
		}
		break;
	case SVC_UINT16:
		{
			auto s1=(const unsigned short*)b1, s2=(const unsigned short*)b2;
			auto d1=(unsigned short*)dst;
			for(int k=0;k<nsamples;++k)
				d1[k]=s1[k]+s2[k];
		}
		break;
	case SVC_FLOAT32:
		{
			auto s1=(const float*)b1, s2=(const float*)b2;
			auto d1=(float*)dst;
			for(int k=0;k<nsamples;++k)
				d1[k]=s1[k]+s2[k];
		}
		break;
	}
}
void			sub_buffers(void *dst, const void *b1, const void *b2, int nsamples, SrcType type)//dst=b1-b2
{
	switch(type)
	{
	case SVC_UINT8:
		{
			auto s1=(const unsigned char*)b1, s2=(const unsigned char*)b2;
			auto d1=(unsigned char*)dst;
			for(int k=0;k<nsamples;++k)
				d1[k]=s1[k]-s2[k];
		}
		break;
	case SVC_UINT16:
		{
			auto s1=(const unsigned short*)b1, s2=(const unsigned short*)b2;
			auto d1=(unsigned short*)dst;
			for(int k=0;k<nsamples;++k)
				d1[k]=s1[k]-s2[k];
		}
		break;
	case SVC_FLOAT32:
		{
			auto s1=(const float*)b1, s2=(const float*)b2;
			auto d1=(float*)dst;
			for(int k=0;k<nsamples;++k)
				d1[k]=s1[k]-s2[k];
		}
		break;
	}
}

//encoder
SVCContext		svc_enc_start(int w, int h, short nchannels, short depth, SrcType type, int fps_num, int fps_den)
{
	if(w<=0||h<=0||nchannels<=0||depth<=0||type<SVC_UINT8||type>SVC_FLOAT32)
	{
		ERROR("Invalid encoder parameters");
		return nullptr;
	}
	context=new SVCContextStruct(w, h, nchannels, depth, type, fps_num, fps_den);
	if(!context->out_data)
	{
		delete context, context=nullptr;
		return nullptr;
	}
#ifdef SUBTRACT_PREV_FRAME
	if(!context->prev_frame)
	{
		free(context->out_data);
		delete context, context=nullptr;
		return nullptr;
	}
#endif
	PROF_INIT();//OpenCL initialization takes a long time
	return context;
}
int				svc_enc_add_frame(SVCContext handle, const void *data)
{
	PROF(WASTE);
	if(!handle)
	{
		ERROR("Encoder handle is null");
		return 0;
	}
	context=(SVCContextStruct*)handle;
	++context->nframes;
	int sample_size=1<<context->srctype;
	const void *frame=nullptr;
#ifdef SUBTRACT_PREV_FRAME
	if(context->nframes==1)
	{
		memcpy(context->prev_frame, data, context->frame_bytesize);
		//TODO: lift prev_frame
	}
	else
		sub_buffers(context->prev_frame, data, context->prev_frame, context->frame_samples, context->srctype);
	frame=context->prev_frame;
#else
	frame=data;
#endif
	PROF(DELTA);
	auto s0=context->out_size;
#ifdef ANS_CL
	if(!ans9_encode(frame, context->out_data, context->out_size, context->out_cap, &context->ans9_ctx, loud))
		return 0;
#elif defined ABAC_CL
	if(!abac9_encode(frame, context->out_data, context->out_size, context->out_cap, &context->abac9_ctx, loud))
		return 0;
#elif defined ANS_SSE2
	if(!rans7_encode(frame, context->frame_res, context->nchannels*sample_size, context->out_data, context->out_size, context->out_cap, loud))
		return 0;
#elif defined ANS_64BIT
	if(!rans6_encode(frame, context->frame_res, context->nchannels*sample_size, context->out_data, context->out_size, context->out_cap, loud))
		return 0;
#else
	if(!rans4_encode(frame, context->frame_res, context->nchannels*sample_size, context->out_data, context->out_size, context->out_cap, loud))
		return 0;
#endif
	//for(int k=0;k<context->nchannels;++k)
	//	if(!abac4_encode((char*)context->prev_frame+k*sample_size, context->frame_res, context->depth, context->nchannels*sample_size, context->out_data, context->out_size, context->out_cap, loud))
	//		return;
#ifdef SUBTRACT_PREV_FRAME
	memcpy(context->prev_frame, data, context->frame_bytesize);
#endif
	return (int)(context->out_size-s0);
}
unsigned char*	svc_enc_finish(SVCContext handle, unsigned long long *bytesize)
{
	if(!handle)
	{
		ERROR("Encoder handle is null");
		return nullptr;
	}
	if(!bytesize)
		ERROR("bytesize pointer is null");
	context=(SVCContextStruct*)handle;
	auto header=(SVCHeader*)context->out_data;
	header->nframes=context->nframes;
	if(bytesize)
		*bytesize=context->out_size;
	return context->out_data;
}

//decoder
SVCContext		svc_dec_start(const unsigned char *data, unsigned long long bytesize)
{
	if(!data||!bytesize)
	{
		ERROR("Invalid decoder parameters");
		return nullptr;
	}
	context=new SVCContextStruct(data, bytesize);
	if(!context->prev_frame)
	{
		delete context, context=nullptr;
		return nullptr;
	}
	context->in_idx=sizeof(SVCHeader);
	PROF_INIT();
	return context;
}
void			svc_dec_get_info(SVCContext handle, SVCHeader *info)
{
	if(!handle)
	{
		ERROR("Decoder handle is null");
		return;
	}
	if(!info)
	{
		ERROR("Info pointer is null");
		return;
	}
	context=(SVCContextStruct*)handle;
	if(!context->in_data)
	{
		ERROR("Badly initialized codec: No encoded data provided");
		return;
	}
	memcpy(info, context->in_data, sizeof(SVCHeader));
}
void			svc_dec_get_frame(SVCContext handle, void *data, const void *guide)
{
	PROF(WASTE);
	if(!handle)
	{
		ERROR("Decoder handle is null");
		return;
	}
	if(!data)
	{
		ERROR("Data pointer is null");
		return;
	}
	context=(SVCContextStruct*)handle;
	if(context->frame_idx>=context->nframes)
	{
		ERROR("Frame index = %d, but there are %d frames", context->frame_idx, context->nframes);
		return;
	}
#ifndef ANS_CL
	memset(data, 0, context->frame_bytesize);
#endif

	int sample_size=1<<context->srctype;
	unsigned char *g2=nullptr;
	if(guide)
	{
		g2=new unsigned char[context->frame_bytesize];
		if(!context->frame_idx)
			memcpy(g2, guide, context->frame_bytesize);
		else
			sub_buffers(g2, guide, context->prev_frame, context->frame_samples, context->srctype);
	}
#ifdef ANS_CL
	if(!ans9_decode(context->in_data, context->in_idx, context->in_size, data, &context->ans9_ctx, loud))
		return;
	if(guide)
	{
		auto original=(int*)g2;
		auto result=(int*)data;
		int imsize=context->w*context->h;
		for(int k=0;k<imsize;++k)
		{
			if(result[k]!=original[k])
			{
				ERROR("Decode error at (%d, %d): dec=0x%08X != guide=0x%08X", k%context->w, k/context->w, result[k], original[k]);
				break;
			}
		}
	}
#elif defined ABAC_CL
	if(!abac9_decode(context->in_data, context->in_idx, context->in_size, data, &context->abac9_ctx, loud))
		return;
	if(guide)
	{
		auto original=(int*)g2;
		auto result=(int*)data;
		int imsize=context->w*context->h;
		for(int k=0;k<imsize;++k)
		{
			if(result[k]!=original[k])
			{
				ERROR("Decode error at (%d, %d): dec=0x%08X != guide=0x%08X", k%context->w, k/context->w, result[k], original[k]);
				break;
			}
		}
	}
#elif defined ANS_SSE2
	if(!rans7_decode(context->in_data, context->in_idx, context->in_size, data, context->frame_res, context->nchannels*sample_size, loud, g2))
		return;
#elif defined ANS_64BIT
	if(!rans6_decode(context->in_data, context->in_idx, context->in_size, data, context->frame_res, context->nchannels*sample_size, loud, g2))
		return;
#else
	if(!rans4_decode(context->in_data, context->in_idx, context->in_size, data, context->frame_res, context->nchannels*sample_size, loud, g2))
		return;
#endif
	if(g2)
		delete[] g2;
	//for(int k=0;k<context->nchannels;++k)
	//	if(!abac4_decode(context->in_data, context->in_idx, context->in_size, (char*)data+k*sample_size, context->frame_res, context->depth, context->nchannels*sample_size, loud))
	//		return;
#ifdef SUBTRACT_PREV_FRAME
	if(!context->frame_idx)
	{
		//TODO: unlift data
	}
	else
		add_buffers(data, data, context->prev_frame, context->frame_samples, context->srctype);
	memcpy(context->prev_frame, data, context->frame_bytesize);
#endif
	++context->frame_idx;
	PROF(DELTA);
}

void			svc_cleanup(SVCContext handle)
{
	if(!handle)
	{
		ERROR("Handle is null");
		return;
	}
	context=(SVCContextStruct*)handle;
	prof_end();
	if(context)
	{
#ifdef ABAC_CL
		abac9_finish(&context->abac9_ctx);
#endif
		if(context->prev_frame)
			free(context->prev_frame);
		if(context->out_data)
			free(context->out_data);
		delete context, context=nullptr;
	}
}
void			svc_set_loglevel(LogLevel loglevel)
{
	loud=loglevel;
}