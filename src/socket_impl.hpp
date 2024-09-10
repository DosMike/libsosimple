#if !defined SOSIMPLE_SOCKET_IMPL_HPP
#define SOSIMPLE_SOCKET_IMPL_HPP

#include "watchdog.hpp"
#include <sosimple/socket.hpp>
#include <memory>
#include <atomic>
#include <thread>

namespace sosimple {

auto
createTCPServer(socket_t acceptedSocket, Endpoint remote) -> std::shared_ptr<ComSocket>;

class SocketBase : public Socket, public std::enable_shared_from_this<SocketBase> {
public:
    mutable socket_t mFD{POSIX_INVALID_DESCRIPTOR}; ///< if detected to be invalid/closed this is set back to -1 for shortcutting behaviour, otherwise constant
    mutable Endpoint mLocal{}; ///< might be lazy read after bind/construction once through getBoundEndpoint(), but doesn't change
    Endpoint mRemote{}; ///< remote empty for udp unicast or tcp listen. put during construction, then unchanged
    mutable int mFlags; ///< only internal markers. not really affecting state, more so reflecting it

    Watchdog mWatchDog{};
    Kind mKind;

private:
    Socket::SocketErrorCallback mSocketErrorEvent{};

public:
    SocketBase() : mKind(Kind::UNSPECIFIED) {};
    SocketBase(socket_t fd, Kind kind) : mFD(fd), mKind(kind) {};
    virtual ~SocketBase();

    auto
    notifySocketError(socket_error error) const -> void;

    auto
    onSocketError(Socket::SocketErrorCallback callback) -> void override;

    auto
    getKind() const -> Kind override
    { return mKind; }

    auto
    isOpen() const -> bool override;

    auto
    setTimeout(std::chrono::milliseconds timeout) -> void override;

    auto
    getNativeSocket() const -> socket_t override;

    auto
    getLocalEndpoint() const -> Endpoint override;

    /// @returns true if ready, false on timeout
    /// @throws socket_error otherwise
    auto
    pollFD(int POLL, unsigned timeoutMS, bool notifyOnTimeout) const -> bool;


};

class ListenSocketImpl : public SocketBase, public ListenSocket {
    AcceptCallback mAcceptEvent;

public:
    ListenSocketImpl() = default;
    explicit ListenSocketImpl(socket_t fd) : SocketBase(fd, Kind::TCP_Listen), ListenSocket() {
        setTimeout(std::chrono::milliseconds::zero()); // listen sockets usually dont time out, they listen patiently
    };

    ~ListenSocketImpl() override;

    // Note: delayed init for shared_from_this
    auto
    start() -> void;

    auto
    onSocketError(Socket::SocketErrorCallback callback) -> void override
    { SocketBase::onSocketError(callback); }

    auto
    getKind() const -> Kind override
    { return SocketBase::getKind(); }

    auto
    isOpen() const -> bool override
    { return SocketBase::isOpen(); }

    auto
    setTimeout(std::chrono::milliseconds timeout) -> void override
    { SocketBase::setTimeout(timeout); }

    auto
    getNativeSocket() const -> socket_t override
    { return SocketBase::getNativeSocket(); }

    auto
    getLocalEndpoint() const -> Endpoint override
    { return SocketBase::getLocalEndpoint(); }

// ----- for io -----

    auto
    notifyAccept(socket_t acceptedSocket, Endpoint remote) -> void;

    auto
    onAccept(AcceptCallback callback) -> void override;

// ----- for polling -----

    /// accept connections in the queue
    /// @return true if at least one connection was accepted
    auto
    accept() -> bool;

    auto
    checkWatchdog() -> void;

};

class ComSocketImpl : public SocketBase, public ComSocket {
    PacketReceivedCallback mPacketReceivedEvent;

public:
    ComSocketImpl() = default;
    ComSocketImpl(socket_t fd, Kind kind) : SocketBase(fd, kind), ComSocket() {};

    ~ComSocketImpl() override;

    // Note: delayed init for shared_from_this
    auto
    start() -> void;

    auto
    onSocketError(Socket::SocketErrorCallback callback) -> void override
    { SocketBase::onSocketError(callback); }

    auto
    getKind() const -> Kind override
    { return SocketBase::getKind(); }

    auto
    isOpen() const -> bool override
    { return SocketBase::isOpen(); }

    auto
    setTimeout(std::chrono::milliseconds timeout) -> void override
    { SocketBase::setTimeout(timeout); }

    auto
    getNativeSocket() const -> socket_t override
    { return SocketBase::getNativeSocket(); }

    auto
    getLocalEndpoint() const -> Endpoint override
    { return SocketBase::getLocalEndpoint(); }

    auto
    getRemoteEndpoint() const -> Endpoint override;

// ----- for io -----

    auto
    notifyPacket(const std::vector<uint8_t>& payload, Endpoint remote) -> void;

    auto
    onPacket(PacketReceivedCallback callback) -> void override;

    auto
    send(const std::vector<uint8_t>& payload, Endpoint remote) const -> void override;

// ----- for polling -----

    /// read one packet of how-ever-many-available-bytes
    /// @return true if at least 1 byte was read, false if no data available or error
    auto
    read() -> bool;

    auto
    checkWatchdog() -> void;
};

}

#endif