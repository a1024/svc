SVC - A simple video codec

Build on MSVC 2013:
Make a solution with two projects:
- libsvc.dll containing: ac2.cpp, libsvc.cpp, OpenCL_wrap.cpp
- svc.exe containing: lodepng.cpp, main.cpp


Benchmarks
ANS on CPU - block size 64:
	Encode: 61.75 MB/s
	Decode: 122.00 MB/s

ANS on OpenCL-GPU:
block	enc		dec		dec		zero buffer
size					no read		compression ratio
	
8	308.87 MB/s	3127.08 MB/s	6994.14 MB/s	10.638579
16	332.08 MB/s	3547.39 MB/s	9523.92 MB/s	41.913559
32	302.60 MB/s	3031.44 MB/s	6364.22 MB/s	162.593849
64	274.04 MB/s	1629.74 MB/s	2307.20 MB/s	580.312041


TODO:
- Optimize the codec
- Decode to OpenGL texture
- Support sound
