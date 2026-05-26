# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "protocol/http3"

module Protocol::HTTP3
	class TestClient < Client
		def initialize(...)
			super
			@response_headers = Hash.new{|hash, key| hash[key] = []}
			@response_body = Hash.new{|hash, key| hash[key] = String.new}
			@body_tasks = []
		end
		
		attr_accessor :responses, :reported_response
		attr_accessor :request_body
		
		def handshake_completed
			stream = submit_request([
				[":method", "GET"],
				[":scheme", "https"],
				[":authority", "localhost"],
				[":path", "/"],
			], request_body)
			
			write_body(stream, request_body) if request_body
		end
		
		def header_received(stream_id, name, value)
			@response_headers[stream_id] << [name, value]
		end
		
		def headers_finished(stream_id, is_final)
			return unless is_final
			
			report_response(stream_id)
			
			close
		end
		
		def data_received(stream_id, chunk)
			@response_body[stream_id] << chunk
		end
		
		def stream_finished(stream_id)
			report_response(stream_id)
			close
		end
		
		def report_response(stream_id)
			return if reported_response
			
			self.reported_response = true
			responses&.push({
				headers: @response_headers[stream_id],
				body: @response_body[stream_id],
			})
		end
		
		def write_body(stream, body, parent: Async::Task.current)
			@body_tasks << parent.async do
				stream.write_body(body)
			end
		end
	end
end
