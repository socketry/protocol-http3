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
			@close_after_response = true
		end
		
		attr_accessor :responses, :reported_response
		attr_accessor :request_body
		attr_accessor :close_after_response
		
		def handshake_completed
			submit_test_request("/")
		end
		
		def header_received(stream_id, name, value)
			@response_headers[stream_id] << [name, value]
		end
		
		def headers_finished(stream_id, is_final)
			return unless is_final
			
			if close_after_response
				report_response(stream_id)
				close
			end
		end
		
		def data_received(stream_id, chunk)
			@response_body[stream_id] << chunk
		end
		
		def stream_finished(stream_id)
			report_response(stream_id)
			close if close_after_response
		end
		
		def report_response(stream_id)
			return if reported_response && close_after_response
			
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
		
		def submit_test_request(path, body = request_body)
			stream = submit_request([
				[":method", "GET"],
				[":scheme", "https"],
				[":authority", "localhost"],
				[":path", path],
			], body)
			
			write_body(stream, body) if body
			
			return stream
		end
	end
end
