# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

teapot_version "3.0"

define_project "ruby-protocol-http3" do |project|
	project.title = "Protocol::HTTP3"
	project.license = "MIT License"
end

define_target "ruby-protocol-http3" do |target|
	target.depends "Language/C++17"
	
	target.depends "Library/Protocol/HTTP3"
	target.depends "Library/ruby"
	target.depends "Build/Files"
	target.depends "Build/Compile/Commands"
	
	target.provides "Ruby/Protocol/HTTP3" do
		source_root = target.package.path + "source"
		
		library_path = build dynamic_library: "Ruby_Protocol_HTTP3", source_files: source_root.glob("**/*.{c,cpp}")
		
		copy source: [library_path], prefix: environment[:ruby_install_path]
		
		compile_commands destination_path: (source_root + "compile_commands.json")
	end
end

define_configuration "ruby-protocol-http3" do |configuration|
	configuration[:source] = "https://github.com/kurocha/"
	
	configuration.require "platforms"
	
	configuration.require "build-make"
	configuration.require "build-cmake"
	
	configuration.require "scheduler-ruby"
	configuration.require "protocol-http3"
	configuration.require "ruby"
	
	configuration.require "generate-template"
	configuration.require "generate-cpp-class"
	
	configuration.require "build-compile-commands"
end
