#pragma once

#include <ruby.h>

#include <Protocol/HTTP3/Stream.hpp>

#include <deque>

namespace Ruby::Protocol::HTTP3 {
	class Stream : public ::Protocol::HTTP3::Stream {
	public:
		Stream(::Protocol::HTTP3::Session & session, ::Protocol::QUIC::Connection & connection, ::Protocol::QUIC::StreamID stream_id);
		virtual ~Stream();

		nghttp3_data_reader * reader() noexcept {return &_reader;}

		void receive_input(const void *data, std::size_t size) override;
		void finish_input() override;

		void acknowledge_output(std::size_t size) override;

		bool output_pending() const noexcept;
		void append_output(VALUE chunk);
		void finish_output();
		void reset_output();

		VALUE shift_input();

		void mark();
		void compact();

		static nghttp3_ssize read_data(nghttp3_conn *connection, std::int64_t stream_id, nghttp3_vec *vectors, std::size_t vector_count, std::uint32_t *flags, void *connection_data, void *stream_data);

	private:
		nghttp3_data_reader _reader;

		std::deque<VALUE> _input_chunks;
		std::deque<VALUE> _output_chunks;
		std::deque<VALUE> _retained_output_chunks;

		std::size_t _acknowledged_output = 0;
		bool _input_finished = false;
		bool _output_finished = false;

		void clear_retained_output();
	};
}
