#!/usr/bin/env ruby
# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

$LOAD_PATH.unshift(File.expand_path("lib", __dir__))

require "protocol/http3"

configuration = Protocol::QUIC::Configuration.new
server_context = Protocol::QUIC::TLS::ServerContext.new
server_context.add_protocol("h3")

dispatcher = Protocol::HTTP3::Dispatcher.new(configuration, server_context)

abort "Configuration mismatch" unless dispatcher.configuration.equal?(configuration)
abort "TLS context mismatch" unless dispatcher.tls_context.equal?(server_context)

GC.start
GC.compact

abort "Configuration mismatch after compaction" unless dispatcher.configuration.equal?(configuration)
abort "TLS context mismatch after compaction" unless dispatcher.tls_context.equal?(server_context)

$stderr.puts "Completed HTTP/3 dispatcher smoke test"
