#include "Dispatcher.hpp"

#include "Reference.hpp"
#include "Server.hpp"

#include "../QUIC/Bindings.hpp"

#include <unordered_map>

VALUE Protocol_HTTP3_Dispatcher = Qnil;

class RubyDispatcher final : public Protocol::QUIC::Dispatcher {
public:
	VALUE self;

private:
	VALUE _configuration;
	VALUE _tls_context;
	std::unordered_map<Protocol::HTTP3::Server *, VALUE> _servers;
	std::unordered_map<Protocol::QUIC::Socket *, VALUE> _sockets;

public:
	RubyDispatcher(VALUE self, VALUE configuration, VALUE tls_context) :
		Protocol::QUIC::Dispatcher(*Protocol_QUIC_Configuration_get(configuration), *Protocol_QUIC_TLS_ServerContext_get(tls_context)),
		self(self),
		_configuration(configuration),
		_tls_context(tls_context)
	{
	}

	VALUE ruby_configuration() noexcept {return _configuration;}
	VALUE ruby_tls_context() noexcept {return _tls_context;}

	Protocol::QUIC::Server * listen(VALUE ruby_socket)
	{
		auto socket = Protocol_QUIC_Socket_get(ruby_socket);

		_sockets[socket] = ruby_socket;

		return Protocol::QUIC::Dispatcher::listen(*socket);
	}

	Protocol::QUIC::Server * create_server(Protocol::QUIC::Socket & socket, const Protocol::QUIC::Address & address, const ngtcp2_pkt_hd & packet_header) override
	{
		auto iterator = _sockets.find(&socket);

		if (iterator == _sockets.end()) {
			rb_raise(rb_eRuntimeError, "Could not find Ruby socket wrapper for native socket.");
		}

		VALUE ruby_socket = iterator->second;
		VALUE ruby_address = Protocol_QUIC_Address_wrap(Protocol_QUIC_Address, address);

		VALUE ruby_packet_header = Protocol_QUIC_PacketHeader_allocate(Protocol_QUIC_PacketHeader);
		ValueReference ruby_packet_header_reference(ruby_packet_header, packet_header);

		VALUE server = rb_funcall(self, rb_intern("create_server"), 3, ruby_socket, ruby_address, ruby_packet_header);
		auto native_server = Protocol_HTTP3_Server_get(server);

		_servers[native_server] = server;

		return native_server;
	}

	void remove(Protocol::QUIC::Server * server) override
	{
		Protocol::QUIC::Dispatcher::remove(server);
		_servers.erase(static_cast<Protocol::HTTP3::Server *>(server));
	}

	void mark()
	{
		rb_gc_mark_movable(self);
		rb_gc_mark_movable(_configuration);
		rb_gc_mark_movable(_tls_context);

		for (auto & [server, ruby_server] : _servers) {
			(void)server;
			rb_gc_mark_movable(ruby_server);
		}

		for (auto & [socket, ruby_socket] : _sockets) {
			(void)socket;
			rb_gc_mark_movable(ruby_socket);
		}
	}

	void compact()
	{
		self = rb_gc_location(self);
		_configuration = rb_gc_location(_configuration);
		_tls_context = rb_gc_location(_tls_context);

		for (auto & [server, ruby_server] : _servers) {
			(void)server;
			ruby_server = rb_gc_location(ruby_server);
		}

		for (auto & [socket, ruby_socket] : _sockets) {
			(void)socket;
			ruby_socket = rb_gc_location(ruby_socket);
		}
	}
};

static void Protocol_HTTP3_Dispatcher_mark(void *data)
{
	if (data) {
		reinterpret_cast<RubyDispatcher *>(data)->mark();
	}
}

static void Protocol_HTTP3_Dispatcher_compact(void *data)
{
	if (data) {
		reinterpret_cast<RubyDispatcher *>(data)->compact();
	}
}

static void Protocol_HTTP3_Dispatcher_free(void *data)
{
	if (data) {
		delete reinterpret_cast<Protocol::QUIC::Dispatcher *>(data);
	}
}

static size_t Protocol_HTTP3_Dispatcher_size(const void *data)
{
	return sizeof(RubyDispatcher);
}

static const rb_data_type_t Protocol_HTTP3_Dispatcher_type = {
	.wrap_struct_name = "Protocol::HTTP3::Dispatcher",
	.function = {
		.dmark = Protocol_HTTP3_Dispatcher_mark,
		.dfree = Protocol_HTTP3_Dispatcher_free,
		.dsize = Protocol_HTTP3_Dispatcher_size,
		.dcompact = Protocol_HTTP3_Dispatcher_compact,
	},
	.data = NULL,
	.flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

Protocol::QUIC::Dispatcher * Protocol_HTTP3_Dispatcher_get(VALUE self)
{
	Protocol::QUIC::Dispatcher *dispatcher;

	TypedData_Get_Struct(self, Protocol::QUIC::Dispatcher, &Protocol_HTTP3_Dispatcher_type, dispatcher);

	return dispatcher;
}

static VALUE Protocol_HTTP3_Dispatcher_allocate(VALUE klass)
{
	return TypedData_Wrap_Struct(klass, &Protocol_HTTP3_Dispatcher_type, NULL);
}

static VALUE Protocol_HTTP3_Dispatcher_initialize(VALUE self, VALUE configuration, VALUE tls_context)
{
	DATA_PTR(self) = new RubyDispatcher(self, configuration, tls_context);

	return self;
}

static VALUE Protocol_HTTP3_Dispatcher_configuration(VALUE self)
{
	auto dispatcher = dynamic_cast<RubyDispatcher *>(Protocol_HTTP3_Dispatcher_get(self));

	return dispatcher->ruby_configuration();
}

static VALUE Protocol_HTTP3_Dispatcher_tls_context(VALUE self)
{
	auto dispatcher = dynamic_cast<RubyDispatcher *>(Protocol_HTTP3_Dispatcher_get(self));

	return dispatcher->ruby_tls_context();
}

static VALUE Protocol_HTTP3_Dispatcher_listen(VALUE self, VALUE socket)
{
	auto dispatcher = dynamic_cast<RubyDispatcher *>(Protocol_HTTP3_Dispatcher_get(self));

	dispatcher->listen(socket);

	return Qnil;
}

void Init_Protocol_HTTP3_Dispatcher(VALUE Protocol_HTTP3)
{
	Protocol_HTTP3_Dispatcher = rb_define_class_under(Protocol_HTTP3, "Dispatcher", rb_cObject);

	rb_define_alloc_func(Protocol_HTTP3_Dispatcher, Protocol_HTTP3_Dispatcher_allocate);
	rb_define_method(Protocol_HTTP3_Dispatcher, "initialize", Protocol_HTTP3_Dispatcher_initialize, 2);

	rb_define_method(Protocol_HTTP3_Dispatcher, "configuration", Protocol_HTTP3_Dispatcher_configuration, 0);
	rb_define_method(Protocol_HTTP3_Dispatcher, "tls_context", Protocol_HTTP3_Dispatcher_tls_context, 0);

	rb_define_method(Protocol_HTTP3_Dispatcher, "listen", Protocol_HTTP3_Dispatcher_listen, 1);
}
