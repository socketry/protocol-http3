#include "HTTP3.hpp"

#include "HTTP3/Dispatcher.hpp"
#include "HTTP3/Server.hpp"

VALUE Protocol_HTTP3 = Qnil;

void Init_Protocol_HTTP3(void)
{
	VALUE Protocol = rb_define_module("Protocol");
	Protocol_HTTP3 = rb_define_module_under(Protocol, "HTTP3");

	Init_Protocol_HTTP3_Server(Protocol_HTTP3);
	Init_Protocol_HTTP3_Dispatcher(Protocol_HTTP3);
}
