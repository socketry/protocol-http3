# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2023-2026, by Samuel Williams.

require "protocol/quic"

require_relative "http3/version"

# Native extension:
require "Ruby_Protocol_HTTP3"

require_relative "http3/stream"

module Protocol::HTTP3
	# An HTTP/3 client connection.
	class Client
		alias native_submit_request submit_request
		
		# Submit a request and return the stream used for the response body.
		def submit_request(headers, body = nil)
			Stream.new(self, native_submit_request(headers, body))
		end
		
		private :read_body_chunk
		private :write_body_chunk
		private :finish_body
		private :reset_body
	end
	
	# An HTTP/3 server connection.
	class Server
		alias native_submit_response submit_response
		
		# Submit a response for the given stream and return the stream used for the response body.
		def submit_response(stream_id, headers, body = nil)
			Stream.new(self, stream_id).tap do
				native_submit_response(stream_id, headers, body)
			end
		end
		
		private :read_body_chunk
		private :write_body_chunk
		private :finish_body
		private :reset_body
	end
end
