# frozen_string_literal: true

require_relative "lib/protocol/http3/version"

Gem::Specification.new do |spec|
	spec.name = "protocol-http3"
	spec.version = Protocol::HTTP3::VERSION
	
	spec.summary = "HTTP/3 protocol implementation using ngtcp2 and nghttp3."
	spec.authors = ["Samuel Williams"]
	spec.license = "MIT"
	
	spec.cert_chain  = ["release.cert"]
	spec.signing_key = File.expand_path("~/.gem/release.pem")
	
	spec.homepage = "https://github.com/socketry/protocol-http3"
	
	spec.metadata = {
		"documentation_uri" => "https://socketry.github.io/protocol-http3/",
		"source_code_uri" => "https://github.com/socketry/protocol-http3.git",
		"funding_uri" => "https://github.com/sponsors/ioquatix",
	}
	
	spec.files = Dir["lib/**/*.rb", "ext/source/**/*.{cpp,hpp}", "*.md", "ext/rakefile.rb", "ext/teapot.rb", "ext/*-lock.yml", base: __dir__]
	spec.require_paths = ["lib"]
	
	spec.extensions = ["ext/rakefile.rb"]
	
	spec.required_ruby_version = ">= 3.3"
	
	spec.add_dependency "protocol-http", "~> 0.62"
	spec.add_dependency "protocol-quic", "~> 0.0.8"
	spec.add_dependency "teapot", "~> 3.5"
	spec.add_dependency "rake"
end
