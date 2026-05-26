# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

module Protocol::HTTP3
	# A single HTTP/3 request or response body stream.
	class Stream
		# Initialize the stream wrapper for the given connection and native stream identifier.
		def initialize(connection, stream_id)
			@connection = connection
			@stream_id = stream_id
		end
		
		attr :connection
		attr :stream_id
		
		# Read the next available body chunk from the stream.
		def read_chunk
			@connection.__send__(:read_body_chunk, @stream_id)
		end
		
		# Write a body chunk to the stream.
		def write_chunk(chunk)
			@connection.__send__(:write_body_chunk, @stream_id, chunk)
			@connection.send_packets
		end
		
		# Finish the stream after all body chunks have been written.
		def finish
			@connection.__send__(:finish_body, @stream_id)
			@connection.send_packets
		end
		
		# Reset the stream with the optional HTTP/3 error code.
		def reset(error_code = nil)
			@connection.__send__(:reset_body, @stream_id, error_code)
			@connection.send_packets
		end
		
		# Write all chunks from the given body to the stream.
		def write_body(body)
			error = nil
			
			begin
				while chunk = body.read
					write_chunk(chunk)
				end
			rescue => error
				reset
				raise
			ensure
				body.close(error)
				finish unless error
			end
		end
	end
end
