#include "Stream.hpp"

#include <stdexcept>

namespace Ruby::Protocol::HTTP3 {
	Stream::Stream(::Protocol::HTTP3::Session & session, ::Protocol::QUIC::Connection & connection, ::Protocol::QUIC::StreamID stream_id) :
		::Protocol::HTTP3::Stream(session, connection, stream_id),
		_reader{read_data}
	{
	}

	Stream::~Stream()
	{
	}

	void Stream::receive_input(const void *data, std::size_t size)
	{
		if (_input_finished) {
			throw std::runtime_error("Cannot receive input after stream input has finished.");
		}

		_input_chunks.push_back(rb_str_new(static_cast<const char *>(data), size));
	}

	void Stream::finish_input()
	{
		_input_finished = true;
	}

	void Stream::acknowledge_output(std::size_t size)
	{
		_acknowledged_output += size;

		while (!_retained_output_chunks.empty() && _acknowledged_output >= static_cast<std::size_t>(RSTRING_LEN(_retained_output_chunks.front()))) {
			_acknowledged_output -= static_cast<std::size_t>(RSTRING_LEN(_retained_output_chunks.front()));
			_retained_output_chunks.pop_front();
		}
	}

	bool Stream::output_pending() const noexcept
	{
		return !_output_chunks.empty();
	}

	void Stream::append_output(VALUE chunk)
	{
		if (_output_finished) {
			throw std::runtime_error("Cannot append output after stream output has finished.");
		}

		chunk = rb_str_to_str(chunk);
		_output_chunks.push_back(rb_str_dup_frozen(chunk));
	}

	void Stream::finish_output()
	{
		_output_finished = true;
	}

	void Stream::reset_output()
	{
		_output_finished = true;
		_output_chunks.clear();
		clear_retained_output();
	}

	VALUE Stream::shift_input()
	{
		if (_input_chunks.empty()) {
			return Qnil;
		}

		auto chunk = _input_chunks.front();
		_input_chunks.pop_front();

		return chunk;
	}

	void Stream::mark()
	{
		for (auto chunk : _input_chunks) {
			rb_gc_mark_movable(chunk);
		}

		for (auto chunk : _output_chunks) {
			rb_gc_mark_movable(chunk);
		}

		for (auto chunk : _retained_output_chunks) {
			rb_gc_mark_movable(chunk);
		}
	}

	void Stream::compact()
	{
		for (auto & chunk : _input_chunks) {
			chunk = rb_gc_location(chunk);
		}

		for (auto & chunk : _output_chunks) {
			chunk = rb_gc_location(chunk);
		}

		for (auto & chunk : _retained_output_chunks) {
			chunk = rb_gc_location(chunk);
		}
	}

	nghttp3_ssize Stream::read_data(nghttp3_conn *connection, std::int64_t stream_id, nghttp3_vec *vectors, std::size_t vector_count, std::uint32_t *flags, void *connection_data, void *stream_data)
	{
		(void)connection;
		(void)stream_id;
		(void)connection_data;

		if (vector_count == 0) {
			return 0;
		}

		auto stream = reinterpret_cast<Stream *>(stream_data);

		if (!stream) {
			*flags |= NGHTTP3_DATA_FLAG_EOF;
			return 0;
		}

		if (stream->_output_chunks.empty()) {
			if (!stream->_output_finished) {
				return NGHTTP3_ERR_WOULDBLOCK;
			}

			*flags |= NGHTTP3_DATA_FLAG_EOF;
			return 0;
		}

		stream->_retained_output_chunks.push_back(stream->_output_chunks.front());
		stream->_output_chunks.pop_front();

		auto chunk = stream->_retained_output_chunks.back();

		vectors[0].base = reinterpret_cast<std::uint8_t *>(RSTRING_PTR(chunk));
		vectors[0].len = RSTRING_LEN(chunk);

		if (stream->_output_finished && stream->_output_chunks.empty()) {
			*flags |= NGHTTP3_DATA_FLAG_EOF;
		}

		return 1;
	}

	void Stream::clear_retained_output()
	{
		_retained_output_chunks.clear();
		_acknowledged_output = 0;
	}
}
