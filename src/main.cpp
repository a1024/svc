//main.cpp - Test and I/O program for libsvc
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

#include	<string.h>
#include	<vector>
#include	<string>
#include	<sys/stat.h>
#define		STB_IMAGE_IMPLEMENTATION
#include	"stb_image.h"
#include	"lodepng.h"
#include	"libsvc.h"
#ifdef __GNUC__
#include	<x86intrin.h>
#else
#pragma		comment(lib, "libsvc.lib")
#endif

	#define		DEBUG_TEST

	#define		PRINT_PROGRESS
//	#define		DEBUG_GUIDE

//	#define		SINGLE_EXECUTABLE


#define		SIZEOF(STATIC_ARRAY)	(sizeof(STATIC_ARRAY)/sizeof(*(STATIC_ARRAY)))
#define		G_BUF_SIZE		1024
#ifdef SINGLE_EXECUTABLE
extern char	g_buf[G_BUF_SIZE];
#else
char		g_buf[G_BUF_SIZE]={};
#endif
void		exit_success()
{
	printf("\nDone.\n");
#ifndef __linux__
	int k=0;
	scanf_s("%d", &k, 1);
#endif
	exit(0);
}
void		crash(const char *format, ...)
{
	if(format)
	{
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
		printf("\n");
	}
	printf("\nCRASH\n");
#ifndef __linux__
	int k=0;
	scanf_s("%d", &k, 1);
#endif
	exit(1);
}
FILE*		fopen_bin(const char *filename, bool read)
{
	FILE *file=nullptr;
	const char *mode=read?"rb":"wb";
#ifdef __GNUC__
	file=fopen(filename, mode);
#else
	fopen_s(&file, filename, mode);
#endif
	return file;
}
#ifdef __linux__
#include	<dirent.h>
#include	<time.h>
int			get_filenames_from_folder(std::string const &path, const char **extensions, int ext_count, std::vector<std::string> &v)
{
	std::string p2=path;
	if(p2.back()=='\\')
		p2.back()='/';
	else if(p2.back()!='/')
		p2.push_back('/');
	
	struct dirent *dirp;
	DIR *dp=opendir(path.c_str());
	if(!dp)
		crash("Cannot open \'%s\'", path.c_str());
	while(dirp=readdir(dp))
		v.push_back(dirp->d_name);
	closedir(dp);
//	std::sort(files.begin(), files.end());
	return 0;
}
#define		console_buffer_size(...)
#else
#include	<Windows.h>
int			strcmp_ci(const char *s1, const char *s2)
{
	while(*s1&&*s2&&*s1==*s2)
		++s1, ++s2;
	return *s1-*s2;
}
void		get_filenames_from_folder(std::string const &path, const char **extensions, int ext_count, std::vector<std::string> &v)
{
	std::string p2=path;
	if(p2.back()=='\\')
		p2.back()='/';
	else if(p2.back()!='/')
		p2.push_back('/');
	
	_WIN32_FIND_DATAA data;
	void *hSearch=FindFirstFileA((p2+'*').c_str(), &data);//.
	if(hSearch==INVALID_HANDLE_VALUE)
	{
		printf("Failed to open %s", path.c_str());
		return;
	}
	FindNextFileA(hSearch, &data);//..
	for(int fileno=0;FindNextFileA(hSearch, &data);++fileno)
	{
		int len=(int)strlen(data.cFileName);
		for(int k2=0;k2<ext_count;++k2)
		{
			int ext_len=(int)strlen(extensions[k2]);
			if(!strcmp_ci(data.cFileName+len-ext_len, extensions[k2]))
			{
				v.push_back(data.cFileName);
				break;
			}
		}
	}
	FindClose(hSearch);
}
void			console_buffer_size(short x, short y)
{
	COORD coord={x, y};
	_SMALL_RECT Rect={0, 0, x-1, y-1};

    HANDLE Handle=GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleScreenBufferSize(Handle, coord);
    SetConsoleWindowInfo(Handle, TRUE, &Rect);
}
#endif
#ifdef __GNUC__
#define			sprintf_s	snprintf
#endif
inline double	time_sec()
{
#ifdef __linux__
	static struct timespec tp={};
	int error=clock_gettime(CLOCK_REALTIME, &tp);
	return tp.tv_sec+1e-9*tp.tv_nsec;
#else
	static long long t=0;
	static LARGE_INTEGER li={};
	QueryPerformanceFrequency(&li);
	t=li.QuadPart;
	QueryPerformanceCounter(&li);
	return (double)li.QuadPart/t;
#endif
}
int			print_time(double elapsed)
{
	if(elapsed<1)
		return printf("%lf ms", elapsed*1000);
	if(elapsed<60)
		return printf("%lf sec", elapsed);
	int minutes=(int)floor(elapsed/60);
	elapsed-=minutes*60;
	if(elapsed<60*60)
		return printf("%d min %lf sec", minutes, elapsed);
	int hours=minutes/60;
	minutes-=hours*60;
	return printf("%dh %dmin %lfsec", hours, minutes, elapsed);
}
int			print_size(unsigned long long size)
{
	const double gain=1./1024;
	if(size<1024)
		return printf("%lld bytes", size);
	if(size<1024*1024)
		return printf("%.2lf KB", size*gain);
	return printf("%.2lf MB", size*gain*gain);
}
long long	get_filesize(const char *filename)
{
#ifdef __GNUC__
	struct stat info={};
	int error=stat(filename, &info);
#else
	struct _stat64 info={};
	int error=_stat64(filename, &info);
#endif
	if(!error)
		return info.st_size;
	//	return 1+!S_ISREG(info.st_mode);
	return 0;
}
void		path_adjust(std::string &path)
{
	for(int k=0;k<(int)path.size();++k)
		if(path[k]=='\\')
			path[k]='/';
	if(path.size()&&path.back()!='/')
		path.push_back('/');
}
#define		SVC_CHECK()		if(svc_check())crash(0)

void		print_ibuf(const int *buffer, int bw, int bh, int x1, int x2, int y1, int y2)
{
	for(int ky=y1;ky<y2;++ky)
	{
		for(int kx=x1;kx<x2;++kx)
		//	printf("%08X-", buffer[bw*ky+kx]);
			printf("%02X-", buffer[bw*ky+kx]&0xFF);
		printf("\n");
	}
}
void		print_ibuf(const unsigned char *buffer, int bw, int bh, int x1, int x2, int y1, int y2)
{
	for(int ky=y1;ky<y2;++ky)
	{
		for(int kx=x1;kx<x2;++kx)
			printf("%02X-", buffer[bw*ky+kx]&0xFF);
		printf("\n");
	}
}
void		print_xor(int *b1, int *b2, int bw, int bh, int x1, int x2, int y1, int y2)
{
	for(int ky=y1;ky<y2;++ky)
	{
		for(int kx=x1;kx<x2;++kx)
			printf("%02X-", (b1[bw*ky+kx]^b2[bw*ky+kx])&0xFF);
		printf("\n");
	}
}
void		print_xor(unsigned char *b1, unsigned char *b2, int bw, int bh, int x1, int x2, int y1, int y2)
{
	for(int ky=y1;ky<y2;++ky)
	{
		for(int kx=x1;kx<x2;++kx)
			printf("%02X-", (b1[bw*ky+kx]^b2[bw*ky+kx])&0xFF);
		printf("\n");
	}
}
void		print_sbuf(const unsigned char *buffer, unsigned long long bytesize)
{
	auto b2=(const unsigned short*)buffer;
	auto count=bytesize>>1;
	if(count>10000)
		count=10000;
	for(unsigned k=0;k<count;++k)
		printf("%04X-", b2[k]);
	printf("\n");
}
#ifdef DEBUG_TEST
const int
	iw=3, ih=3,
//	iw=5, ih=5,
//	iw=8, ih=8,
//	iw=31, ih=17,
	image_size=iw*ih;
unsigned char buf[image_size]=
{
	0, 0, 0,
	0, 0, 0xFF,
	0, 0, 0xFF,

	//10,   8,  10,   9,   0,
	//22,  19,  16,  15,  -6,
	//24,  16,  22,  21,  -6,
	//24,  15,  28,  16,  -5,
	//13,   0,   5,  10,  -7,

	//182, 181, 183, 183, 185, 186, 182, 184, 191, 189, 189, 188, 190, 191, 192, 192,
	//187, 181, 184, 188, 191, 191, 191, 191, 192, 192, 191, 190, 192, 192, 193, 195,
	//191, 185, 187, 190, 191, 191, 191, 192, 193, 192, 192, 191, 191, 193, 193, 195,
	//194, 190, 189, 191, 190, 192, 192, 193, 194, 194, 194, 194, 194, 193, 193, 195,
	//193, 191, 192, 193, 192, 192, 194, 195, 196, 196, 197, 197, 197, 195, 196, 198,
	//192, 192, 192, 193, 193, 193, 194, 194, 195, 195, 195, 196, 196, 196, 198, 200,
	//193, 193, 193, 194, 195, 196, 195, 195, 195, 195, 195, 196, 196, 197, 198, 199,
	//197, 195, 194, 195, 195, 196, 196, 195, 196, 197, 196, 195, 197, 199, 200, 199,
	//199, 197, 196, 196, 196, 196, 196, 196, 196, 201, 199, 197, 199, 201, 202, 201,
	//200, 198, 197, 198, 198, 199, 199, 198, 197, 199, 198, 198, 198, 200, 201, 201,
	//200, 198, 198, 199, 199, 200, 200, 199, 198, 201, 200, 199, 200, 200, 201, 199,
	//199, 199, 199, 200, 200, 201, 201, 198, 197, 201, 200, 199, 199, 199, 200, 199,
	//200, 200, 200, 200, 201, 202, 202, 199, 199, 202, 201, 200, 200, 200, 200, 200,
	//199, 200, 200, 200, 201, 203, 203, 202, 201, 202, 201, 200, 200, 201, 201, 201,
	//200, 200, 200, 201, 202, 203, 204, 203, 203, 202, 201, 201, 201, 202, 203, 203,
	//201, 201, 202, 202, 203, 205, 205, 205, 204, 203, 202, 202, 202, 203, 204, 204,

	//182, 181, 183, 183, 185, 186, 182, 184, 191, 189, 189, 188, 190, 191, 192, 192, 191, 193, 189, 186, 186, 188, 191, 191, 191, 192, 192, 192, 193, 195, 195,
	//187, 181, 184, 188, 191, 191, 191, 191, 192, 192, 191, 190, 192, 192, 193, 195, 196, 191, 190, 191, 193, 196, 197, 197, 196, 200, 201, 202, 203, 203, 200,
	//191, 185, 187, 190, 191, 191, 191, 192, 193, 192, 192, 191, 191, 193, 193, 195, 194, 194, 191, 192, 193, 194, 195, 196, 195, 198, 198, 198, 199, 199, 199,
	//194, 190, 189, 191, 190, 192, 192, 193, 194, 194, 194, 194, 194, 193, 193, 195, 196, 196, 195, 195, 195, 195, 195, 196, 197, 198, 198, 198, 198, 199, 200,
	//193, 191, 192, 193, 192, 192, 194, 195, 196, 196, 197, 197, 197, 195, 196, 198, 199, 196, 196, 197, 196, 195, 195, 197, 199, 200, 200, 200, 200, 201, 201,
	//192, 192, 192, 193, 193, 193, 194, 194, 195, 195, 195, 196, 196, 196, 198, 200, 199, 196, 196, 196, 195, 195, 195, 197, 199, 199, 200, 201, 202, 202, 202,
	//193, 193, 193, 194, 195, 196, 195, 195, 195, 195, 195, 196, 196, 197, 198, 199, 199, 196, 197, 197, 197, 197, 198, 199, 200, 199, 200, 201, 202, 202, 202,
	//197, 195, 194, 195, 195, 196, 196, 195, 196, 197, 196, 195, 197, 199, 200, 199, 198, 200, 199, 199, 200, 201, 201, 201, 201, 201, 201, 202, 203, 203, 203,
	//199, 197, 196, 196, 196, 196, 196, 196, 196, 201, 199, 197, 199, 201, 202, 201, 199, 202, 202, 201, 202, 203, 204, 203, 202, 202, 202, 203, 203, 204, 205,
	//200, 198, 197, 198, 198, 199, 199, 198, 197, 199, 198, 198, 198, 200, 201, 201, 201, 202, 202, 201, 201, 201, 201, 201, 202, 199, 201, 202, 203, 204, 204,
	//200, 198, 198, 199, 199, 200, 200, 199, 198, 201, 200, 199, 200, 200, 201, 199, 199, 201, 201, 201, 201, 201, 201, 201, 201, 200, 201, 202, 203, 204, 204,
	//199, 199, 199, 200, 200, 201, 201, 198, 197, 201, 200, 199, 199, 199, 200, 199, 199, 201, 201, 202, 202, 202, 202, 202, 201, 201, 201, 201, 202, 204, 205,
	//200, 200, 200, 200, 201, 202, 202, 199, 199, 202, 201, 200, 200, 200, 200, 200, 200, 202, 203, 204, 205, 205, 204, 204, 203, 203, 202, 201, 202, 205, 206,
	//199, 200, 200, 200, 201, 203, 203, 202, 201, 202, 201, 200, 200, 201, 201, 201, 201, 201, 202, 203, 204, 204, 203, 203, 202, 203, 201, 200, 202, 204, 205,
	//200, 200, 200, 201, 202, 203, 204, 203, 203, 202, 201, 201, 201, 202, 203, 203, 202, 202, 202, 203, 203, 204, 203, 203, 203, 205, 204, 203, 204, 205, 206,
	//201, 201, 202, 202, 203, 205, 205, 205, 204, 203, 202, 202, 202, 203, 204, 204, 204, 203, 203, 203, 204, 204, 204, 204, 204, 206, 206, 206, 206, 206, 207,
	//200, 202, 202, 203, 204, 206, 206, 206, 205, 204, 204, 203, 203, 204, 204, 204, 204, 205, 204, 204, 205, 205, 205, 206, 206, 207, 207, 207, 207, 207, 208,

	//0, 0, 0, 0, 0, 1, 3, 4,
	//1, 0, 0, 0, 0, 0, 0, 0,
	//1, 0, 1, 0, 0, 0, 0, 0,
	//3, 2, 1, 0, 0, 0, 0, 0,
	//6, 5, 3, 0, 0, 0, 0, 0,
	//2, 3, 1, 0, 0, 1, 1, 0,
	//0, 1, 1, 0, 2, 5, 5, 2,
	//0, 2, 3, 1, 2, 4, 3, 0,

	//0, 0, 0, 0, 0, 1, 3, 4, 12, 7, 1, 0, 0, 1, 2, 2,
	//1, 0, 0, 0, 0, 0, 0, 0,  8, 4, 1, 0, 0, 2, 2, 0,
	//1, 0, 1, 0, 0, 0, 0, 0,  3, 1, 0, 0, 0, 0, 0, 0,
	//3, 2, 1, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	//6, 5, 3, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	//2, 3, 1, 0, 0, 1, 1, 0,  1, 0, 0, 1, 0, 0, 0, 2,
	//0, 1, 1, 0, 2, 5, 5, 2,  1, 0, 2, 4, 2, 0, 0, 4,
	//0, 2, 3, 1, 2, 4, 3, 0,  0, 0, 3, 6, 4, 0, 1, 5,
	//0, 3, 6, 4, 2, 3, 2, 0,  0, 2, 3, 1, 1, 2, 0, 0,
	//0, 2, 4, 3, 1, 1, 2, 3,  2, 3, 3, 1, 1, 3, 1, 1,
	//3, 3, 4, 4, 2, 1, 2, 6,  2, 3, 2, 0, 1, 3, 4, 4,
	//8, 4, 3, 4, 4, 2, 2, 5,  0, 0, 0, 0, 0, 2, 3, 3,
	//6, 1, 0, 2, 4, 2, 2, 3,  0, 0, 0, 0, 1, 1, 2, 2,
	//1, 0, 0, 0, 3, 3, 1, 0,  0, 0, 0, 0, 1, 0, 1, 2,
	//0, 0, 0, 0, 0, 1, 1, 0,  1, 0, 0, 0, 1, 1, 3, 4,
	//0, 2, 4, 0, 0, 0, 0, 0,  4, 1, 0, 1, 1, 1, 3, 6,

	//0x55555555, 0x55555555,
	//0x55555555, 0x55555555,

	//0xBAADF00D, 0xBAADF00D, 0xBAADF00D, 0xBAADF00D,
	//0xBAADF00D, 0xBAADF00D, 0xBAADF00D, 0xBAADF00D,
	//0xBAADF00D, 0xBAADF00D, 0xBAADF00D, 0xBAADF00D,
	//0xBAADF00D, 0xBAADF00D, 0xBAADF00D, 0xBAADF00D,

	//0xFF020100, 0xFF050403, 0xFF080706, 0xFF0B0A09,
	//0xFF0E0D0C, 0xFF11100F, 0xFF141312, 0xFF171615,
	//0xFF1A1918, 0xFF1D1C1B, 0xFF201F1E, 0xFF232221,
	//0xFF262524, 0xFF292827, 0xFF2C2B2A, 0xFF2F2E2D,

	//0x00010203, 0x04050607,
	//0x08090A0B, 0x0C0D0E0F,
};
unsigned char b2[image_size];
#if 0
static size_t GetStackUsage()//https://stackoverflow.com/questions/1740888/determining-stack-space-with-visual-studio
{
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(&mbi, &mbi, sizeof(mbi));
    // now mbi.AllocationBase = reserved stack memory base address

    VirtualQuery(mbi.AllocationBase, &mbi, sizeof(mbi));
    // now (mbi.BaseAddress, mbi.RegionSize) describe reserved (uncommitted) portion of the stack
    // skip it

    VirtualQuery((char*)mbi.BaseAddress + mbi.RegionSize, &mbi, sizeof(mbi));
    // now (mbi.BaseAddress, mbi.RegionSize) describe the guard page
    // skip it

    VirtualQuery((char*)mbi.BaseAddress + mbi.RegionSize, &mbi, sizeof(mbi));
    // now (mbi.BaseAddress, mbi.RegionSize) describe the committed (i.e. accessed) portion of the stack

    return mbi.RegionSize;
}
#endif
void		test()
{
	svc_set_loglevel(SVC_LOG_PROFILER);
	//buf[iw*3+3]=1;
	//for(int k=0;k<image_size;++k)
	//	buf[k]=k?255/k:255;
	//{
	//	auto p=(unsigned char*)(buf+k);
	//	p[0]=rand();
	//	p[1]=rand();
	//	p[2]=rand();
	//	p[3]=rand();
	//	//p[0]=rand(), p[0]>>=4;
	//	//p[1]=rand(), p[1]>>=4;
	//	//p[2]=rand(), p[2]>>=4;
	//	//p[3]=rand(), p[3]>>=4;
	//}
	//	buf[k]=rand();
	//	buf[k]=k/8;
	//	buf[k]=k;
	//	buf[k]=0x55555555;

	const int nchannels=1;
	const int depth=8;
	printf("src:\n"), print_ibuf(buf, iw, ih, 0, iw, 0, ih);
	auto encoder=svc_enc_start(iw, ih, nchannels, depth, SVC_UINT8, 1, 1);	SVC_CHECK();
	svc_enc_add_frame(encoder, buf);			SVC_CHECK();
//	svc_enc_add_frame(encoder, buf);			SVC_CHECK();
	unsigned long long csize=0;
	auto ptr=svc_enc_finish(encoder, &csize);	SVC_CHECK();
//	print_sbuf(ptr, csize);
	auto cdata=new unsigned char[csize];
	memcpy(cdata, ptr, csize);
	svc_cleanup(encoder);		SVC_CHECK();

	auto decoder=svc_dec_start(cdata, csize);	SVC_CHECK();
	SVCHeader info={};
	svc_dec_get_info(decoder, &info);			SVC_CHECK();
	svc_dec_get_frame(decoder, b2, buf);		SVC_CHECK();
	printf("decoded 1:\n"), print_ibuf(b2, iw, ih, 0, iw, 0, ih);
//	svc_dec_get_frame(decoder, b2, buf);		SVC_CHECK();
	//printf("decoded 2:\n"), print_ibuf(b2, iw, ih, 0, iw, 0, ih);
	printf("xor:\n"), print_xor(buf, b2, iw, ih, 0, iw, 0, ih);
	svc_cleanup(decoder);		SVC_CHECK();

	delete[] cdata;
	exit_success();
}
//void		test2()
//{
//	int b2[100]={};
//	printf("Stack usage: %lld\n", (long long)GetStackUsage());
//}
#endif
void		benchmark(int frame_w, int frame_h, int nframes, bool verify, int loud)
{
	printf("Benchmark\n");
	if(loud)
		svc_set_loglevel(SVC_LOG_PROFILER);
	int iw=frame_w, ih=frame_h, nch=4;
	int framebytes=iw*ih*sizeof(int);
	auto original_image=(unsigned char*)malloc(framebytes);
	memset(original_image, 0, framebytes);

	auto encoder=svc_enc_start(iw, ih, nch, 8, SVC_UINT8, 1, 1);	SVC_CHECK();
	auto s1=time_sec();
	auto t1=__rdtsc();
	for(int k=0;k<nframes;++k)
	{
		if(loud)
			printf("\rEncoding %3d/%3d...\t\t", k+1, nframes);
		svc_enc_add_frame(encoder, original_image);	SVC_CHECK();
	}
	auto t2=__rdtsc();
	auto s2=time_sec();
	unsigned long long csize=0;
	auto ptr=svc_enc_finish(encoder, &csize);	SVC_CHECK();
	printf("Compressed size: %lld bytes, ratio = %lf\n", csize, (double)iw*ih*nch*nframes/csize);
	printf(
		"Encoded %d %dx%dx%d frames in:\n"
		"  %lld cycles, %lf cycles/frame  %lf sec, %lf ms/frame  %.2lf MB/s\n",
		nframes, iw, ih, nch,
		t2-t1, (double)(t2-t1)/nframes,
		s2-s1, 1000.*(s2-s1)/nframes,
		iw*ih*nch*nframes/((s2-s1)*1024*1024));
	auto cdata=new unsigned char[csize];
	memcpy(cdata, ptr, csize);
	svc_cleanup(encoder);		SVC_CHECK();
	auto buf=(unsigned char*)malloc(framebytes);
		
	auto decoder=svc_dec_start(cdata, csize);	SVC_CHECK();
	SVCHeader info={};
	svc_dec_get_info(decoder, &info);			SVC_CHECK();
	s1=time_sec();
	t1=__rdtsc();
	for(int k=0;k<nframes;++k)
	{
		if(loud)
			printf("\rDecoding %3d/%3d...\t\t", k+1, nframes);
		svc_dec_get_frame(decoder, buf, verify?original_image:nullptr);	SVC_CHECK();
	}
	t2=__rdtsc();
	s2=time_sec();
	svc_cleanup(decoder);		SVC_CHECK();
	printf(
		"Decoded in:  %lld cycles, %lf cycles/frame  %lf sec, %lf ms/frame  %.2lf MB/s\n",
		t2-t1, (double)(t2-t1)/nframes,
		s2-s1, 1000.*(s2-s1)/nframes,
		iw*ih*nch*nframes/((s2-s1)*1024*1024));
	free(buf);
	free(original_image);
	delete[] cdata;
}
int			main(int argc, char **argv)
{
	//printf("Stack usage: %lld\n", (long long)GetStackUsage());
	//test2();

	console_buffer_size(120, 9000);
	auto t1=time_sec();
	printf("Simple Video Codec - %s %s\n\n", __DATE__, __TIME__);
	
#ifdef DEBUG_TEST
	test();//
#endif

	bool bm=argc>1&&!strcmp(argv[1], "-bm");
	if(argc!=4&&!bm)
	{
		printf(
			"Usage:\n"
			"\n"
			"svc -enc input output\n"
			"  input:    Path to a folder with PNG images.\n"
			"  output:   Result svc filename\n"
			"\n"
			"svc -dec input output\n"
			"  input:    Source svc filename\n"
			"  output:   Path to a destination folder.\n"
			"            The result frames are numbered PNG files.\n"
			"\n"
			"svc -bm\n"
			"  encodes dummy data then decodes .svc without saving\n"
			"\n"
			"Examples:\n"
			"  svc -enc /path/to/PNGs out.svc\n"
			"  svc -dec in.svc /path/to/dest\n"
			"\n"
			);
		exit_success();
		return 0;
	}
	const char *extensions[]=
	{
		".jpg",
		".png",
	};
	const int ext_count=SIZEOF(extensions);
	SVCContext handle=nullptr;
	int iw=0, ih=0, nch=0;
	unsigned char *original_image=nullptr;
	FILE *file=nullptr;
	//svc_set_loglevel(SVC_LOG_PROFILER);
	//bool dummyenc=!strcmp(argv[1], "-denc"), dummydec=!strcmp(argv[1], "-ddec");
	if(bm)
		benchmark(1920, 1080, 128, false, false);
		//benchmark(128, 128, 2, true, true);
		//benchmark(64, 64, 1, true, true);
	else if(!strcmp(argv[1], "-enc"))
	{
		printf("Encoder\n");
		std::string path=argv[2];
		path_adjust(path);
		printf("src: %s\n", path.c_str());
		printf("dst: %s\n", argv[3]);
		std::vector<std::string> filenames;
		get_filenames_from_folder(path, extensions, ext_count, filenames);
		int nframes=(int)filenames.size();
		if(!nframes)
		{
			printf("Error: no PNG files found in %s\n", argv[2]);
			exit_success();
			return 1;
		}
		printf("Found %d frames\n", nframes);
#ifdef PRINT_PROGRESS
		printf("\rProcessing %d/%d...\t\t", 1, nframes);
#endif
		int nchannels=4;
		//if(dummyenc)
		//{
		//	iw=1920, ih=1080, nch=4;
		//	int framebytes=iw*ih*sizeof(int);
		//	original_image=(unsigned char*)malloc(framebytes);
		//	memset(original_image, 0, framebytes);
		//}
		//else
		//{
			original_image=stbi_load((path+filenames[0]).c_str(), &iw, &ih, &nch, 4);
			if(!original_image)
			{
				printf("Error: no PNG files found in %s\n", argv[2]);
				exit_success();
				return 1;
			}
		//}
		handle=svc_enc_start(iw, ih, nchannels, 8, SVC_UINT8, 1, 1);SVC_CHECK();
		svc_enc_add_frame(handle, original_image);					SVC_CHECK();
		//if(!dummyenc)
			free(original_image);
		printf("\n%dx%d, %d channels\n", iw, ih, nch);
		for(int k=1;k<nframes;++k)
		{
#ifdef PRINT_PROGRESS
			printf("\rProcessing %d/%d...\t\t", k+1, nframes);
#endif
			//if(!dummyenc)
			//{
				int iw2=0, ih2=0, nch2=0;
				original_image=stbi_load((path+filenames[k]).c_str(), &iw2, &ih2, &nch2, 4);
				if(!original_image||iw2!=iw||ih2!=ih)
				{
					if(original_image)
						printf("Error: different frame dimensions: expected %dx%d got %dx%d\n", iw, ih, iw2, ih2);
					else
						printf("Error: failed to open %s\n", (path+filenames[k]).c_str());
					if(original_image)
						free(original_image);
					svc_cleanup(handle);	SVC_CHECK();
					exit_success();
					return 1;
				}
			//}
			int csize=svc_enc_add_frame(handle, original_image);	SVC_CHECK();
			//int usize=iw*ih*nch;
			//printf("\ncompression ratio = %d / %d = %.2lf%%\n", usize, csize, 100.*usize/csize);//
			//if(!dummyenc)
				free(original_image);
		}
		//if(dummyenc)
		//	free(original_image);
		unsigned long long outsize=0;
		auto data=svc_enc_finish(handle, &outsize);		SVC_CHECK();
		printf("Compressed size: ");
		print_size(outsize);
		printf("\n");
		unsigned long long usize=(unsigned long long)iw*ih*nchannels*nframes;
		printf("Ratio = %dx%dx%d*%d / %lld = %lf\n", iw, ih, nchannels, nframes, outsize, (double)usize/outsize);
		//if(!dummyenc)
		//{
			file=fopen_bin(argv[3], false);
			//fopen_s(&file, argv[3], "wb");
			if(!file)
			{
				printf("Error: cannot open %s for writing.\n", argv[3]);
				svc_cleanup(handle);	SVC_CHECK();
				exit_success();
				return 1;
			}
			fwrite(data, 1, outsize, file);
			fclose(file);
		//}
		svc_cleanup(handle);	SVC_CHECK();
	}
	else if(!strcmp(argv[1], "-dec"))
	{
		printf("Decoder\n");
		std::string outpath=argv[3];
		path_adjust(outpath);
		printf("src: %s\n", argv[2]);
		printf("dst: %s\n", outpath.c_str());
		auto bytesize=get_filesize(argv[2]);
		if(!bytesize)
		{
			printf("Error: cannot open %s", argv[2]);
			exit_success();
			return 1;
		}
		file=fopen_bin(argv[2], true);
		//fopen_s(&file, argv[2], "rb");
		unsigned char *data=(unsigned char*)malloc(bytesize);
		fread(data, 1, bytesize, file);
		fclose(file);
		handle=svc_dec_start(data, bytesize);	SVC_CHECK();
		SVCHeader info={};
		svc_dec_get_info(handle, &info);		SVC_CHECK();
		printf("Compressed size: ");
		print_size(bytesize);
		printf("\n");
		printf("%d frames in file\n", info.nframes);
		printf("%dx%d, %d channels, %dbits, %s, %d frames, fps=%d/%d\n", info.w, info.h, info.nchannels, info.depth, svc_format2str(info.srctype), info.nframes, info.fps_num, info.fps_den);
		//switch(info.srctype)
		//{
		//case SVC_UINT8:printf("UINT8\n");break;
		//case SVC_UINT16:printf("UINT16\n");break;
		//case SVC_FLOAT32:printf("FLOAT32\n");break;
		//}
		int frameres=info.w*info.h;
		auto buffer=(unsigned char*)malloc(frameres*sizeof(int));
#ifdef DEBUG_GUIDE
		std::string guidepath="D:/Share Box/Scope/20210428 1 moon/batch 2/";//
	//	std::string guidepath="D:/Share Box/Scope/20220226 1/png/";//
		std::vector<std::string> guidenames;
		get_filenames_from_folder(guidepath, extensions, ext_count, guidenames);//
#endif

		for(int k=0;k<info.nframes;++k)
		{
#ifdef PRINT_PROGRESS
			printf("\rProcessing %d/%d...\t\t", k+1, info.nframes);
#endif
			
#ifdef DEBUG_GUIDE
			int iw2=0, ih2=0, nch2=0;
			original_image=stbi_load((guidepath+guidenames[k]).c_str(), &iw2, &ih2, &nch2, 4);//
			if(!original_image)
				crash("Failed to open %s", (guidepath+guidenames[k]).c_str());
#endif

			svc_dec_get_frame(handle, buffer, original_image);	SVC_CHECK();
			//if(!dummydec)
			//{
				sprintf_s(g_buf, G_BUF_SIZE, "%s%05d.PNG", outpath.c_str(), k+1);
				lodepng::encode(g_buf, buffer, info.w, info.h);
			//}
		}
		svc_cleanup(handle);	SVC_CHECK();
		free(buffer);
		free(data);
	}
	else
		printf("Error: invalid argument \'%s\'", argv[1]);
	auto t2=time_sec();
	printf("\nDone. Elapsed ");
	print_time(t2-t1);
	printf("\n");
	exit_success();
	return 0;
}