#include "Server.hpp"

#include "Dispatcher.hpp"

#include "../QUIC/Bindings.hpp"

#include <unordered_map>
#include <vector>

VALUE Ruby_Protocol_HTTP3_Server = Qnil;

namespace Ruby::Protocol::HTTP3 {

	class Server : public ::Protocol::HTTP3::Server {
	public:
		VALUE self;

	private:
		VALUE _dispatcher;
		VALUE _configuration;
		VALUE _tls_context;
		VALUE _socket;
		VALUE _remote_address;
		std::unordered_map<::Protocol::QUIC::StreamID, VALUE> _streams;

	public:
		Server(VALUE self, VALUE dispatcher, VALUE configuration, VALUE tls_context, VALUE socket, VALUE remote_address, VALUE packet_header, VALUE original_connection_id) :
			::Protocol::HTTP3::Server(*Ruby_Protocol_HTTP3_Dispatcher_get(dispatcher), *Ruby_Protocol_QUIC_Configuration_get(configuration), *Ruby_Protocol_QUIC_TLS_ServerContext_get(tls_context), *Ruby_Protocol_QUIC_Socket_get(socket), *Ruby_Protocol_QUIC_Address_get(remote_address), *Ruby_Protocol_QUIC_PacketHeader_get(packet_header), nullptr),
			self(self),
			_dispatcher(dispatcher),
			_configuration(configuration),
			_tls_context(tls_context),
			_socket(socket),
			_remote_address(remote_address)
		{
			(void)original_connection_id;
		}

		virtual ~Server()
		{
		}

		void header_received(::Protocol::QUIC::StreamID stream_id, std::int32_t token, nghttp3_rcbuf *name, nghttp3_rcbuf *value, std::uint8_t flags, void *stream_data) override
		{
			(void)token;
			(void)flags;
			(void)stream_data;

			if (!rb_respond_to(self, rb_intern("header_received"))) {
				return;
			}

			auto name_buffer = nghttp3_rcbuf_get_buf(name);
			auto value_buffer = nghttp3_rcbuf_get_buf(value);

			rb_funcall(
				self,
				rb_intern("header_received"),
				3,
				RB_LL2NUM(stream_id),
				rb_str_new(reinterpret_cast<const char *>(name_buffer.base), name_buffer.len),
				rb_str_new(reinterpret_cast<const char *>(value_buffer.base), value_buffer.len)
			);
		}

		void headers_finished(::Protocol::QUIC::StreamID stream_id, bool is_final, void *stream_data) override
		{
			(void)stream_data;

			if (rb_respond_to(self, rb_intern("headers_finished"))) {
				rb_funcall(self, rb_intern("headers_finished"), 2, RB_LL2NUM(stream_id), is_final ? Qtrue : Qfalse);
			}
		}

		void settings_received(const nghttp3_proto_settings *settings) override
		{
			(void)settings;

			if (rb_respond_to(self, rb_intern("settings_received"))) {
				rb_funcall(self, rb_intern("settings_received"), 0);
			}
		}

		void stream_finished(::Protocol::QUIC::StreamID stream_id, void *stream_data) override
		{
			(void)stream_data;

			if (rb_respond_to(self, rb_intern("stream_finished"))) {
				rb_funcall(self, rb_intern("stream_finished"), 1, RB_LL2NUM(stream_id));
			}
		}

		void disconnect() override
		{
			::Protocol::HTTP3::Server::disconnect();
			_streams.clear();
		}

		void mark()
		{
			rb_gc_mark_movable(self);
			rb_gc_mark_movable(_dispatcher);
			rb_gc_mark_movable(_configuration);
			rb_gc_mark_movable(_tls_context);
			rb_gc_mark_movable(_socket);
			rb_gc_mark_movable(_remote_address);

			for (auto & [stream_id, stream] : _streams) {
				(void)stream_id;
				rb_gc_mark_movable(stream);
			}
		}

		void compact()
		{
			self = rb_gc_location(self);
			_dispatcher = rb_gc_location(_dispatcher);
			_configuration = rb_gc_location(_configuration);
			_tls_context = rb_gc_location(_tls_context);
			_socket = rb_gc_location(_socket);
			_remote_address = rb_gc_location(_remote_address);

			for (auto & [stream_id, stream] : _streams) {
				(void)stream_id;
				stream = rb_gc_location(stream);
			}
		}
	};

}

static void Ruby_Protocol_HTTP3_Server_mark(void *data)
{
	if (data) {
		reinterpret_cast<Ruby::Protocol::HTTP3::Server *>(data)->mark();
	}
}

static void Ruby_Protocol_HTTP3_Server_compact(void *data)
{
	if (data) {
		reinterpret_cast<Ruby::Protocol::HTTP3::Server *>(data)->compact();
	}
}

static void Ruby_Protocol_HTTP3_Server_free(void *data)
{
	if (data) {
		delete reinterpret_cast<Protocol::HTTP3::Server *>(data);
	}
}

static size_t Ruby_Protocol_HTTP3_Server_size(const void *data)
{
	return sizeof(Ruby::Protocol::HTTP3::Server);
}

static const rb_data_type_t Ruby_Protocol_HTTP3_Server_type = {
	.wrap_struct_name = "Protocol::HTTP3::Server",
	.function = {
		.dmark = Ruby_Protocol_HTTP3_Server_mark,
		.dfree = Ruby_Protocol_HTTP3_Server_free,
		.dsize = Ruby_Protocol_HTTP3_Server_size,
		.dcompact = Ruby_Protocol_HTTP3_Server_compact,
	},
	.data = NULL,
	.flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

Protocol::HTTP3::Server * Ruby_Protocol_HTTP3_Server_get(VALUE self)
{
	Protocol::HTTP3::Server *server;

	TypedData_Get_Struct(self, Protocol::HTTP3::Server, &Ruby_Protocol_HTTP3_Server_type, server);

	return server;
}

static VALUE Ruby_Protocol_HTTP3_Server_allocate(VALUE klass)
{
	return TypedData_Wrap_Struct(klass, &Ruby_Protocol_HTTP3_Server_type, NULL);
}

static VALUE Ruby_Protocol_HTTP3_Server_initialize(VALUE self, VALUE dispatcher, VALUE configuration, VALUE tls_context, VALUE socket, VALUE remote_address, VALUE packet_header, VALUE original_connection_id)
{
	DATA_PTR(self) = new Ruby::Protocol::HTTP3::Server(self, dispatcher, configuration, tls_context, socket, remote_address, packet_header, original_connection_id);

	return self;
}

static VALUE Ruby_Protocol_HTTP3_Server_send_packets(VALUE self)
{
	auto server = Ruby_Protocol_HTTP3_Server_get(self);

	server->send_packets();

	return Qnil;
}

static VALUE Ruby_Protocol_HTTP3_Server_submit_response(VALUE self, VALUE stream_id, VALUE headers)
{
	auto server = Ruby_Protocol_HTTP3_Server_get(self);
	auto count = RARRAY_LEN(headers);
	std::vector<nghttp3_nv> native_headers;
	native_headers.reserve(count);

	for (long index = 0; index < count; ++index) {
		VALUE pair = rb_ary_entry(headers, index);
		VALUE name = rb_ary_entry(pair, 0);
		VALUE value = rb_ary_entry(pair, 1);

		name = rb_str_to_str(name);
		value = rb_str_to_str(value);

		native_headers.push_back(nghttp3_nv{
			.name = reinterpret_cast<std::uint8_t *>(RSTRING_PTR(name)),
			.value = reinterpret_cast<std::uint8_t *>(RSTRING_PTR(value)),
			.namelen = static_cast<std::size_t>(RSTRING_LEN(name)),
			.valuelen = static_cast<std::size_t>(RSTRING_LEN(value)),
			.flags = NGHTTP3_NV_FLAG_NONE,
		});
	}

	server->submit_response(RB_NUM2LL(stream_id), native_headers.data(), native_headers.size());
	server->send_packets();

	return Qnil;
}

void Init_Ruby_Protocol_HTTP3_Server(VALUE Protocol_HTTP3)
{
	Ruby_Protocol_HTTP3_Server = rb_define_class_under(Protocol_HTTP3, "Server", rb_cObject);

	rb_define_alloc_func(Ruby_Protocol_HTTP3_Server, Ruby_Protocol_HTTP3_Server_allocate);
	rb_define_method(Ruby_Protocol_HTTP3_Server, "initialize", Ruby_Protocol_HTTP3_Server_initialize, 7);

	rb_define_method(Ruby_Protocol_HTTP3_Server, "send_packets", Ruby_Protocol_HTTP3_Server_send_packets, 0);
	rb_define_method(Ruby_Protocol_HTTP3_Server, "submit_response", Ruby_Protocol_HTTP3_Server_submit_response, 2);
}
