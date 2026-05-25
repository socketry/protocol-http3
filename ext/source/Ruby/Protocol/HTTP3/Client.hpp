#pragma once

#include <ruby.h>

#include <Protocol/HTTP3/Session.hpp>
#include <Protocol/QUIC/Client.hpp>

extern "C" {
	extern VALUE Ruby_Protocol_HTTP3_Client;

	void Init_Ruby_Protocol_HTTP3_Client(VALUE Protocol_HTTP3);

	Protocol::QUIC::Client * Ruby_Protocol_HTTP3_Client_get(VALUE self);
}
