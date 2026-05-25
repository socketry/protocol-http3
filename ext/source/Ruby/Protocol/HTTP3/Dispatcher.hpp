#pragma once

#include <ruby.h>

#include <Protocol/QUIC/Dispatcher.hpp>

extern "C" {
	extern VALUE Ruby_Protocol_HTTP3_Dispatcher;

	void Init_Ruby_Protocol_HTTP3_Dispatcher(VALUE Protocol_HTTP3);

	Protocol::QUIC::Dispatcher * Ruby_Protocol_HTTP3_Dispatcher_get(VALUE self);
}
