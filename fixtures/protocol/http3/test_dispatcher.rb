# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "protocol/http3"
require "protocol/http3/test_server"

module Protocol::HTTP3
	class TestDispatcher < Dispatcher
		def create_server(socket, address, packet_header)
			TestServer.new(self, configuration, tls_context, socket, address, packet_header, nil)
		end
	end
end
