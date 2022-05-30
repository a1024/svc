//profiler.h - A simple CPU cycle profiler
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
#ifndef PROFILER_H
#define PROFILER_H

//	#define	PROFILER

#ifdef PROFILER

#ifdef __GNUC__
#include<x86intrin.h>
#else
#include<intrin.h>
#endif

#define			PROF_STAGES\
	PROF_LABEL(WASTE)\
	\
	PROF_LABEL(HISTOGRAM)\
	PROF_LABEL(HISTOGRAM_INIT)\
	PROF_LABEL(HISTOGRAM_LOOKUP)\
	PROF_LABEL(PREP)\
	PROF_LABEL(INITIALIZE_STATS)\
	PROF_LABEL(INITIALIZE_DATA)\
	PROF_LABEL(INITIALIZE_SIZES)\
	PROF_LABEL(INITIALIZE_ICDF)\
	PROF_LABEL(ENCODE)\
	PROF_LABEL(DECODE)\
	PROF_LABEL(READ)\
	PROF_LABEL(PACK)\
	PROF_LABEL(DELTA)\
	\
	PROF_LABEL(INITIALIZE)\
	PROF_LABEL(SEND_FRAME)\
	PROF_LABEL(ENCODE_PREP)\
	PROF_LABEL(FETCH)\
	PROF_LABEL(RENORM)\
	PROF_LABEL(UPDATE)\
	\
	PROF_LABEL(ENC_ANALYZE_PLANE)\
	PROF_LABEL(ENC_BYPASS_PLANE)\
	PROF_LABEL(ENC_AC)\
	PROF_LABEL(DEC_BYPASS_PLANE)\
	PROF_LABEL(DEC_AC)

enum			ProfilerStage
{
#define			PROF_LABEL(LABEL)	PROF_##LABEL,
	PROF_STAGES
#undef			PROF_LABEL
	PROF_COUNT,
};

#ifndef PROFILER_IMPLEMENTATION

extern long long prof_cycles[PROF_COUNT], prof_temp;
void			prof_end();

#else

#include		<stdio.h>
#include		<string.h>
const char		*prof_labels[]=
{
#define			PROF_LABEL(LABEL)	#LABEL,
	PROF_STAGES
#undef			PROF_LABEL
};
long long		prof_cycles[PROF_COUNT]={}, prof_temp=0;
void			prof_end()
{
	//static int callcount=0;
	//if(callcount==1)
	//	int LOL_1=0;
	//++callcount;

	int pad=0;
	for(int k=0;k<PROF_COUNT;++k)
	{
		int len=(int)strlen(prof_labels[k]);
		if(pad<len)
			pad=len;
	}
	//pad+=2;

	long long sum=0;
	for(int k=0;k<PROF_COUNT;++k)
		sum+=prof_cycles[k];
	printf("\nPROFILER\nLabel%*s\tcycles\t\tpercentage\n", pad-5, "");
	for(int k=0;k<PROF_COUNT;++k)
	{
		int printed=printf("%s", prof_labels[k]);
		printf("%*s\t%10lld\t%5.2lf%%\n", pad-printed, "", prof_cycles[k], 100.*prof_cycles[k]/sum);
	}
		//printf("%s:\t%lld\t%.2lf%%\n", prof_labels[k], prof_cycles[k], 100.*prof_cycles[k]/sum);
	printf("\n");
}

#endif//PROFILER_IMPLEMENTATION
#undef			PROF_STAGES
#define			PROF_INIT()		memset(prof_cycles, 0, PROF_COUNT*sizeof(long long)), prof_temp=__rdtsc()
#define			PROF(LABEL)		prof_cycles[PROF_##LABEL]+=__rdtsc()-prof_temp, prof_temp=__rdtsc()

#else
#define			PROF_INIT()
#define			PROF(...)
#define			prof_end()
#endif


#endif