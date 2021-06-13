#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void NetworkInterface::send_arp(bool reqeust, EthernetAddress ethernet, uint32_t ip) {
    ARPMessage arp;

    arp.sender_ip_address = _ip_address.ipv4_numeric();
    arp.sender_ethernet_address = _ethernet_address;

    if (reqeust) {
        arp.opcode = ARPMessage::OPCODE_REQUEST;
        arp.target_ip_address = ip;
    } else {
        arp.opcode = ARPMessage::OPCODE_REPLY;
        arp.target_ethernet_address = ethernet;
        arp.target_ip_address = ip;
    }
    EthernetFrame frame;
    frame.payload() = arp.serialize();

    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.header().dst = ethernet;
    frame.header().src = _ethernet_address;

    _frames_out.push(frame);
}
//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    EthernetFrame send_frame;
    send_frame.header().src = _ethernet_address;
    send_frame.header().type = EthernetHeader::TYPE_IPv4;
    send_frame.payload() = dgram.serialize();

    // 没找到映射
    if (_cache.find(next_hop_ip) == _cache.end() || _cache[next_hop_ip].first == ETHERNET_BROADCAST) {
        _queue.push_back(make_pair(dgram, next_hop));
        // 发送ARP 请求
        if (_cache.find(next_hop_ip) == _cache.end()) {
            _cache[next_hop_ip] = {ETHERNET_BROADCAST, 1000 * 5};
            send_arp(true, ETHERNET_BROADCAST, next_hop_ip);
        }
    } else {
        send_frame.header().dst = _cache[next_hop_ip].first;
        _frames_out.push(send_frame);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    EthernetAddress dst = frame.header().dst;

    if (dst != _ethernet_address && dst != ETHERNET_BROADCAST)
        return nullopt;

    // 出现新地址/ARP reply时 遍历_queue发送
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram recv;
        if (recv.parse(frame.payload().concatenate()) == ParseResult::NoError)
            return recv;
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        if (arp.parse(frame.payload().concatenate()) != ParseResult::NoError) {
            return nullopt;
        }

        // add in _cache
        _cache[arp.sender_ip_address] = {arp.sender_ethernet_address, 1000 * 30};

        if (arp.opcode == ARPMessage::OPCODE_REQUEST) {
            //发送 arp respond
            if (arp.target_ip_address == _ip_address.ipv4_numeric()) {
                send_arp(false, arp.sender_ethernet_address, arp.sender_ip_address);
            }
        }

        for (auto it = _queue.begin(); it != _queue.end();) {
            if (_cache.find(it->second.ipv4_numeric()) == _cache.end()) {
                // 发送 ARP请求
                send_arp(true, ETHERNET_BROADCAST, it->second.ipv4_numeric());
                it++;
            } else if (_cache[it->second.ipv4_numeric()].first != ETHERNET_BROADCAST) {
                send_datagram(it->first, it->second);
                it = _queue.erase(it);
            } else {
                it++;
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // 扫描_cache 若发现过期的地址 删除，若地址为broadcast地址，则重发ARP请求?
    for (auto it = _cache.begin(); it != _cache.end();) {
        it->second.second = it->second.second - static_cast<int64_t>(ms_since_last_tick);
        if (it->second.second < 0) {
            it = _cache.erase(it);
        } else {
            it++;
        }
    }
}
