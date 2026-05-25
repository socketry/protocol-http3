# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "protocol/http3"

module Protocol::HTTP3
	class TestClient < Client
		def initialize(...)
			super
			@response_headers = Hash.new{|hash, key| hash[key] = []}
		end
		
		attr_accessor :responses, :reported_response
		
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
			
			unless reported_response
				self.reported_response = true
				responses&.push(@response_headers[stream_id])
			end
			
			close
		end
	end
end
