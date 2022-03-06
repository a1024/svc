//OpenCL_wrap.cpp - OpenCL wrapper implementation
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

#include		"OpenCL_wrap.h"
#ifdef __linux__
#include		<dlfcn.h>
#else
#include		<Windows.h>
#endif
#include		<stdio.h>
#include		<stdarg.h>
#include		<string>
#include		<fstream>
const char		file[]=__FILE__;
#define			SIZEOF(STATIC_ARRAY)	(sizeof(STATIC_ARRAY)/sizeof(*(STATIC_ARRAY)))
#define			G_BUF_SIZE	1024
extern char		g_buf[G_BUF_SIZE];

//I/O
bool			open_text(const char *filename, std::string &data)
{
	std::ifstream input(filename);
	if(!input.is_open())
		return false;
	data.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
	input.close();
	return true;
}


//OpenCL loader
#ifndef OCL_STATIC_LINK
#define 		OPENCL_FUNC(clFunc)	decltype(clFunc) *p_##clFunc=nullptr
#define 		OPENCL_FUNC2(clFunc)	decltype(clFunc) *p_##clFunc=nullptr
#include		"OpenCL_func.h"
#undef			OPENCL_FUNC
#undef			OPENCL_FUNC2

int				OpenCL_state=0;
void			*hOpenCL=nullptr;
#endif

const char*		clerr2str(int error)
{
	const char *a=nullptr;
#define 		EC(x)		case x:a=(const char*)#x;break;
#define 		EC2(n, x)	case n:a=(const char*)#x;break;
	switch(error)
	{
	EC(CL_SUCCESS)
	EC(CL_DEVICE_NOT_FOUND)
	EC(CL_DEVICE_NOT_AVAILABLE)
	EC(CL_COMPILER_NOT_AVAILABLE)
	EC(CL_MEM_OBJECT_ALLOCATION_FAILURE)
	EC(CL_OUT_OF_RESOURCES)
	EC(CL_OUT_OF_HOST_MEMORY)
	EC(CL_PROFILING_INFO_NOT_AVAILABLE)
	EC(CL_MEM_COPY_OVERLAP)
	EC(CL_IMAGE_FORMAT_MISMATCH)
	EC(CL_IMAGE_FORMAT_NOT_SUPPORTED)
	EC(CL_BUILD_PROGRAM_FAILURE)
	EC(CL_MAP_FAILURE)
//#ifdef CL_VERSION_1_1
	EC(CL_MISALIGNED_SUB_BUFFER_OFFSET)
	EC(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST)
//#endif
//#ifdef CL_VERSION_1_2
	EC(CL_COMPILE_PROGRAM_FAILURE)
	EC(CL_LINKER_NOT_AVAILABLE)
	EC(CL_LINK_PROGRAM_FAILURE)
	EC(CL_DEVICE_PARTITION_FAILED)
	EC(CL_KERNEL_ARG_INFO_NOT_AVAILABLE)
//#endif
	EC(CL_INVALID_VALUE)
	EC(CL_INVALID_DEVICE_TYPE)
	EC(CL_INVALID_PLATFORM)
	EC(CL_INVALID_DEVICE)
	EC(CL_INVALID_CONTEXT)
	EC(CL_INVALID_QUEUE_PROPERTIES)
	EC(CL_INVALID_COMMAND_QUEUE)
	EC(CL_INVALID_HOST_PTR)
	EC(CL_INVALID_MEM_OBJECT)
	EC(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR)
	EC(CL_INVALID_IMAGE_SIZE)
	EC(CL_INVALID_SAMPLER)
	EC(CL_INVALID_BINARY)
	EC(CL_INVALID_BUILD_OPTIONS)
	EC(CL_INVALID_PROGRAM)
	EC(CL_INVALID_PROGRAM_EXECUTABLE)
	EC(CL_INVALID_KERNEL_NAME)
	EC(CL_INVALID_KERNEL_DEFINITION)
	EC(CL_INVALID_KERNEL)
	EC(CL_INVALID_ARG_INDEX)
	EC(CL_INVALID_ARG_VALUE)
	EC(CL_INVALID_ARG_SIZE)
	EC(CL_INVALID_KERNEL_ARGS)
	EC(CL_INVALID_WORK_DIMENSION)
	EC(CL_INVALID_WORK_GROUP_SIZE)
	EC(CL_INVALID_WORK_ITEM_SIZE)
	EC(CL_INVALID_GLOBAL_OFFSET)
	EC(CL_INVALID_EVENT_WAIT_LIST)
	EC(CL_INVALID_EVENT)
	EC(CL_INVALID_OPERATION)
	EC(CL_INVALID_GL_OBJECT)
	EC(CL_INVALID_BUFFER_SIZE)
	EC(CL_INVALID_MIP_LEVEL)
	EC(CL_INVALID_GLOBAL_WORK_SIZE)
//#ifdef CL_VERSION_1_1
	EC(CL_INVALID_PROPERTY)
//#endif
//#ifdef CL_VERSION_1_2
	EC(CL_INVALID_IMAGE_DESCRIPTOR)
	EC(CL_INVALID_COMPILER_OPTIONS)
	EC(CL_INVALID_LINKER_OPTIONS)
	EC(CL_INVALID_DEVICE_PARTITION_COUNT)
//#endif
//#ifdef CL_VERSION_2_0
	EC2(-69, CL_INVALID_PIPE_SIZE)
	EC2(-70, CL_INVALID_DEVICE_QUEUE)
//#endif
//#ifdef CL_VERSION_2_2
	EC2(-71, CL_INVALID_SPEC_ID)
	EC2(-72, CL_MAX_SIZE_RESTRICTION_EXCEEDED)
//#endif
//	EC(CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR)
//	EC(CL_PLATFORM_NOT_FOUND_KHR)
	EC2(-1002, CL_INVALID_D3D10_DEVICE_KHR)
	EC2(-1003, CL_INVALID_D3D10_RESOURCE_KHR)
	EC2(-1004, CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR)
	EC2(-1005, CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR)
	EC2(-1006, CL_INVALID_D3D11_DEVICE_KHR)
	EC2(-1007, CL_INVALID_D3D11_RESOURCE_KHR)
	EC2(-1008, CL_D3D11_RESOURCE_ALREADY_ACQUIRED_KHR)
	EC2(-1009, CL_D3D11_RESOURCE_NOT_ACQUIRED_KHR)
#ifndef __linux__
	EC2(-1010, CL_INVALID_D3D9_DEVICE_NV_or_CL_INVALID_DX9_DEVICE_INTEL)
	EC2(-1011, CL_INVALID_D3D9_RESOURCE_NV_or_CL_INVALID_DX9_RESOURCE_INTEL)
	EC2(-1012, CL_D3D9_RESOURCE_ALREADY_ACQUIRED_NV_or_CL_DX9_RESOURCE_ALREADY_ACQUIRED_INTEL)
	EC2(-1013, CL_D3D9_RESOURCE_NOT_ACQUIRED_NV_or_CL_DX9_RESOURCE_NOT_ACQUIRED_INTEL)
#endif
	EC2(-1092, CL_EGL_RESOURCE_NOT_ACQUIRED_KHR)
	EC2(-1093, CL_INVALID_EGL_OBJECT_KHR)
	EC2(-1094, CL_INVALID_ACCELERATOR_INTEL)
	EC2(-1095, CL_INVALID_ACCELERATOR_TYPE_INTEL)
	EC2(-1096, CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL)
	EC2(-1097, CL_ACCELERATOR_TYPE_NOT_SUPPORTED_INTEL)
	EC2(-1098, CL_INVALID_VA_API_MEDIA_ADAPTER_INTEL)
	EC2(-1099, CL_INVALID_VA_API_MEDIA_SURFACE_INTEL)
	EC2(-1101, CL_VA_API_MEDIA_SURFACE_NOT_ACQUIRED_INTEL)
	case 1:a="File failure";break;//
	default:
		a="???";
		break;
	}
#undef			EC
#undef			EC2
	return a;
}
#ifndef OCL_STATIC_LINK
void 			load_OpenCL_API()
{
	if(!OpenCL_state)
	{
		static const char *libocl_paths[]=//https://stackoverflow.com/questions/31611790/using-opencl-in-the-new-android-studio
		{
#ifdef __ANDROID__
			"/system/vendor/lib64/libOpenCL.so",
			"/system/lib/libOpenCL.so",
			"/system/vendor/lib/libOpenCL.so",
			"/system/vendor/lib/egl/libGLES_mali.so",
			"/system/vendor/lib/libPVROCL.so",
			"/data/data/org.pocl.libs/files/lib/libpocl.so",
#elif defined __linux__
			"libOpenCL.so",
			"/usr/lib/libOpenCL.so",
			"/usr/local/lib/libOpenCL.so",
			"/usr/local/lib/libpocl.so",
			"/usr/lib64/libOpenCL.so",
			"/usr/lib32/libOpenCL.so",
#else
			"OpenCL.dll",
#endif
		};
		const int npaths=SIZEOF(libocl_paths);
#ifdef __linux__
		for(int k=0;k<npaths&&!hOpenCL;++k)
		{
			hOpenCL=dlopen(libocl_paths[k], RTLD_NOW);
			if(!hOpenCL)
				printf("Not found: %s\n", libocl_paths[k]);
		}
#else
		for(int k=0;k<npaths&&!hOpenCL;++k)
			hOpenCL=LoadLibraryA(libocl_paths[k]);
#endif
		MY_ASSERT(hOpenCL, "Cannot find OpenCL library\n");

#ifdef __linux__
#define			OPENCL_FUNC(FUNC)	p_##FUNC=(decltype(p_##FUNC))dlsym(hOpenCL, #FUNC), MY_ASSERT(p_##FUNC, "OpenCL function not found")
#define			OPENCL_FUNC2(FUNC)	p_##FUNC=(decltype(p_##FUNC))dlsym(hOpenCL, #FUNC)
#else
#define			OPENCL_FUNC(FUNC)	p_##FUNC=(decltype(p_##FUNC))GetProcAddress((HMODULE)hOpenCL, #FUNC), MY_ASSERT(p_##FUNC, "Failed to load API function")
#define			OPENCL_FUNC2(FUNC)	p_##FUNC=(decltype(p_##FUNC))GetProcAddress((HMODULE)hOpenCL, #FUNC)
#endif
#include		"OpenCL_func.h"
#undef			OPENCL_FUNC
#undef			OPENCL_FUNC2

		OpenCL_state=1;
	}
}
#endif


//OpenCL wrapper
const char			*kernelnames[]=
{
#define				CLFUNC(LABEL, ARGCOUNT)		#LABEL,
#include			"cl_kernel_names.h"
#undef				CLFUNC
};

cl_platform_id		*ocl_platforms=nullptr;
cl_device_id		*ocl_devices=nullptr;
cl_context			ocl_context=nullptr;
cl_command_queue	ocl_commandqueue=nullptr;
size_t				ocl_maxlocalsize=0;
cl_program			ocl_program=nullptr;

std::map<cl_mem, size_t> bufferinfo;
CLKernel			kernels[OCL_NKERNELS]={};
bool				ocl_initialized=false;
int					ocl_init(const char *srcname, int print_info)
{
	if(ocl_initialized)
		return true;
	ocl_initialized=true;
#ifndef OCL_STATIC_LINK
	load_OpenCL_API();
#endif

	unsigned nplatforms=0;
	int error=p_clGetPlatformIDs(0, nullptr, &nplatforms);			CL_CHECK(error);	MY_ASSERT(nplatforms, "OpenCL platform count: %d\n", nplatforms);
	ocl_platforms=new cl_platform_id[nplatforms];
	error=p_clGetPlatformIDs(nplatforms, ocl_platforms, nullptr);	CL_CHECK(error);

	unsigned ndevices=0;
	error=p_clGetDeviceIDs(ocl_platforms[0], CL_DEVICE_TYPE_GPU, 0, nullptr, &ndevices);			CL_CHECK(error);	MY_ASSERT(ndevices, "OpenCL device count: %d\n", ndevices);
	ocl_devices=new cl_device_id[ndevices];
	error=p_clGetDeviceIDs(ocl_platforms[0], CL_DEVICE_TYPE_GPU, ndevices, ocl_devices, nullptr);	CL_CHECK(error);

	//get info
	size_t retlen=0;
	if(print_info)
	{
		error=p_clGetPlatformInfo(ocl_platforms[0], CL_PLATFORM_VERSION, G_BUF_SIZE, g_buf, &retlen);CL_CHECK(error);
		printf("OpenCL platform: %s\n", g_buf);
		error=p_clGetDeviceInfo(ocl_devices[0], CL_DEVICE_NAME, G_BUF_SIZE, g_buf, &retlen);	CL_CHECK(error);
		printf("Device: %s\n", g_buf);
		error=p_clGetDeviceInfo(ocl_devices[0], CL_DEVICE_VENDOR, G_BUF_SIZE, g_buf, &retlen);	CL_CHECK(error);
		printf("Vendor: %s\n", g_buf);
		error=p_clGetDeviceInfo(ocl_devices[0], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &ocl_maxlocalsize, &retlen);	CL_CHECK(error);
		printf("CL_DEVICE_MAX_WORK_GROUP_SIZE = %d\n", (int)ocl_maxlocalsize);
		error=p_clGetDeviceInfo(ocl_devices[0], CL_DEVICE_ADDRESS_BITS, sizeof(size_t), &ocl_maxlocalsize, &retlen);	CL_CHECK(error);
		printf("CL_DEVICE_ADDRESS_BITS = %d\n", (int)ocl_maxlocalsize);
		error=p_clGetDeviceInfo(ocl_devices[0], CL_DEVICE_EXTENSIONS, G_BUF_SIZE, g_buf, &retlen);	CL_CHECK(error);
		printf("Extensions:\n%s\n", g_buf);
		std::string extensions=g_buf;
		auto extpos=extensions.find("cl_khr_gl_sharing");
		bool cl_gl_interop=extpos!=std::string::npos;
		if(!cl_gl_interop)
			printf("\n\tcl_khr_gl_sharing not supported\n\n");
	}

	//create context & command queue
#ifdef CL_GL_INTEROP
	cl_context_properties properties[8]={};		
	if(cl_gl_interop)
	{
		auto gl_context=eglGetCurrentContext();//changes when resuming
		auto egl_display=eglGetCurrentDisplay();
		properties[0]=CL_GL_CONTEXT_KHR,	properties[1]=(cl_context_properties)gl_context;//https://stackoverflow.com/questions/26802905/getting-opengl-buffers-using-opencl
		properties[2]=CL_EGL_DISPLAY_KHR,	properties[3]=(cl_context_properties)egl_display;
		properties[4]=CL_CONTEXT_PLATFORM,	properties[5]=(cl_context_properties)platform;
		properties[6]=0, properties[7]=0;
	}
	else
	{
		properties[0]=CL_CONTEXT_PLATFORM, properties[1]=(cl_context_properties)platform;
		properties[2]=0, properties[3]=0;
	}
	context=p_clCreateContext(properties, 1, &devices[0], nullptr, nullptr, &error);	CL_CHECK(error);
#else
	ocl_context=p_clCreateContext(nullptr, ndevices, ocl_devices, nullptr, nullptr, &error);	CL_CHECK(error);
#endif
	ocl_commandqueue=p_clCreateCommandQueue(ocl_context, ocl_devices[0], 0, &error);			CL_CHECK(error);

	//compile kernel
	std::string kernel_src;
	MY_ASSERT(open_text(srcname, kernel_src), "\nCannot open \'%s\'\n", srcname);
	auto k_src=kernel_src.data();
	auto k_len=kernel_src.size();
	ocl_program=p_clCreateProgramWithSource(ocl_context, 1, (const char**)&k_src, &k_len, &error);	CL_CHECK(error);
	error=p_clBuildProgram(ocl_program, 1, ocl_devices, "-D__OPEN_CL__", nullptr, nullptr);
	if(error)
	{
		error=p_clGetProgramBuildInfo(ocl_program, ocl_devices[0], CL_PROGRAM_BUILD_LOG, 0, nullptr, &retlen);
		std::string log(retlen+1, '\0');
		error=p_clGetProgramBuildInfo(ocl_program, ocl_devices[0], CL_PROGRAM_BUILD_LOG, log.size(), &log[0], &retlen);
		MY_ASSERT(false, "\nOpenCL Compilation failed:\n%s\n", log.c_str());
		return false;
	}

	for(int k=0;k<OCL_NKERNELS;++k)
		kernels[k].extract(kernelnames[k]);
	return true;
}
void				ocl_finish()
{
	int error=0;
	for(auto it=bufferinfo.begin();it!=bufferinfo.end();++it)
	{
		error=p_clReleaseMemObject(it->first);CL_CHECK(error);
	}
	bufferinfo.clear();

	for(int k=0;k<OCL_NKERNELS;++k)
		kernels[k].release();

	error=p_clReleaseProgram(ocl_program);		CL_CHECK(error);

	error=p_clReleaseCommandQueue(ocl_commandqueue);CL_CHECK(error);
	error=p_clReleaseContext(ocl_context);			CL_CHECK(error);
	delete[] ocl_platforms;
	delete[] ocl_devices;
	ocl_platforms=nullptr, ocl_devices=nullptr;
}

void				ocl_call_kernel(int kernel_idx, size_t worksize, CLBuffer *args, int nargs)
{
	int error=0;
	auto func=kernels[kernel_idx].func;
	for(int k=0;k<nargs;++k)
	{
		error=p_clSetKernelArg(func, k, sizeof(cl_mem), &args[k].handle);//CL_CHECK(error);
		MY_ASSERT(!error, "%s: func = %p, arg %d = %p\n", clerr2str(error), func, k, args[k].handle);
	}
	error=p_clEnqueueNDRangeKernel(ocl_commandqueue, func, 1, nullptr, &worksize, nullptr, 0, nullptr, nullptr);CL_CHECK(error);
}