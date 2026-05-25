# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "protocol/http3/client_server"

describe Protocol::HTTP3 do
	it "can exchange HTTP/3 request and response headers using Async" do
		result = Protocol::HTTP3::Fixtures.exchange
		
		expect(result[:request_headers]).to be(:include?, [":method", "GET"])
		expect(result[:request_headers]).to be(:include?, [":path", "/"])
		
		expect(result[:response_headers]).to be(:include?, [":status", "200"])
		expect(result[:response_headers]).to be(:include?, ["server", "protocol-http3-test"])
	end
	
	it "can exchange an HTTP/3 response body using Async" do
		result = Protocol::HTTP3::Fixtures.exchange(response_body: "Hello World!")
		
		expect(result[:response_headers]).to be(:include?, [":status", "200"])
		expect(result[:response_body]).to be == "Hello World!"
	end
	
	it "can exchange an HTTP/3 request body using Async" do
		result = Protocol::HTTP3::Fixtures.exchange(request_body: "Hello Server!")
		
		expect(result[:request_headers]).to be(:include?, [":method", "GET"])
		expect(result[:request_body]).to be == "Hello Server!"
	end
end
