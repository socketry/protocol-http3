# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "async"
require "async/queue"
require "openssl"
require "protocol/http3/test_client"
require "protocol/http3/test_dispatcher"
require "socket"
require "tempfile"

module Protocol::HTTP3::Fixtures
	def self.self_signed_certificate
		key = OpenSSL::PKey::RSA.new(2048)
		certificate = OpenSSL::X509::Certificate.new
		certificate.version = 2
		certificate.serial = 1
		certificate.subject = OpenSSL::X509::Name.parse("/CN=localhost")
		certificate.issuer = certificate.subject
		certificate.public_key = key.public_key
		certificate.not_before = Time.now
		certificate.not_after = Time.now + 3600
		
		extension_factory = OpenSSL::X509::ExtensionFactory.new
		extension_factory.subject_certificate = certificate
		extension_factory.issuer_certificate = certificate
		certificate.add_extension(extension_factory.create_extension("basicConstraints", "CA:FALSE", true))
		certificate.add_extension(extension_factory.create_extension("keyUsage", "digitalSignature,keyEncipherment", true))
		certificate.add_extension(extension_factory.create_extension("subjectAltName", "DNS:localhost,IP:127.0.0.1", false))
		certificate.sign(key, OpenSSL::Digest::SHA256.new)
		
		return certificate, key
	end
	
	def self.write_tempfile(name, content)
		file = Tempfile.new(name)
		file.write(content)
		file.flush
		return file
	end
	
	def self.server_context
		server_context = Protocol::QUIC::TLS::ServerContext.new
		server_context.add_protocol("h3")
		
		certificate, key = self_signed_certificate
		certificate_file = write_tempfile(["protocol-http3", ".crt"], certificate.to_pem)
		key_file = write_tempfile(["protocol-http3", ".key"], key.to_pem)
		
		server_context.load_certificate_file(certificate_file.path)
		server_context.load_private_key_file(key_file.path)
		
		return server_context
	end
	
	def self.client_context
		client_context = Protocol::QUIC::TLS::ClientContext.new
		client_context.add_protocol("h3")
		return client_context
	end
	
	def self.address
		Protocol::QUIC::Address.resolve("127.0.0.1", "0", ::Socket::AF_INET, ::Socket::SOCK_DGRAM, ::Socket::AI_PASSIVE).first
	end
	
	def self.bound_socket(address)
		socket = Protocol::QUIC::Socket.new(address.family, ::Socket::SOCK_DGRAM, ::Socket::IPPROTO_UDP)
		socket.bind(address)
		return socket
	end
	
	def self.exchange(configuration: Protocol::QUIC::Configuration.new, request_body: nil, response_body: nil)
		server_socket = bound_socket(address)
		local_address = server_socket.local_address
		result = nil
		
		requests = Async::Queue.new
		responses = Async::Queue.new
		
		dispatcher = Protocol::HTTP3::TestDispatcher.new(configuration, server_context)
		dispatcher.requests = requests
		dispatcher.server_options[:response_body] = response_body
		
		Async do |task|
			server_task = task.async do
				loop do
					dispatcher.receive(server_socket)
				end
			end
			
			client_socket = Protocol::QUIC::Socket.new(local_address.family, ::Socket::SOCK_DGRAM, ::Socket::IPPROTO_UDP)
			client_socket.connect(local_address)
			
			client = Protocol::HTTP3::TestClient.new(configuration, client_context, client_socket, local_address, 1)
			client.responses = responses
			client.request_body = request_body
			
			client_task = task.async do
				client.send_packets
				
				loop do
					break if client.receive(client_socket) == false
				end
			end
			
			task.with_timeout(10) do
				request = requests.dequeue
				response = responses.dequeue
				
				result = {
					request_headers: request[:headers],
					request_body: request[:body],
					response_headers: response[:headers],
					response_body: response[:body],
				}
			end
		ensure
			client&.close
			client_task&.stop
			server_task&.stop
		end.wait
		
		return result
	end
end
