# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

require "protocol/http3/test_dispatcher"

describe Protocol::HTTP3::Dispatcher do
	let(:configuration) {Protocol::QUIC::Configuration.new}
	let(:server_context) {Protocol::QUIC::TLS::ServerContext.new}
	
	it "can retain its HTTP/3 server dependencies" do
		server_context.add_protocol("h3")
		
		dispatcher = Protocol::HTTP3::TestDispatcher.new(configuration, server_context)
		
		expect(dispatcher.configuration).to be == configuration
		expect(dispatcher.tls_context).to be == server_context
		
		GC.start
		GC.compact
		
		expect(dispatcher.configuration).to be == configuration
		expect(dispatcher.tls_context).to be == server_context
	end
end
