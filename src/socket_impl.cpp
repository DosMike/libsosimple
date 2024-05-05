#include <sosimple/socket.hpp>
#include <sosimple/worker.hpp>
#include <sosimple/netheaders.hpp>

#include "socket_impl.hpp"

#define SC_DEFAULT_BUFFER_SIZE 4096

/// mark socket as connected. a connected socket does not accept a remote argument when sending
#define SC_SOCKFLAG_CONNECTED 1
/// flag a socket as closed: whether is has been opened or not, this socket is dead
#define SC_SOCKFLAG_CLOSED 2

/// marking the socket closed, closing the file descriptor and notifying gets a bit repetitive...
#define SOCKET_ERROR(ERROR_ENUM, MESSAGE) {\
    mFlags |= SC_SOCKFLAG_CLOSED; \
    close(mFD); \
    notifySocketError(socket_error((ERROR_ENUM), (MESSAGE))); \
}

static auto
errno2str(int error) -> std::string {
    const char* name = strerrorname_np(error);
    const char* desc = strerrordesc_np(error);
    return std::format("{} ({}): {}", name, error, desc);
}

static auto
getBoundAddress(socket_t fd) -> sosimple::Endpoint
{
    sockaddr_storage myaddr{};
    socklen_t addrlen = sizeof(sockaddr_in6);;
    int success = ::getsockname(fd, (sockaddr*)&myaddr, &addrlen);
    int error = errno;
    if (success == -1)
        throw new sosimple::socket_error(sosimple::SocketError::Generic, "Could not read local address for socket: "+errno2str(error));
    sosimple::Endpoint boundEndpoint = sosimple::Endpoint((sockaddr*)&myaddr, addrlen);
    return boundEndpoint;
}

static auto
setDefaultSockOpts(int fd) -> void
{
    int yes = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))==-1)
        throw sosimple::socket_error(sosimple::SocketError::Configuration, "Unable to set socket option SO_REUSEADDR: " + errno2str(errno));
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes))==-1)
        throw sosimple::socket_error(sosimple::SocketError::Configuration, "Unable to set socket option SO_REUSEPORT: " + errno2str(errno));
    if (::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes))==-1)
        throw sosimple::socket_error(sosimple::SocketError::Configuration, "Unable to set socket option SO_KEEPALIVE: " + errno2str(errno));
    int bufferSz = SC_DEFAULT_BUFFER_SIZE;
    if (::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufferSz, sizeof(bufferSz))==-1)
        throw sosimple::socket_error(sosimple::SocketError::Configuration, "Unable to set socket option SO_SNDBUF: " + errno2str(errno));
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufferSz, sizeof(bufferSz))==-1)
        throw sosimple::socket_error(sosimple::SocketError::Configuration, "Unable to set socket option SO_RCVBUF: " + errno2str(errno));
}

auto
sosimple::createUDPUnicast(Endpoint bindAddr) -> std::shared_ptr<ComSocket>
{
    // create the socket
    int domain = bindAddr.isIPv4() ? AF_INET : AF_INET6;
    int type = SOCK_DGRAM | SOCK_NONBLOCK;
    int protocol = IPPROTO_UDP;
    socket_t fd = ::socket( domain, type, protocol );
    int error = errno;
    if (fd == -1) {
        if (error == EACCES) throw socket_error(SocketError::Permission, "Unable to create socket: "+errno2str(error));
        else if (error == EMFILE || error == ENFILE) throw socket_error(SocketError::HandleLimit, "Unable to create socket: "+errno2str(error));
        else throw socket_error(SocketError::Generic, "Unable to create socket: " + errno2str(error));
    }
    //making the shared pointer here so we can't forget to close() the fd
    auto socket = std::make_shared<ComSocketImpl>(fd, Socket::Kind::UDP_Unicast);

    // bind the socket
    sockaddr_storage addr{};
    bindAddr.toSockaddrStorage(addr);
    int success = ::bind( fd, (sockaddr*)&addr, sizeof(addr) );
    error = errno;
    if (success == -1) {
        if (error == EACCES) throw socket_error(SocketError::Permission, "Unable to bind socket: "+errno2str(error));
        else if (error == EADDRINUSE) {
            if (bindAddr.getPort() == 0) throw socket_error(SocketError::HandleLimit, "Unable to bind socket: EADDRINUSE (98): No free ephemeral ports");
            else throw socket_error(SocketError::Bind, "Unable to bind socket: EADDRINUSE (98): Address in use");
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
sosimple::createUDPMulticast(Endpoint interface, Endpoint multicastGroup) -> std::shared_ptr<ComSocket>
{
    if (interface.isIPv4() != multicastGroup.isIPv4())
        throw socket_error(SocketError::Configuration, "Unable to create socket: interface and multicast group were not specified with the same address family (IPv4/IPv6)");

    // create the socket
    int domain = multicastGroup.isIPv4() ? AF_INET : AF_INET6;
    int type = SOCK_DGRAM | SOCK_NONBLOCK;
    int protocol = IPPROTO_UDP;
    socket_t fd = ::socket( domain, type, protocol );
    int error = errno;
    if (fd == -1) {
        if (error == EACCES) throw socket_error(SocketError::Permission, "Unable to create socket: "+errno2str(error));
        else if (error == EMFILE || error == ENFILE) throw socket_error(SocketError::HandleLimit, "Unable to create socket: "+errno2str(error));
        else throw socket_error(SocketError::Generic, "Unable to create socket: " + errno2str(error));
    }
    //making the shared pointer here so we can't forget to close() the fd
    auto socket = std::make_shared<ComSocketImpl>(fd, Socket::Kind::UDP_Multicast);
    socket->mRemote = multicastGroup;

    // bind the socket
    sockaddr_storage addr{};
    addr.ss_family = domain; // bind to "any" == unfiltered
    int success = ::bind( fd, (sockaddr*)&addr, sizeof(addr) );
    error = errno;
    if (success == -1) {
        if (error == EACCES) throw socket_error(SocketError::Permission, "Unable to bind socket: "+errno2str(error));
        else if (error == EADDRINUSE) throw socket_error(SocketError::Bind, "Unable to bind socket: "+errno2str(error));
        else throw socket_error(SocketError::Generic, "Unable to bind socket: " + errno2str(error));
    }
    if (interface.isAny())
        socket->mLocal = getBoundAddress(fd);
    else
        socket->mLocal = interface;

    // set basic socket options
    setDefaultSockOpts(fd);

    // add membership
    int yes=1;
    if (domain == AF_INET) {
        struct ip_mreq mreq{};
        multicastGroup.getInAddr(mreq.imr_multiaddr);
        interface.getInAddr(mreq.imr_interface);
        if (::setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))==-1)
            throw socket_error(SocketError::Configuration, "Unable to join multicast group: " + errno2str(errno));
        if (::setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &yes, sizeof(yes))==-1)
            throw socket_error(SocketError::Configuration, "Unable to set socket option SO_KEEPALIVE: " + errno2str(errno));
    } else {
        struct ipv6_mreq mreq{};
        multicastGroup.getIn6Addr(mreq.ipv6mr_multiaddr);
        if (::setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq, sizeof(mreq))==-1)
            throw socket_error(SocketError::Configuration, "Unable to join multicast group: " + errno2str(errno));
        if (::setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &yes, sizeof(yes))==-1)
            throw socket_error(SocketError::Configuration, "Unable to set socket option SO_KEEPALIVE: " + errno2str(errno));
    }

    socket->start();
    return socket;
}

auto
sosimple::createTCPListen(Endpoint bindAddr) -> std::shared_ptr<ListenSocket>
{
    if (bindAddr.isAny())
        throw socket_error(SocketError::Configuration, "Can not bind listen socket to unspecified endpoint");

    // create the socket
    int domain = bindAddr.isIPv4() ? AF_INET : AF_INET6;
    int type = SOCK_STREAM | SOCK_NONBLOCK;
    int protocol = IPPROTO_TCP;
    socket_t fd = ::socket( domain, type, protocol );
    int error = errno;
    if (fd == -1) {
        if (error == EACCES) throw socket_error(SocketError::Permission, "Unable to create socket: "+errno2str(error));
        else if (error == EMFILE || error == ENFILE) throw socket_error(SocketError::HandleLimit, "Unable to create socket: "+errno2str(error));
        else throw socket_error(SocketError::Generic, "Unable to create socket: " + errno2str(error));
    }
    //making the shared pointer here so we can't forget to close() the fd
    auto socket = std::make_shared<ListenSocketImpl>(fd);

    // bind the socket
    sockaddr_storage addr{};
    bindAddr.toSockaddrStorage(addr);
    int success = ::bind( fd, (sockaddr*)&addr, sizeof(addr) );
    error = errno;
    if (success == -1) {
        if (error == EACCES) throw socket_error(SocketError::Permission, "Unable to bind socket: "+errno2str(error));
        else if (error == EADDRINUSE) {
            if (bindAddr.getPort() == 0) throw socket_error(SocketError::HandleLimit, "Unable to bind socket: EADDRINUSE (98): No free ephemeral ports");
            else throw socket_error(SocketError::Bind, "Unable to bind socket: EADDRINUSE (98): Address in use");
        } else throw socket_error(SocketError::Generic, "Unable to bind socket: " + errno2str(error));
    }
    socket->mLocal = bindAddr;

    // set basic socket options
    setDefaultSockOpts(fd);

    // prepare the listen queue size
    if (::listen(fd, 1)==-1) //we should queue them away quick enough anyways, but whatever
        throw socket_error(SocketError::Configuration, "Unable to set listen queue size: " + errno2str(errno));

    socket->start();
    return socket;
}

auto
sosimple::createTCPClient(Endpoint bindAddr, Endpoint remote) -> std::shared_ptr<ComSocket>
{
    if (bindAddr.isIPv4() != remote.isIPv4())
        throw socket_error(SocketError::Configuration, "Unable to create socket: local and remote endpoints were not specified with the same address family (IPv4/IPv6)");

    // create the socket
    int domain = bindAddr.isIPv4() ? AF_INET : AF_INET6;
    int type = SOCK_STREAM | SOCK_NONBLOCK;
    int protocol = IPPROTO_TCP;
    socket_t fd = ::socket( domain, type, protocol );
    int error = errno;
    if (fd == -1) {
        if (error == EACCES) throw socket_error(SocketError::Permission, "Unable to create socket: "+errno2str(error));
        else if (error == EMFILE || error == ENFILE) throw socket_error(SocketError::HandleLimit, "Unable to create socket: "+errno2str(error));
        else throw socket_error(SocketError::Generic, "Unable to create socket: " + errno2str(error));
    }
    //making the shared pointer here so we can't forget to close() the fd
    auto socket = std::make_shared<ComSocketImpl>(fd, Socket::Kind::TCP_Client);
    socket->mFlags |= SC_SOCKFLAG_CONNECTED;
    socket->mRemote = remote;

    // bind the socket
    sockaddr_storage addr{};
    bindAddr.toSockaddrStorage(addr);
    int success = ::bind( fd, (sockaddr*)&addr, sizeof(addr) );
    error = errno;
    if (success == -1) {
        if (error == EACCES) throw socket_error(SocketError::Permission, "Unable to bind socket: " + errno2str(error));
        else if (error == EADDRINUSE) {
            if (bindAddr.getPort() == 0) throw socket_error(SocketError::HandleLimit, "Unable to bind socket: EADDRINUSE (98): No free ephemeral ports");
            else throw socket_error(SocketError::Bind, "Unable to bind socket: EADDRINUSE (98): Address already in use");
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
    if (::connect(fd, (sockaddr*)&addr, sizeof(addr))==-1) {
        int error = errno;
        if (error == EAGAIN || error == EINPROGRESS)
            { /*ignore*/ }
        else if (error == EINTR || error == ENETUNREACH)
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
    if (mFD < 0 || (mFlags & SC_SOCKFLAG_CLOSED)) return false;

    return pollFD(POLLOUT, 10'000, true) && (mFlags & SC_SOCKFLAG_CLOSED)==0;
}

auto
sosimple::SocketBase::setTimeout(std::chrono::milliseconds timeout) -> void
{
    mWatchDog.setTimeout(timeout);
    if (timeout == std::chrono::microseconds::zero()) {
        mWatchDog.setCallback({}); //ignore timeouts
    } else {
        mWatchDog.setCallback([this](){ SOCKET_ERROR(SocketError::Timeout, "Socket watchdog tripped: Timeout") });
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
    pollfd pollreq{};
    pollreq.fd = mFD;
    pollreq.events = pollval;
    int result = ::poll(&pollreq, 1, timeoutMS); // 10 second timeout
    int error = errno;
    if (result > 0) {
        if (pollreq.revents != pollval) SOCKET_ERROR(SocketError::BrokenPipe, "Unable to poll connection: "+errno2str(error))
        else return true;
    } else if (result == 0) {
        if (notifyOnTimeout) SOCKET_ERROR(SocketError::Timeout, "Unable to poll connection: "+errno2str(error))
    }
    else if (error == ENOMEM) SOCKET_ERROR(SocketError::NoMemory, "Unable to poll connection: "+errno2str(error))
    else SOCKET_ERROR(SocketError::Generic, "Unable to poll connection: " + errno2str(error))
    return false;
}

sosimple::SocketBase::~SocketBase()
{
    if ((mFlags & SC_SOCKFLAG_CLOSED)==0) close(mFD);
}

sosimple::ListenSocketImpl::~ListenSocketImpl()
{
    mRunning = false;
    mWorker.join();
}

auto
sosimple::ListenSocketImpl::start() -> void
{
    mWorker = std::thread([this](){threadMain();});
}

auto
sosimple::ListenSocketImpl::threadMain() -> void
{
    while (mRunning && (mFlags & SC_SOCKFLAG_CLOSED)==0) {
        sockaddr_storage addr{};
        socklen_t addrSz = sizeof(addr);
        socket_t fd = ::accept4(mFD, (sockaddr*)&addr, &addrSz, SOCK_NONBLOCK);
        int error = errno;

        if (fd == -1) {
            if (error == EAGAIN || error == EWOULDBLOCK) {
                // nothin polled within a ms and the watchdog trips? -> timeout!
                if (!pollFD(POLLIN, 1, false) && !mWatchDog.check()) mRunning = false;
            } else if (error == ECONNABORTED) {
                continue; // "a connection?"
            } else if (error == ENOBUFS || error == ENOMEM) {
                mRunning = false;
                SOCKET_ERROR(SocketError::NoMemory, "Could not accept connection: "+errno2str(error))
            } else if (error == EMFILE || error == ENFILE) {
                mRunning = false;
                SOCKET_ERROR(SocketError::HandleLimit, "Could not accept connection: "+errno2str(error))
            } else if (error == EINTR) {
                mRunning = false;
                SOCKET_ERROR(SocketError::BrokenPipe, "Could not accept connection: "+errno2str(error))
            } else {
                mRunning = false;
                SOCKET_ERROR(SocketError::Generic, "Could not accept connection: "+errno2str(error))
            }
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
    mRunning = false;
    mWorker.join();
}

auto
sosimple::ComSocketImpl::start() -> void
{
    mWorker = std::thread([this](){threadMain();});
}

auto
sosimple::ComSocketImpl::threadMain() -> void
{
    uint8_t chunk[4096];
    const bool connected = (mFlags & SC_SOCKFLAG_CONNECTED) != 0;
    while (mRunning && (mFlags & SC_SOCKFLAG_CLOSED)==0) {
        int read;
        Endpoint from;
        sockaddr_storage addr{};
        socklen_t addrSz = sizeof(addr);
        if (connected) {
            read = ::recv(mFD, chunk, sizeof(chunk), 0);
        } else {
            read = ::recvfrom(mFD, chunk, sizeof(chunk), 0, (sockaddr*)&addr, &addrSz);
        }
        int error = errno;
        if (read == -1) {
            if (error == EAGAIN || error == EWOULDBLOCK) {
                // nothin polled within a ms and the watchdog trips? -> timeout!
                if (!pollFD(POLLIN, 1, false) && !mWatchDog.check()) mRunning = false;
            } else if (error == ENOMEM) {
                mRunning = false;
                SOCKET_ERROR(SocketError::NoMemory, "Could not read socket: "+errno2str(error))
            } else if (error == EINTR || error == ECONNREFUSED || error == ENOTCONN) {
                mRunning = false;
                SOCKET_ERROR(SocketError::BrokenPipe, "Could not read socket: "+errno2str(error))
            } else {
                mRunning = false;
                SOCKET_ERROR(SocketError::Generic, "Could not read socket: "+errno2str(error))
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
        result = ::sendto(mFD, payload.data(), payload.size(), 0, (sockaddr*)&addr, sizeof(addr));
        error = errno;
    } else if (mKind == Kind::UDP_Unicast) {
        sockaddr_storage addr{};
        remote.toSockaddrStorage(addr);
        result = ::sendto(mFD, payload.data(), payload.size(), 0, (sockaddr*)&addr, sizeof(addr));
        error = errno;
    } else {
        result = ::send(mFD, payload.data(), payload.size(), 0);
        error = errno;
    }
    if (result == -1) {
        if (error == EAGAIN || error == EWOULDBLOCK) {
            /* pass */
        } else if (error == EACCES) {
            SOCKET_ERROR(SocketError::Permission, "Could not send on socket: "+errno2str(error))
        } else if (error == ENOBUFS || error == ENOMEM || error == EMSGSIZE) {
            SOCKET_ERROR(SocketError::NoMemory, "Could not send on socket: "+errno2str(error))
        } else if (error == ECONNRESET || error == ENOTCONN || error == EPIPE) {
            SOCKET_ERROR(SocketError::BrokenPipe, "Could not send on socket: "+errno2str(error))
        } else if (error == EDESTADDRREQ || error == EISCONN || error == EPIPE) {
            SOCKET_ERROR(SocketError::Configuration, "Could not send on socket: "+errno2str(error))
        } else {
            SOCKET_ERROR(SocketError::Generic, "Could not send on socket: "+errno2str(error))
        }
    }
}