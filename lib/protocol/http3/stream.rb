# frozen_string_literal: true

# Released under the MIT License.
# Copyright, 2026, by Samuel Williams.

module Protocol::HTTP3
	class Stream
		def initialize(connection, stream_id)
			@connection = connection
			@stream_id = stream_id
		end

		attr :connection
		attr :stream_id

		def read_chunk
			@connection.__send__(:read_body_chunk, @stream_id)
		end

		def write_chunk(chunk)
			@connection.__send__(:write_body_chunk, @stream_id, chunk)
			@connection.send_packets
		end

		def finish
			@connection.__send__(:finish_body, @stream_id)
			@connection.send_packets
		end

		def reset(error_code = nil)
			@connection.__send__(:reset_body, @stream_id, error_code)
			@connection.send_packets
		end

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
