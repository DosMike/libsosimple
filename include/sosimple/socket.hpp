#if !defined SOSIMPLE_SOCKET_HPP
#define SOSIMPLE_SOCKET_HPP

#include "sosimple/platforms.hpp"
#include "sosimple/endpoint.hpp"
#include "sosimple/utilities.hpp"
#include <chrono>

namespace sosimple {

class ComSocket;
class ListenSocket;


SOSIMPLE_API auto
createUDPUnicast(Endpoint bind) -> std::shared_ptr<ComSocket>;

SOSIMPLE_API auto
createUDPMulticast(Endpoint bind, Endpoint multicastgroup) -> std::shared_ptr<ComSocket>;

SOSIMPLE_API auto
createTCPListen(Endpoint bind) -> std::shared_ptr<ListenSocket>;

SOSIMPLE_API auto
createTCPClient(Endpoint local, Endpoint remote) -> std::shared_ptr<ComSocket>;


/**
 * More or less a wrapper for posix sockets, with factory methods for different kinds of connections.
 *
 * Note on multicast: while any endpoint can technically join any multicast group and will receive all UDP messages directly targeted to the
 * socket, this library only supports one multicast group or a unicast connection on a signle socket. Retrieving the multicast group a message
 * was sent to requires massive overhead (raw socket, parsing the frames manually) as posix sockets only give you the unicast remote.
 */
class SOSIMPLE_API Socket {
public:
    using SocketErrorCallback = std::function<void(socket_error)>;
    enum class Kind {
        UNSPECIFIED, ///< a socket instance with this value was probably not constructed properly
        UDP_Unicast, ///< UDP socket for direct communication
        UDP_Multicast, ///< UDP socket talking with a multicast group
        TCP_Listen, ///< TCP listen socket accepting connections
        TCP_Server, ///< TCP socket accepted from a listen socket
        TCP_Client, ///< TCP socket connected to a server socket
    };

protected:
    Socket() = default;

public:
    virtual ~Socket() = default;

    // move is ok; copy would close FD, block that
    Socket(Socket&&) = default;
    Socket(const Socket&) = delete;
    Socket& operator=(Socket&&) = default;
    Socket& operator=(const Socket&) = delete;

    virtual auto
    onSocketError(SocketErrorCallback callback) -> void = 0;

    virtual auto
    getKind() const -> Kind = 0;

    virtual auto
    isOpen() const -> bool = 0;

    virtual auto
    setTimeout(std::chrono::milliseconds timeout) -> void = 0;

    virtual auto
    getNativeSocket() const -> socket_t = 0;

    virtual auto
    getLocalEndpoint() const -> Endpoint = 0;
};

class SOSIMPLE_API ListenSocket : public Socket {
public:
    using AcceptCallback = std::function<void(std::shared_ptr<ComSocket>,Endpoint)>;

protected:
    ListenSocket() = default;

public:
    ~ListenSocket() override = default;

    virtual auto
    onAccept(AcceptCallback callback) -> void = 0;

};

class SOSIMPLE_API ComSocket : public Socket {
public:
    using PacketReceivedCallback = std::function<void(const std::vector<uint8_t>&, Endpoint)>;

protected:
    ComSocket() = default;

public:
    ~ComSocket() override = default;

    /// returns the connected remote for tcp, the multicast group for udp, nothing for unicast udp
    virtual auto
    getRemoteEndpoint() const -> Endpoint = 0;

    virtual auto
    onPacket(PacketReceivedCallback callback) -> void = 0;

    /// send a bunch of bytes. remote is ignored for udp multicast and tcp sockets
    virtual auto
    send(const std::vector<uint8_t>& payload, Endpoint remote={}) const -> void = 0;
};

}

#endif