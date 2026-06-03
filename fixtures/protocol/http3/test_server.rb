# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "protocol/http3"

module Protocol::HTTP3
	class TestServer < Server
		def initialize(...)
			super
			@request_headers = Hash.new{|hash, key| hash[key] = []}
			@request_body = Hash.new{|hash, key| hash[key] = String.new}
			@responded = {}
			@body_tasks = []
			@report_all_requests = false
		end
		
		attr_accessor :requests, :reported_request
		attr_accessor :response_body
		attr_accessor :report_all_requests
		
		def header_received(stream_id, name, value)
			@request_headers[stream_id] << [name, value]
		end
		
		def headers_finished(stream_id, is_final)
			return unless is_final
			
			respond(stream_id)
		end
		
		def data_received(stream_id, chunk)
			@request_body[stream_id] << chunk
		end
		
		def stream_finished(stream_id)
			respond(stream_id)
		end
		
		def respond(stream_id)
			return if @responded[stream_id]
			
			@responded[stream_id] = true
			
			if report_all_requests || !reported_request
				self.reported_request = true
				requests&.push({
					headers: @request_headers[stream_id],
					body: @request_body[stream_id],
				})
			end
			
			stream = submit_response(stream_id, [
				[":status", "200"],
				["server", "protocol-http3-test"],
			], response_body)
			
			write_body(stream, response_body) if response_body
		end
		
		def write_body(stream, body, parent: Async::Task.current)
			@body_tasks << parent.async do
				stream.write_body(body)
			end
		end
	end
end
