# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "protocol/http3/client_server"
require "protocol/http/body/buffered"

describe Protocol::HTTP3 do
	class ChunkedBody < Protocol::HTTP::Body::Buffered
		def initialize(*chunks)
			super(chunks)
			@reads = 0
		end

		attr_reader :reads

		def read
			@reads += 1
			super
		end
	end

	class ReusedStringBody
		def initialize(*chunks)
			@chunks = chunks
			@buffer = String.new
			@reads = 0
		end

		attr_reader :reads

		def read
			@reads += 1

			if chunk = @chunks.shift
				@buffer.replace(chunk)
			end
		end

		def close(error = nil)
			@error = error
		end
	end

	it "can exchange HTTP/3 request and response headers using Async" do
		result = Protocol::HTTP3::Fixtures.exchange

		expect(result[:request_headers]).to be(:include?, [":method", "GET"])
		expect(result[:request_headers]).to be(:include?, [":path", "/"])

		expect(result[:response_headers]).to be(:include?, [":status", "200"])
		expect(result[:response_headers]).to be(:include?, ["server", "protocol-http3-test"])
	end

	it "can exchange an HTTP/3 response body using Async" do
		body = ChunkedBody.new("Hello", " ", "World!")
		result = Protocol::HTTP3::Fixtures.exchange(response_body: body)

		expect(result[:response_headers]).to be(:include?, [":status", "200"])
		expect(result[:response_body]).to be == "Hello World!"
		expect(body.reads).to be == 4
	end

	it "can exchange an HTTP/3 request body using Async" do
		body = ChunkedBody.new("Hello", " ", "Server!")
		result = Protocol::HTTP3::Fixtures.exchange(request_body: body)

		expect(result[:request_headers]).to be(:include?, [":method", "GET"])
		expect(result[:request_body]).to be == "Hello Server!"
		expect(body.reads).to be == 4
	end

	it "duplicates output chunks before retaining them" do
		body = ReusedStringBody.new("Hello", " ", "World!")
		result = Protocol::HTTP3::Fixtures.exchange(response_body: body)

		expect(result[:response_body]).to be == "Hello World!"
		expect(body.reads).to be == 4
	end
end
