/*
 * UNaviPoll.cpp
 *
 *  Created on: 2014-9-26
 *      Author: li.lei
 */

#include "unavi/core/UNaviPoll.h"

#ifdef __linux__
#include "unavi/core/UNaviPollEpoll.h"
#endif
#include "unavi/core/UNaviPollSelect.h"

using namespace std;
using namespace unavi;

UNaviPoll* UNaviPoll::new_poller(PollType type)
{
#ifdef __linux__
	if (type == EPOLL) {
		return new UNaviEPoll();
	}
	else
#endif
	if (type == SELECT) {
		return new UNaviSelectPoll();
	}
	else
		return NULL;
}
