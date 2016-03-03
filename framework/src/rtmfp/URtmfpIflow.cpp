/*
 * URtmfpIflow.cpp
 *
 *  Created on: 2014-11-5
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpIflow.h"
#include "unavi/rtmfp/URtmfpSession.h"
#include "unavi/rtmfp/URtmfpException.h"
#include "unavi/rtmfp/URtmfpFlowChunk.h"
#include <cassert>
#ifndef RTMFP_SESSION_DEBUG
#include "unavi/rtmfp/URtmfpProto.h"
#else
#include "unavi/rtmfp/URtmfpDebugProto.h"
#endif
#include "unavi/rtmfp/URtmfpUtil.h"

using namespace std;
using namespace urtmfp;
using namespace unavi;

void FragAssemble::fill1(uint64_t argseqid, FlowFragmentCtrl type, const uint8_t* content,
			uint32_t len, bool will_pair, bool abondon)
{
	seqid = argseqid;
	if ( len > 0 && abondon==false) {
		if ( type == MSG_LAST_FRAG || type == MSG_WHOLE_FRAG ) {
			has_last = 1;

			if (size == 0 || size - 1 < len) {
				size = len + 1;
				ass_buf = (uint8_t*)::realloc(ass_buf, size);
			}
			ass_buf[len] = 0;

			if ( type == MSG_WHOLE_FRAG )
				has_first = 1;
		}
		else {
			if (type == MSG_FIRST_FRAG)
				has_first = 1;

			if (will_pair) {
				serve_pair = 1;

				if (size < len || ( len*2 > size -1)) {
					size = (len+16)*2 + 64;
					ass_buf = (uint8_t*)::realloc(ass_buf, size);
				}
			}
			else {
				if ( size < len) {
					size = len;
					ass_buf = (uint8_t*)::realloc(ass_buf, size);
				}
			}
		}

		::memcpy(ass_buf,content,len);
		used = len;
	}
	else {
		if ( type == MSG_LAST_FRAG || type == MSG_WHOLE_FRAG ) {
			has_last = 1;
			if ( type == MSG_WHOLE_FRAG )
				has_first = 1;
		}
		else {
			if (type == MSG_FIRST_FRAG)
				has_first = 1;

			if (will_pair) {
				serve_pair = 1;
			}
		}

		if (abondon)
			has_abn = 1;
	}
}

void FragAssemble::fill2(const uint8_t* content, FlowFragmentCtrl type, uint32_t len, bool abondon)
{
	if (!serve_pair) return;
	has_pair = 1;
	if ( len > 0 && abondon == false) {
		if ( type == MSG_LAST_FRAG ) {
			has_last = 1;
			if ( len + used > size - 1) {
				size = len + used + 1;
				ass_buf = (uint8_t*)::realloc(ass_buf, size);
			}
			ass_buf[used+len] = 0;
		}
		else {
			if ( len + used > size ) {
				size = len + used ;
				ass_buf = (uint8_t*)::realloc(ass_buf, size);
			}
		}

		::memcpy(ass_buf + used, content, len);
		used += len;
	}
	else {
		if ( type == MSG_LAST_FRAG )
			has_last = 1;

		if (abondon)
			has_abn = 1;
	}
}



URtmfpIflow::URtmfpIflow(URtmfpSession& ssn,uint32_t flowid)
	:URtmfpFlow(ssn,flowid),
	hole_cnt(0),
	filled_to(0),
	first_fsn(NULL),
	fsn(0),
	ass_sum(0),
	ready_sum(0),
	bitmap_ack_buf(NULL),
	bitmap_ack_bufsz(0),
	range_ack_buf(NULL),
	range_ack_bufsz(0),
	pending_ack(NULL),
	need_ack(false),
	relate_notified(false),
	relating(NULL),
	finish_seq(NULL),
	rejected(false),
	reject_reason(0),
	last_notified_bufsz(session.proto.cur_suggest_recvbuf(session.pipe_id))
{
}

URtmfpIflow::~URtmfpIflow()
{
	if (first_fsn)delete first_fsn;
	if (finish_seq)delete finish_seq;
	FragAssemble* ass;
	while ( (ass=dynamic_cast<FragAssemble*>(ass_list.get_head()))) {
		ass->quit_list();
		session.recycle_ass_node(ass);
	}

	URtmfpMsg* msg;
	while ( (msg=dynamic_cast<URtmfpMsg*>(ready_msgs.get_head()))) {
		msg->quit_list();
		session.recycle_msgnode(msg);
	}

	if ( range_ack_buf ) ::free(range_ack_buf);
	if ( bitmap_ack_buf) ::free(bitmap_ack_buf);
	if ( pending_ack ) {
		UBitmapAckChunk* ba = dynamic_cast<UBitmapAckChunk*>(pending_ack);
		if ( ba )
			delete ba;
		else {
			URangeAckChunk* ra = dynamic_cast<URangeAckChunk*>(pending_ack);
			if (ra) delete ra;
		}
	}
}

void URtmfpIflow::push_ready(URtmfpMsg& msg) {
	ready_msgs.insert_tail(msg);
	ready_sum += msg.length;
	if ( ready_link.empty() ) {
		session.set_ready_iflow(this);
	}
}

void URtmfpIflow::push_data_chunk(uint64_t seqid, uint64_t fsn_off,
	uint8_t frag_opts,
	const uint8_t* content,
	uint32_t size)
{
	FlowFragmentCtrl frag_type = URtmfpUtil::decode_frag_ctrl(frag_opts);
	if ( (frag_opts & Flow_frag_abn_bit) == 0 && (size ==0 || content == NULL) ) {
		URtmfpException e(URTMFP_IDATACHUNK_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
	if ( seqid == 0 || fsn_off > seqid) {
		URtmfpException e(URTMFP_IDATACHUNK_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	/*
	 * 接收缓存管理逻辑：
	 *
	 * 首先的问题是确认对端使用的第一个fsn值，它将作为本端连续接收空间进度filled_to的初值。只有filled_to+1
	 * 及之后的被连续接收的data chunk，并且能形成完整的msg，才会被递送给上层。
	 * -- 其中有一种极特殊情形，按协议不应该出现，即对方初始的若干data chunk提高了fsn值，本端最早收到的第一个
	 * chunk是fsn更高的新包，而更早的包更晚收到，此时，会发生fsn向前偏移的问题，如果此时已经有完整消息被递交
	 * 给用户，那么旧的包不予处理，如果还没有发生过递送，则允许fsn值向前偏移（此时只可能是多data chunk的msg）
	 *
	 * 在每次处理chunk之后，均立刻检查filled_to是否递增，如果当前chunk是完整消息或者分片的尾，则即可将消息
	 * 放入ready队列。
	 *
	 * chunk组装缓冲区只缓冲不能递送给用户的消息的缓冲，包括缓冲开头可能存在的多chunk 未完msg，以及第一个空洞
	 * 之后的所有其他chunk。chunk组装区的管理原则是最少的内存分配和拷贝次数。
	 *
	 * 当本地连续接收空间小于fsn时，在下次ack时，清理那些在fsn之前还未完整的消息，如果存在的话,根据协议，
	 * 累计ack seq不能小于最大接收到的fsn值,在此之前，有较小可能受到填充空洞的包，贸然清理fsn之前的缓冲
	 * 过于激进.
	 *
	 * 在ready消息队列中的消息的大小之和以及当前组装缓冲区的接收chunk的总大小 之和， 和建议的接收缓冲区
	 * 大小进行比较，如果已大于建议值，在没有未完整消息或者空洞时，ack的buf值填零，如果有空洞或者未完整消息，
	 * bufavailave值设置为1
	 */

	uint64_t prev_fsn = fsn;
	fsn = seqid - fsn_off;
	set_first_fsn(fsn);

	if ( prev_fsn > fsn ) {
		//fsn发生了向前的漂移，两种可能：
		//1: 非法的对端
		//2: 后发出的包提高了fsn值，但是后发出的包先到达, 此时采用大的fsn值
		fsn = prev_fsn;
	}

	if ( frag_opts & Flow_frag_fin_bit ) {
		if (finish_seq == NULL)
			finish_seq = new uint64_t(seqid);
		if ( frag_type == MSG_FIRST_FRAG || frag_type == MSG_MIDDLE_FRAG) {
			URtmfpException e(URTMFP_IDATACHUNK_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
		}
		else if ( *finish_seq < seqid ) {
			URtmfpException e(URTMFP_IDATACHUNK_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
		}
	}

	fill_ass_node(frag_opts,seqid,content,size);
}

bool URtmfpIflow::set_first_fsn(uint64_t fsn)
{
	if ( first_fsn == NULL ) {
		//收到第一个chunk时
		first_fsn = new uint64_t(fsn);
		filled_to = fsn;
		return true;
	}
	else if ( fsn < *first_fsn ) {
		//filled_to对应chunk或者是ass_list中，或者是已经放入ready_msg中的chunk
		if ( filled_to > *first_fsn ) { //肯定已经接收了连续的从first_fsn到filled_to的chunk
			if ( ass_sum == 0) {
				//已经发生过把消息递送给用户的情形
				return false;
			}
			else {
				//filled_to还指向ass_list中的chunk
				FragAssemble* head = dynamic_cast<FragAssemble*>(ass_list.get_head());
				if ( head->seqid != *first_fsn + 1) //已经有部分buf形成完整的消息地送给了用户
					return false;
			}
		}
		filled_to = *first_fsn = fsn;
		return true;
	}
	else {
		//不做处理，从历史fsn中已获得了更小的序列号
		return true;
	}
}

void URtmfpIflow::fill_ass_node(uint8_t opts, uint64_t seqid,
		const uint8_t* content,
		uint32_t sz)
{
	FlowFragmentCtrl type = URtmfpUtil::decode_frag_ctrl(opts);
	bool is_abondon = opts & Flow_frag_abn_bit;
	BindPair bind = get_bind(seqid);
	FragAssemble* new_ass = NULL;
	uint64_t prev_filled = filled_to;

	need_ack = true;

	if ( seqid <= filled_to ) {
		session.enable_emerge_ack(); //收到重发的包
		if ( is_abondon == false )
			return;
	}

	if (bind.prev_node) {
		if ( bind.prev_node->seqid == seqid ) {
			session.enable_emerge_ack(); //收到重发的包
			if ( is_abondon )
				bind.prev_node->set_abondon();
			return;
		}
		else if ( bind.prev_node->seqid + 1 == seqid ) {
			if ( bind.prev_node->has_pair  ) {
				session.enable_emerge_ack();
				if ( is_abondon )
					bind.prev_node->set_abondon();
				return;
			}

			if ( bind.prev_node->has_last ) {
				//检查非法包
				if ( type != MSG_WHOLE_FRAG && type != MSG_FIRST_FRAG) {
					URtmfpException e(URTMFP_IDATACHUNK_ERROR);
					uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
				}

				new_ass = session.get_new_assnode();
				if ( type == MSG_FIRST_FRAG ) {
					new_ass->fill1(seqid,type,content,sz,true, is_abondon);
				}
				else {
					new_ass->fill1(seqid,type,content,sz,false, is_abondon);
				}

				ass_list.insert_after(*bind.prev_node, *new_ass);
				ass_sum += sz;
			}
			else {
				if ( type == MSG_WHOLE_FRAG || type == MSG_FIRST_FRAG) {
					URtmfpException e(URTMFP_IDATACHUNK_ERROR);
					uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
				}

				bind.prev_node->fill2(content,type,sz, is_abondon);
				ass_sum += sz;
			}

			if ( bind.prev_node->seqid == filled_to ) {
				//可能未到达真实的fsn处
				filled_to++;
				FragAssemble* ck_ass = bind.next_node;
				while(ck_ass) {
					if ( ck_ass->seqid == filled_to + 1) {
						if (ck_ass->has_pair) {
							filled_to+=2;
						}
						else {
							filled_to++;
						}
						ck_ass = dynamic_cast<FragAssemble*>(ass_list.get_next(ck_ass));
						continue;
					}
					else
						break;
				}
			}

		hole_check1:
			if (bind.next_node) {
				if (bind.next_node->seqid - 1 == seqid)
					hole_cnt--;
			}
		}
		else { //与前继节点不相邻
			if (bind.next_node) {
				//有后继节点, 空洞保持
				if ( bind.next_node->seqid - 1 > seqid) {
					new_ass = session.get_new_assnode();
					if ( type == MSG_FIRST_FRAG || type == MSG_MIDDLE_FRAG ) {
						new_ass->fill1(seqid,type,content,sz,true, is_abondon);
					}
					else {
						new_ass->fill1(seqid,type,content,sz,false, is_abondon);
					}

					ass_list.insert_after(*bind.prev_node, *new_ass);
					ass_sum += sz;

					//新增了空洞
					hole_cnt++;
				}
				else {
					if ( type == MSG_WHOLE_FRAG || type == MSG_LAST_FRAG) {
						if (!bind.next_node->has_first) {
							URtmfpException e(URTMFP_IDATACHUNK_ERROR);
							uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
						}
					}
					else if ( bind.next_node->has_first ) {
						URtmfpException e(URTMFP_IDATACHUNK_ERROR);
						uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
					}

					new_ass = session.get_new_assnode();
					new_ass->fill1(seqid,type,content,sz,false, is_abondon);

					ass_list.insert_after(*bind.prev_node, *new_ass);
					ass_sum += sz;
				}
			}
			else { //与前一个节点不相邻，前一个节点是尾部
				new_ass = session.get_new_assnode();
				if ( type == MSG_FIRST_FRAG || type == MSG_MIDDLE_FRAG ) {
					new_ass->fill1(seqid,type,content,sz,true, is_abondon);
				}
				else {
					new_ass->fill1(seqid,type,content,sz,false, is_abondon);
				}

				ass_list.insert_after(*bind.prev_node, *new_ass);
				ass_sum += sz;

				if ( bind.prev_node->seqid + 2 == seqid ) {
					if (bind.prev_node->has_pair == 0) {
						hole_cnt++;
					}
				}
				else {
					hole_cnt++;
				}
			}
		}
	}
	else if (bind.next_node) { //有后继节点，但是没有前驱节点

		if ( seqid + 1 == bind.next_node->seqid ) {
			//紧邻后继节点
			if ( type == MSG_WHOLE_FRAG || type == MSG_LAST_FRAG ) {
				if ( !bind.next_node->has_first ) {
					URtmfpException e(URTMFP_IDATACHUNK_ERROR);
					uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
				}
			}
			else if ( bind.next_node->has_first ) {
				URtmfpException e(URTMFP_IDATACHUNK_ERROR);
				uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
			}

			new_ass = session.get_new_assnode();
			new_ass->fill1(seqid, type, content, sz, false, is_abondon);

			ass_list.insert_before(*bind.next_node,*new_ass);
			ass_sum += sz;

			// fiiled_to最小为0
			if ( filled_to + 1 == seqid) {
				//空洞被填补
				hole_cnt --;
				filled_to++; //可能未到达真实的fsn处
				FragAssemble* ck_nd = bind.next_node;
				while(ck_nd) {
					if ( ck_nd->seqid == filled_to+1) {
						if ( ck_nd->has_pair ) {
							filled_to+=2;
						}
						else {
							filled_to++;
						}
						ck_nd = dynamic_cast<FragAssemble*>(ass_list.get_next(ck_nd));
						continue;
					}
					else
						break;
				}
			}
		}
		else { //与后继节点有空洞
			new_ass = session.get_new_assnode();
			if ( type == MSG_FIRST_FRAG || type == MSG_MIDDLE_FRAG ) {
				new_ass->fill1(seqid, type, content, sz, true, is_abondon);
			}
			else {
				new_ass->fill1(seqid, type, content, sz, false, is_abondon);
			}

			ass_list.insert_before(*bind.next_node,*new_ass);
			ass_sum += sz;

			//此处，filled_to 肯定小于seqid
			if ( filled_to + 1 != seqid ) {
				hole_cnt++; //新增了空洞
			}
			else {
				filled_to++;
			}
		}
	}
	else {
		new_ass = session.get_new_assnode();
		if ( type == MSG_FIRST_FRAG || type == MSG_MIDDLE_FRAG) {
			new_ass->fill1(seqid, type, content, sz, true, is_abondon);
		}
		else
			new_ass->fill1(seqid, type, content, sz, false, is_abondon);

		ass_list.insert_head(*new_ass);
		ass_sum += sz;

		if (filled_to + 1 < seqid ) {
			hole_cnt = 1;
		}
		else {
			filled_to++;
		}
	}

check_fillto:
	if ( filled_to == prev_filled)
		return;

	if ( type != MSG_WHOLE_FRAG && type != MSG_LAST_FRAG )
		return;

	//可能有可用的消息
	//从头部开始遍历, 如果头没有start标记，则找到有start标记的第一个node
	//之前的Node全部丢弃
	new_ass = dynamic_cast<FragAssemble*>(ass_list.get_head());
	FragAssemble* pending_ass = new_ass;
	bool has_abondon = false;

	do {
		if ( new_ass->has_abn ) has_abondon = true;

		if ( new_ass->has_last ) { // new_ass 肯定有has_last的情形
			if (pending_ass->has_first==0) {
				has_abondon = false;
				do {
					pending_ass->quit_list();
					ass_sum -= pending_ass->used;
					if ( pending_ass == new_ass) {
						session.recycle_ass_node(pending_ass);
						new_ass = pending_ass = dynamic_cast<FragAssemble*>(ass_list.get_head());
						break;
					}
					else {
						session.recycle_ass_node(pending_ass);
						pending_ass = dynamic_cast<FragAssemble*>(ass_list.get_head());
					}
				} while(true);
			}
			else {
				if ( new_ass == pending_ass ) {
					pending_ass->quit_list();
					ass_sum -= pending_ass->used;
					if (!has_abondon) {
						URtmfpMsg* new_msg = session.get_new_msgnode();
						new_msg->set(flow_id, pending_ass->ass_buf, pending_ass->used, pending_ass->size);
						push_ready(*new_msg);
						pending_ass->ass_buf = NULL;
						pending_ass->size = 0;
					}
					else {
						has_abondon = false;
					}

					session.recycle_ass_node(pending_ass);
					new_ass = pending_ass = dynamic_cast<FragAssemble*>(ass_list.get_head());
				}
				else {
					new_ass = pending_ass;

					if ( has_abondon == false ) {
						uint32_t msg_sz = 0;
						do {
							msg_sz += new_ass->used;
							if ( new_ass->has_last )
								break;
							new_ass = dynamic_cast<FragAssemble*>(ass_list.get_next(new_ass));
						} while(true);

						URtmfpMsg* new_msg = session.get_new_msgnode();
						new_msg->set(flow_id,msg_sz);
						ass_sum -= msg_sz;

						do {
							new_msg->push_content(pending_ass->ass_buf, pending_ass->used);
							pending_ass->quit_list();

							if ( pending_ass == new_ass ) {
								session.recycle_ass_node(pending_ass);
								push_ready(*new_msg);
								new_ass = pending_ass = dynamic_cast<FragAssemble*>(ass_list.get_head());
								break;
							}
							else {
								session.recycle_ass_node(pending_ass);
								pending_ass = dynamic_cast<FragAssemble*>(ass_list.get_head());
							}
						} while( true);
					}
					else {
						do {
							pending_ass->quit_list();
							ass_sum -= pending_ass->used;
							if ( pending_ass == new_ass ) {
								session.recycle_ass_node(pending_ass);
								new_ass = pending_ass = dynamic_cast<FragAssemble*>(ass_list.get_head());
								has_abondon = false;
								break;
							}
							else {
								session.recycle_ass_node(pending_ass);
								pending_ass = dynamic_cast<FragAssemble*>(ass_list.get_head());
							}
						} while( true);
					}
				}
			}
		}
		else {
			new_ass = dynamic_cast<FragAssemble*>(ass_list.get_next(new_ass));
		}

		if ( new_ass == NULL || new_ass->seqid > filled_to ) {
			break;
		}
	} while(true);
}

URtmfpIflow::BindPair	URtmfpIflow::get_bind(uint64_t seqid)
{
	BindPair ret;
	ret.next_node = dynamic_cast<FragAssemble*>(ass_list.get_head());
	ret.prev_node = dynamic_cast<FragAssemble*>(ass_list.get_tail());

	if ( ret.next_node == NULL ) return ret;

	if ( ret.next_node->seqid > seqid) {
		ret.prev_node = NULL;
	}
	else if ( seqid >= ret.prev_node->seqid ) {
		ret.next_node = NULL;
	}
	else {
		if ( seqid - ret.next_node->seqid < ret.prev_node->seqid - seqid ) {
			//从头遍历
			ret.prev_node = NULL;
			do {
				if ( ret.next_node->seqid <= seqid ) {
					ret.prev_node = ret.next_node;
					ret.next_node = dynamic_cast<FragAssemble*>(ass_list.get_next(ret.prev_node));
					continue;
				}
				else
					break;
			} while ( ret.next_node );
		}
		else {
			ret.next_node = NULL;
			do {
				if ( ret.prev_node->seqid > seqid ) {
					ret.next_node = ret.prev_node;
					ret.prev_node = dynamic_cast<FragAssemble*>(ass_list.get_prev(ret.next_node));
					continue;
				}
				else
					break;
			} while( ret.prev_node );
		}
	}

	return ret;
}

void URtmfpIflow::try_msg_afterFSN()
{
	//检查是否有完整的消息
	FragAssemble* pend_h = NULL, *pend_t = NULL, *h = NULL;
	h = dynamic_cast<FragAssemble*>(ass_list.get_head());
	bool has_abn = false;
	do {
		if ( h->has_abn ) has_abn = true;

		if ( pend_h == NULL) pend_h = h;
		if ( h->has_last == 0) {
			h = dynamic_cast<FragAssemble*>(ass_list.get_next(h));
			continue;
		}
		else
			pend_t = h;

		if (pend_h->has_first) {
			if (pend_h == pend_t) {
				ass_sum -= pend_t->used;
				pend_t->quit_list();
				if (has_abn == false) {
					URtmfpMsg* msg = session.get_new_msgnode();
					msg->set(flow_id, pend_t->ass_buf, pend_t->used, pend_t->size);
					pend_t->ass_buf = NULL;
					pend_t->size = 0;
					push_ready(*msg);
				}
				else {
					has_abn = false;
				}
				session.recycle_ass_node(pend_t);
			}
			else {
				pend_t = pend_h;
				if ( has_abn == false ) {
					URtmfpMsg* msg = session.get_new_msgnode();
					uint32_t msg_sz = 0;
					do {
						msg_sz += pend_t->used;
						if (pend_t->has_last)
							break;
						pend_t = dynamic_cast<FragAssemble*>(ass_list.get_next(pend_t));
					} while (true);

					ass_sum -= msg_sz;

					msg->set(flow_id, msg_sz);
					do {
						msg->push_content(pend_h->ass_buf, pend_h->used);
						pend_h->quit_list();
						session.recycle_ass_node(pend_h);
						if ( pend_h == pend_t ) {
							push_ready(*msg);
							break;
						}
						else {
							pend_h = dynamic_cast<FragAssemble*>(ass_list.get_head());
							continue;
						}
					} while(true);
				}
				else {
					do {
						pend_h->quit_list();
						ass_sum -= pend_h->used;
						session.recycle_ass_node(pend_h);
						if ( pend_h == pend_t ) {
							break;
						}
						else {
							pend_h = dynamic_cast<FragAssemble*>(ass_list.get_head());
							continue;
						}
					} while(true);
					has_abn = false;
				}
			}

			pend_h = pend_t = NULL;
		}
		else {
			do {
				pend_h->quit_list();
				ass_sum -= pend_h->used;
				session.recycle_ass_node(pend_h);
				if ( pend_h == pend_t ) {
					break;
				}
				else {
					pend_h = dynamic_cast<FragAssemble*>(ass_list.get_head());
					continue;
				}
			} while(true);
			has_abn = false;
			pend_h = pend_t = NULL;
		}

		h = dynamic_cast<FragAssemble*>(ass_list.get_head());
		if ( h == NULL || h->seqid > filled_to)
			break;
	} while ( true );

	return;
}

void URtmfpIflow::clean_ass_beforeFSN()
{
	if ( filled_to >= fsn ) return;
	FragAssemble* h = NULL, *n = NULL, *pend_h = NULL;

	//找到连续空间的尾节点, 此处ass_list肯定有内容
	FragAssemble* succ_tail = h = dynamic_cast<FragAssemble*>(ass_list.get_head());
	while ( (n = dynamic_cast<FragAssemble*>(ass_list.get_next(succ_tail)) ) ) {
		if ( (succ_tail->has_pair && succ_tail->seqid + 2 == n->seqid) ||
			( !succ_tail->has_pair && succ_tail->seqid + 1 == n->seqid )) {
			succ_tail = n;
			continue;
		}
		else
			break;
	}

	if ( filled_to < h->seqid - 1) { //如果ass缓冲头有空洞，那么只可能是这个条件

		if ( fsn < h->seqid - 1) {
			filled_to = fsn;
			return;
		}

		//头部的空洞消失
		hole_cnt--;
		if ( succ_tail->has_pair )
			filled_to = succ_tail->seqid + 1;
		else
			filled_to = succ_tail->seqid;
	}

	//连续空间包含了fsn
	if ( succ_tail->seqid >= fsn || (succ_tail->has_pair && succ_tail->seqid+1 == fsn)) {
		try_msg_afterFSN();
		return;
	}
	else { //连续空间的结尾，在fsn之前
		do {
			//首先从头清理至连续空间结尾处
			h = dynamic_cast<FragAssemble*>(ass_list.get_head());
			do {
				ass_sum -= h->used;
				h->quit_list();
				if ( h == succ_tail ) {
					session.recycle_ass_node(h);
					succ_tail = pend_h = dynamic_cast<FragAssemble*>(ass_list.get_head());
					break;
				}
				else {
					session.recycle_ass_node(h);
					h = dynamic_cast<FragAssemble*>(ass_list.get_head());
					continue;
				}
			} while(true);

			if ( pend_h ) {
				while ( (n = dynamic_cast<FragAssemble*>(ass_list.get_next(succ_tail))) ) {
					if ( (succ_tail->has_pair && succ_tail->seqid + 2 == n->seqid) ||
						( !succ_tail->has_pair && succ_tail->seqid + 1 == n->seqid )) {
						succ_tail = n;
						continue;
					}
					else
						break;
				}

				hole_cnt--;
				if ( succ_tail->has_pair )
					filled_to = succ_tail->seqid + 1;
				else
					filled_to = succ_tail->seqid;

				if ( succ_tail->seqid >= fsn || (succ_tail->has_pair && succ_tail->seqid+1 == fsn)) {
					try_msg_afterFSN();
					return;
				}
				//继续清理空洞
			}
			else {
				//assbuf已经为空
				filled_to = fsn;
				return;
			}
		}
		while(true);
	}
}

void URtmfpIflow::fill_bm(int bitpos, int cnt, bool set)
{
	uint8_t* pbyte = bitmap_ack_buf + bitpos/8;
	bitpos %= 8;

	while(cnt--) {

		if ( set ) {
			*pbyte |= (uint8_t)0x1<< (7 - bitpos);
		}
		else {
			*pbyte &= ~((uint8_t)0x1<<(7 - bitpos));
		}

		if (bitpos==7) {
			bitpos = 0;
			pbyte++;
		}
		else
			bitpos++;
	}
}

void URtmfpIflow::get_ack_content(const uint8_t*& range_raw, uint32_t& range_sz,
	const uint8_t*& bm_raw, uint32_t& bm_sz)
{
	int bitmap_ack_pos = 0, range_ack_pos = 0;
	if (bitmap_ack_bufsz == 0) {
		bitmap_ack_bufsz = 128;
		bitmap_ack_buf = (uint8_t*)malloc(bitmap_ack_bufsz);
	}
	if (range_ack_bufsz == 0) {
		range_ack_bufsz = 128;
		range_ack_buf = (uint8_t*)malloc(range_ack_bufsz);
	}

	FragAssemble* head_ass = dynamic_cast<FragAssemble*>(ass_list.get_head());
	if ( head_ass == NULL ) {
		range_raw = NULL;
		range_sz = 0;
		bm_raw = NULL;
		bm_sz = 0;
		return;
	}

	FragAssemble* succ_tail = head_ass, *succ_head = head_ass, *n=NULL;
	while ( (n = dynamic_cast<FragAssemble*>(ass_list.get_next(succ_tail))) ) {
		if ( (succ_tail->has_pair && succ_tail->seqid + 2 == n->seqid) ||
			( !succ_tail->has_pair && succ_tail->seqid + 1 == n->seqid )) {
			succ_tail = n;
			continue;
		}
		else
			break;
	}

	n = dynamic_cast<FragAssemble*>(ass_list.get_tail());

	uint32_t hole_sz = 0;
	int bit_sz;

	if ( n->has_pair )
		bit_sz = n->seqid  - filled_to - 1;
	else
		bit_sz = n->seqid - filled_to - 2;

	if ( filled_to < head_ass->seqid ) {
		assert(filled_to < head_ass->seqid - 1);

		int bm_sz = 0;
		if ( bit_sz % 8)
			bm_sz = bit_sz/8 + 1;
		else
			bm_sz = bit_sz/8;

		if ( bitmap_ack_bufsz < bm_sz) {
			bitmap_ack_bufsz = bm_sz;
			bitmap_ack_buf = (uint8_t*)::realloc(bitmap_ack_buf, bitmap_ack_bufsz);
		}

		//描述头部的空洞
		hole_sz = head_ass->seqid - filled_to - 2;
		range_ack_pos += URtmfpUtil::vln_encode(hole_sz, range_ack_buf);

		bit_sz = 0;
		fill_bm(bit_sz, hole_sz, false);
		bit_sz += hole_sz;

		if (succ_tail->has_pair)
			hole_sz = succ_tail->seqid - succ_head->seqid + 1;
		else
			hole_sz = succ_tail->seqid - succ_head->seqid ;

		//连续空间
		if ( range_ack_pos + URtmfpUtil::vln_length(hole_sz) > range_ack_bufsz) {
			range_ack_bufsz += 128;
			range_ack_buf = (uint8_t*)::realloc(range_ack_buf,range_ack_bufsz);
		}
		range_ack_pos += URtmfpUtil::vln_encode(hole_sz, range_ack_buf+range_ack_pos);

		fill_bm(bit_sz, hole_sz+1, true);
		bit_sz += hole_sz+1;
	}
	else if ( n == succ_tail ) { //整体无空洞，n此时指向ass_buf的尾部
		range_raw = NULL;
		range_sz = 0;
		bm_raw = NULL;
		bm_sz = 0;
		return;
	}
	else {
		int bm_sz = 0;
		if ( bit_sz % 8)
			bm_sz = bit_sz/8 + 1;
		else
			bm_sz = bit_sz/8;

		if ( bitmap_ack_bufsz < bm_sz) {
			bitmap_ack_bufsz = bm_sz;
			bitmap_ack_buf = (uint8_t*)::realloc(bitmap_ack_buf, bitmap_ack_bufsz);
		}

		bit_sz = 0;
	}

	do {
		succ_head = dynamic_cast<FragAssemble*>(ass_list.get_next(succ_tail));
		if (!succ_head) break;

		if ( succ_tail->has_pair)
			hole_sz = succ_head->seqid - succ_tail->seqid - 3;
		else
			hole_sz = succ_head->seqid - succ_tail->seqid - 2;

		//空洞
		if ( range_ack_pos + URtmfpUtil::vln_length(hole_sz) > range_ack_bufsz) {
			range_ack_bufsz += 128;
			range_ack_buf = (uint8_t*)::realloc(range_ack_buf,range_ack_bufsz);
		}
		range_ack_pos += URtmfpUtil::vln_encode(hole_sz, range_ack_buf + range_ack_pos);

		if ( bit_sz == 0) {
			fill_bm(bit_sz, hole_sz, false);
			bit_sz += hole_sz;
		}
		else {
			fill_bm(bit_sz, hole_sz + 1, false);
			bit_sz += hole_sz + 1;
		}

		succ_tail = succ_head;
		while ( (n = dynamic_cast<FragAssemble*>(ass_list.get_next(succ_tail))) ) {
			if ( (succ_tail->has_pair && succ_tail->seqid + 2 == n->seqid) ||
				( !succ_tail->has_pair && succ_tail->seqid + 1 == n->seqid )) {
				succ_tail = n;
				continue;
			}
			else
				break;
		}

		if (succ_tail->has_pair)
			hole_sz = succ_tail->seqid - succ_head->seqid + 1;
		else
			hole_sz = succ_tail->seqid - succ_head->seqid ;

		//连续空间
		if ( range_ack_pos + URtmfpUtil::vln_length(hole_sz) > range_ack_bufsz) {
			range_ack_bufsz += 128;
			range_ack_buf = (uint8_t*)::realloc(range_ack_buf,range_ack_bufsz);
		}
		range_ack_pos += URtmfpUtil::vln_encode(hole_sz, range_ack_buf+range_ack_pos);

		fill_bm(bit_sz, hole_sz+1, true);
		bit_sz += hole_sz+1;
	} while(1);

	range_raw = range_ack_buf;
	range_sz = range_ack_pos;

	bm_raw = bitmap_ack_buf;
	bm_sz = bitmap_ack_pos;
}

const uint8_t* URtmfpIflow::calc_ack_header(uint32_t& sz, uint32_t &buf_notify)
{
	clean_ass_beforeFSN();

	uint32_t sugg_recv_buf = session.proto.cur_suggest_recvbuf(session.pipe_id);
	int64_t left = (int64_t)sugg_recv_buf - ready_sum - ass_sum;

	if ( left < 1024) {
		left = 0;
		if (hole_cnt > 0) {
			left = 1;
		}
		else {
			FragAssemble* tail_ass = dynamic_cast<FragAssemble*>(ass_list.get_tail());
			if (tail_ass) { //没有空洞时，组装缓冲有内容，只可能是有且只有一个未完消息，且其chunk是连续的
				left = 1;
			}
		}
	}
	else {
		left /= 1024;
	}

	buf_notify = left * 1024;

	sz  = URtmfpUtil::vln_encode(flow_id, ack_header);
	sz += URtmfpUtil::vln_encode(left, ack_header + sz);
	sz += URtmfpUtil::vln_encode(filled_to, ack_header + sz);
	return ack_header;
}

uint32_t URtmfpIflow::get_user_meta(const uint8_t*& meta)
{
	vector<URtmfpOpt>::iterator it;
	for ( it = opts.list.begin(); it != opts.list.end(); it++ ) {
		if ( it->type == 0 ) {
			meta = it->value;
			return it->length;
		}
	}
	meta = NULL;
	return 0;
}

void URtmfpIflow::reject(uint64_t reason)
{
	rejected = true;
	reject_reason = reason;
	UFlowExcpChunk chunk(flow_id, reason);
	session.send_sys_chunk(chunk);
}

bool URtmfpIflow::get_relating_opt(int64_t& oflowid)
{
	vector<URtmfpOpt>::iterator it;
	for ( it = opts.list.begin(); it != opts.list.end(); it++ ) {
		if ( it->type == 0x0a ) {
			uint64_t flowid;
			int sz = URtmfpUtil::vln_decode(it->value,flowid);
			if ( sz != it->length ) {
				URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
				uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
			}
			oflowid = flowid;
			return true;
		}
	}
	oflowid = -1;
	return false;
}
