#include "SExprTk.hpp"
#include <array>
#include <iostream>
#include <string_view>

int main() {
    sexprtk::SExprTk rt;
    sexprtk::XASEventDispatcher disp;
    auto cartable = rt.parse(sexprtk::Source::from_string("(hello (world 42))"), &disp);

    std::cout << "events: " << disp.size() << "\n";
    for (const auto& ev : disp.buffered) {
        sexprtk_xas::EventWire wire{ev.sequence, ev.type, ev.payload};
        auto frame = sexprtk_xas::DatagramFrame::from_event(wire);
        auto round = frame.to_event();
        std::cout << ev.sequence << "|" << sexprtk_xas::to_string(ev.type) << "|" << ev.payload
                  << "  frame_bytes=" << frame.bytes.size()
                  << "  round-trip=" << round.encode() << '\n';
    }
}
