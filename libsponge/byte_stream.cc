#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : stream(), capacity_(capacity) {}

size_t ByteStream::write(const string &data) {
    if (input_ended())
        return 0;

    size_t accept = min(capacity_ - stream.size(), data.size());

    stream.append(data, 0, accept);

    // 写入计数
    written_ += accept;
    return accept;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const { return stream.substr(0, len); }

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    stream.erase(0, len);
    read_ += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string out = stream.substr(0, len);
    pop_output(out.size());
    return out;
}

void ByteStream::end_input() { end_ = true; }

bool ByteStream::input_ended() const { return end_; }

size_t ByteStream::buffer_size() const { return stream.size(); }

bool ByteStream::buffer_empty() const { return stream.empty(); }

bool ByteStream::eof() const { return end_ && stream.empty(); }

size_t ByteStream::bytes_written() const { return written_; }

size_t ByteStream::bytes_read() const { return read_; }

size_t ByteStream::remaining_capacity() const { return capacity_ - stream.size(); }
