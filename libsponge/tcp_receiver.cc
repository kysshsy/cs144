#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.
#include <iostream>
template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().syn) {
        ISN = seg.header().seqno;
    }
    if (!ISN) {
        return;
    }
    _reassembler.push_substring(seg.payload().copy(),
                                unwrap(seg.header().seqno, ISN.value(), stream_out().bytes_written()) -
                                    (seg.header().syn ? 0 : 1),
                                seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!ISN)  // https://en.cppreference.com/w/cpp/utility/optional
        return nullopt;

    return wrap(stream_out().bytes_written() + 1 + stream_out().input_ended(), ISN.value());
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
