#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), buffer(capacity, '\0'), intervals(), seq_num(0), eof_num(INT32_MAX) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.

// TODO: 并查集优化
// TODO: unassembled 优化
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t begin_seq = seq_num;
    size_t end_seq = seq_num - _output.buffer_size() + _capacity;

    if (max(index, begin_seq) > min(index + data.size(), end_seq))
        return;

    if (!data.empty()) {
        vector<pair<size_t, size_t>>::iterator pos = intervals.end();
        for (auto it = intervals.begin(); it != intervals.end(); it++) {
            if (index > it->first) {
                continue;
            }
            pos = intervals.insert(it, make_pair(max(begin_seq, index), min(end_seq, index + data.size())));

            break;
        }
        // 最后插入
        if (pos == intervals.end()) {
            intervals.push_back(make_pair(max(begin_seq, index), min(end_seq, index + data.size())));
        }

        // 合并
        auto it = intervals.begin();

        while (it != intervals.end()) {
            auto next = std::next(it);

            if (next == intervals.end())
                break;

            while (next != intervals.end() && next->first <= it->second) {
                it->second = max(it->second, next->second);
                next = intervals.erase(next);
            }
            it = next;
        }
    }
    size_t begin = max(index, begin_seq);
    size_t end = min(index + data.size(), end_seq);

    buffer.replace(
        buffer.begin() + (begin - seq_num), buffer.begin() + (end - seq_num), data.substr(begin - index, end - begin));

    if (eof) {
        eof_num = min(eof_num, index + data.size());
    }

    // 整理
    size_t temp_seq = seq_num;
    while (!intervals.empty() && intervals.begin()->first == temp_seq) {
        auto it = intervals.begin();
        _output.write(buffer.substr(it->first - seq_num, it->second - it->first));

        temp_seq = it->second;
        intervals.erase(it);
    }

    if (temp_seq != seq_num) {
        buffer = buffer.substr(temp_seq - seq_num);
        buffer.resize(_capacity, '\0');

        seq_num = temp_seq;
    }

    if (seq_num == eof_num) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t sum = 0;
    for (auto it : intervals) {
        sum += it.second - it.first;
    }
    return sum;
}

bool StreamReassembler::empty() const { return (unassembled_bytes() == 0); }
