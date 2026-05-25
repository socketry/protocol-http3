#include "HTTP3.hpp"

#include "HTTP3/Client.hpp"
#include "HTTP3/Dispatcher.hpp"
#include "HTTP3/Server.hpp"

#include "QUIC/Bindings.hpp"

VALUE Ruby_Protocol_HTTP3 = Qnil;

static VALUE Ruby_Protocol_HTTP3_file_descriptor(VALUE self, VALUE socket)
{
	(void)self;

	return INT2NUM(Ruby_Protocol_QUIC_Socket_get(socket)->descriptor());
}

void Init_Ruby_Protocol_HTTP3(void)
{
	VALUE Protocol = rb_define_module("Protocol");
	Ruby_Protocol_HTTP3 = rb_define_module_under(Protocol, "HTTP3");

	rb_define_singleton_method(Ruby_Protocol_HTTP3, "file_descriptor", Ruby_Protocol_HTTP3_file_descriptor, 1);

	Init_Ruby_Protocol_HTTP3_Client(Ruby_Protocol_HTTP3);
	Init_Ruby_Protocol_HTTP3_Server(Ruby_Protocol_HTTP3);
	Init_Ruby_Protocol_HTTP3_Dispatcher(Ruby_Protocol_HTTP3);
}
