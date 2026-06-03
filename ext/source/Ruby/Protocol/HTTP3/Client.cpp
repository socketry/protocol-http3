#include "Client.hpp"
#include "Stream.hpp"

#include "../QUIC/Bindings.hpp"

#include <array>
#include <stdexcept>
#include <vector>

VALUE Ruby_Protocol_HTTP3_Client = Qnil;

namespace Ruby::Protocol::HTTP3 {
	class Client final : public ::Protocol::QUIC::Client, public ::Protocol::HTTP3::Session {
	public:
		VALUE self;

	private:
		VALUE _configuration;
		VALUE _tls_context;
		VALUE _socket;
		VALUE _remote_address;

	public:
		Client(VALUE self, VALUE configuration, VALUE tls_context, VALUE socket, VALUE remote_address, VALUE chosen_version) :
			::Protocol::QUIC::Client(*Ruby_Protocol_QUIC_Configuration_get(configuration), *Ruby_Protocol_QUIC_TLS_ClientContext_get(tls_context), *Ruby_Protocol_QUIC_Socket_get(socket), *Ruby_Protocol_QUIC_Address_get(remote_address), RB_NUM2UINT(chosen_version)),
			::Protocol::HTTP3::Session(::Protocol::HTTP3::Session::Role::CLIENT),
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

		::Protocol::QUIC::Stream * create_stream(::Protocol::QUIC::StreamID stream_id) override
		{
			return new Stream(*this, *this, stream_id);
		}

		Stream * stream_for(::Protocol::QUIC::StreamID stream_id)
		{
			auto stream = reinterpret_cast<::Protocol::QUIC::Stream *>(ngtcp2_conn_get_stream_user_data(::Protocol::QUIC::Client::native_handle(), stream_id));
			auto ruby_stream = dynamic_cast<Stream *>(stream);

			if (!ruby_stream) {
				throw std::runtime_error("Could not find HTTP/3 stream.");
			}

			return ruby_stream;
		}

		::Protocol::QUIC::Connection::Status send_stream_data() override
		{
			std::array<::Protocol::QUIC::Byte, 1024*64> packet;
			std::array<nghttp3_vec, 16> http_vectors;

			while (true) {
				::Protocol::QUIC::StreamID stream_id = -1;
				bool is_final = false;

				nghttp3_ssize vector_count = 0;

				try {
					vector_count = write_stream_data(stream_id, is_final, http_vectors.data(), http_vectors.size());
				} catch (const std::system_error &) {
					close_pending_streams();
					return Status(NGTCP2_ERR_CALLBACK_FAILURE);
				}

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
				::Protocol::QUIC::StreamDataFlags flags = is_final ? NGTCP2_WRITE_STREAM_FLAG_FIN : 0;

				auto result = ngtcp2_conn_writev_stream(::Protocol::QUIC::Client::native_handle(), &path_storage.path, &packet_info, packet.data(), packet.size(), &written_length, flags, stream_id, stream_vectors.data(), stream_vectors.size(), ::Protocol::QUIC::timestamp());

				if (result == NGTCP2_ERR_STREAM_DATA_BLOCKED || result == NGTCP2_ERR_STREAM_SHUT_WR) {
					block_stream(stream_id);
					return Status(result);
				}

				if (result < 0) {
					return Status(result);
				}

				if (result > 0) {
					send_packet(path_storage.path, packet_info, packet.data(), result);
				}

				if (written_length < 0) {
					break;
				}

				add_write_offset(stream_id, static_cast<std::size_t>(written_length));

				if (result == 0 && written_length == 0) {
					break;
				}
			}

			return Status::OK;
		}

		void close_pending_streams()
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

		void stream_data_received(::Protocol::QUIC::StreamID stream_id, const std::uint8_t *data, std::size_t size, void *stream_data) override
		{
			(void)stream_data;

			auto stream = stream_for(stream_id);
			stream->receive_input(data, size);

			if (rb_respond_to(self, rb_intern("data_received"))) {
				rb_funcall(self, rb_intern("data_received"), 2, RB_LL2NUM(stream_id), stream->shift_input());
			}
		}

		void stream_data_acknowledged(::Protocol::QUIC::StreamID stream_id, std::uint64_t size, void *stream_data) override
		{
			(void)stream_data;

			stream_for(stream_id)->acknowledge_output(size);
		}

		void stream_closed(::Protocol::QUIC::StreamID stream_id, std::uint64_t error_code, void *stream_data) override
		{
			(void)error_code;
			(void)stream_data;
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

			stream_for(stream_id)->finish_input();

			if (rb_respond_to(self, rb_intern("stream_finished"))) {
				rb_funcall(self, rb_intern("stream_finished"), 1, RB_LL2NUM(stream_id));
			}
		}

		void disconnect() override
		{
			::Protocol::QUIC::Client::disconnect();
		}

		void mark()
		{
			rb_gc_mark_movable(self);
			rb_gc_mark_movable(_configuration);
			rb_gc_mark_movable(_tls_context);
			rb_gc_mark_movable(_socket);
			rb_gc_mark_movable(_remote_address);

			for (auto & [stream_id, stream] : ::Protocol::QUIC::Connection::_streams) {
				(void)stream_id;

				if (auto ruby_stream = dynamic_cast<Stream *>(stream)) {
					ruby_stream->mark();
				}
			}
		}

		void compact()
		{
			self = rb_gc_location(self);
			_configuration = rb_gc_location(_configuration);
			_tls_context = rb_gc_location(_tls_context);
			_socket = rb_gc_location(_socket);
			_remote_address = rb_gc_location(_remote_address);

			for (auto & [stream_id, stream] : ::Protocol::QUIC::Connection::_streams) {
				(void)stream_id;

				if (auto ruby_stream = dynamic_cast<Stream *>(stream)) {
					ruby_stream->compact();
				}
			}
		}

		void submit_request_with_body(::Protocol::QUIC::StreamID stream_id, const nghttp3_nv *headers, std::size_t count, VALUE body)
		{
			auto stream = stream_for(stream_id);
			
			if (NIL_P(body)) {
				submit_request(stream_id, headers, count, nullptr, stream);
			} else {
				submit_request(stream_id, headers, count, stream->reader(), stream);
			}
		}

		void append_body(::Protocol::QUIC::StreamID stream_id, VALUE chunk)
		{
			auto stream = stream_for(stream_id);
			auto was_empty = !stream->output_pending();

			stream->append_output(chunk);

			if (was_empty) {
				resume_stream(stream_id);
			}
		}

		void finish_body(::Protocol::QUIC::StreamID stream_id)
		{
			auto stream = stream_for(stream_id);
			auto was_empty = !stream->output_pending();

			stream->finish_output();

			if (was_empty) {
				resume_stream(stream_id);
			}
		}

		void reset_body(::Protocol::QUIC::StreamID stream_id, std::uint64_t error_code)
		{
			stream_for(stream_id)->reset_output();
			shutdown_stream_write(stream_id);
			close_stream(stream_id, error_code);
			ngtcp2_conn_shutdown_stream_write(::Protocol::QUIC::Client::native_handle(), 0, stream_id, error_code);
		}
	};

}

static void Ruby_Protocol_HTTP3_Client_mark(void *data)
{
	if (data) {
		reinterpret_cast<Ruby::Protocol::HTTP3::Client *>(data)->mark();
	}
}

static void Ruby_Protocol_HTTP3_Client_compact(void *data)
{
	if (data) {
		reinterpret_cast<Ruby::Protocol::HTTP3::Client *>(data)->compact();
	}
}

static void Ruby_Protocol_HTTP3_Client_free(void *data)
{
	if (data) {
		delete reinterpret_cast<Protocol::QUIC::Client *>(data);
	}
}

static size_t Ruby_Protocol_HTTP3_Client_size(const void *data)
{
	return sizeof(Ruby::Protocol::HTTP3::Client);
}

static const rb_data_type_t Ruby_Protocol_HTTP3_Client_type = {
	.wrap_struct_name = "Protocol::HTTP3::Client",
	.function = {
		.dmark = Ruby_Protocol_HTTP3_Client_mark,
		.dfree = Ruby_Protocol_HTTP3_Client_free,
		.dsize = Ruby_Protocol_HTTP3_Client_size,
		.dcompact = Ruby_Protocol_HTTP3_Client_compact,
	},
	.parent = &Ruby_Protocol_QUIC_Connection_type,
	.data = NULL,
	.flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

Protocol::QUIC::Client * Ruby_Protocol_HTTP3_Client_get(VALUE self)
{
	Protocol::QUIC::Client *client;

	TypedData_Get_Struct(self, Protocol::QUIC::Client, &Ruby_Protocol_HTTP3_Client_type, client);

	return client;
}

static Ruby::Protocol::HTTP3::Client * Ruby_Protocol_HTTP3_Client_native_get(VALUE self)
{
	return dynamic_cast<Ruby::Protocol::HTTP3::Client *>(Ruby_Protocol_HTTP3_Client_get(self));
}

static VALUE Ruby_Protocol_HTTP3_Client_allocate(VALUE klass)
{
	return TypedData_Wrap_Struct(klass, &Ruby_Protocol_HTTP3_Client_type, NULL);
}

static VALUE Ruby_Protocol_HTTP3_Client_initialize(VALUE self, VALUE configuration, VALUE tls_context, VALUE socket, VALUE remote_address, VALUE chosen_version)
{
	DATA_PTR(self) = new Ruby::Protocol::HTTP3::Client(self, configuration, tls_context, socket, remote_address, chosen_version);

	return self;
}

static VALUE Ruby_Protocol_HTTP3_Client_connect(VALUE self)
{
	auto client = Ruby_Protocol_HTTP3_Client_get(self);

	client->connect();

	return Qnil;
}

static VALUE Ruby_Protocol_HTTP3_Client_close(VALUE self)
{
	auto client = Ruby_Protocol_HTTP3_Client_get(self);

	client->close();

	return Qnil;
}

static VALUE Ruby_Protocol_HTTP3_Client_send_packets(VALUE self)
{
	auto client = Ruby_Protocol_HTTP3_Client_get(self);

	client->send_packets();

	return Qnil;
}

static VALUE Ruby_Protocol_HTTP3_Client_receive(VALUE self, VALUE ruby_socket)
{
	auto client = Ruby_Protocol_HTTP3_Client_get(self);
	auto socket = Ruby_Protocol_QUIC_Socket_get(ruby_socket);

	auto path = ngtcp2_conn_get_path(client->native_handle());

	if (!path) {
		rb_raise(rb_eRuntimeError, "Could not get QUIC client path.");
	}

	auto status = client->receive_packets(*path, *socket);

	switch (status) {
	case Protocol::QUIC::Connection::Status::OK:
		client->send_packets();

		return Qtrue;
	case Protocol::QUIC::Connection::Status::CLOSING:
	case Protocol::QUIC::Connection::Status::DRAINING:
		return Qfalse;
	default:
		rb_raise(rb_eRuntimeError, "Could not receive QUIC packet.");
	}
}

static VALUE Ruby_Protocol_HTTP3_Client_submit_request(int argc, VALUE *argv, VALUE self)
{
	VALUE headers;
	VALUE body = Qnil;

	rb_scan_args(argc, argv, "11", &headers, &body);

	auto client = Ruby_Protocol_HTTP3_Client_native_get(self);
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

	client->submit_request_with_body(stream_id, native_headers.data(), native_headers.size(), body);
	client->send_packets();

	return RB_LL2NUM(stream_id);
}

static VALUE Ruby_Protocol_HTTP3_Client_write_body_chunk(VALUE self, VALUE stream_id, VALUE chunk)
{
	auto client = Ruby_Protocol_HTTP3_Client_native_get(self);

	client->append_body(RB_NUM2LL(stream_id), chunk);

	return Qnil;
}

static VALUE Ruby_Protocol_HTTP3_Client_read_body_chunk(VALUE self, VALUE stream_id)
{
	auto client = Ruby_Protocol_HTTP3_Client_native_get(self);

	return client->stream_for(RB_NUM2LL(stream_id))->shift_input();
}

static VALUE Ruby_Protocol_HTTP3_Client_finish_body(VALUE self, VALUE stream_id)
{
	auto client = Ruby_Protocol_HTTP3_Client_native_get(self);

	client->finish_body(RB_NUM2LL(stream_id));

	return Qnil;
}

static VALUE Ruby_Protocol_HTTP3_Client_reset_body(int argc, VALUE *argv, VALUE self)
{
	VALUE stream_id;
	VALUE error_code;
	rb_scan_args(argc, argv, "11", &stream_id, &error_code);

	auto client = Ruby_Protocol_HTTP3_Client_native_get(self);
	auto native_error_code = NIL_P(error_code) ? NGHTTP3_H3_INTERNAL_ERROR : RB_NUM2ULL(error_code);

	client->reset_body(RB_NUM2LL(stream_id), native_error_code);

	return Qnil;
}

void Init_Ruby_Protocol_HTTP3_Client(VALUE Protocol_HTTP3)
{
	Ruby_Protocol_HTTP3_Client = rb_define_class_under(Protocol_HTTP3, "Client", rb_cObject);

	rb_define_alloc_func(Ruby_Protocol_HTTP3_Client, Ruby_Protocol_HTTP3_Client_allocate);
	rb_define_method(Ruby_Protocol_HTTP3_Client, "initialize", Ruby_Protocol_HTTP3_Client_initialize, 5);

	rb_define_method(Ruby_Protocol_HTTP3_Client, "connect", Ruby_Protocol_HTTP3_Client_connect, 0);
	rb_define_method(Ruby_Protocol_HTTP3_Client, "close", Ruby_Protocol_HTTP3_Client_close, 0);
	rb_define_method(Ruby_Protocol_HTTP3_Client, "send_packets", Ruby_Protocol_HTTP3_Client_send_packets, 0);
	rb_define_method(Ruby_Protocol_HTTP3_Client, "receive", Ruby_Protocol_HTTP3_Client_receive, 1);
	rb_define_method(Ruby_Protocol_HTTP3_Client, "submit_request", RUBY_METHOD_FUNC(Ruby_Protocol_HTTP3_Client_submit_request), -1);
	rb_define_method(Ruby_Protocol_HTTP3_Client, "write_body_chunk", Ruby_Protocol_HTTP3_Client_write_body_chunk, 2);
	rb_define_method(Ruby_Protocol_HTTP3_Client, "read_body_chunk", Ruby_Protocol_HTTP3_Client_read_body_chunk, 1);
	rb_define_method(Ruby_Protocol_HTTP3_Client, "finish_body", Ruby_Protocol_HTTP3_Client_finish_body, 1);
	rb_define_method(Ruby_Protocol_HTTP3_Client, "reset_body", RUBY_METHOD_FUNC(Ruby_Protocol_HTTP3_Client_reset_body), -1);
}
