#include <sosimple/socket.hpp>
#include <sosimple/worker.hpp>
#include <sosimple/platforms.hpp>

#include "socket_impl.hpp"
#include "platforms_internal.hpp"

#define SC_DEFAULT_BUFFER_SIZE 4096

/// mark socket as connected. a connected socket does not accept a remote argument when sending
#define SC_SOCKFLAG_CONNECTED 1
/// flag a socket as closed: whether is has been opened or not, this socket is dead
#define SC_SOCKFLAG_CLOSED 2

/// marking the socket closed, closing the file descriptor and notifying gets a bit repetitive...
#define SOSIMPLE_SOCKET_ERROR(ERROR_ENUM, MESSAGE) {\
    mFlags |= SC_SOCKFLAG_CLOSED; \
    POSIX_CLOSE(mFD); \
    notifySocketError(socket_error((ERROR_ENUM), (MESSAGE))); \
}

static auto
errno2str(int error, const char* custom_desc=nullptr) -> std::string {
    const char* name = GNU_STRERRORNAME_NP(error);
    const char* desc;
    if (custom_desc) desc = custom_desc;
    else desc = GNU_STRERRORDESC_NP(error);
    if (name == nullptr) name = "<NULLPTR>";
    if (desc == nullptr) desc = "<NULLPTR>";
    return std::format("{} ({}): {}", name, error, desc);
}

static auto
getBoundAddress(socket_t fd) -> sosimple::Endpoint
{
    sockaddr_storage myaddr{};
    socklen_t addrlen = sizeof(sockaddr_in6);;
    int success = ::getsockname(fd, (sockaddr*)&myaddr, &addrlen);
    int error = POSIX_ERRNO;
    if (success == -1)
        throw new sosimple::socket_error(sosimple::SocketError::Generic, "Could not read local address for socket: "+errno2str(error));
    sosimple::Endpoint boundEndpoint = sosimple::Endpoint((sockaddr*)&myaddr, addrlen);
    return boundEndpoint;
}

static auto
unblockSocket(int fd) -> bool
{
#if defined _WIN32
    unsigned long mode = 1;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    return (flags != -1) && (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
#endif
}

static auto
setDefaultSockOpts(int fd) -> void
{
    int yes = 1;
    if (POSIX_SETSOCKOPT(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))==-1)
        throw sosimple::socket_error(sosimple::SocketError::Configuration, "Unable to set socket option SO_REUSEADDR: " + errno2str(POSIX_ERRNO));
#if defined SO_REUSEPORT
    if (POSIX_SETSOCKOPT(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes))==-1)
        throw sosimple::socket_error(sosimple::SocketError::Configuration, "Unable to set socket option SO_REUSEPORT: " + errno2str(POSIX_ERRNO));
#endif
    if (POSIX_SETSOCKOPT(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes))==-1)
        throw sosimple::socket_error(sosimple::SocketError::Configuration, "Unable to set socket option SO_KEEPALIVE: " + errno2str(POSIX_ERRNO));
    int bufferSz = SC_DEFAULT_BUFFER_SIZE;
    if (POSIX_SETSOCKOPT(fd, SOL_SOCKET, SO_SNDBUF, &bufferSz, sizeof(bufferSz))==-1)
        throw sosimple::socket_error(sosimple::SocketError::Configuration, "Unable to set socket option SO_SNDBUF: " + errno2str(POSIX_ERRNO));
    if (POSIX_SETSOCKOPT(fd, SOL_SOCKET, SO_RCVBUF, &bufferSz, sizeof(bufferSz))==-1)
        throw sosimple::socket_error(sosimple::SocketError::Configuration, "Unable to set socket option SO_RCVBUF: " + errno2str(POSIX_ERRNO));
}

auto
sosimple::createUDPUnicast(Endpoint bindAddr) -> std::shared_ptr<ComSocket>
{
    SOSIMPLE_SOCKET_INIT;

    // create the socket, non-blocking
    int domain = bindAddr.isIPv4() ? AF_INET : AF_INET6;
    int type = SOCK_DGRAM;
    int protocol = IPPROTO_UDP;
    socket_t fd = POSIX_SOCKET( domain, type, protocol );
    int error = POSIX_ERRNO;
    if (!POSIX_ISVALIDDESCRIPTOR(fd)) {
        if (error == SOCKET_ERRNO_EACCES) throw socket_error(SocketError::Permission, "Unable to create socket: " + errno2str(error));
        else if (error == SOCKET_ERRNO_EMFILE || error == SOCKET_ERRNO_ENFILE) throw socket_error(SocketError::HandleLimit, "Unable to create socket: " + errno2str(error));
        else throw socket_error(SocketError::Generic, "Unable to create socket: " + errno2str(error));
    }
    if (!unblockSocket(fd)) {
        POSIX_CLOSE(fd);
        throw socket_error(SocketError::Configuration, "Could not switch socket to unblocking");
    }
    //making the shared pointer here so we can't forget to close() the fd
    auto socket = std::make_shared<ComSocketImpl>(fd, Socket::Kind::UDP_Unicast);

    // bind the socket
    sockaddr_storage addr{};
    bindAddr.toSockaddrStorage(addr);
    int success = POSIX_BIND( fd, (sockaddr*)&addr, sizeof(addr) );
    error = POSIX_ERRNO;
    if (success == -1) {
        if (error == SOCKET_ERRNO_EACCES) throw socket_error(SocketError::Permission, "Unable to bind socket: " + errno2str(error));
        else if (error == SOCKET_ERRNO_EADDRINUSE) {
            if (bindAddr.getPort() == 0) throw socket_error(SocketError::HandleLimit, "Unable to bind socket: " + errno2str(error, "No free ephemeral ports"));
            else throw socket_error(SocketError::Bind, "Unable to bind socket: " + errno2str(error, "Address in use"));
        } else throw socket_error(SocketError::Generic, "Unable to bind socket: " + errno2str(error));
    }
    if (bindAddr.isAny())
        socket->mLocal = getBoundAddress(fd);
    else
        socket->mLocal = bindAddr;

    // set basic socket options
    setDefaultSockOpts(fd);

    socket->start();
    return socket;
}

auto
sosimple::createUDPMulticast(Endpoint iface, Endpoint multicastGroup) -> std::shared_ptr<ComSocket>
{
    SOSIMPLE_SOCKET_INIT;

    if (iface.isIPv4() != multicastGroup.isIPv4())
        throw socket_error(SocketError::Configuration, "Unable to create socket: interface and multicast group were not specified with the same address family (IPv4/IPv6)");

    // create the socket
    int domain = multicastGroup.isIPv4() ? AF_INET : AF_INET6;
    int type = SOCK_DGRAM;
    int protocol = IPPROTO_UDP;
    socket_t fd = POSIX_SOCKET( domain, type, protocol );
    int error = POSIX_ERRNO;
    if (!POSIX_ISVALIDDESCRIPTOR(fd)) {
        if (error == SOCKET_ERRNO_EACCES) throw socket_error(SocketError::Permission, "Unable to create socket: "+errno2str(error));
        else if (error == SOCKET_ERRNO_EMFILE || error == SOCKET_ERRNO_ENFILE) throw socket_error(SocketError::HandleLimit, "Unable to create socket: "+errno2str(error));
        else throw socket_error(SocketError::Generic, "Unable to create socket: " + errno2str(error));
    }
    if (!unblockSocket(fd)) {
        POSIX_CLOSE(fd);
        throw socket_error(SocketError::Configuration, "Could not switch socket to unblocking");
    }
    //making the shared pointer here so we can't forget to close() the fd
    auto socket = std::make_shared<ComSocketImpl>(fd, Socket::Kind::UDP_Multicast);
    socket->mRemote = multicastGroup;

    // bind the socket
    sockaddr_storage addr{};
    addr.ss_family = domain; // bind to "any" == unfiltered
    int success = POSIX_BIND( fd, (sockaddr*)&addr, sizeof(addr) );
    error = POSIX_ERRNO;
    if (success == -1) {
        if (error == SOCKET_ERRNO_EACCES) throw socket_error(SocketError::Permission, "Unable to bind socket: "+errno2str(error));
        else if (error == SOCKET_ERRNO_EADDRINUSE) throw socket_error(SocketError::Bind, "Unable to bind socket: "+errno2str(error));
        else throw socket_error(SocketError::Generic, "Unable to bind socket: " + errno2str(error));
    }
    if (iface.isAny())
        socket->mLocal = getBoundAddress(fd);
    else
        socket->mLocal = iface;

    // set basic socket options
    setDefaultSockOpts(fd);

    // add membership
    int yes=1;
    if (domain == AF_INET) {
        struct ip_mreq mreq{};
        multicastGroup.getInAddr(mreq.imr_multiaddr);
        iface.getInAddr(mreq.imr_interface);
        if (POSIX_SETSOCKOPT(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))==-1)
            throw socket_error(SocketError::Configuration, "Unable to join multicast group: " + errno2str(POSIX_ERRNO));
        if (POSIX_SETSOCKOPT(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &yes, sizeof(yes))==-1)
            throw socket_error(SocketError::Configuration, "Unable to set socket option SO_KEEPALIVE: " + errno2str(POSIX_ERRNO));
    } else {
        struct ipv6_mreq mreq{};
        multicastGroup.getIn6Addr(mreq.ipv6mr_multiaddr);
        if (POSIX_SETSOCKOPT(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq, sizeof(mreq))==-1)
            throw socket_error(SocketError::Configuration, "Unable to join multicast group: " + errno2str(POSIX_ERRNO));
        if (POSIX_SETSOCKOPT(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &yes, sizeof(yes))==-1)
            throw socket_error(SocketError::Configuration, "Unable to set socket option SO_KEEPALIVE: " + errno2str(POSIX_ERRNO));
    }

    socket->start();
    return socket;
}

auto
sosimple::createTCPListen(Endpoint bindAddr) -> std::shared_ptr<ListenSocket>
{
    SOSIMPLE_SOCKET_INIT;

    if (bindAddr.isAny())
        throw socket_error(SocketError::Configuration, "Can not bind listen socket to unspecified endpoint");

    // create the socket
    int domain = bindAddr.isIPv4() ? AF_INET : AF_INET6;
    int type = SOCK_STREAM;
    int protocol = IPPROTO_TCP;
    socket_t fd = POSIX_SOCKET( domain, type, protocol );
    int error = POSIX_ERRNO;
    if (!POSIX_ISVALIDDESCRIPTOR(fd)) {
        if (error == SOCKET_ERRNO_EACCES) throw socket_error(SocketError::Permission, "Unable to create socket: "+errno2str(error));
        else if (error == SOCKET_ERRNO_EMFILE || error == SOCKET_ERRNO_ENFILE) throw socket_error(SocketError::HandleLimit, "Unable to create socket: "+errno2str(error));
        else throw socket_error(SocketError::Generic, "Unable to create socket: " + errno2str(error));
    }
    if (!unblockSocket(fd)) {
        POSIX_CLOSE(fd);
        throw socket_error(SocketError::Configuration, "Could not switch socket to unblocking");
    }
    //making the shared pointer here so we can't forget to close() the fd
    auto socket = std::make_shared<ListenSocketImpl>(fd);

    // bind the socket
    sockaddr_storage addr{};
    bindAddr.toSockaddrStorage(addr);
    int success = POSIX_BIND( fd, (sockaddr*)&addr, sizeof(addr) );
    error = POSIX_ERRNO;
    if (success == -1) {
        if (error == SOCKET_ERRNO_EACCES) throw socket_error(SocketError::Permission, "Unable to bind socket: " + errno2str(error));
        else if (error == SOCKET_ERRNO_EADDRINUSE) {
            if (bindAddr.getPort() == 0) throw socket_error(SocketError::HandleLimit, "Unable to bind socket: " + errno2str(error, "No free ephemeral ports"));
            else throw socket_error(SocketError::Bind, "Unable to bind socket: " + errno2str(error, "Address in use"));
        } else throw socket_error(SocketError::Generic, "Unable to bind socket: " + errno2str(error));
    }
    socket->mLocal = bindAddr;

    // set basic socket options
    setDefaultSockOpts(fd);

    // prepare the listen queue size
    if (POSIX_LISTEN(fd, 1)==-1) //we should queue them away quick enough anyways, but whatever
        throw socket_error(SocketError::Configuration, "Unable to set listen queue size: " + errno2str(POSIX_ERRNO));

    socket->start();
    return socket;
}

auto
sosimple::createTCPClient(Endpoint bindAddr, Endpoint remote) -> std::shared_ptr<ComSocket>
{
    SOSIMPLE_SOCKET_INIT;

    if (bindAddr.isIPv4() != remote.isIPv4())
        throw socket_error(SocketError::Configuration, "Unable to create socket: local and remote endpoints were not specified with the same address family (IPv4/IPv6)");

    // create the socket
    int domain = bindAddr.isIPv4() ? AF_INET : AF_INET6;
    int type = SOCK_STREAM;
    int protocol = IPPROTO_TCP;
    socket_t fd = POSIX_SOCKET( domain, type, protocol );
    int error = POSIX_ERRNO;
    if (!POSIX_ISVALIDDESCRIPTOR(fd)) {
        if (error == SOCKET_ERRNO_EACCES) throw socket_error(SocketError::Permission, "Unable to create socket: "+errno2str(error));
        else if (error == SOCKET_ERRNO_EMFILE || error == SOCKET_ERRNO_ENFILE) throw socket_error(SocketError::HandleLimit, "Unable to create socket: "+errno2str(error));
        else throw socket_error(SocketError::Generic, "Unable to create socket: " + errno2str(error));
    }
    if (!unblockSocket(fd)) {
        POSIX_CLOSE(fd);
        throw socket_error(SocketError::Configuration, "Could not switch socket to unblocking");
    }
    //making the shared pointer here so we can't forget to close() the fd
    auto socket = std::make_shared<ComSocketImpl>(fd, Socket::Kind::TCP_Client);
    socket->mFlags |= SC_SOCKFLAG_CONNECTED;
    socket->mRemote = remote;

    // bind the socket
    sockaddr_storage addr{};
    bindAddr.toSockaddrStorage(addr);
    int success = POSIX_BIND( fd, (sockaddr*)&addr, sizeof(addr) );
    error = POSIX_ERRNO;
    if (success == -1) {
        if (error == SOCKET_ERRNO_EACCES) throw socket_error(SocketError::Permission, "Unable to bind socket: " + errno2str(error));
        else if (error == SOCKET_ERRNO_EADDRINUSE) {
            if (bindAddr.getPort() == 0) throw socket_error(SocketError::HandleLimit, "Unable to bind socket: " + errno2str(error, "No free ephemeral ports"));
            else throw socket_error(SocketError::Bind, "Unable to bind socket: " + errno2str(error, "Address already in use"));
        } else throw socket_error(SocketError::Generic, "Unable to bind socket: " + errno2str(error));
    }
    if (bindAddr.isAny())
        socket->mLocal = getBoundAddress(fd);
    else
        socket->mLocal = bindAddr;

    // set basic socket options
    setDefaultSockOpts(fd);

    // connect to remote
    // We would like to get all connection related errors in the socket error callback but that would requrie a single use member holding the error.
    // i dont like that, so we just throw it with the rest, socket_error should be expected already anyways
    ::memset(&addr, 0, sizeof(addr));
    remote.toSockaddrStorage(addr);
    if (POSIX_CONNECT(fd, (sockaddr*)&addr, sizeof(addr))==-1) {
        int error = POSIX_ERRNO;
        if (error == SOCKET_ERRNO_EAGAIN || error == SOCKET_ERRNO_EINPROGRESS || error == SOCKET_ERRNO_EWOULDBLOCK)
            { /*ignore*/ }
        else if (error == SOCKET_ERRNO_EINTR || error == SOCKET_ERRNO_ENETUNREACH)
            throw socket_error(SocketError::BrokenPipe, "Unable to connect socket: " + errno2str(error));
        else
            throw socket_error(SocketError::Generic, "Unable to connect socket: " + errno2str(error));
    }

    socket->start();
    return socket;
}

auto
sosimple::createTCPServer(socket_t acceptedSocket, Endpoint remote) -> std::shared_ptr<ComSocket>
{
    // these should be set up for the most part, we'll just wrap them
    auto socket = std::make_shared<ComSocketImpl>(acceptedSocket, Socket::Kind::TCP_Server);
    socket->mLocal = getBoundAddress(acceptedSocket);
    socket->mFlags |= SC_SOCKFLAG_CONNECTED;
    socket->mRemote = remote;

    // set basic socket options
    setDefaultSockOpts(acceptedSocket);

    socket->start();
    return socket;
}

auto
sosimple::SocketBase::onSocketError(SocketErrorCallback callback) -> void
{
    mSocketErrorEvent = callback;
}

auto
sosimple::SocketBase::notifySocketError(socket_error error) const -> void
{
    if (Worker::isStarted())
        sosimple::Worker::queue([wself=weak_from_this(),error=error](){
            auto self = wself.lock();
            if (self) self->mSocketErrorEvent(error);
            return false;
        });
    else
        mSocketErrorEvent(error);
}

auto
sosimple::SocketBase::isOpen() const -> bool
{
    if (!POSIX_ISVALIDDESCRIPTOR(mFD) || (mFlags & SC_SOCKFLAG_CLOSED)) return false;

    return pollFD(POLLOUT, 10'000, true) && (mFlags & SC_SOCKFLAG_CLOSED)==0;
}

auto
sosimple::SocketBase::setTimeout(std::chrono::milliseconds timeout) -> void
{
    mWatchDog.setTimeout(timeout);
    if (timeout == std::chrono::microseconds::zero()) {
        mWatchDog.setCallback({}); //ignore timeouts
    } else {
        mWatchDog.setCallback([wself=weak_from_this()](){
            auto self = wself.lock();
            if (self) {
                self->mFlags |= 2;
                POSIX_CLOSE(self->mFD);
                self->notifySocketError(socket_error((SocketError::Timeout), ("Socket watchdog tripped: Timeout")));
            }
        });
    }
}

auto
sosimple::SocketBase::getNativeSocket() const -> socket_t
{
    return mFD;
}

auto
sosimple::SocketBase::getLocalEndpoint() const -> Endpoint
{
    if (mLocal.isAny()) mLocal = getBoundAddress(mFD);
    return mLocal;
}

auto
sosimple::SocketBase::pollFD(int pollval, unsigned timeoutMS, bool notifyOnTimeout) const -> bool
{
    constexpr int pollerror = (POLLERR|POLLHUP|POLLNVAL);
    pollfd pollreq{};
    pollreq.fd = mFD;
    pollreq.events = pollval;
    int result = POSIX_POLL(&pollreq, 1, timeoutMS); // 10 second timeout
    int error = POSIX_ERRNO;
    if (result > 0) {
        if ((pollreq.revents & pollerror) != 0) {
            auto emsg = std::format("Poll responded with {:04X}", pollreq.revents);
            SOSIMPLE_SOCKET_ERROR(SocketError::BrokenPipe, "Unable to poll connection: "+errno2str(error, emsg.c_str()))
        }
        else return true;
    } else if (result == 0) {
        if (notifyOnTimeout) SOSIMPLE_SOCKET_ERROR(SocketError::Timeout, "Unable to poll connection: "+errno2str(error))
    }
    else if (error == SOCKET_ERRNO_ENOMEM) SOSIMPLE_SOCKET_ERROR(SocketError::NoMemory, "Unable to poll connection: "+errno2str(error))
    else SOSIMPLE_SOCKET_ERROR(SocketError::Generic, "Unable to poll connection: " + errno2str(error))
    return false;
}

sosimple::SocketBase::~SocketBase()
{
    if ((mFlags & SC_SOCKFLAG_CLOSED)==0) POSIX_CLOSE(mFD);
}

sosimple::ListenSocketImpl::~ListenSocketImpl()
{
    mRunning.store(false);
    if (mWorker.joinable())
        mWorker.join();
}

auto
sosimple::ListenSocketImpl::start() -> void
{
    mWorker = std::thread([wself=weak_from_this()](){
        auto self = std::static_pointer_cast<ListenSocketImpl>(wself.lock());
        if (self) self->threadMain();
    });
}

auto
sosimple::ListenSocketImpl::threadMain() -> void
{
    while (mRunning && (mFlags & SC_SOCKFLAG_CLOSED)==0) {
        sockaddr_storage addr{};
        socklen_t addrSz = sizeof(addr);
        socket_t fd = POSIX_ACCEPT(mFD, (sockaddr*)&addr, &addrSz);
        int error = POSIX_ERRNO;

        if (!POSIX_ISVALIDDESCRIPTOR(fd)) {
            if (error == SOCKET_ERRNO_EAGAIN || error == SOCKET_ERRNO_EWOULDBLOCK) {
                // nothin polled within a ms and the watchdog trips? -> timeout!
                if (!pollFD(POLLIN, 1, false) && !mWatchDog.check()) mRunning = false;
            } else if (error == SOCKET_ERRNO_ECONNABORTED) {
                continue; // "a connection?"
            } else if (error == SOCKET_ERRNO_ENOBUFS || error == SOCKET_ERRNO_ENOMEM) {
                mRunning = false;
                SOSIMPLE_SOCKET_ERROR(SocketError::NoMemory, "Could not accept connection: "+errno2str(error))
            } else if (error == SOCKET_ERRNO_EMFILE || error == SOCKET_ERRNO_ENFILE) {
                mRunning = false;
                SOSIMPLE_SOCKET_ERROR(SocketError::HandleLimit, "Could not accept connection: "+errno2str(error))
            } else if (error == SOCKET_ERRNO_EINTR) {
                mRunning = false;
                SOSIMPLE_SOCKET_ERROR(SocketError::BrokenPipe, "Could not accept connection: "+errno2str(error))
            } else {
                mRunning = false;
                SOSIMPLE_SOCKET_ERROR(SocketError::Generic, "Could not accept connection: "+errno2str(error))
            }
        } else if (!unblockSocket(fd)) {
            mRunning = false;
            POSIX_CLOSE(fd);
            SOSIMPLE_SOCKET_ERROR(SocketError::Generic, "Could not unblock accepted socket");
        } else {
            mWatchDog.reset();
            notifyAccept(fd, Endpoint{(sockaddr*)&addr, addrSz});
        }
    }
}

auto
sosimple::ListenSocketImpl::notifyAccept(socket_t acceptedSocket, Endpoint remote) -> void
{
    auto socket = createTCPServer(acceptedSocket, remote);
    if (Worker::isStarted()) {
        sosimple::Worker::queue([wself=weak_from_this(),socket=socket,remote=remote](){
            auto self = std::dynamic_pointer_cast<ListenSocketImpl>(wself.lock());
            if (self) self->mAcceptEvent(socket, remote);
            return false;
        });
    } else
        mAcceptEvent(socket, remote);
}

auto
sosimple::ListenSocketImpl::onAccept(AcceptCallback callback) -> void
{
    mAcceptEvent = callback;
}

sosimple::ComSocketImpl::~ComSocketImpl()
{
    mRunning.store(false);
    if (mWorker.joinable())
        mWorker.join();
}

auto
sosimple::ComSocketImpl::start() -> void
{
    mWorker = std::thread([wself=weak_from_this()](){
        auto self = std::static_pointer_cast<ComSocketImpl>(wself.lock());
        if (self) self->threadMain();
    });
}

auto
sosimple::ComSocketImpl::threadMain() -> void
{
    uint8_t chunk[4096];
    const bool connected = (mFlags & SC_SOCKFLAG_CONNECTED) != 0;
    pollFD(POLLOUT, 2000, false); // wait for the connection (avoid early ENOTCONN)
    while (mRunning && (mFlags & SC_SOCKFLAG_CLOSED)==0) {
        int read;
        Endpoint from;
        sockaddr_storage addr{};
        socklen_t addrSz = sizeof(addr);
        if (connected) {
            read = POSIX_RECV(mFD, chunk, sizeof(chunk), 0);
        } else {
            read = POSIX_RECVFROM(mFD, chunk, sizeof(chunk), 0, (sockaddr*)&addr, &addrSz);
        }
        int error = POSIX_ERRNO;
        if (read == -1) {
            if (error == SOCKET_ERRNO_EAGAIN || error == SOCKET_ERRNO_EWOULDBLOCK) {
                // nothin polled within a ms and the watchdog trips? -> timeout!
                if (!pollFD(POLLIN, 1, false) && !mWatchDog.check()) mRunning = false;
            } else if (error == SOCKET_ERRNO_ENOMEM) {
                mRunning = false;
                SOSIMPLE_SOCKET_ERROR(SocketError::NoMemory, "Could not read socket: "+errno2str(error))
            } else if (error == SOCKET_ERRNO_EINTR || error == SOCKET_ERRNO_ECONNREFUSED || error == SOCKET_ERRNO_ENOTCONN) {
                mRunning = false;
                SOSIMPLE_SOCKET_ERROR(SocketError::BrokenPipe, "Could not read socket: "+errno2str(error))
            } else {
                mRunning = false;
                SOSIMPLE_SOCKET_ERROR(SocketError::Generic, "Could not read socket: "+errno2str(error))
            }
        } else if (read>0) {
            if (connected)
                from = mRemote;
            else
                from = Endpoint{(sockaddr*)&addr, addrSz};
            mWatchDog.reset();
            notifyPacket(std::vector<uint8_t>(chunk+0, chunk+read), from);
        }
    }
}

auto
sosimple::ComSocketImpl::getRemoteEndpoint() const -> Endpoint
{
    return mRemote;
}

auto
sosimple::ComSocketImpl::notifyPacket(const std::vector<uint8_t>& payload, Endpoint remote) -> void
{
    if (Worker::isStarted()) {
        sosimple::Worker::queue([wself=weak_from_this(), payload=std::vector<uint8_t>(payload), remote=remote](){
            auto self = std::dynamic_pointer_cast<ComSocketImpl>(wself.lock());
            if (self) self->mPacketReceivedEvent(payload, remote);
            return false;
        });
    } else
        mPacketReceivedEvent(payload, remote);
}

auto
sosimple::ComSocketImpl::onPacket(PacketReceivedCallback callback) -> void
{
    mPacketReceivedEvent = callback;
}

auto
sosimple::ComSocketImpl::send(const std::vector<uint8_t>& payload, Endpoint remote) const -> void
{
    if ((mFlags & SC_SOCKFLAG_CLOSED)!=0) {
        notifySocketError(socket_error(SocketError::BrokenPipe, "Can not send message: Socket was closed"));
        return;
    }

    int result; int error;
    if (mKind == Kind::UDP_Multicast) {
        sockaddr_storage addr{};
        mRemote.toSockaddrStorage(addr);
        result = POSIX_SENDTO(mFD, payload.data(), payload.size(), 0, (sockaddr*)&addr, sizeof(addr));
        error = POSIX_ERRNO;
    } else if (mKind == Kind::UDP_Unicast) {
        sockaddr_storage addr{};
        remote.toSockaddrStorage(addr);
        result = POSIX_SENDTO(mFD, payload.data(), payload.size(), 0, (sockaddr*)&addr, sizeof(addr));
        error = POSIX_ERRNO;
    } else {
        result = POSIX_SEND(mFD, payload.data(), payload.size(), 0);
        error = POSIX_ERRNO;
    }
    if (result == -1) {
        if (error == SOCKET_ERRNO_EAGAIN || error == SOCKET_ERRNO_EWOULDBLOCK) {
            /* pass */
        } else if (error == SOCKET_ERRNO_EACCES) {
            SOSIMPLE_SOCKET_ERROR(SocketError::Permission, "Could not send on socket: "+errno2str(error))
        } else if (error == SOCKET_ERRNO_ENOBUFS || error == SOCKET_ERRNO_ENOMEM || error == SOCKET_ERRNO_EMSGSIZE) {
            SOSIMPLE_SOCKET_ERROR(SocketError::NoMemory, "Could not send on socket: "+errno2str(error))
        } else if (error == SOCKET_ERRNO_ECONNRESET || error == SOCKET_ERRNO_ENOTCONN || error == SOCKET_ERRNO_EPIPE) {
            SOSIMPLE_SOCKET_ERROR(SocketError::BrokenPipe, "Could not send on socket: "+errno2str(error))
        } else if (error == SOCKET_ERRNO_EDESTADDRREQ || error == SOCKET_ERRNO_EISCONN || error == SOCKET_ERRNO_EPIPE) {
            SOSIMPLE_SOCKET_ERROR(SocketError::Configuration, "Could not send on socket: "+errno2str(error))
        } else {
            SOSIMPLE_SOCKET_ERROR(SocketError::Generic, "Could not send on socket: "+errno2str(error))
        }
    }
}