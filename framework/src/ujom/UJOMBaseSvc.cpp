/*
 * UJOMBaseSvc.cpp
 *
 *  Created on: 2014-12-4
 *      Author: dell
 */

#include "unavi/ujom/UJOMBaseSvc.h"
#include "unavi/ujom/UJOMPeer.h"

using namespace urtmfp;
using namespace unavi;
using namespace std;
using namespace ujom;


UJOMPeer* UJOMBaseSvc::new_server_peer(const UOMBPeerIdntfr& id)
{
	return new UJOMPeer;
}

UJOMPeer* UJOMBaseSvc::new_client_peer(const UOMBPeerIdntfr& idfr)
{
	return new UJOMPeer;
}

void UJOMBaseSvc::quit_peer(const UOMBPeerIdntfr& id)
{
	return;
}

void UJOMBaseProxy::ochannel_exception(UOChannel och,
    OChannelErrCode code, void* detail)
{
	UOMBObj::close();
}

void UJOMBaseProxy::ichannel_exception(UIChannel ich,
    IChannelErrCode code, void* detail)
{
	UOMBObj::close();
}

void UJOMBaseProxy::input_idle_timedout()
{
}

void UJOMBaseProxy::proc_close(UOMBCmd& cmd, UIChannel ich)
{
	UOMBObj::close();
}

void UJOMBaseServant::ochannel_exception(UOChannel och,
    OChannelErrCode code, void* detail)
{
	UOMBObj::close();
}

void UJOMBaseServant::ichannel_exception(UIChannel ich,
    IChannelErrCode code, void* detail)
{
	UOMBObj::close();
}

void UJOMBaseServant::input_idle_timedout()
{
}

void UJOMBaseServant::proc_close(UOMBCmd& cmd, UIChannel ich)
{
	UOMBObj::close();
}
