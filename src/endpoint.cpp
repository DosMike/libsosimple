#include <sosimple.hpp>
#include <map>
#include <string>
#include <endian.h>

sosimple::Endpoint::Endpoint(const std::string& ipaddr, uint16_t port)
{
    std::string validated = utility::ip::fromString(ipaddr);
    this->mPort = port;
    if (ipaddr.find(':') != std::string::npos) {
        this->mKind = Kind::IPv6;
        if (inet_pton(AF_INET6, validated.c_str(), this->mAddr) != 1)
            throw std::runtime_error("Invalid address: "+validated);
    } else {
        this->mKind = Kind::IPv4;
        if (inet_pton(AF_INET, validated.c_str(), this->mAddr) != 1)
            throw std::runtime_error("Invalid address: "+validated);
    }
}

sosimple::Endpoint::Endpoint(const sockaddr* const addr, socklen_t size)
{
    if (addr->sa_family == AF_INET && size == sizeof(sockaddr_in)) {
        this->mKind = Kind::IPv4;
        const sockaddr_in* const ipv4 = reinterpret_cast<const sockaddr_in* const>(addr);
        std::memcpy(this->mAddr, &(ipv4->sin_addr.s_addr), sizeof(ipv4->sin_addr.s_addr));
        this->mPort = ::be16toh(ipv4->sin_port);
    } else if (addr->sa_family == AF_INET6 && size == sizeof(sockaddr_in6)) {
        this->mKind = Kind::IPv6;
        const sockaddr_in6* const ipv6 = reinterpret_cast<const sockaddr_in6* const>(addr);
        std::memcpy(this->mAddr, &(ipv6->sin6_addr.s6_addr), sizeof(ipv6->sin6_addr.s6_addr));
        this->mPort = ::be16toh(ipv6->sin6_port);
    } else
        throw std::runtime_error("Invalid address family or mismatching data size");
}

sosimple::Endpoint::Endpoint(const sosimple::Endpoint&) = default;
sosimple::Endpoint::Endpoint(sosimple::Endpoint&&) = default;
sosimple::Endpoint& sosimple::Endpoint::operator=(const sosimple::Endpoint&) = default;
sosimple::Endpoint& sosimple::Endpoint::operator=(sosimple::Endpoint&&) = default;

auto sosimple::Endpoint::operator==(const sosimple::Endpoint& ep) const -> bool
{
    if (this->mKind != ep.mKind) return false;
    int max = ep.mKind == sosimple::Endpoint::Kind::IPv4 ? 4 : 16;
    for (int i{0}; i<max; i++) {
        if (this->mAddr[i] != ep.mAddr[i]) return false;
    }
    if (this->mPort != ep.mPort) return false;
    return true;
}

auto sosimple::Endpoint::operator<(const sosimple::Endpoint& ep) const -> bool
{
    if (this->mKind != ep.mKind) return this->mKind == sosimple::Endpoint::Kind::IPv4;
    int max = ep.mKind == sosimple::Endpoint::Kind::IPv4 ? 4 : 16;
    for (int i{0}; i<max; i++) {
        if (this->mAddr[i] != ep.mAddr[i]) return this->mAddr[i] < ep.mAddr[i];
    }
    if (this->mPort != ep.mPort) return this->mPort < ep.mPort;
    return false;
}

auto sosimple::Endpoint::isAny() const -> bool
{
    if (mPort == 0) return true;
    if (mKind == sosimple::Endpoint::Kind::IPv4) {
        return *reinterpret_cast<const uint32_t*>(mAddr) == 0;
    } else {
        return *reinterpret_cast<const uint64_t*>(mAddr) == 0 && *reinterpret_cast<const uint64_t*>(mAddr+8) == 0;
    }
}

auto sosimple::Endpoint::isMulticast() const -> bool
{
    if (mKind == sosimple::Endpoint::Kind::IPv4) {
        return mAddr[0] >= 224 && mAddr[0] <= 239;
    } else {
        return mAddr[0] == 0xff;
    }
}

auto sosimple::Endpoint::toSockaddrStorage(sockaddr_storage& storage) const -> void
{
    if (isIPv4()) {
        sockaddr_in* addr4 = (sockaddr_in*)&storage;
        addr4->sin_family = AF_INET;
        addr4->sin_port = ::htobe16(mPort);
        getInAddr(addr4->sin_addr);
    } else {
        sockaddr_in6* addr6 = (sockaddr_in6*)&storage;
        addr6->sin6_family = AF_INET;
        addr6->sin6_port = ::htobe16(mPort);
        getIn6Addr(addr6->sin6_addr);
    }
}

auto operator<<(std::ostream& os, const sosimple::Endpoint& ep) -> std::ostream&
{
    char buffer[INET6_ADDRSTRLEN];
    bool isv4 = ep.isIPv4();
    if (isv4) {
        struct in_addr addr;
        ep.getInAddr(addr);
        inet_ntop(AF_INET, &addr, buffer, sizeof(buffer));
        os << buffer << ":" << (int)ep.getPort();
    } else {
        struct in6_addr addr;
        ep.getIn6Addr(addr);
        inet_ntop(AF_INET6, &addr, buffer, sizeof(buffer));
        os << "[" << buffer << "]:" << (int)ep.getPort();
    }
    return os;
}

auto std::hash<sosimple::Endpoint>::operator()(const sosimple::Endpoint& ep) const noexcept -> size_t
{
    size_t hash{7};
    if (ep.isIPv4()) {
        struct in_addr addr;
        ep.getInAddr(addr);
        hash = 31 * hash + *reinterpret_cast<uint32_t*>(&addr);
    } else {
        struct in6_addr addr;
        ep.getIn6Addr(addr);
        hash = 31 * hash + *reinterpret_cast<uint64_t*>(addr.s6_addr);
        hash = 31 * hash + *reinterpret_cast<uint64_t*>(addr.s6_addr+8);
    }
    hash = 31 * hash + ep.getPort();
    return hash;
}

