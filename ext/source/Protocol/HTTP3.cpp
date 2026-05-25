#include "HTTP3.hpp"

#include "HTTP3/Client.hpp"
#include "HTTP3/Dispatcher.hpp"
#include "HTTP3/Server.hpp"

#include "QUIC/Bindings.hpp"

VALUE Protocol_HTTP3 = Qnil;

static VALUE Protocol_HTTP3_file_descriptor(VALUE self, VALUE socket)
{
	(void)self;

	return INT2NUM(Protocol_QUIC_Socket_get(socket)->descriptor());
}

void Init_Protocol_HTTP3(void)
{
	VALUE Protocol = rb_define_module("Protocol");
	Protocol_HTTP3 = rb_define_module_under(Protocol, "HTTP3");

	rb_define_singleton_method(Protocol_HTTP3, "file_descriptor", Protocol_HTTP3_file_descriptor, 1);

	Init_Protocol_HTTP3_Client(Protocol_HTTP3);
	Init_Protocol_HTTP3_Server(Protocol_HTTP3);
	Init_Protocol_HTTP3_Dispatcher(Protocol_HTTP3);
}
