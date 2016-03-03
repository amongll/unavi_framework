/*
 * UNaviPollSelect.cpp
 *
 *  Created on: 2014-10-13
 *      Author: li.lei 
 */

#include "unavi/core/UNaviPollSelect.h"
#include "unavi/core/UNaviCycle.h"

using namespace std;
using namespace unavi;

UNaviSelectPoll::UNaviSelectPoll():
	max_fd(-1)
{
	FD_ZERO(&r_reg_set);
	FD_ZERO(&w_reg_set);
}

UNaviSelectPoll::~UNaviSelectPoll()
{
}

bool UNaviSelectPoll::regist_read(int fd)
{
	if (fd == -1) return false;
	if (fd > max_fd) max_fd = fd;
	FD_SET(fd,&r_reg_set);
	return true;
}

bool UNaviSelectPoll::regist_write(int fd)
{
	if (fd == -1) return false;
	if (fd > max_fd) max_fd = fd;
	FD_SET(fd,&w_reg_set);
	return true;
}

bool UNaviSelectPoll::del_read(int fd)
{
	if (fd == -1) return false;
	if ( max_fd == fd ) {
		if ( !FD_ISSET(fd,&w_reg_set) ) {
			int i;
			for( i=max_fd-1; i>=0; i--) {
				if ( FD_ISSET(i,&w_reg_set) || FD_ISSET(i,&r_reg_set) ) {
					break;
				}
			}
			max_fd = i;
		}
	}
	FD_CLR(fd,&r_reg_set);
	return true;
}

bool UNaviSelectPoll::del_write(int fd)
{
	if (fd == -1) return false;
	if ( max_fd == fd ) {
		if ( !FD_ISSET(fd,&r_reg_set) ) {
			int i;
			for( i=max_fd-1; i>=0; i--) {
				if ( FD_ISSET(i,&w_reg_set) || FD_ISSET(i,&r_reg_set) ) {
					break;
				}
			}
			max_fd = i;
		}
	}
	FD_CLR(fd,&w_reg_set);
	return true;
}

int UNaviSelectPoll::poll(UNaviCycle* cycle, int64_t timeout)
{
    int retval, j, numevents = 0;
    UNaviEvent::FileEventMask mask;
    timeval to;
    if (timeout>=0) {
		to.tv_sec = 0;
		to.tv_usec = timeout;
    }

	memcpy(&r_set,&r_reg_set,sizeof(fd_set));
	memcpy(&w_set,&w_reg_set,sizeof(fd_set));
	retval = select(max_fd+1,&r_set,&w_set,NULL,timeout>=0?&to:NULL);

    if (retval > 0) {
    	for (int i=0; i<=max_fd; i++) {
    		mask = 0;
    		if ( FD_ISSET(i,&r_set) )
    			mask |= UNaviEvent::EV_READ;
    		if ( FD_ISSET(i, &w_set) )
    			mask |= UNaviEvent::EV_WRITE;
    		if (mask) {
    			cycle->fire_file_event(i,mask);
    			numevents++;
    		}
    	}
    }

    return numevents;
}
