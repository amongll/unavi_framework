/*
 * UNaviPollSelect.h
 *
 *  Created on: 2014-10-13
 *      Author: li.lei 
 */

#ifndef UNAVIPOLLSELECT_H_
#define UNAVIPOLLSELECT_H_

#include "unavi/core/UNaviPoll.h"

UNAVI_NMSP_BEGIN

class UNaviSelectPoll: public UNaviPoll
{
public:
	UNaviSelectPoll();
	virtual ~UNaviSelectPoll();

	virtual bool regist_read(int fd);
	virtual bool regist_write(int fd);
	virtual bool del_read(int fd);
	virtual bool del_write(int fd);
	virtual int poll(UNaviCycle* cycle, int64_t timeout);
private:
	int max_fd;
	fd_set r_reg_set;
	fd_set w_reg_set;
	fd_set r_set;
	fd_set w_set;
};

UNAVI_NMSP_END

#endif /* UNAVIPOLLSELECT_H_ */
