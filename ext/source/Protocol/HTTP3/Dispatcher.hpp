#pragma once

#include <ruby.h>

#include <Protocol/QUIC/Dispatcher.hpp>

extern "C" {
	extern VALUE Protocol_HTTP3_Dispatcher;

	void Init_Protocol_HTTP3_Dispatcher(VALUE Protocol_HTTP3);

	Protocol::QUIC::Dispatcher * Protocol_HTTP3_Dispatcher_get(VALUE self);
}
