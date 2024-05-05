#if !defined SOSIMPLE_WATCHDOG_HPP
#define SOSIMPLE_WATCHDOG_HPP

#include <chrono>
#include <functional>
#include <iostream>

namespace sosimple {

class Watchdog {
    using clock = std::chrono::steady_clock;
    using interval = std::chrono::milliseconds;
    using TimeoutCallback = std::function<void()>;
    interval timeout;
    clock::time_point last_reset{clock::now()};
    TimeoutCallback onTimeoutEvent{};

public:
    Watchdog()
    : timeout(interval::zero()) {}
    explicit Watchdog(std::chrono::milliseconds interval, TimeoutCallback callback={})
    : timeout(interval), onTimeoutEvent(callback) {}

    inline auto
    setTimeout(std::chrono::milliseconds interval)
    { timeout = interval; }

    inline auto
    setCallback(TimeoutCallback newCallback)
    { onTimeoutEvent = newCallback; }

    inline auto
    check() const -> bool
    {
        if (timeout == interval::zero())
            return true;
        auto fine = clock::now() < last_reset + timeout;
        if (!fine && onTimeoutEvent) try {
            onTimeoutEvent();
        } catch(...){
        }
        return fine;
    }

    inline auto
    reset() -> void
    { last_reset = clock::now(); }

    inline auto
    check_and_reset() -> bool
    { auto tripped = check(); reset(); return tripped; }
};

}

#endif