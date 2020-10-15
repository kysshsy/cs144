#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _since_last_receive; }

void TCPConnection::send_segments() {
    while (!_sender.segments_out().empty()) {
        TCPSegment &send_seg = _sender.segments_out().front();
        //填充 ackno 和 win
        if (_receiver.ackno()) {
            send_seg.header().ack = true;
            send_seg.header().ackno = _receiver.ackno().value();
            send_seg.header().win = _receiver.window_size();
        }

        _segments_out.push(send_seg);
        _sender.segments_out().pop();
    }
}
void TCPConnection::clean_shutdown() {
    if (_active_close) {
        if (time_since_last_segment_received() < 10 * _cfg.rt_timeout) {
            return;
        }
    }

    _active = false;
    _linger_after_streams_finish = false;
}
void TCPConnection::unclean_shutdown(bool active) {
    _active = false;
    _linger_after_streams_finish = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();

    if (active) {
        _sender.send_empty_segment();
        TCPSegment &send_seg = _sender.segments_out().back();
        send_seg.header().rst = true;

        _segments_out.push(send_seg);
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // segment_received 有两个任务
    // 第一个是 解析seg 给sender 和 receiver
    // 第二个是 取出sender要发送的seg，依据receiver的 ack 和 winsize 填充

    // 解析seg
    // RST 包
    _since_last_receive = 0;

    if (seg.header().rst) {
        // kill all
        unclean_shutdown(false);
        return;
    }
    //被动关闭connect
    if (seg.header().fin && !_sender.stream_in().eof()) {
        _active_close = false;
        _linger_after_streams_finish = false;
    }
    _receiver.segment_received(seg);

    if (seg.header().syn) {
        _sender.fill_window();
    }
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (_sender.next_seqno_absolute() > 0)
            _sender.fill_window();
    }

    // 若没有segment
    if (seg.length_in_sequence_space() && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    send_segments();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _since_last_receive += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        unclean_shutdown(true);
        return;
    }
    send_segments();

    if (_sender.stream_in().eof() && _receiver.stream_out().eof() && _sender.bytes_in_flight() == 0) {
        clean_shutdown();
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
}

void TCPConnection::connect() {
    _active = true;
    _sender.fill_window();
    send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
<<<<<<< HEAD
            unclean_shutdown(true);
=======
>>>>>>> CS144 Lab 4 assignment
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
