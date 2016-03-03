/*
 * URtmfpMsg.h
 *
 *  Created on: 2014-10-20
 *      Author:	li.lei 
 */

#ifndef URTMFPMSG_H_
#define URTMFPMSG_H_

#include "unavi/rtmfp/URtmfpCommon.h"
#include "unavi/util/UNaviListable.h"
#include "unavi/util/UNaviException.h"

URTMFP_NMSP_BEGIN
class URtmfpSession;
class URtmfpIflow;
struct URtmfpMsg : public UNaviListable
{
	URtmfpMsg(uint32_t flowid = 0xffffffff, const uint8_t* content = NULL, int32_t size = 0);
	const uint32_t flow_id;
	const uint8_t* raw;
	const int32_t length;

private:
	friend class URtmfpSession;
	friend class URtmfpIflow;
	virtual ~URtmfpMsg();

	void reset();
	void set(uint32_t id, uint32_t msg_size)
	{
		uint32_t* pflowid = const_cast<uint32_t*>(&flow_id);
		uint8_t** praw = const_cast<uint8_t**>(&raw);
		*pflowid = id;
		if ( buf_sz-1 >= msg_size )
			return;
		else {
			if (*praw) {
				*praw = (uint8_t*)::realloc(*praw, msg_size + 1);
			}
			else
				*praw = (uint8_t*)::malloc(msg_size + 1);

			buf_sz = msg_size + 1;
		}
	}
	void push_content(const uint8_t* buf,uint32_t len);
	void set(uint32_t flowid, uint8_t* buf, int32_t len, uint32_t bufsz);
	void set(uint32_t flowid, const uint8_t* buf, int32_t len);
	uint32_t buf_sz;
};

URTMFP_NMSP_END

#endif /* URTMFPMSG_H_ */
