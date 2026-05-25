# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "protocol/http3"

module Protocol::HTTP3
	class TestServer < Server
		def initialize(...)
			super
			@request_headers = Hash.new{|hash, key| hash[key] = []}
		end
		
		attr_accessor :requests, :reported_request
		
		def header_received(stream_id, name, value)
			@request_headers[stream_id] << [name, value]
		end
		
		def headers_finished(stream_id, is_final)
			return unless is_final
			
			unless reported_request
				self.reported_request = true
				requests&.push(@request_headers[stream_id])
			end
			
			submit_response(stream_id, [
				[":status", "200"],
				["server", "protocol-http3-test"],
			])
		end
	end
end
