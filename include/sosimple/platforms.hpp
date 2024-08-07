#if !defined SOSIMPLE_NETHEADERS_HPP
#define SOSIMPLE_NETHEADERS_HPP

// common bunch of network, socket and other headers that i dont want to repeat over and over.
// this also aliases used posix apis to POSIX_* macros because windows is not 100% compatible at times, and the macros can hide the pain.
// feel free to use the macros in case you need to do some manual plumbing with socket file descriptors.

//std
#include <cstdint>
#include <string>
#include <stdexcept>
#include <memory>
#include <functional>

//posix
#if defined __linux__
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <ifaddrs.h>
    #include <netdb.h>
    #include <poll.h>
    #include <endian.h>
    #include <fcntl.h>

    /// Socket type defined for clarity
    typedef int socket_t;
    #define POSIX_INVALID_DESCRIPTOR -1
    #define POSIX_ISVALIDDESCRIPTOR(x) ((x)>=0)
    #define POSIX_ERRNO errno

    #define POSIX_SOCKET(domain, type, protocol) ::socket((domain), (type), (protocol))
    #define POSIX_BIND(fd, addr, addrlen) ::bind((fd), (addr), (addrlen))
    #define POSIX_LISTEN(fd, backlog) ::listen((fd), (backlog))
    #define POSIX_CONNECT(fd, addr, addrlen) ::connect((fd), (addr), (addrlen))
    #define POSIX_CLOSE(x) ::close(x)
    #define POSIX_POLL(fds, cnt, to) ::poll((fds), (cnt), (to))
    #define POSIX_ACCEPT(lfd, remote, remsz) ::accept((lfd), (remote), (remsz))
    #define POSIX_RECV(fd, buffer, bufsz, flags) ::recv((fd), reinterpret_cast<void*>(buffer), (bufsz), (flags))
    #define POSIX_RECVFROM(fd, buffer, bufsz, flags, remote, remsz) ::recvfrom((fd), reinterpret_cast<void*>(buffer), (bufsz), (flags), (remote), (remsz))
    #define POSIX_SEND(fd, buffer, bufsz, flags) ::send((fd), reinterpret_cast<void const*>(buffer), (bufsz), (flags))
    #define POSIX_SENDTO(fd, buffer, bufsz, flags, remote, remsz) ::sendto((fd), reinterpret_cast<void const*>(buffer), (bufsz), (flags), (remote), (remsz))
    #define POSIX_SETSOCKOPT(fd, level, option, vptr, vsize) ::setsockopt((fd), (level), (option), reinterpret_cast<void const*>(vptr), (vsize))
    #define POSIX_GETSOCKOPT(fd, level, option, vptr, vsize) ::getsockopt((fd), (level), (option), reinterpret_cast<void*>(vptr), (vsize))

    // remap relevant errnos, thanks Microsoft
    #define SOCKET_ERRNO_EACCES        EACCES
    #define SOCKET_ERRNO_EADDRINUSE    EADDRINUSE
    #define SOCKET_ERRNO_EAGAIN        EAGAIN
    #define SOCKET_ERRNO_ECONNABORTED  ECONNABORTED
    #define SOCKET_ERRNO_ECONNREFUSED  ECONNREFUSED
    #define SOCKET_ERRNO_ECONNRESET    ECONNRESET
    #define SOCKET_ERRNO_EDESTADDRREQ  EDESTADDRREQ
    #define SOCKET_ERRNO_EINPROGRESS   EINPROGRESS
    #define SOCKET_ERRNO_EINTR         EINTR
    #define SOCKET_ERRNO_EISCONN       EISCONN
    #define SOCKET_ERRNO_EMFILE        EMFILE
    #define SOCKET_ERRNO_EMSGSIZE      EMSGSIZE
    #define SOCKET_ERRNO_ENETUNREACH   ENETUNREACH
    #define SOCKET_ERRNO_ENFILE        ENFILE
    #define SOCKET_ERRNO_ENOBUFS       ENOBUFS
    #define SOCKET_ERRNO_ENOMEM        ENOMEM
    #define SOCKET_ERRNO_ENOTCONN      ENOTCONN
    #define SOCKET_ERRNO_EPIPE         EPIPE
    #define SOCKET_ERRNO_EWOULDBLOCK   EWOULDBLOCK

#elif defined _WIN32
    #include <Winsock2.h>
    #include <WS2tcpip.h>
    // vv-- more compat stuff i didn't want to squeeze in here or requires implementation
    #include "sosimple/windows_compat.hpp"

    /// Socket type defined for clarity
    typedef SOCKET socket_t;
    #define POSIX_INVALID_DESCRIPTOR INVALID_SOCKET
    #define POSIX_ISVALIDDESCRIPTOR(x) ((x)!=POSIX_INVALID_DESCRIPTOR)
    #define POSIX_ERRNO WSAGetLastError()
    
    #define POSIX_SOCKET(domain, type, protocol) ::socket((domain), (type), (protocol))
    #define POSIX_BIND(fd, addr, addrlen) ::bind((fd), (addr), (addrlen))
    #define POSIX_LISTEN(fd, backlog) ::listen((fd), (backlog))
    #define POSIX_CONNECT(fd, addr, addrlen) ::connect((fd), (addr), (addrlen))
    #define POSIX_CLOSE(x) ::closesocket(x)
    #define POSIX_POLL(fds, cnt, to) ::WSAPoll((fds), (cnt), (to))
    #define POSIX_ACCEPT(lfd, remote, remsz) ::accept((lfd), (remote), (remsz))
    #define POSIX_RECV(fd, buffer, bufsz, flags) ::recv((fd), reinterpret_cast<char*>(buffer), (bufsz), (flags))
    #define POSIX_RECVFROM(fd, buffer, bufsz, flags, remote, remsz) ::recvfrom((fd), reinterpret_cast<char*>(buffer), (bufsz), (flags), (remote), (remsz))
    #define POSIX_SEND(fd, buffer, bufsz, flags) ::send((fd), reinterpret_cast<char const*>(buffer), (bufsz), (flags))
    #define POSIX_SENDTO(fd, buffer, bufsz, flags, remote, remsz) ::sendto((fd), reinterpret_cast<char const*>(buffer), (bufsz), (flags), (remote), (remsz))
    #define POSIX_SETSOCKOPT(fd, level, option, vptr, vsize) ::setsockopt((fd), (level), (option), reinterpret_cast<char const*>(vptr), (vsize))
    #define POSIX_GETSOCKOPT(fd, level, option, vptr, vsize) ::getsockopt((fd), (level), (option), reinterpret_cast<char*>(vptr), (vsize))

    #define POSIX_E_EXPAND(x) (x)
    // thanks for commenting those out Microsoft...
    
    #define SOCKET_ERRNO_EACCES        WSAEACCES
    #define SOCKET_ERRNO_EADDRINUSE    WSAEADDRINUSE
    #define SOCKET_ERRNO_EAGAIN        WSATRY_AGAIN
    #define SOCKET_ERRNO_ECONNABORTED  WSAECONNABORTED
    #define SOCKET_ERRNO_ECONNREFUSED  WSAECONNREFUSED
    #define SOCKET_ERRNO_ECONNRESET    WSAECONNRESET
    #define SOCKET_ERRNO_EDESTADDRREQ  WSAEDESTADDRREQ
    #define SOCKET_ERRNO_EINPROGRESS   WSAEINPROGRESS
    #define SOCKET_ERRNO_EINTR         WSAEINTR
    #define SOCKET_ERRNO_EISCONN       WSAEISCONN
    #define SOCKET_ERRNO_EMFILE        WSAEMFILE
    #define SOCKET_ERRNO_EMSGSIZE      WSAEMSGSIZE
    #define SOCKET_ERRNO_ENETUNREACH   WSAENETUNREACH
    #define SOCKET_ERRNO_ENFILE        WSAETOOMANYREFS
    #define SOCKET_ERRNO_ENOBUFS       WSAENOBUFS
    #define SOCKET_ERRNO_ENOMEM        WSA_NOT_ENOUGH_MEMORY
    #define SOCKET_ERRNO_ENOTCONN      WSAENOTCONN
    #define SOCKET_ERRNO_EPIPE         -1 // no equivalent
    #define SOCKET_ERRNO_EWOULDBLOCK   WSAEWOULDBLOCK

#endif

#endif
