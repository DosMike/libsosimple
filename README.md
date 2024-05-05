# LibSoSimple

Trying to make UDP and TCP sockets as easy as possible, without having to care about POSIX sockets, but still giving you access.
Creating a socket is done using one function call. Processing incoming connections, packets and errors is done with optionally async callbacks based in a worker thread that is managed for you.

The name is a left over from a previous concept I wanted to use, but didn't bother with any further.

***Currently this library is Linux only because the Windows POSIX compatible API is not quite compatible***

### Sockets

The there are sosimple::create* functions for every kind of supported socket.
During setup of a socket, these functions will throw a socket_error instance. Every socket has an error callback that will be invoked for any issue the underlying API runs into.

Sockets are open as long as your application holds ownership over the instances. Once they destruct, the sockets close.
You can retrieve the native socket handle / file descriptor to configure it further if you need to.

#### Listen socket

TCP listen server use a special ListenSocket type, that has an onAccept callback giving you the ComSocket instances of newly accepted connections

#### Com socket

All other sockets are ComSockets and have a onPacket callback as well as a send function. The send function has an endpoint argument for where to
send to, but this argument is not supported for UDP multicast or TCP. Just pass in an empty Endpoint `{}`.

Receiving packets from a TCP stream happens in 4096 byte chunks. If your message is larger, it will call back multiple times and you have to
stich the message back together. A protocol embedding message length can be helping in that. Sending is similarly done in 4096 chunks.

### Utilities

Besides a simple socket interface, there's also a hand full of utilities that make your life easier.

#### utility::ip

Two convenience functions for working with interfaces. One, listInterfaces(), returns a set of strings with all interface names on your system that are IPv4 or IPv6. The other function, fromString(), resolves "lo" to loopback "any" to 0.0.0.0 literal IPv4/IPv6 representations and interface names into the canonical string representation. This can then easily be resolved into a posix compatible byte array with inet_pton(). This is autmatically called for you when you construct an Endpoint instance.

#### Worker

Worker is a single threaded async handler for callbacks. While it might not be best solution for long running tasks on high connection count servers, it gets most jobs done fine.

#### on_exit

A callback wrapper that executes a callable when the current context ends, be it normally or exceptionally. Can be used to clean up things that would otherwise linger, similar to a finally block in the try...catch of other languages.

#### pending\<T>

A utility similar in conecpt to an std::optional, where it can an can not have a value, with the major difference, that you can wait for a pending to retrieve a value. Be careful tho, wait() will also release if the pending is about to be destructed.

#### Watchdog

Simple pollable watchdog class that invokes a callback when tripped.
Call check() to test if it timed out, and reset() to reset the timeout.

### Examples

```c++
try {
    // Creating the worker thread
    auto worker = sosimple::Worker::make_thread();
    auto finally = sosimple::on_exit{[&](){
        sosimple::Worker::stop();
        worker.join();
    }};

    // Creating a UDP socket
    auto udpSocket = sosimple::createUDPUnicast({"lo", 5000});
    udpSocket->setTimeout(std::chrono::milliseconds(10'000));
    udpSocket->onPacket(onPacketReceived);
    udpSocket->onSocketError(onConnectionError);
    std::string msg = "Hello World";
    std::vector<uint8_t> payload{ msg.begin(), msg.end() };
    if (udpSocket->isOpen()) udpSocket->send(payload, {"lo", 5100});

    // Creating a TCP server
    auto sockListen = sosimple::createTCPListen({"lo", 6000});
    sockListen->onSocketError(onConnectionError);
    // set up the listen socket to wait for a connection
    sosimple::pending<std::shared_ptr<sosimple::ComSocket>> pendingSocket{};
    sockListen->onAccept([&](std::shared_ptr<sosimple::ComSocket> connection, sosimple::Endpoint remote) {
        std::cout << "Received connection from " << remote << " to local " << connection->getLocalEndpoint() << "\n";
        connection->onPacket(onPacketReceived);
        connection->onSocketError(onConnectionError);
        pendingSocket = std::move(connection); //get ownership
    });
    // wait for the callbacks to give us a connection
    pendingSocket.wait_for(std::chrono::milliseconds(100));
    if (!pendingSocket) {
        std::cout << "Nobody connected" << std::endl;
        return;
    }

    // Create a TCP client
    auto sockClient = sosimple::createTCPClient({}, {"lo", 6000});
    sockClient->onPacket(onPacketReceived);
    sockClient->onSocketError(onConnectionError);

} catch (socckchan::socket_error& error) {
    // sosimple::create* functions throw on misconfiguration
    // all other errors are retrieve with onSocketError(), including
    // connections closing
    std::cerr << "Failed to create a socket: " << error.what() << "\n";
}
```

### Future Plans

* Add an SSL wrapper for easy SSL/TLS connections
