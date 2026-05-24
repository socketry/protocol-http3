# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2023-2026, by Samuel Williams.

require "protocol/http3/version"

describe Protocol::HTTP3 do
	it "has a version" do
		expect(Protocol::HTTP3::VERSION).to be =~ /\d+\.\d+\.\d+/
	end
end
