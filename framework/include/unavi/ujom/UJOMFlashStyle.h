/*
 * UJOMFlashStyle.h
 *
 *  Created on: 2014-12-25
 *      Author: li.lei
 */

#ifndef UJOMFLASHSTYLE_H_
#define UJOMFLASHSTYLE_H_

#include "unavi/ujom/UJOMBaseSvc.h"
#include "unavi/ujom/UJOMPeer.h"

UJOM_NMSP_BEGIN

class FlashConnProxy;
class FlashStreamProxy;

struct FlashClient : public UJOMPeerApp
{
	FlashClient();

	void process_play_busi(const UOMBPeerBusiness& play_info);
	void process_pub_busi(const UOMBPeerBusiness& pub_info);

	virtual UOMBProxy* get_new_proxy(const UOMBSign& obj_type);
	virtual UOMBServant* get_new_servant(const UOMBSign& obj_id);

	std::string pub_stream;
	FlashStreamProxy* pub_proxy;

	FlashConnProxy* cnn_proxy;
	std::vector<FlashStreamProxy*> play_proxies;
	urtmfp::URtmfpPeerId *server_id;
};

class FlashConnSvt;
class FlashStreamSvt;
struct FlashClientServant : public UJOMPeerApp
{
	FlashClientServant();

	void process_pub_busi(const UOMBPeerBusiness& active_pub);

	virtual UOMBProxy* get_new_proxy(const UOMBSign& obj_type);
	virtual UOMBServant* get_new_servant(const UOMBSign& obj_id);
	FlashConnSvt* cnn_svt;
	std::vector<FlashStreamSvt*> stream_svts;
};

class FlashConnProxy : public UJOMBaseProxy
{
public:
	static UOMBSign type_sign;
	FlashConnProxy();

	virtual UOMBCmd* decode_call(const uint8_t* raw, uint32_t sz)throw(UOMBException);

	virtual void init_proxy_id(const UOMBSign& type_sign, const UOMBPeerApp& app, UOMBSign& dst_sign);

	void proc_result(UOMBCmd& cmd, UIChannel ich);
	void proc_status(UOMBCmd& cmd, UIChannel ich);
	void proc_stream_pub(UOMBCmd& cmd, UIChannel ich);

	UIChannel man_ich;
	UOChannel man_och;

	FlashClient* flash;
};

class FlashConnSvt : public UJOMBaseServant
{
public:
	static UOMBSign type_sign;
	FlashConnSvt();

	virtual UOMBCmd* decode_call(const uint8_t* raw, uint32_t sz)throw(UOMBException);
	virtual void parse_signature(const UOMBSign& sign);
	void proc_connect(UOMBCmd& cmd, UIChannel ich);

	UIChannel man_ich;
	UOChannel man_och;

	FlashClientServant* flash;
};

class FlashStreamProxy : public UJOMBaseProxy
{
public:
	static UOMBSign type_sign;
	FlashStreamProxy();

	virtual UOMBCmd* decode_call(const uint8_t* raw, uint32_t sz)throw(UOMBException);

	virtual void init_proxy_id(const UOMBSign& type_sign, const UOMBPeerApp& app, UOMBSign& dst_sign);

	void proc_status(UOMBCmd& cmd, UIChannel ich);
	void proc_stream_data(UOMBCmd& cmd, UIChannel ich);
	void proc_send(UOMBCmd& cmd, UIChannel ich);

	UIChannel info_ich;
	UOChannel ctrl_och;
	UIChannel stream_ich;

	FlashClient* flash;
};

class FlashStreamSvt : public UJOMBaseServant
{
public:
	static UOMBSign type_sign;
	FlashStreamSvt();

	virtual UOMBCmd* decode_call(const uint8_t* raw, uint32_t sz)throw(UOMBException);

	virtual void parse_signature(const UOMBSign& sign);

	void proc_play(UOMBCmd& cmd, UIChannel ich);
	void proc_publish(UOMBCmd& cmd, UIChannel ich);
	void proc_stop(UOMBCmd& cmd, UIChannel ich);
	void proc_send(UOMBCmd& cmd, UIChannel ich);

	UIChannel ctrl_ich;
	UOChannel stream_och;
	UIChannel pub_ich;
	std::string stream_name;

	FlashClientServant* flash;

	void notify_stream_update();

	struct StreamSmartDrawer : public unavi::UNaviEvent
	{
		FlashStreamSvt& target;
		StreamSmartDrawer(FlashStreamSvt& svt);
		virtual void process_event();
	};
	friend class StreamSmartDrawer;
	StreamSmartDrawer monitor;

	UNaviListable join_link;
};

struct JSON
{
	typedef unavi::UNaviRef<JSON> Ref;
	JSON():
		data(NULL)
	{}
	~JSON()
	{
		if(data)
			json_decref(data);
	}
	json_t* data;
};

struct UJOMStreamCache
{
	UJOMStreamCache(const char* nm);
	std::string name;

	void join(FlashStreamSvt* svt);
	void quit(FlashStreamSvt* svt);
	JSON::Ref draw(FlashStreamSvt* svt);

	hash_map<FlashStreamSvt*, uint64_t> players;

	uint64_t head_seq;
	std::vector<JSON::Ref> data;
	std::vector<UNaviList> joiners;

	struct StreamMonitor : public unavi::UNaviEvent
	{
		UJOMStreamCache& target;
		StreamMonitor(UJOMStreamCache& svc);
		virtual void process_event();
	};
	StreamMonitor monitor;
};

class FlashSvc : public UJOMBaseSvc
{
	static UOMBSign svc_sign;
	static void declare();
	static void declare_cmds();

	virtual void run_init();
	virtual void clean_up();

	virtual UOMBPeerApp* get_new_app(bool server);

	virtual unavi::UOMBServant* get_new_servant(const unavi::UOMBSign* servant_sign);
	virtual unavi::UOMBProxy* get_new_proxy(const unavi::UOMBSign* proxy_sign);
	virtual FlashSvc* dup()const {
		return new FlashSvc;
	}

	hash_map<std::string, UJOMStreamCache> thr_local_stream_cache;

	static uint64_t join_stream(FlashStreamSvt* svt, const char* stream_nm);
	static uint64_t publish_stream(const char* stream_nm, std::list<json_t*> objs);
	static uint64_t detach_stream(FlashStreamSvt* svt, const char* stream_nm);
};

struct FlashStream
{
	FlashStream(const char* nm);

	void join(FlashSvc* player);
	void quit(FlashSvc* player);
	void draw(FlashSvc* player, uint64_t prev_off, std::list<json_t*>& out, bool& end);
	void played(FlashSvc* player, uint64_t till);
	void publish(const json_t* data);

	std::string name;

	unavi::UNaviRWLock player_lock;
	hash_map<FlashSvc*,uint64_t> players;

	unavi::UNaviLock till_lock;
	uint64_t min_till;

	unavi::UNaviRWLock data_lock;
	uint64_t head_seq;
	std::vector<json_t*> data;
	bool flag_end;

	static unavi::UNaviRWLock nm_lk;
	static hash_map<std::string, FlashStream> streams;
};

UJOM_NMSP_END

#endif /* UJOMFLASHSTYLE_H_ */
