#include <sosimple.hpp>
#include "platforms_internal.hpp"

auto sosimple::utility::ip::listInterfaces() -> std::set<std::string>
{
    std::set<std::string> result;

#if defined __linux__

    struct ifaddrs *addrs;
    getifaddrs(&addrs);

    for (struct ifaddrs *addr = addrs; addr != nullptr; addr = addr->ifa_next) {
        if (addr->ifa_addr && addr->ifa_addr->sa_family == AF_INET || addr->ifa_addr->sa_family == AF_INET6) {
            result.emplace(std::string(addr->ifa_name));
        }
    }

    freeifaddrs(addrs);

#elif defined _WIN32

    SOSIMPLE_SOCKET_INIT;

#else

    #error Unsupported platform

#endif
    return result;
}

auto sosimple::utility::ip::fromString(const std::string& name) -> std::string
{
    if (name == "lo" || name == "localhost" || name == "loopback") {
        return "127.0.0.1";
    } else if (name == "any") {
        return "0.0.0.0";
    }

    char guess = '\0'; //keep track if we estimated this to be an ipv4 or ipv6
    for (char c : name) {
        if (c == '.') {
            if (guess == '\0') guess = '4';
            // can contain dots after colon
        } else if (c == ':') {
            if (guess == '\0') guess = '6';
            else if (guess != '6') { guess = '\0'; break; } // not a valid ip string, try interface name
        } else if (std::isdigit(static_cast<uint8_t>(c))) {
            continue; //always valid, can't guess type from that alone
        } else if (std::isxdigit(static_cast<uint8_t>(c))) {
            if (guess == '\0') guess = '6';
            else if (guess != '6') { guess = '\0'; break; } // not a valid ip string, try interface name
        } else {
            guess = '\0'; break; // not a valid ip string, try interface name
        }
    }
    if (guess != '\0')
        return name; //this should now pass inet_pton()

    char host[NI_MAXHOST]={};
    bool resolved{false};

#if defined __linux__

    struct ifaddrs *addrs;
    getifaddrs(&addrs);

    for (struct ifaddrs *addr = addrs; addr != nullptr; addr = addr->ifa_next) {
        if (addr->ifa_addr && (addr->ifa_addr->sa_family == AF_INET || addr->ifa_addr->sa_family == AF_INET6) && strcasecmp(addr->ifa_name, name.c_str())==0) {
            bool ipv4 = (addr->ifa_addr->sa_family == AF_INET);
            int errorno = getnameinfo(addr->ifa_addr, ipv4 ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (errorno != 0) {
                std::string error = "Failed to resolve address: getnameinfo returned ";
                error += gai_strerror(errorno);
                throw std::runtime_error(error);
            }
            resolved = true;
            break;
        }
    }

    freeifaddrs(addrs);

#elif defined _WIN32

    SOSIMPLE_SOCKET_INIT;

#else

    #error Unsupported platform

#endif

    if (!resolved)
        throw std::runtime_error("Failed to resolve address: not an address and no interface with that name");
    return host;
}

