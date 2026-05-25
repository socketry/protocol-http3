#include "Dispatcher.hpp"

#include "Server.hpp"

#include "../QUIC/Bindings.hpp"

#include <array>
#include <cerrno>
#include <sys/socket.h>
#include <unordered_map>

VALUE Ruby_Protocol_HTTP3_Dispatcher = Qnil;

namespace Ruby::Protocol::HTTP3 {

	class Dispatcher final : public ::Protocol::QUIC::Dispatcher {
	public:
		VALUE self;

	private:
		VALUE _configuration;
		VALUE _tls_context;
		std::unordered_map<::Protocol::HTTP3::Server *, VALUE> _servers;
		std::unordered_map<::Protocol::QUIC::Socket *, VALUE> _sockets;

	public:
		Dispatcher(VALUE self, VALUE configuration, VALUE tls_context) :
			::Protocol::QUIC::Dispatcher(*Ruby_Protocol_QUIC_Configuration_get(configuration), *Ruby_Protocol_QUIC_TLS_ServerContext_get(tls_context)),
			self(self),
			_configuration(configuration),
			_tls_context(tls_context)
		{
		}

		VALUE ruby_configuration() noexcept {return _configuration;}
		VALUE ruby_tls_context() noexcept {return _tls_context;}

		VALUE listen(VALUE ruby_socket)
		{
			auto socket = Ruby_Protocol_QUIC_Socket_get(ruby_socket);

			_sockets[socket] = ruby_socket;

			auto server = ::Protocol::QUIC::Dispatcher::listen(*socket);

			if (server) {
				auto iterator = _servers.find(static_cast<::Protocol::HTTP3::Server *>(server));

				if (iterator == _servers.end()) {
					rb_raise(rb_eRuntimeError, "Could not find Ruby server wrapper for native server.");
				}

				return iterator->second;
			}

			return Qnil;
		}

		VALUE receive(VALUE ruby_socket)
		{
			auto socket = Ruby_Protocol_QUIC_Socket_get(ruby_socket);
			_sockets[socket] = ruby_socket;

			std::array<::Protocol::QUIC::Byte, 1024*64> buffer;
			::Protocol::QUIC::Address remote_address;

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

			ngtcp2_version_cid version_cid;
			auto result = ngtcp2_pkt_decode_version_cid(&version_cid, buffer.data(), length, ::Protocol::QUIC::DEFAULT_SCID_LENGTH);

			if (result == 0) {
				auto server = process_packet(*socket, remote_address, buffer.data(), length, ::Protocol::QUIC::ECN::UNSPECIFIED, version_cid);

				if (server) {
					auto iterator = _servers.find(static_cast<::Protocol::HTTP3::Server *>(server));

					if (iterator == _servers.end()) {
						rb_raise(rb_eRuntimeError, "Could not find Ruby server wrapper for native server.");
					}

					return iterator->second;
				}
			}
			else if (result == NGTCP2_ERR_VERSION_NEGOTIATION) {
				send_version_negotiation(*socket, version_cid, remote_address);
			}
			else {
				rb_raise(rb_eRuntimeError, "Could not decode QUIC version/CID: %s", ngtcp2_strerror(result));
			}

			return Qnil;
		}

		::Protocol::QUIC::Server * create_server(::Protocol::QUIC::Socket & socket, const ::Protocol::QUIC::Address & address, const ngtcp2_pkt_hd & packet_header) override
		{
			auto iterator = _sockets.find(&socket);

			if (iterator == _sockets.end()) {
				rb_raise(rb_eRuntimeError, "Could not find Ruby socket wrapper for native socket.");
			}

			VALUE ruby_socket = iterator->second;
			VALUE ruby_address = Ruby_Protocol_QUIC_Address_wrap(Ruby_Protocol_QUIC_Address, address);

			VALUE ruby_packet_header = Ruby_Protocol_QUIC_PacketHeader_allocate(Ruby_Protocol_QUIC_PacketHeader);
			DATA_PTR(ruby_packet_header) = new ngtcp2_pkt_hd(packet_header);

			VALUE server = rb_funcall(self, rb_intern("create_server"), 3, ruby_socket, ruby_address, ruby_packet_header);
			auto native_server = Ruby_Protocol_HTTP3_Server_get(server);

			_servers[native_server] = server;

			return native_server;
		}

		void remove(::Protocol::QUIC::Server * server) override
		{
			::Protocol::QUIC::Dispatcher::remove(server);
			_servers.erase(static_cast<::Protocol::HTTP3::Server *>(server));
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

}

static void Ruby_Protocol_HTTP3_Dispatcher_mark(void *data)
{
	if (data) {
		reinterpret_cast<Ruby::Protocol::HTTP3::Dispatcher *>(data)->mark();
	}
}

static void Ruby_Protocol_HTTP3_Dispatcher_compact(void *data)
{
	if (data) {
		reinterpret_cast<Ruby::Protocol::HTTP3::Dispatcher *>(data)->compact();
	}
}

static void Ruby_Protocol_HTTP3_Dispatcher_free(void *data)
{
	if (data) {
		delete reinterpret_cast<Protocol::QUIC::Dispatcher *>(data);
	}
}

static size_t Ruby_Protocol_HTTP3_Dispatcher_size(const void *data)
{
	return sizeof(Ruby::Protocol::HTTP3::Dispatcher);
}

static const rb_data_type_t Ruby_Protocol_HTTP3_Dispatcher_type = {
	.wrap_struct_name = "Protocol::HTTP3::Dispatcher",
	.function = {
		.dmark = Ruby_Protocol_HTTP3_Dispatcher_mark,
		.dfree = Ruby_Protocol_HTTP3_Dispatcher_free,
		.dsize = Ruby_Protocol_HTTP3_Dispatcher_size,
		.dcompact = Ruby_Protocol_HTTP3_Dispatcher_compact,
	},
	.data = NULL,
	.flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

Protocol::QUIC::Dispatcher * Ruby_Protocol_HTTP3_Dispatcher_get(VALUE self)
{
	Protocol::QUIC::Dispatcher *dispatcher;

	TypedData_Get_Struct(self, Protocol::QUIC::Dispatcher, &Ruby_Protocol_HTTP3_Dispatcher_type, dispatcher);

	return dispatcher;
}

static VALUE Ruby_Protocol_HTTP3_Dispatcher_allocate(VALUE klass)
{
	return TypedData_Wrap_Struct(klass, &Ruby_Protocol_HTTP3_Dispatcher_type, NULL);
}

static VALUE Ruby_Protocol_HTTP3_Dispatcher_initialize(VALUE self, VALUE configuration, VALUE tls_context)
{
	DATA_PTR(self) = new Ruby::Protocol::HTTP3::Dispatcher(self, configuration, tls_context);

	return self;
}

static VALUE Ruby_Protocol_HTTP3_Dispatcher_configuration(VALUE self)
{
	auto dispatcher = dynamic_cast<Ruby::Protocol::HTTP3::Dispatcher *>(Ruby_Protocol_HTTP3_Dispatcher_get(self));

	return dispatcher->ruby_configuration();
}

static VALUE Ruby_Protocol_HTTP3_Dispatcher_tls_context(VALUE self)
{
	auto dispatcher = dynamic_cast<Ruby::Protocol::HTTP3::Dispatcher *>(Ruby_Protocol_HTTP3_Dispatcher_get(self));

	return dispatcher->ruby_tls_context();
}

static VALUE Ruby_Protocol_HTTP3_Dispatcher_listen(VALUE self, VALUE socket)
{
	auto dispatcher = dynamic_cast<Ruby::Protocol::HTTP3::Dispatcher *>(Ruby_Protocol_HTTP3_Dispatcher_get(self));

	return dispatcher->listen(socket);
}

static VALUE Ruby_Protocol_HTTP3_Dispatcher_receive(VALUE self, VALUE socket)
{
	auto dispatcher = dynamic_cast<Ruby::Protocol::HTTP3::Dispatcher *>(Ruby_Protocol_HTTP3_Dispatcher_get(self));

	return dispatcher->receive(socket);
}

void Init_Ruby_Protocol_HTTP3_Dispatcher(VALUE Protocol_HTTP3)
{
	Ruby_Protocol_HTTP3_Dispatcher = rb_define_class_under(Protocol_HTTP3, "Dispatcher", rb_cObject);

	rb_define_alloc_func(Ruby_Protocol_HTTP3_Dispatcher, Ruby_Protocol_HTTP3_Dispatcher_allocate);
	rb_define_method(Ruby_Protocol_HTTP3_Dispatcher, "initialize", Ruby_Protocol_HTTP3_Dispatcher_initialize, 2);

	rb_define_method(Ruby_Protocol_HTTP3_Dispatcher, "configuration", Ruby_Protocol_HTTP3_Dispatcher_configuration, 0);
	rb_define_method(Ruby_Protocol_HTTP3_Dispatcher, "tls_context", Ruby_Protocol_HTTP3_Dispatcher_tls_context, 0);

	rb_define_method(Ruby_Protocol_HTTP3_Dispatcher, "listen", Ruby_Protocol_HTTP3_Dispatcher_listen, 1);
	rb_define_method(Ruby_Protocol_HTTP3_Dispatcher, "receive", Ruby_Protocol_HTTP3_Dispatcher_receive, 1);
}
