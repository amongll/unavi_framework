/*
 * UOMBPeerRunMgr.cpp
 *
 *  Created on: 2014-12-30
 *      Author: li.lei
 */

#include "UOMBPeerRunMgr.h"
#include "unavi/frame/UOMBPeerApp.h"

using namespace std;
using namespace unavi;

hash_map<const char*, OMBSvcAppRegTable::Ref> OMBSvcAppRegTable::global;
UNaviRWLock OMBSvcAppRegTable::global_lk;

bool OMBAppRegBucket::have_app(const UOMBPeerIdntfr& id, int64_t& generation, uint32_t& workerid,
	int64_t& should_gen)
{
	UNaviScopedRWLock rlk(&lk,false);
	hash_map<const UOMBPeerIdntfr*, const UOMBPeerApp*>::iterator it =
		peers.find(&id);
	if ( it == peers.end() ) {
		rlk.upgrade();
		should_gen = generation_gen += OMBSvcAppRegTable::bucket_size;
		return false;
	}
	else {
		workerid = it->second->worker_id;
		generation = it->second->generation;
		return true;
	}
}

bool OMBAppRegBucket::have_app(const UOMBPeerApp& dst, bool& generation_match)
{
	UNaviScopedRWLock rlk(&lk,false);
	hash_map<const UOMBPeerIdntfr*, const UOMBPeerApp*>::iterator it =
		peers.find(dst.get_peer_id());
	generation_match = false;
	if ( it == peers.end() )
		return false;

	if (dst.generation == it->second->generation) {
		UOMBPeerApp* pdst = const_cast<UOMBPeerApp*>(&dst);
		pdst->worker_id = it->second->worker_id;
		generation_match = true;
		return true;
	}
	else
		return true;
}

const UOMBPeerApp* OMBAppRegBucket::get_app(const UOMBPeerIdntfr& dst)
{
	UNaviScopedRWLock rlk(&lk,false);
	hash_map<const UOMBPeerIdntfr*, const UOMBPeerApp*>::iterator it =
		peers.find(&dst);
	if ( it == peers.end() )
		return NULL;

	return it->second;
}

OMBAppRegBucket::OMBAppRegBucket(int64_t start_gen):
	generation_gen(start_gen)
{
}

bool OMBAppRegBucket::regist_app(UOMBPeerApp& obj)
{
	UNaviScopedRWLock rlk(&lk,false);
	hash_map<const UOMBPeerIdntfr*, const UOMBPeerApp*>::iterator it =
		peers.find(obj.get_peer_id());

	if ( it == peers.end() ) {
		rlk.upgrade();
		obj.generation = (generation_gen += OMBSvcAppRegTable::bucket_size);
		peers[obj.get_peer_id()] = &obj;
		return true;
	}
	else {
		if ( obj.generation != it->second->generation )
			return false;

		rlk.upgrade();
		it->second = &obj;
		return true;
	}
}

bool unavi::OMBAppRegBucket::regist_app(UOMBPeerApp& obj, int64_t generation)
{
	UNaviScopedRWLock rlk(&lk,false);
	hash_map<const UOMBPeerIdntfr*, const UOMBPeerApp*>::iterator it =
		peers.find(obj.get_peer_id());

	if ( it == peers.end() ) {
		rlk.upgrade();
		obj.generation = generation;
		peers[obj.get_peer_id()] = &obj;
		return true;
	}
	else {
		if ( obj.generation != it->second->generation )
			return false;

		rlk.upgrade();
		it->second = &obj;
		return true;
	}
}

bool unavi::OMBAppRegBucket::unregist_app(const UOMBPeerApp& obj)
{
	UNaviScopedRWLock rlk(&lk, false);
	hash_map<const UOMBPeerIdntfr*, const UOMBPeerApp*>::iterator it =
		peers.find(obj.get_peer_id());
	if ( it == peers.end() )
		return true;

	if ( obj.generation != it->second->generation)
		return false;

	rlk.upgrade();
	peers.erase(it);
	return true;
}

OMBSvcAppRegTable::OMBSvcAppRegTable()
{
	table.resize(bucket_size,NULL);
}

bool OMBSvcAppRegTable::have_app(const UOMBPeerApp& dst, bool& generation_match)
{
	size_t bucket_id = dst.get_peer_id()->hash() % bucket_size;
	OMBSvcAppRegTable* type_table = NULL;
	OMBAppRegBucket* bkt = NULL;
	hash_map<const char*, OMBSvcAppRegTable::Ref>::iterator it;
	generation_match = false;
	UNaviScopedRWLock rlk(&global_lk,false);
	const char* type_name = typeid(*dst.get_peer_id()).name();
	it = global.find(type_name);
	if ( it == global.end() ) {
		return false;
	}
	else {
		type_table = it->second.get();
		bkt = type_table->table[bucket_id];
		if ( bkt == 0)
			return false;
		else {
			rlk.reset();
			return bkt->have_app(dst, generation_match);
		}
	}
}

bool OMBSvcAppRegTable::have_app(const UOMBPeerIdntfr& dst, int64_t& generation, uint32_t& workerid,
	int64_t& should_gen)
{
	size_t bucket_id = dst.hash() % bucket_size;
	OMBSvcAppRegTable* type_table = NULL;
	OMBAppRegBucket* bkt = NULL;
	hash_map<const char*, OMBSvcAppRegTable::Ref>::iterator it;
	UNaviScopedRWLock rlk(&global_lk,false);
	const char* type_name = typeid(dst).name();
	it = global.find(type_name);
	if ( it == global.end() ) {
		rlk.upgrade();

		type_table = new OMBSvcAppRegTable;
		global[type_name].reset(type_table);

		bkt = type_table->table[bucket_id] = new OMBAppRegBucket(bucket_id);
		rlk.reset();
		should_gen = bkt->get_shadow_generation();
		return false;
	}
	else {
		type_table = it->second.get();
		bkt = type_table->table[bucket_id];
		if ( bkt == 0) {
			rlk.upgrade();
			bkt = type_table->table[bucket_id] = new OMBAppRegBucket(bucket_id);
			rlk.reset();
			should_gen = bkt->get_shadow_generation();
			return false;
		}
		else {
			rlk.reset();
			return bkt->have_app(dst, generation,workerid,should_gen);
		}
	}
}

const UOMBPeerApp* OMBSvcAppRegTable::get_app(const UOMBPeerIdntfr& dst)
{
	size_t bucket_id = dst.hash() % bucket_size;
	OMBSvcAppRegTable* type_table = NULL;
	OMBAppRegBucket* bkt = NULL;
	hash_map<const char*, OMBSvcAppRegTable::Ref>::iterator it;
	UNaviScopedRWLock rlk(&global_lk,false);
	const char* type_name = typeid(dst).name();
	it = global.find(type_name);
	if ( it == global.end() ) {
		return false;
	}
	else {
		type_table = it->second.get();
		bkt = type_table->table[bucket_id];
		if ( bkt == 0)
			return NULL;
		else {
			rlk.reset();
			return bkt->get_app(dst);
		}
	}
}

bool OMBSvcAppRegTable::regist_app(UOMBPeerApp& obj)
{
	size_t bucket_id = obj.get_peer_id()->hash() % bucket_size;
	OMBSvcAppRegTable* type_table = NULL;
	OMBAppRegBucket* bkt = NULL;
	hash_map<const char*, OMBSvcAppRegTable::Ref>::iterator it;

	UNaviScopedRWLock rlk(&global_lk,false);
	const char* type_name = typeid(*obj.get_peer_id()).name();
	it = global.find(type_name);
	if ( it == global.end() ) {
		rlk.upgrade();

		type_table = new OMBSvcAppRegTable;
		global[type_name].reset(type_table);

		bkt = type_table->table[bucket_id] = new OMBAppRegBucket(bucket_id);
		rlk.reset();
	}
	else {
		type_table = it->second.get();
		bkt = type_table->table[bucket_id];
		if (bkt)
		{
			rlk.reset();
			return bkt->regist_app(obj);
		}
		else {
			rlk.upgrade();
			bkt = type_table->table[bucket_id] = new OMBAppRegBucket(bucket_id);
			rlk.reset();
		}
	}

	return bkt->regist_app(obj);
}



bool unavi::OMBSvcAppRegTable::regist_app(UOMBPeerApp& obj, int64_t generation)
{
	size_t bucket_id = obj.get_peer_id()->hash() % bucket_size;
	OMBSvcAppRegTable* type_table = NULL;
	OMBAppRegBucket* bkt = NULL;
	hash_map<const char*, OMBSvcAppRegTable::Ref>::iterator it;

	UNaviScopedRWLock rlk(&global_lk,false);
	const char* type_name = typeid(*obj.get_peer_id()).name();
	it = global.find(type_name);
	if ( it == global.end() ) {
		rlk.upgrade();

		type_table = new OMBSvcAppRegTable;
		global[type_name].reset(type_table);

		bkt = type_table->table[bucket_id] = new OMBAppRegBucket(bucket_id);
		rlk.reset();
	}
	else {
		type_table = it->second.get();
		bkt = type_table->table[bucket_id];
		if (bkt)
		{
			rlk.reset();
			return bkt->regist_app(obj, generation);
		}
		else {
			rlk.upgrade();
			bkt = type_table->table[bucket_id] = new OMBAppRegBucket(bucket_id);
			rlk.reset();
		}
	}

	return bkt->regist_app(obj, generation);
}

bool unavi::OMBSvcAppRegTable::unregist_app(const UOMBPeerApp& obj)
{
	size_t bucket_id = obj.get_peer_id()->hash() % bucket_size;
	OMBSvcAppRegTable* type_table = NULL;
	OMBAppRegBucket* bkt = NULL;
	hash_map<const char*, OMBSvcAppRegTable::Ref>::iterator it;

	UNaviScopedRWLock rlk(&global_lk,false);
	const char* type_name = typeid(*obj.get_peer_id()).name();
	it = global.find(type_name);
	if ( it == global.end() ) {
		return true;
	}
	else {
		type_table = it->second.get();
		bkt = type_table->table[bucket_id];
		if (bkt)
		{
			rlk.reset();
			return bkt->unregist_app(obj);
		}
	}

	return true;
}

