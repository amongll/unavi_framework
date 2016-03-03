/*
 * test_ssid_codec.cpp
 *
 *  Created on: 2014-12-12
 *      Author: dell
 */

#include "unavi/rtmfp/URtmfpUtil.h"
#include <assert.h>

using namespace urtmfp;
int main()
{
	uint32_t a = 339;
	uint8_t dst_buf[12] = {0,0,0,0,'a','b','c','d','e','f','g','h'};
	URtmfpUtil::ssid_encode(a ,(const uint8_t*)dst_buf+4, dst_buf);

	assert((a == URtmfpUtil::ssid_decode(dst_buf)));
	return 0;
}



