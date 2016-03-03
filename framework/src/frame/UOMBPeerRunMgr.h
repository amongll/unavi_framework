/*
 * UOMBPeerRunMgr.h
 *
 *  Created on: 2014-12-27
 *      Author: li.lei
 */

#ifndef UOMBPEERRUNMGR_H_
#define UOMBPEERRUNMGR_H_

#include "unavi/frame/UOMBCommon.h"
#include "unavi/util/UNaviRWLock.h"
#include "unavi/util/UNaviRef.h"
#include "unavi/frame/UOMBPeerApp.h"

UNAVI_NMSP_BEGIN

struct OMBAppRegBucket
{
	OMBAppRegBucket(int64_t start_gen);
	bool have_app(const UOMBPeerIdntfr& id, int64_t& generation, uint32_t& workerid,
		int64_t& should_generation);
	bool have_app(const UOMBPeerApp& dst, bool& generation_match);

	const UOMBPeerApp* get_app(const UOMBPeerIdntfr& id);
	bool regist_app(UOMBPeerApp& obj);
	bool regist_app(UOMBPeerApp& obj, int64_t generation);
	bool unregist_app(const UOMBPeerApp& obj);
	int64_t get_shadow_generation();
	UNaviRWLock lk;
	int64_t generation_gen;
	hash_map<const UOMBPeerIdntfr*,const UOMBPeerApp*> peers;
};

struct OMBSvcAppRegTable
{
	typedef UNaviRef<OMBSvcAppRegTable> Ref;
	OMBSvcAppRegTable();
	static bool have_app(const UOMBPeerIdntfr& id, int64_t& generation, uint32_t& workerid, int64_t& should_generation);
	static bool have_app(const UOMBPeerApp& dst, bool& generation_match);

	static const UOMBPeerApp* get_app(const UOMBPeerIdntfr& id);
	static bool regist_app(UOMBPeerApp& obj);
	static bool regist_app(UOMBPeerApp& obj, int64_t generation);
	static bool unregist_app(const UOMBPeerApp& obj);
	std::vector<OMBAppRegBucket*> table;
	static const int bucket_size = 1024;
	static UNaviRWLock global_lk;
	static hash_map<const char*, OMBSvcAppRegTable::Ref> global;
};

UNAVI_NMSP_END

#endif /* UOMBPEERRUNMGR_H_ */
