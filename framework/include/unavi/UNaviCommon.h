/** \brief 
 * UNaviCommon.h
 *  Created on: 2014-9-3
 *      Author: li.lei
 *  brief: 
 */

#ifndef UNAVICOMMON_H_
#define UNAVICOMMON_H_
#include <cstring>
#include <cstdio>
#include <pthread.h>
#include <cstdlib>
#include <cstdarg>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <error.h>
#include <deque>
#include <vector>
#include <map>
#include <set>
#include <ext/hash_map>
#include <iostream>
#include <sstream>
#include <jansson.h>

#define UNAVI_NMSP_BEGIN namespace unavi {
#define UNAVI_NMSP_END }

using namespace __gnu_cxx;

#define UNAVI_MAX_WORKER 128
#define UNAVI_MAX_IOSVC 64

namespace unavi {};

namespace __gnu_cxx {
	template<>
	struct hash<std::string>
	{
		size_t operator()(const std::string& obj) const
		{
			return __stl_hash_string(obj.c_str());
		}
	};
}

#endif /* UNAVICOMMON_H_ */
