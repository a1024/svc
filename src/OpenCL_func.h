#if defined OPENCL_FUNC||OPENCL_FUNC2

OPENCL_FUNC(clGetPlatformIDs);
OPENCL_FUNC(clGetPlatformInfo);
OPENCL_FUNC(clGetDeviceIDs);
OPENCL_FUNC(clGetDeviceInfo);
OPENCL_FUNC(clCreateContext);
OPENCL_FUNC(clReleaseContext);
OPENCL_FUNC(clRetainContext);
OPENCL_FUNC(clGetContextInfo);
OPENCL_FUNC(clCreateCommandQueue);
OPENCL_FUNC(clCreateProgramWithSource);
OPENCL_FUNC(clBuildProgram);
OPENCL_FUNC(clGetProgramBuildInfo);
OPENCL_FUNC(clGetProgramInfo);
OPENCL_FUNC(clCreateProgramWithBinary);
OPENCL_FUNC(clCreateBuffer);
OPENCL_FUNC(clCreateKernel);
OPENCL_FUNC(clSetKernelArg);
//OPENCL_FUNC(clEnqueueFillBuffer);//OpenCL 1.2+
OPENCL_FUNC(clEnqueueWriteBuffer);
OPENCL_FUNC(clEnqueueCopyBuffer);
OPENCL_FUNC(clEnqueueNDRangeKernel);
OPENCL_FUNC(clEnqueueReadBuffer);
OPENCL_FUNC(clFlush);
OPENCL_FUNC(clFinish);
OPENCL_FUNC(clCreateFromGLBuffer);
OPENCL_FUNC2(clCreateFromGLTexture);//OpenCL 1.2+?
OPENCL_FUNC(clReleaseMemObject);
OPENCL_FUNC(clReleaseKernel);
OPENCL_FUNC(clReleaseProgram);
OPENCL_FUNC(clReleaseCommandQueue);

#endif