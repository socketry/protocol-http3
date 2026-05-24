#pragma once

#include <ruby.h>

#include <Protocol/HTTP3/Server.hpp>

extern "C" {
	extern VALUE Protocol_HTTP3_Server;

	void Init_Protocol_HTTP3_Server(VALUE Protocol_HTTP3);

	Protocol::HTTP3::Server * Protocol_HTTP3_Server_get(VALUE self);
}
