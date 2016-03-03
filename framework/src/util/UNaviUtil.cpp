/** \brief 
 * UNaviUtil.cpp
 *  Created on: 2014-9-5
 *      Author: li.lei
 *  brief: 
 */

#include "unavi/util/UNaviUtil.h"

using namespace unavi;

uint64_t UNaviUtil::get_time_ml()
{
#ifndef WIN32
   timeval t;
   gettimeofday(&t, 0);
   return t.tv_sec * 1000ULL + t.tv_usec/1000;
#else
   LARGE_INTEGER ccf;
   HANDLE hCurThread = ::GetCurrentThread();
   DWORD_PTR dwOldMask = ::SetThreadAffinityMask(hCurThread, 1);
   if (QueryPerformanceFrequency(&ccf))
   {
      LARGE_INTEGER cc;
      if (QueryPerformanceCounter(&cc))
      {
         SetThreadAffinityMask(hCurThread, dwOldMask);
         return (cc.QuadPart * 1000000ULL / ccf.QuadPart);
      }
   }

   SetThreadAffinityMask(hCurThread, dwOldMask);
   return GetTickCount() * 1000ULL;
#endif
}

uint64_t UNaviUtil::get_time_mc()
{
#ifndef WIN32
   timeval t;
   gettimeofday(&t, 0);
   return t.tv_sec * 1000000ULL + t.tv_usec;
#else
   LARGE_INTEGER ccf;
   HANDLE hCurThread = ::GetCurrentThread();
   DWORD_PTR dwOldMask = ::SetThreadAffinityMask(hCurThread, 1);
   if (QueryPerformanceFrequency(&ccf))
   {
      LARGE_INTEGER cc;
      if (QueryPerformanceCounter(&cc))
      {
         SetThreadAffinityMask(hCurThread, dwOldMask);
         return (cc.QuadPart * 1000000ULL / ccf.QuadPart);
      }
   }

   SetThreadAffinityMask(hCurThread, dwOldMask);
   return GetTickCount() * 1000ULL;
#endif
}

bool UNaviUtil::big_ending = check_big_ending();

union _TestBigEnding
{
	uint16_t i;
	uint8_t c;
};

bool UNaviUtil::check_big_ending()
{
	_TestBigEnding b;
	b.i = 0x0102;
	return b.c==0x01;
}
