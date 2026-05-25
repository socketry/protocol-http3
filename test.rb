#!/usr/bin/env ruby
# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

$LOAD_PATH.unshift(File.expand_path("lib", __dir__))
$LOAD_PATH.unshift(File.expand_path("fixtures", __dir__))

require "protocol/http3/client_server"

result = Protocol::HTTP3::Fixtures.exchange

unless result[:request_headers].include?([":method", "GET"])
	abort "Request headers did not include GET method: #{result[:request_headers].inspect}"
end

unless result[:response_headers].include?([":status", "200"])
	abort "Response headers did not include 200 status: #{result[:response_headers].inspect}"
end

$stderr.puts "Completed HTTP/3 client/server exchange"
