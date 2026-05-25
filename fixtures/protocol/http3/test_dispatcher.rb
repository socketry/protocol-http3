# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "protocol/http3"
require "protocol/http3/test_server"

module Protocol::HTTP3
	class TestDispatcher < Dispatcher
		attr_accessor :requests
		
		def create_server(socket, address, packet_header)
			server = TestServer.new(self, configuration, tls_context, socket, address, packet_header, nil)
			server.requests = requests
			return server
		end
	end
end
