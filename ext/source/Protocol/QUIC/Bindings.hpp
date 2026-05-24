#pragma once

#include <ruby.h>

#include <Protocol/QUIC/Address.hpp>
#include <Protocol/QUIC/Configuration.hpp>
#include <Protocol/QUIC/Dispatcher.hpp>
#include <Protocol/QUIC/Socket.hpp>
#include <Protocol/QUIC/TLS/ServerContext.hpp>

#include <ngtcp2/ngtcp2.h>

extern "C" {
	extern VALUE Protocol_QUIC_Address;
	extern VALUE Protocol_QUIC_PacketHeader;

	Protocol::QUIC::Address * Protocol_QUIC_Address_get(VALUE self);
	VALUE Protocol_QUIC_Address_wrap(VALUE klass, const Protocol::QUIC::Address & address);

	Protocol::QUIC::Configuration * Protocol_QUIC_Configuration_get(VALUE self);
	Protocol::QUIC::Dispatcher * Protocol_QUIC_Dispatcher_get(VALUE self);

	ngtcp2_pkt_hd * Protocol_QUIC_PacketHeader_get(VALUE self);
	VALUE Protocol_QUIC_PacketHeader_allocate(VALUE klass);

	Protocol::QUIC::Socket * Protocol_QUIC_Socket_get(VALUE self);
	Protocol::QUIC::TLS::ServerContext * Protocol_QUIC_TLS_ServerContext_get(VALUE self);
}
