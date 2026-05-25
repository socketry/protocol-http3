#!/usr/bin/env ruby
# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "async"
require "localhost"
require "protocol/http3"
require "socket"

module HelloWorld
	HOST = ENV.fetch("HOST", "127.0.0.1")
	PORT = ENV.fetch("PORT", "12345")
	
	def self.remote_address
		Protocol::QUIC::Address.resolve(HOST, PORT, ::Socket::AF_INET, ::Socket::SOCK_DGRAM, 0).first
	end
	
	def self.client_context
		Localhost::Authority.fetch("localhost")
		
		Protocol::QUIC::TLS::ClientContext.new.tap do |context|
			context.add_protocol("h3")
		end
	end
	
	class Client < Protocol::HTTP3::Client
		def initialize(...)
			super
			@response_headers = Hash.new{|hash, key| hash[key] = []}
			@response_body = Hash.new{|hash, key| hash[key] = String.new}
			@complete = false
		end
		
		attr_reader :complete
		
		def handshake_completed
			submit_request([
				[":method", "GET"],
				[":scheme", "https"],
				[":authority", "localhost"],
				[":path", "/"],
			])
		end
		
		def header_received(stream_id, name, value)
			@response_headers[stream_id] << [name, value]
		end
		
		def headers_finished(stream_id, is_final)
			return unless is_final
			
			print_response(stream_id)
			complete!
		end
		
		def data_received(stream_id, chunk)
			@response_body[stream_id] << chunk
		end
		
		def stream_finished(stream_id)
			print_response(stream_id)
			complete!
		end
		
		def print_response(stream_id)
			$stdout.puts "Response headers:"
			@response_headers[stream_id].each do |name, value|
				$stdout.puts "  #{name}: #{value}"
			end
			
			$stdout.puts
			$stdout.write @response_body[stream_id]
		end
		
		def complete!
			@complete = true
			close
		end
	end
end

configuration = Protocol::QUIC::Configuration.new
remote_address = HelloWorld.remote_address

socket = Protocol::QUIC::Socket.new(remote_address.family, ::Socket::SOCK_DGRAM, ::Socket::IPPROTO_UDP)
socket.connect(remote_address)

client = HelloWorld::Client.new(configuration, HelloWorld.client_context, socket, remote_address, 1)

Async do |task|
	$stderr.puts "HTTP/3 client connecting to #{HelloWorld::HOST}:#{HelloWorld::PORT}"
	
	task.with_timeout(10) do
		client.send_packets
		
		until client.complete
			break if client.receive(socket) == false
		end
	end
ensure
	client.close
	socket.close if socket.respond_to?(:close)
end
