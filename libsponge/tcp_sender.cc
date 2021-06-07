#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _outstanding{}
    , _retransmit_timeout(retx_timeout)
    , _timeup{nullopt}
    , _retransmits{0}
    , _window_size{1}
    , _sending{0} {}

uint64_t TCPSender::bytes_in_flight() const { return _sending; }
// syn ack前 不应该发送
// fin 发送后 不应该发送
void TCPSender::fill_window() {
    size_t window_size = _window_size;

    if (_window_size == 0) {
        window_size = 1;
    }

    while (_sending < window_size) {
        TCPSegment newSeg;

        // set header
        newSeg.header().seqno = next_seqno();

        // syn
        newSeg.header().syn = (_next_seqno == 0) ? true : false;

        // 读取stream_in 填充window
        int64_t window = (_sending >= window_size) ? 0 : window_size - _sending;

        window -= (newSeg.header().syn);

        if (window < 0)
            return;
        newSeg.payload() = _stream.read(min(static_cast<size_t>(window), TCPConfig::MAX_PAYLOAD_SIZE));

        // fin在读取数据之后决定 若窗口不够 暂时不发送
        // fin
        window -= newSeg.length_in_sequence_space();
        newSeg.header().fin = (_stream.eof() && window) ? true : false;

        //无效segment 只在send_empty_segment 发送
        if (!newSeg.length_in_sequence_space())
            return;
        // fin 已发送
        if (_stream.eof() && next_seqno_absolute() == _stream.bytes_read() + 2)
            return;

        // send and save
        _segments_out.push(newSeg);
        _sending += newSeg.length_in_sequence_space();
        _next_seqno += newSeg.length_in_sequence_space();
        _outstanding.push(newSeg);
        if (!_timeup) {
            _timeup = _retransmit_timeout;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;

    uint64_t ack_seqno = unwrap(ackno, _isn, _next_seqno);
    while (!_outstanding.empty()) {
        auto &seg = _outstanding.front();
        uint64_t seg_seq = unwrap(seg.header().seqno, _isn, _next_seqno);
        if (seg_seq + seg.length_in_sequence_space() > ack_seqno)
            break;

        _sending -= seg.length_in_sequence_space();
        _outstanding.pop();

        _retransmit_timeout = _initial_retransmission_timeout;
        _retransmits = 0;

        if (_outstanding.empty()) {
            _timeup = nullopt;
        } else {
            _timeup = _retransmit_timeout;
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timeup)
        return;
    // retramsmit

    _timeup = *_timeup - ms_since_last_tick;
    if (_timeup <= 0) {
        _segments_out.push(_outstanding.front());
        if (_window_size != 0) {
            _retransmits++;
            _retransmit_timeout *= 2;
        }
        _timeup = _retransmit_timeout;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmits; }

void TCPSender::send_empty_segment() {
    // delay ACK 学到了
    // 当没有数据需要发送，但需要回应ACK时，发送delay ACK
    TCPSegment newSeg;
    newSeg.header().seqno = next_seqno();
    _segments_out.push(newSeg);
}
