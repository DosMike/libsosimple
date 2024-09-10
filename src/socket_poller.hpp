#include <map>
#include <mutex>
#include <memory>

#include <sosimple/utilities.hpp>
#include "socket_impl.hpp"

namespace sosimple {

class SocketPoller {
    std::map<socket_t, std::weak_ptr<sosimple::Socket>> sockets{};
    std::mutex socket_mutex{};
    std::thread pollThread{};
    bool isPolling{false};

    SocketPoller()=default;

    auto
    operator()() -> bool;

public:
    void
    operator+=(std::shared_ptr<sosimple::Socket> const& socket);

    void
    operator-=(socket_t descriptor);

    auto static
    get() -> SocketPoller&;

};

}