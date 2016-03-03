/*
 * UNaviRtmfpCommon.h
 *
 *  Created on: 2014-10-15
 *      Author: li.lei
 */

#ifndef UNAVIRTMFPCOMMON_H_
#define UNAVIRTMFPCOMMON_H_

#include "unavi/UNaviCommon.h"

#define RTMFP_DEBUG 1

#ifdef RTMFP_DEBUG
#define RTMFP_SESSION_DEBUG 1
#endif

#define URTMFP_NMSP_BEGIN namespace urtmfp {
#define URTMFP_NMSP_END }

using namespace unavi;

URTMFP_NMSP_BEGIN

const int CIPHER_CBC_BLOCK = 16;
const int MAX_VLN_SIZE = 10;

enum ChunkType
{
	IHELLO_CHUNK = 0x30,
	FWD_IHELLO_CHUNK = 0x0f,
	RHELLO_CHUNK = 0x70,
	REDIRECT_CHUNK = 0x71,
	IIKEYING_CHUNK = 0x38,
	RIKEYING_CHUNK = 0x78,
	COOKIE_CHANGE_CHUNK = 0x79,

	PING_CHUNK = 0x01,
	PING_REPLY_CHUNK = 0x41,

	DATA_CHUNK = 0x10,
	NEXT_DATA_CHUNK = 0x11,
	BITMAP_ACK_CHUNK = 0x50,
	RANGES_ACK_CHUNK = 0x51,

	BUFFER_PROBE_CHUNK = 0x18,
	FLOW_EXP_CHUNK = 0x5e,

	CLOSE_CHUNK = 0x0c,
	CLOSE_ACK_CHUNK = 0x4c,

	PADING0_CHUNK = 0x00,
	PADING1_CHUNK = 0xff,

	FRAGMENT_CHUNK = 0x7f,
#ifdef RTMFP_SESSION_DEBUG
	DEBUG_SIMPLE_IHELLO_CHUNK = 0x81,
	DEBUG_SIMPLE_RHELLO_CHUNK = 0x82,
	DEBUG_SIMPLE_RHELLO_ACK_CHUNK = 0x83,
#endif
	INAVLID_CHUNK = 0x0ff
};

enum AddressOrig
{
	UNKNOWN_ADDR = 0x00,
	LOCAL_ADDR = 0x01,
	REMOTE_ADDR = 0x02,
	RELAY_ADDR = 0x03
};

extern const uint8_t Addr_orig_bits[4] ;

enum SessionMod
{
	FORBIDDEN_MOD = 0x0,
	INITIATOR_MOD = 0x1,
	RESPONDER_MOD = 0x2,
	STARTUP_MOD = 0x3
};

extern const uint8_t Session_mod_bits[4];

enum FlowFragmentCtrl
{
	MSG_WHOLE_FRAG = 0x0,
	MSG_FIRST_FRAG = 0x1,
	MSG_LAST_FRAG = 0x2,
	MSG_MIDDLE_FRAG = 0x3
};

extern const uint8_t Flow_frag_ctrl_bits[4];
extern const uint8_t Flow_frag_ctrl_clear_bit;

extern const uint8_t Flow_frag_opt_bit;
extern const uint8_t Flow_frag_abn_bit;
extern const uint8_t Flow_frag_fin_bit;

extern const uint64_t Vln_size_mark[10];

extern int log_id;

URTMFP_NMSP_END

#endif /* UNAVIRTMFPCOMMON_H_ */
