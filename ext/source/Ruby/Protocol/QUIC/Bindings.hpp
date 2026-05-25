#pragma once

#include <ruby.h>

#include <Protocol/QUIC/Address.hpp>
#include <Protocol/QUIC/Configuration.hpp>
#include <Protocol/QUIC/Client.hpp>
#include <Protocol/QUIC/Connection.hpp>
#include <Protocol/QUIC/Dispatcher.hpp>
#include <Protocol/QUIC/Socket.hpp>
#include <Protocol/QUIC/TLS/ClientContext.hpp>
#include <Protocol/QUIC/TLS/ServerContext.hpp>

#include <ngtcp2/ngtcp2.h>

extern "C" {
	extern VALUE Ruby_Protocol_QUIC_Address;
	extern VALUE Ruby_Protocol_QUIC_PacketHeader;
	extern const rb_data_type_t Ruby_Protocol_QUIC_Connection_type;

	Protocol::QUIC::Address * Ruby_Protocol_QUIC_Address_get(VALUE self);
	VALUE Ruby_Protocol_QUIC_Address_wrap(VALUE klass, const Protocol::QUIC::Address & address);

	Protocol::QUIC::Configuration * Ruby_Protocol_QUIC_Configuration_get(VALUE self);
	Protocol::QUIC::Client * Ruby_Protocol_QUIC_Client_get(VALUE self);
	Protocol::QUIC::Connection * Ruby_Protocol_QUIC_Connection_get(VALUE self);
	Protocol::QUIC::Dispatcher * Ruby_Protocol_QUIC_Dispatcher_get(VALUE self);

	ngtcp2_pkt_hd * Ruby_Protocol_QUIC_PacketHeader_get(VALUE self);
	VALUE Ruby_Protocol_QUIC_PacketHeader_allocate(VALUE klass);

	Protocol::QUIC::Socket * Ruby_Protocol_QUIC_Socket_get(VALUE self);
	Protocol::QUIC::TLS::ClientContext * Ruby_Protocol_QUIC_TLS_ClientContext_get(VALUE self);
	Protocol::QUIC::TLS::ServerContext * Ruby_Protocol_QUIC_TLS_ServerContext_get(VALUE self);
}
