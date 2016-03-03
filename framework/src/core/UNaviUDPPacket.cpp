/*
 * UNaviPacket.cpp
 *
 *  Created on: 2014-9-23
 *      Author: dell
 */

#include "unavi/core/UNaviUDPPacket.h"
#include "unavi/util/UNaviRef.h"
#include "unavi/core/UNaviUDPPipe.h"
#include "unavi/core/UNaviWorker.h"

using namespace std;
using namespace unavi;

const char* UNaviPacketExp::err_str[] = {
	"UDPPacket is not enough for read",
	"UDPPacket address not allowed",
	"UDPPacket seek not allowed"
};

UNaviUDPPacket* UNaviUDPPacket::new_outpacket(uint32_t pipe_id,size_t size)
{
	UNaviUDPPipe* pipe = UNaviUDPPipe::get_pipe(pipe_id);
	if (pipe==NULL)
		return NULL;
	return pipe->work_new_output(size);
}

UNaviUDPPacket* UNaviUDPPacket::new_outpacket(UNaviIOSvc *svc,size_t size)
{
	uint32_t pid = UNaviCycle::pipe_id(svc);
	UNaviUDPPipe* pipe = UNaviUDPPipe::get_pipe(pid);
	if (pipe==NULL)
		return NULL;
	return pipe->work_new_output(size);
}

UNaviUDPPacket* UNaviUDPPacket::new_proto_outpacket(uint8_t proto_id, size_t size)
{
	uint32_t pid = UNaviWorker::rand_proto_pipeid(proto_id);
	UNaviUDPPipe* pipe = UNaviUDPPipe::get_pipe(pid);
	if (pipe==NULL)
		return NULL;
	return pipe->work_new_output(size);
}

void UNaviUDPPacket::release_in_packet()
{
	UNaviUDPPipe* pipe = UNaviUDPPipe::get_pipe(pipe_id);
	if (pipe==NULL) {
		delete this;
	}
	pipe->work_release_in(*this);
}

void UNaviUDPPacket::cancel_out_packet()
{
	delete this;
}

void UNaviUDPPacket::output(const sockaddr& peer)
{
	UNaviUDPPipe* pipe = UNaviUDPPipe::get_pipe(pipe_id);
	if (pipe==NULL) {
		delete this;
	}

	if (peer.sa_family==AF_INET) {
		memcpy(&in_peer_addr,&peer,sizeof(sockaddr_in));
	}
	else if (peer.sa_family==AF_INET6) {
		memcpy(&in6_peer_addr,&peer,sizeof(sockaddr_in6));
	}
	pipe->work_push(*this);
}

#ifdef DEBUG
void UNaviUDPPacket::dump(std::ostream& os)
{
	char buf[1024];
	int i=0,j=0, off = 0;
	for(; i<used; i++) {
		if ( ++j == 8 ) {
			off += snprintf(buf+off,sizeof(buf)-off,"%02x\n", this->buf[i]);
			j=0;
			os << buf ;
			off = 0;
		}
		else {
			off += snprintf(buf+off,sizeof(buf)-off,"%02x ", this->buf[i]);
		}
	}
}
#endif
