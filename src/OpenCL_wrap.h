//OpenCL_wrap.h - OpenCL wrapper include
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
#ifndef	OCL_LOADER_H
#define	OCL_LOADER_H

#include<stdarg.h>
#include<map>
#define	CL_TARGET_OPENCL_VERSION	120
#include<CL/opencl.h>
//const char	header[]=__FILE__;

#ifdef __ANDROID__
	#define		OCL_STATIC_LINK
#endif

//error handling
bool			set_error(const char *file, int line, const char *msg, ...);
#define			MY_ASSERT(SUCCESS, MESSAGE, ...)	((SUCCESS)!=0||set_error(__FILE__, __LINE__, MESSAGE, ##__VA_ARGS__))

//OpenCL loader
#ifdef OCL_STATIC_LINK
	#define		p_clGetPlatformIDs			clGetPlatformIDs
	#define		p_clGetPlatformInfo			clGetPlatformInfo
	#define		p_clGetDeviceIDs			clGetDeviceIDs
	#define		p_clGetDeviceInfo			clGetDeviceInfo
	#define		p_clCreateContext			clCreateContext
	#define		p_clReleaseContext			clReleaseContext
	#define		p_clRetainContext			clRetainContext
	#define		p_clGetContextInfo			clGetContextInfo
	#define		p_clCreateCommandQueue		clCreateCommandQueue
	#define		p_clCreateProgramWithSource	clCreateProgramWithSource
	#define		p_clBuildProgram			clBuildProgram
	#define		p_clGetProgramBuildInfo		clGetProgramBuildInfo	
	#define		p_clGetProgramInfo			clGetProgramInfo
	#define		p_clCreateProgramWithBinary	clCreateProgramWithBinary
	#define		p_clCreateBuffer			clCreateBuffer
	#define		p_clCreateKernel			clCreateKernel
	#define		p_clSetKernelArg			clSetKernelArg
//	#define		p_clEnqueueFillBuffer		clEnqueueFillBuffer			//OpenCL 1.2+
	#define		p_clEnqueueWriteBuffer		clEnqueueWriteBuffer	
	#define		p_clEnqueueCopyBuffer		clEnqueueCopyBuffer
	#define		p_clEnqueueNDRangeKernel	clEnqueueNDRangeKernel
	#define		p_clEnqueueReadBuffer		clEnqueueReadBuffer
	#define		p_clFlush					clFlush
	#define		p_clFinish					clFinish	
	#define		p_clCreateFromGLBuffer		clCreateFromGLBuffer
	#define		p_clCreateFromGLTexture		clCreateFromGLTexture		//OpenCL 1.2+?
	#define		p_clReleaseMemObject		clReleaseMemObject
	#define		p_clReleaseKernel			clReleaseKernel
	#define		p_clReleaseProgram			clReleaseProgram
	#define		p_clReleaseCommandQueue		clReleaseCommandQueue
#else
extern int		OpenCL_state;
void 			load_OpenCL_API();
#endif

const char*		clerr2str(int error);
#define			CL_CHECK(ERROR)			MY_ASSERT(!(ERROR), "OpenCL error %d: %s\n", ERROR, clerr2str(ERROR))

#ifndef OCL_STATIC_LINK
#define 		OPENCL_FUNC(clFunc)		extern decltype(clFunc) *p_##clFunc
#define 		OPENCL_FUNC2(clFunc)	extern decltype(clFunc) *p_##clFunc
#include		"OpenCL_func.h"
#undef			OPENCL_FUNC
#undef			OPENCL_FUNC2
#endif


//OpenCL wrapper
enum				CLFunctionType
{
#define				CLFUNC(LABEL, ARGCOUNT)		OCL_##LABEL,
#include			"cl_kernel_names.h"
#undef				CLFUNC
	OCL_NKERNELS,
};
extern const char	*kernelnames[];

extern cl_platform_id	*ocl_platforms;
extern cl_device_id		*ocl_devices;
extern cl_context		ocl_context;
extern cl_command_queue	ocl_commandqueue;
extern size_t			ocl_maxlocalsize;
extern cl_program		ocl_program;

enum				CLBufferType
{
	BUFFER_READ_ONLY,
	BUFFER_WRITE_ONLY,
	BUFFER_READ_WRITE,
};
typedef std::pair<cl_mem, size_t> BufEntry;
extern std::map<cl_mem, size_t> bufferinfo;
inline size_t		ocl_query_mem_usage()
{
	size_t usage=0;
	for(auto it=bufferinfo.begin();it!=bufferinfo.end();++it)
		usage+=it->second;
	return usage*sizeof(float);
}
struct				CLBuffer
{
	cl_mem handle;
	CLBuffer():handle(nullptr){}
	CLBuffer(void *h):handle((cl_mem)h){}
	size_t size()const
	{
		auto it=bufferinfo.find(handle);
		MY_ASSERT(it!=bufferinfo.end(), "Invalid buffer handle");
		return it->second;
	}
	void create(size_t count, CLBufferType buffertype, void *host=nullptr)
	{
		unsigned long flags=0;
		switch(buffertype)
		{
		case BUFFER_READ_ONLY:	flags=CL_MEM_READ_ONLY;	break;
		case BUFFER_WRITE_ONLY:	flags=CL_MEM_WRITE_ONLY;break;
		case BUFFER_READ_WRITE:	flags=CL_MEM_READ_WRITE;break;
		}
		if(host)
			flags|=CL_MEM_USE_HOST_PTR;
		int error=0;
		handle=p_clCreateBuffer(ocl_context, flags, count*sizeof(float), host, &error);CL_CHECK(error);
		bufferinfo.insert(BufEntry(handle, count));
	}
	void release()
	{
		if(handle)
		{
			int error=0;
			error=p_clReleaseMemObject(handle);CL_CHECK(error);
			bufferinfo.erase(handle);
			handle=nullptr;
		}
	}
	void read_nonblocking(void *dst)const
	{
		int error=0;
		//error=p_clFlush(ocl_commandqueue);	CL_CHECK(error);
		//error=p_clFinish(ocl_commandqueue);	CL_CHECK(error);
		auto s=size();
		error=p_clEnqueueReadBuffer(ocl_commandqueue, handle, CL_FALSE, 0, s*sizeof(int), dst, 0, nullptr, nullptr);CL_CHECK(error);
	}
	void read(void *dst)const
	{
		int error=0;
		error=p_clFlush(ocl_commandqueue);	CL_CHECK(error);
		error=p_clFinish(ocl_commandqueue);	CL_CHECK(error);
		auto s=size();
		error=p_clEnqueueReadBuffer(ocl_commandqueue, handle, CL_TRUE, 0, s*sizeof(int), dst, 0, nullptr, nullptr);CL_CHECK(error);
	}
	//void read_sub(void *dst, size_t offset, size_t count)const
	//{
	//	int error=0;
	//	error=p_clFlush(ocl_commandqueue);  CL_CHECK(error);
	//	error=p_clFinish(ocl_commandqueue); CL_CHECK(error);
	//	error=p_clEnqueueReadBuffer(ocl_commandqueue, handle, CL_TRUE, offset*sizeof(float), count*sizeof(float), dst, 0, nullptr, nullptr);
	//}
	float* read()const//don't forget to delete buffer
	{
		auto dst=new float[size()];
		read(dst);
		return dst;
	}
	float* read_sub(size_t offset, size_t count)const//don't forget to delete buffer
	{
		auto dst=new float[count];
		int error=0;
		error=p_clFlush(ocl_commandqueue);  CL_CHECK(error);
		error=p_clFinish(ocl_commandqueue); CL_CHECK(error);
		error=p_clEnqueueReadBuffer(ocl_commandqueue, handle, CL_TRUE, offset*sizeof(float), count*sizeof(float), dst, 0, nullptr, nullptr);
		return dst;
	}
	void write(const void *src)
	{
		auto s=size();
		int error=p_clEnqueueWriteBuffer(ocl_commandqueue, handle, CL_FALSE, 0, s*sizeof(float), src, 0, nullptr, nullptr);CL_CHECK(error);
	}
	void write_sub(const void *src, size_t offset, size_t count)
	{
		int error=p_clEnqueueWriteBuffer(ocl_commandqueue, handle, CL_FALSE, offset*sizeof(float), count*sizeof(float), src, 0, nullptr, nullptr);CL_CHECK(error);
	}
	//void copy_to(CLBuffer const &dst)
	//{
	//	auto s=size();
	//	MY_ASSERT(s==dst.size(), "Size mismatch");
	//	int error=p_clEnqueueCopyBuffer(ocl_commandqueue, handle, dst.handle, 0, 0, s*sizeof(float), 0, nullptr, nullptr);CL_CHECK(error);
	//}
	void copy_from(CLBuffer const &src)
	{
		auto s=size();
		MY_ASSERT(s==src.size(), "Size mismatch");
		int error=p_clEnqueueCopyBuffer(ocl_commandqueue, src.handle, handle, 0, 0, s*sizeof(float), 0, nullptr, nullptr);CL_CHECK(error);
	}
};
struct				CLKernel
{
	cl_kernel func;
	CLKernel():func(nullptr){}
	CLKernel(void *h):func((cl_kernel)h){}
	void extract(const char *name)
	{
		int error=0;
		func=p_clCreateKernel(ocl_program, name, &error);
		MY_ASSERT(!error, "Couldn't find kernel %s", name);
	}
	void release()
	{
		int error=0;
		error=p_clReleaseKernel(func);CL_CHECK(error);
	}
	//void call(size_t worksize, CLBuffer *args, int nargs)
	//{
	//	int error=0;
	//	for(int k=0;k<nargs;++k)
	//	{
	//		error=p_clSetKernelArg(func, k, sizeof(cl_mem), &args[k].handle);//CL_CHECK(error);
	//		MY_ASSERT(!error, "%s: func = %p, arg %d = %p\n", clerr2str(error), func, k, args[k].handle);
	//	}
	//	error=p_clEnqueueNDRangeKernel(ocl_commandqueue, func, 1, nullptr, &worksize, nullptr, 0, nullptr, nullptr);CL_CHECK(error);
	//}
};
extern CLKernel		kernels[OCL_NKERNELS];

int					ocl_init(const char *srcname, int print_info);
void				ocl_finish();
inline void			ocl_sync()
{
	int error=0;
	error=p_clFlush(ocl_commandqueue);	CL_CHECK(error);
	error=p_clFinish(ocl_commandqueue);	CL_CHECK(error);
}
void				ocl_call_kernel(int kernel_idx, size_t worksize, CLBuffer *args, int nargs);//call ocl_sync first

#endif
