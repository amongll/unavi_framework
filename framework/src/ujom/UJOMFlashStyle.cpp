/*
 * UJOMFlashStyle.cpp
 *
 *  Created on: 2014-12-25
 *      Author: li.lei
 */

#include "unavi/ujom/UJOMFlashStyle.h"
#include "unavi/rtmfp/URtmfpDebugProto.h"

using namespace std;
using namespace unavi;
using namespace ujom;
using namespace urtmfp;

FlashClient::FlashClient():
	UJOMPeerApp(UOMBPeerApp::PEER_CLIENT),
	pub_proxy(NULL),
	cnn_proxy(NULL),
	server_id(NULL)
{
}

void FlashClient::process_business(const UOMBPeerBusiness& busi)
{
}

UOMBProxy* FlashClient::get_new_proxy(const UOMBSign& obj_type)
{
}

UOMBServant* FlashClient::get_new_servant(const UOMBSign& obj_id)
{
}

FlashClientServant::FlashClientServant():
	UJOMPeerApp(UOMBPeerApp::PEER_CLIENT)
{
}

void FlashClientServant::process_business(const UOMBPeerBusiness& busi)
{
}

UOMBProxy* FlashClientServant::get_new_proxy(const UOMBSign& obj_type)
{
}

UOMBServant* FlashClientServant::get_new_servant(const UOMBSign& obj_id)
{
}

FlashConnProxy::FlashConnProxy()
{
}

UOMBCmd* FlashConnProxy::decode_call(const uint8_t* raw, uint32_t sz)throw(UOMBException)
{
}

void FlashConnProxy::init_proxy_id(const UOMBSign& type_sign,
    const UOMBPeerApp& app, UOMBSign& dst_sign)
{
}

void FlashConnProxy::proc_result(UOMBCmd& cmd, UIChannel ich)
{
}

void FlashConnProxy::proc_status(UOMBCmd& cmd, UIChannel ich)
{
}

void FlashConnProxy::proc_stream_pub(UOMBCmd& cmd, UIChannel ich)
{
}

FlashConnSvt::FlashConnSvt()
{
}

UOMBCmd* FlashConnSvt::decode_call(const uint8_t* raw, uint32_t sz)throw(UOMBException)
{
}

void FlashConnSvt::parse_signature(const UOMBSign& sign)
{
}

void FlashConnSvt::proc_connect(UOMBCmd& cmd, UIChannel ich)
{
}

FlashStreamProxy::FlashStreamProxy()
{
}

UOMBCmd* FlashStreamProxy::decode_call(const uint8_t* raw, uint32_t sz)throw(UOMBException)
{
}

void FlashStreamProxy::init_proxy_id(const UOMBSign& type_sign,
    const UOMBPeerApp& app, UOMBSign& dst_sign)
{
}

void FlashStreamProxy::proc_status(UOMBCmd& cmd, UIChannel ich)
{
}

void FlashStreamProxy::proc_stream_data(UOMBCmd& cmd, UIChannel ich)
{
}

void FlashStreamProxy::proc_send(UOMBCmd& cmd, UIChannel ich)
{
}

FlashStreamSvt::FlashStreamSvt():
	monitor(*this)
{
}

UOMBCmd* FlashStreamSvt::decode_call(const uint8_t* raw, uint32_t sz)throw(UOMBException)
{
}

void FlashStreamSvt::parse_signature(const UOMBSign& sign)
{
}

void FlashStreamSvt::proc_play(UOMBCmd& cmd, UIChannel ich)
{
}

void FlashStreamSvt::proc_publish(UOMBCmd& cmd, UIChannel ich)
{
}

void FlashStreamSvt::proc_stop(UOMBCmd& cmd, UIChannel ich)
{
}

void FlashStreamSvt::proc_send(UOMBCmd& cmd, UIChannel ich)
{
}

void FlashStreamSvt::notify_stream_update()
{
}

FlashStreamSvt::StreamSmartDrawer::StreamSmartDrawer(FlashStreamSvt& svt):
	target(svt)
{
}

void FlashStreamSvt::StreamSmartDrawer::process_event()
{
}

UJOMStreamCache::UJOMStreamCache(const char* nm):
	monitor(*this)
{
}

void UJOMStreamCache::join(FlashStreamSvt* svt)
{
}

void UJOMStreamCache::quit(FlashStreamSvt* svt)
{
}

JSON::Ref UJOMStreamCache::draw(FlashStreamSvt* svt)
{
}

UJOMStreamCache::StreamMonitor::StreamMonitor(UJOMStreamCache& svc):
	target(svc)
{
}

void UJOMStreamCache::StreamMonitor::process_event()
{
}

void FlashSvc::declare()
{
}

void FlashSvc::declare_cmds()
{
}

UOMBPeerApp* FlashSvc::get_new_app(bool server)
{
}

unavi::UOMBServant* FlashSvc::get_new_servant(
    const unavi::UOMBSign* servant_sign)
{
}

unavi::UOMBProxy* FlashSvc::get_new_proxy(
    const unavi::UOMBSign* proxy_sign)
{
}

uint64_t FlashSvc::join_stream(FlashStreamSvt* svt, const char* stream_nm)
{
}

uint64_t FlashSvc::publish_stream(const char* stream_nm,
    std::list<json_t*> objs)
{
}

void ujom::FlashSvc::run_init()
{
}

void ujom::FlashSvc::clean_up()
{
}

uint64_t FlashSvc::detach_stream(FlashStreamSvt* svt,
    const char* stream_nm)
{
}

FlashStream::FlashStream(const char* nm)
{
}

void FlashStream::join(FlashSvc* player)
{
}

void FlashStream::quit(FlashSvc* player)
{
}

void FlashStream::draw(FlashSvc* player, uint64_t prev_off,
    std::list<json_t*>& out, bool& end)
{
}

void FlashStream::played(FlashSvc* player, uint64_t till)
{
}

void FlashStream::publish(const json_t* data)
{
}


