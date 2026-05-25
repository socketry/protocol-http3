# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "protocol/http3"
require "protocol/http3/test_server"

module Protocol::HTTP3
	class TestDispatcher < Dispatcher
		attr_accessor :requests
		attr_accessor :server_options
		
		def initialize(...)
			super
			@server_options = {}
		end
		
		def create_server(socket, address, packet_header)
			server = TestServer.new(self, configuration, tls_context, socket, address, packet_header, nil)
			server.requests = requests
			server.response_body = server_options[:response_body]
			return server
		end
	end
end
