#if !defined SOSIMPLE_ENDPOINT_HPP
#define SOSIMPLE_ENDPOINT_HPP

#include "exports.hpp"

#include "sosimple/netheaders.hpp"
#include <cstring>

namespace sosimple {

/**
 * And enpoint describes where connections come from or go to. This includes the IP address (IPv4/IPv6) and a port.
 * A port can and should usually only be used once per protocol (UDP/TCP) to avoid receiving data you are not actually
 * interested in. For local endpoints, the IP address usually equates to a network interface, the network adapter
 * you send data from. There's also some special addresses and ports: 0.0.0.0 is the "any" address and 0 is the any port.
 * These usually get resolved automatically on binding a socket, but might remain unspecific. Under other circumstances
 * it acts as a filter allowing any address. Other addresses and address ranges have different meansings as well, like
 * 127.0.0.1 being loopback/localhost, 255.255.255.255 being broadcast (make your net admins mad), and 224.0.0.0 to
 * 239.255.255.255 being multicast groups.
 * This is a wrapper for sockaddr data, aware of address family.
 */
class SOSIMPLE_API Endpoint {
public:
    /// Create an empty
    inline Endpoint()
    : mKind(Kind::IPv4), mAddr({0}), mPort(0) {}
    /// Read an endpoint from a posix structure
    Endpoint(const sockaddr* const addr, socklen_t size);
    /// Parse an endpoint from an address string and port number
    /// @param ipaddr can be "any", "lo", "localhost", "loopback", IPv4 or IPv6 string rep, interface name
    Endpoint(const std::string& ipaddr, uint16_t port);
    virtual
    ~Endpoint() = default;
public:
    enum class SOSIMPLE_API Kind {
        IPv4,
        IPv6,
    };
private:
    uint8_t mAddr[16];
    uint16_t mPort;
    Kind mKind;

public:
    // copy & move
    Endpoint(const Endpoint&);
    Endpoint(Endpoint&&);
    Endpoint& operator=(const Endpoint&);
    Endpoint& operator=(Endpoint&&);

    // sort & compare
    auto
    operator==(const Endpoint&) const -> bool;

    auto
    operator<(const Endpoint&) const -> bool;

    // other members
    inline auto
    isIPv4() const -> bool
    { return mKind == Kind::IPv4; }

    inline auto
    isIPv6() const -> bool
    { return mKind == Kind::IPv6; }

    inline auto
    getPort() const -> uint16_t
    { return mPort; }

    /// fill a posix structure from this instance
    inline auto
    getInAddr(struct in_addr& addr) const -> void
    { if (mKind != Kind::IPv4) throw std::runtime_error("Can not fill struct in_addr from non IPv4 endpoint"); memcpy(&addr, this->mAddr, 4); }

    /// fill a posix structure from this instance
    inline auto
    getIn6Addr(struct in6_addr& addr) const -> void
    { if (mKind != Kind::IPv6) throw std::runtime_error("Can not fill struct in_addr6 from non IPv6 endpoint"); memcpy(&addr, this->mAddr, 16); }

    /// return true if address or port is undefined (ANY addr = 0.0.0.0, ANY port = 0)
    /// this is usually the case for late binding (e.g. tcp clients) or if this endpoint was not configured
    auto
    isAny() const -> bool;

    /// return true if the address is in the multicast range
    auto
    isMulticast() const -> bool;

    auto
    toSockaddrStorage(sockaddr_storage& storage) const -> void;
};

}

auto SOSIMPLE_API
operator<<(std::ostream&, const sosimple::Endpoint&) -> std::ostream&;

template<>
struct SOSIMPLE_API std::hash<sosimple::Endpoint> {
    std::size_t operator()(const sosimple::Endpoint&) const noexcept;
};

#endif
