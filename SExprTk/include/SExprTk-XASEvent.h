#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace sexprtk_xas {

enum class EventType : std::uint8_t {
    BeginList = 1,
    Atom = 2,
    EndList = 3,
    Comment = 4,
    Error = 5
};

constexpr std::uint8_t kMagic = 0x5A;
constexpr std::uint8_t kVersion = 1;
constexpr std::size_t  kHeaderSize = 8;

inline std::string_view to_string(EventType type) {
    switch (type) {
    case EventType::BeginList: return "begin";
    case EventType::Atom:      return "atom";
    case EventType::EndList:   return "end";
    case EventType::Comment:   return "comment";
    case EventType::Error:     return "error";
    }
    return "atom";
}

inline EventType event_type_from_string(std::string_view text) {
    if (text == "begin")   return EventType::BeginList;
    if (text == "end")     return EventType::EndList;
    if (text == "comment") return EventType::Comment;
    if (text == "error")   return EventType::Error;
    return EventType::Atom;
}

struct EventWire {
    std::uint64_t sequence {0};
    EventType type {EventType::Atom};
    std::string payload {};

    std::string encode() const;
    static EventWire decode(std::string_view text);
};

struct DatagramFrame {
    std::vector<std::uint8_t> bytes {};

    static DatagramFrame from_event(const EventWire& event);
    static DatagramFrame from_text(std::string_view text);
    EventWire to_event() const;
    std::string_view text() const;
};

inline std::string EventWire::encode() const {
    return std::to_string(sequence) + "|" + std::string(to_string(type)) + "|" + payload;
}

inline EventWire EventWire::decode(std::string_view text) {
    EventWire event;
    const auto a = text.find('|');
    const auto b = text.find('|', a == std::string_view::npos ? a : a + 1);
    if (a != std::string_view::npos) event.sequence = std::stoull(std::string(text.substr(0, a)));
    if (a != std::string_view::npos && b != std::string_view::npos) {
        event.type = event_type_from_string(text.substr(a + 1, b - a - 1));
        event.payload = std::string(text.substr(b + 1));
    }
    return event;
}

inline DatagramFrame DatagramFrame::from_event(const EventWire& event) {
    const auto encoded = event.encode();
    DatagramFrame frame;
    frame.bytes.reserve(kHeaderSize + encoded.size());
    frame.bytes.push_back(kMagic);
    frame.bytes.push_back(kVersion);
    frame.bytes.push_back(static_cast<std::uint8_t>(event.type));
    frame.bytes.push_back(static_cast<std::uint8_t>(encoded.size() >> 24) & 0xFF);
    frame.bytes.push_back(static_cast<std::uint8_t>(encoded.size() >> 16) & 0xFF);
    frame.bytes.push_back(static_cast<std::uint8_t>(encoded.size() >> 8)  & 0xFF);
    frame.bytes.push_back(static_cast<std::uint8_t>(encoded.size())       & 0xFF);
    frame.bytes.push_back(0); // reserved
    frame.bytes.insert(frame.bytes.end(), encoded.begin(), encoded.end());
    return frame;
}

inline DatagramFrame DatagramFrame::from_text(std::string_view text) {
    DatagramFrame frame;
    frame.bytes.reserve(kHeaderSize + text.size());
    frame.bytes.push_back(kMagic);
    frame.bytes.push_back(kVersion);
    frame.bytes.push_back(static_cast<std::uint8_t>(EventType::Atom));
    frame.bytes.push_back(static_cast<std::uint8_t>(text.size() >> 24) & 0xFF);
    frame.bytes.push_back(static_cast<std::uint8_t>(text.size() >> 16) & 0xFF);
    frame.bytes.push_back(static_cast<std::uint8_t>(text.size() >> 8)  & 0xFF);
    frame.bytes.push_back(static_cast<std::uint8_t>(text.size())       & 0xFF);
    frame.bytes.push_back(0);
    frame.bytes.insert(frame.bytes.end(), text.begin(), text.end());
    return frame;
}

inline EventWire DatagramFrame::to_event() const {
    if (bytes.size() < kHeaderSize || bytes[0] != kMagic || bytes[1] != kVersion) {
        throw std::runtime_error("invalid datagram frame");
    }
    std::uint32_t len = (static_cast<std::uint32_t>(bytes[3]) << 24)
                      | (static_cast<std::uint32_t>(bytes[4]) << 16)
                      | (static_cast<std::uint32_t>(bytes[5]) << 8)
                      |  static_cast<std::uint32_t>(bytes[6]);
    if (bytes.size() < kHeaderSize + len) throw std::runtime_error("truncated datagram frame");
    std::string_view sv(reinterpret_cast<const char*>(bytes.data() + kHeaderSize), len);
    auto ev = EventWire::decode(sv);
    ev.type = static_cast<EventType>(bytes[2]);
    return ev;
}

inline std::string_view DatagramFrame::text() const {
    if (bytes.size() < kHeaderSize) return {};
    std::uint32_t len = (static_cast<std::uint32_t>(bytes[3]) << 24)
                      | (static_cast<std::uint32_t>(bytes[4]) << 16)
                      | (static_cast<std::uint32_t>(bytes[5]) << 8)
                      |  static_cast<std::uint32_t>(bytes[6]);
    if (bytes.size() < kHeaderSize + len) return {};
    return std::string_view(reinterpret_cast<const char*>(bytes.data() + kHeaderSize), len);
}

} // namespace sexprtk_xas
