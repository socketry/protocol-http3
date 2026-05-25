#include "Client.hpp"

#include "../QUIC/Bindings.hpp"

#include <Protocol/HTTP3/Stream.hpp>

#include <array>
#include <cerrno>
#include <sys/socket.h>
#include <unordered_map>
#include <vector>

VALUE Protocol_HTTP3_Client = Qnil;

class RubyClient final : public Protocol::QUIC::Client, public Protocol::HTTP3::Session {
public:
	VALUE self;

private:
	VALUE _configuration;
	VALUE _tls_context;
	VALUE _socket;
	VALUE _remote_address;
	std::unordered_map<Protocol::QUIC::StreamID, VALUE> _streams;

public:
	RubyClient(VALUE self, VALUE configuration, VALUE tls_context, VALUE socket, VALUE remote_address, VALUE chosen_version) :
		Protocol::QUIC::Client(*Protocol_QUIC_Configuration_get(configuration), *Protocol_QUIC_TLS_ClientContext_get(tls_context), *Protocol_QUIC_Socket_get(socket), *Protocol_QUIC_Address_get(remote_address), RB_NUM2UINT(chosen_version)),
		Protocol::HTTP3::Session(Protocol::HTTP3::Session::Role::CLIENT),
		self(self),
		_configuration(configuration),
		_tls_context(tls_context),
		_socket(socket),
		_remote_address(remote_address)
	{
	}

	void handshake_completed() override
	{
		auto control_stream = open_unidirectional_stream();
		auto encoder_stream = open_unidirectional_stream();
		auto decoder_stream = open_unidirectional_stream();

		bind_control_stream(control_stream->stream_id());
		bind_qpack_streams(encoder_stream->stream_id(), decoder_stream->stream_id());

		send_packets();

		if (rb_respond_to(self, rb_intern("handshake_completed"))) {
			rb_funcall(self, rb_intern("handshake_completed"), 0);
		}
	}

	Protocol::QUIC::Stream * create_stream(Protocol::QUIC::StreamID stream_id) override
	{
		return new Protocol::HTTP3::Stream(*this, *this, stream_id);
	}

	Protocol::QUIC::Connection::Status send_stream_data() override
	{
		std::array<Protocol::QUIC::Byte, 1024*64> packet;
		std::array<nghttp3_vec, 16> http_vectors;

		while (true) {
			Protocol::QUIC::StreamID stream_id = -1;
			bool is_final = false;

			auto vector_count = write_stream_data(stream_id, is_final, http_vectors.data(), http_vectors.size());

			if (stream_id < 0) {
				break;
			}

			std::vector<ngtcp2_vec> stream_vectors;
			stream_vectors.reserve(static_cast<std::size_t>(vector_count));

			for (nghttp3_ssize index = 0; index < vector_count; ++index) {
				stream_vectors.push_back(ngtcp2_vec{
					.base = http_vectors[index].base,
					.len = http_vectors[index].len,
				});
			}

			ngtcp2_path_storage path_storage;
			ngtcp2_path_storage_zero(&path_storage);
			ngtcp2_pkt_info packet_info;
			ngtcp2_ssize written_length = 0;
			Protocol::QUIC::StreamDataFlags flags = is_final ? NGTCP2_WRITE_STREAM_FLAG_FIN : 0;

			auto result = ngtcp2_conn_writev_stream(Protocol::QUIC::Client::native_handle(), &path_storage.path, &packet_info, packet.data(), packet.size(), &written_length, flags, stream_id, stream_vectors.data(), stream_vectors.size(), Protocol::QUIC::timestamp());

			if (result == NGTCP2_ERR_STREAM_DATA_BLOCKED || result == NGTCP2_ERR_STREAM_SHUT_WR) {
				block_stream(stream_id);
				return Status(result);
			}

			if (result < 0) {
				return Status(result);
			}

			add_write_offset(stream_id, static_cast<std::size_t>(written_length));

			if (result > 0) {
				send_packet(path_storage.path, packet_info, packet.data(), result);
			}

			if (result == 0 && written_length == 0) {
				break;
			}
		}

		return Status::OK;
	}

	void header_received(Protocol::QUIC::StreamID stream_id, std::int32_t token, nghttp3_rcbuf *name, nghttp3_rcbuf *value, std::uint8_t flags, void *stream_data) override
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

	void headers_finished(Protocol::QUIC::StreamID stream_id, bool is_final, void *stream_data) override
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

	void stream_finished(Protocol::QUIC::StreamID stream_id, void *stream_data) override
	{
		(void)stream_data;

		if (rb_respond_to(self, rb_intern("stream_finished"))) {
			rb_funcall(self, rb_intern("stream_finished"), 1, RB_LL2NUM(stream_id));
		}
	}

	void disconnect() override
	{
		Protocol::QUIC::Client::disconnect();
		_streams.clear();
	}

	void mark()
	{
		rb_gc_mark_movable(self);
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

static void Protocol_HTTP3_Client_mark(void *data)
{
	if (data) {
		reinterpret_cast<RubyClient *>(data)->mark();
	}
}

static void Protocol_HTTP3_Client_compact(void *data)
{
	if (data) {
		reinterpret_cast<RubyClient *>(data)->compact();
	}
}

static void Protocol_HTTP3_Client_free(void *data)
{
	if (data) {
		delete reinterpret_cast<Protocol::QUIC::Client *>(data);
	}
}

static size_t Protocol_HTTP3_Client_size(const void *data)
{
	return sizeof(RubyClient);
}

static const rb_data_type_t Protocol_HTTP3_Client_type = {
	.wrap_struct_name = "Protocol::HTTP3::Client",
	.function = {
		.dmark = Protocol_HTTP3_Client_mark,
		.dfree = Protocol_HTTP3_Client_free,
		.dsize = Protocol_HTTP3_Client_size,
		.dcompact = Protocol_HTTP3_Client_compact,
	},
	.parent = &Protocol_QUIC_Connection_type,
	.data = NULL,
	.flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

Protocol::QUIC::Client * Protocol_HTTP3_Client_get(VALUE self)
{
	Protocol::QUIC::Client *client;

	TypedData_Get_Struct(self, Protocol::QUIC::Client, &Protocol_HTTP3_Client_type, client);

	return client;
}

static RubyClient * Protocol_HTTP3_RubyClient_get(VALUE self)
{
	return dynamic_cast<RubyClient *>(Protocol_HTTP3_Client_get(self));
}

static VALUE Protocol_HTTP3_Client_allocate(VALUE klass)
{
	return TypedData_Wrap_Struct(klass, &Protocol_HTTP3_Client_type, NULL);
}

static VALUE Protocol_HTTP3_Client_initialize(VALUE self, VALUE configuration, VALUE tls_context, VALUE socket, VALUE remote_address, VALUE chosen_version)
{
	DATA_PTR(self) = new RubyClient(self, configuration, tls_context, socket, remote_address, chosen_version);

	return self;
}

static VALUE Protocol_HTTP3_Client_connect(VALUE self)
{
	auto client = Protocol_HTTP3_Client_get(self);

	client->connect();

	return Qnil;
}

static VALUE Protocol_HTTP3_Client_close(VALUE self)
{
	auto client = Protocol_HTTP3_Client_get(self);

	client->close();

	return Qnil;
}

static VALUE Protocol_HTTP3_Client_send_packets(VALUE self)
{
	auto client = Protocol_HTTP3_Client_get(self);

	client->send_packets();

	return Qnil;
}

static VALUE Protocol_HTTP3_Client_receive(VALUE self, VALUE ruby_socket)
{
	auto client = Protocol_HTTP3_Client_get(self);
	auto socket = Protocol_QUIC_Socket_get(ruby_socket);
	std::array<Protocol::QUIC::Byte, 1024*64> buffer;
	Protocol::QUIC::Address remote_address;

	iovec vector{
		.iov_base = buffer.data(),
		.iov_len = buffer.size(),
	};

	msghdr message{
		.msg_name = &remote_address.data,
		.msg_namelen = sizeof(remote_address.data),
		.msg_iov = &vector,
		.msg_iovlen = 1,
	};

	auto length = recvmsg(socket->descriptor(), &message, MSG_DONTWAIT);

	if (length == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return Qnil;
		}

		rb_sys_fail("recvmsg");
	}

	remote_address.length = message.msg_namelen;

	auto path = ngtcp2_conn_get_path(client->native_handle());

	if (!path) {
		rb_raise(rb_eRuntimeError, "Could not get QUIC client path.");
	}

	ngtcp2_pkt_info packet_info{
		.ecn = static_cast<std::uint8_t>(Protocol::QUIC::ECN::UNSPECIFIED),
	};

	auto result = ngtcp2_conn_read_pkt(client->native_handle(), path, &packet_info, buffer.data(), length, Protocol::QUIC::timestamp());

	if (result < 0) {
		auto status = client->handle_error(result, "ngtcp2_conn_read_pkt");

		if (status == Protocol::QUIC::Connection::Status::CLOSING || status == Protocol::QUIC::Connection::Status::DRAINING) {
			return Qfalse;
		}

		rb_raise(rb_eRuntimeError, "Could not read QUIC packet: %s", ngtcp2_strerror(result));
	}

	client->send_packets();

	return Qtrue;
}

static VALUE Protocol_HTTP3_Client_submit_request(VALUE self, VALUE headers)
{
	auto client = Protocol_HTTP3_RubyClient_get(self);
	auto stream = client->open_bidirectional_stream();
	auto stream_id = stream->stream_id();
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

	client->submit_request(stream_id, native_headers.data(), native_headers.size());
	client->send_packets();

	return RB_LL2NUM(stream_id);
}

void Init_Protocol_HTTP3_Client(VALUE Protocol_HTTP3)
{
	Protocol_HTTP3_Client = rb_define_class_under(Protocol_HTTP3, "Client", rb_cObject);

	rb_define_alloc_func(Protocol_HTTP3_Client, Protocol_HTTP3_Client_allocate);
	rb_define_method(Protocol_HTTP3_Client, "initialize", Protocol_HTTP3_Client_initialize, 5);

	rb_define_method(Protocol_HTTP3_Client, "connect", Protocol_HTTP3_Client_connect, 0);
	rb_define_method(Protocol_HTTP3_Client, "close", Protocol_HTTP3_Client_close, 0);
	rb_define_method(Protocol_HTTP3_Client, "send_packets", Protocol_HTTP3_Client_send_packets, 0);
	rb_define_method(Protocol_HTTP3_Client, "receive", Protocol_HTTP3_Client_receive, 1);
	rb_define_method(Protocol_HTTP3_Client, "submit_request", Protocol_HTTP3_Client_submit_request, 1);
}
