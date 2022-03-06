SVC - A simple video codec

Build on MSVC 2013:
Make a solution with two projects:
- libsvc.dll containing: ac2.cpp, libsvc.cpp, OpenCL_wrap.cpp
- svc.exe containing: lodepng.cpp, main.cpp

Benchmark
ANS on CPU:
- Subtract prev frame:
	Encode: 61.75 MB/s
	Decode: 122.00 MB/s

ANS on OpenCL-GPU:
- Subtract prev frame:
	Encode: 589.69 MB/s
	Decode: 697.85 MB/s
- Encode each frame separately:
	Encode: 865.45 MB/s
	Decode: 1222.99 MB/s
