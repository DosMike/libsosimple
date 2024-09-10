#include "socket_poller.hpp"

auto
sosimple::SocketPoller::get() -> SocketPoller& {
    static SocketPoller instance{};
    return instance;
}

auto
sosimple::SocketPoller::operator+=(std::shared_ptr<sosimple::Socket> const& socket) -> void
{
    std::unique_lock lock{socket_mutex};
    // thread management
    if (!isPolling) {
        isPolling = true;
        pollThread = std::thread{[self=this]{
            int delayms = 0;
            while (self->isPolling) {
                if ((*self)()) {
                    // under load
                    delayms = 0;
                    std::this_thread::yield();
                } else {
                    // we just had an idle loop, relax a bit
                    if (delayms < 50) delayms += 1;
                    std::this_thread::sleep_for(std::chrono::milliseconds(delayms));
                }
            }
        }};
    }

    // state
    sockets.emplace(socket->getNativeSocket(), std::weak_ptr<sosimple::Socket>{socket});
}

auto
sosimple::SocketPoller::operator-=(socket_t descriptor) -> void
{
    std::unique_lock lock{socket_mutex};

    // state
    auto where = sockets.find(descriptor);
    if (where != sockets.end()) sockets.erase(where);

    // thread management
    if (sockets.empty() && isPolling) {
        isPolling = false;
        pollThread.join();
    }
}

auto
sosimple::SocketPoller::operator()() -> bool
{
    std::unique_lock lock{socket_mutex};
    decltype(sockets) copy{sockets};
    lock.unlock(); // we got our copy, we dont care anymore

    // read all sockets that have data
    bool areWeBusy{false};
    for (auto [_,wsock] : copy) {
        // no error and no data, skip read
        auto sock = wsock.lock();
        if (!sock) { continue; }

        if (auto comsock = std::dynamic_pointer_cast<sosimple::ComSocketImpl>(sock)) {
            if (comsock->read()) areWeBusy = true;
            else comsock->checkWatchdog();
        } else if (auto listsock = std::dynamic_pointer_cast<sosimple::ListenSocketImpl>(sock)) {
            if (listsock->accept()) areWeBusy = true;
            else listsock->checkWatchdog();
        }
    }
    return areWeBusy;
}