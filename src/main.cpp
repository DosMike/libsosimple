#include <sosimple.hpp>
#include <iostream>

#include <thread>
#include <mutex>
#include <condition_variable>

#define fun auto

fun
onPacketReceived(const std::vector<uint8_t>& packet, sosimple::Endpoint remote) -> void
{
    std::cout << "Received " << packet.size() << " bytes from " << remote << "\n";
}
fun
onConnectionError(sosimple::socket_error error) -> void
{
    std::cerr << error.what() << "\n";
}

fun
utils_test() -> void
{
    std::cout << " -- Utils Test" << std::endl;
    // list interfaces and IP addrs
    auto ifs = sosimple::utility::ip::listInterfaces();
    for (const auto& i : ifs) {
        std::cout << i << ": " << sosimple::utility::ip::fromString(i) << "\n";
    }
    //restart worker a couple times
    for (int i = 0; i < 10; i++) {
        std::cout << "Restarting worker ("<<(i+1)<<")" << std::endl;
        std::thread worker = sosimple::Worker::make_thread();
        sosimple::Worker::stop();
        if (worker.joinable()) worker.join();
        std::this_thread::sleep_for(std::chrono::milliseconds(i*10));
    }
}

fun
udp_test() -> void
{
    std::cout << " -- UDP Test" << std::endl;
    auto worker = sosimple::Worker::make_thread();
    auto finally = sosimple::on_exit{[&](){
        sosimple::Worker::stop();
        worker.join();
    }};

    std::cout << " Creating sockets" << std::endl;
    auto chan1 = sosimple::createUDPUnicast({"lo", 5000});
    auto chan2 = sosimple::createUDPUnicast({"lo", 5001});
    chan1->onPacket(onPacketReceived);
    chan1->onSocketError(onConnectionError);
    chan2->onSocketError(onConnectionError);

    std::cout << " Sending message" << std::endl;
    std::string msg = "Hello World";
    std::vector<uint8_t> payload{ msg.begin(), msg.end() };
    chan2->send(payload, chan1->getLocalEndpoint());

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

fun
tcp_test() -> void
{
    std::cout << " -- TCP Test" << std::endl;
    auto worker = sosimple::Worker::make_thread();
    auto finally = sosimple::on_exit{[&](){
        sosimple::Worker::stop();
        worker.join();
    }};

    std::cout << " Creating sockets" << std::endl;
    auto sockListen = sosimple::createTCPListen({"lo", 5100});
    sockListen->onSocketError(onConnectionError);
    // set up the listen socket to wait for a connection
    sosimple::pending<std::shared_ptr<sosimple::ComSocket>> pendingSocket{};
    sockListen->onAccept([&](std::shared_ptr<sosimple::ComSocket> connection, sosimple::Endpoint remote) {
        std::cout << "Received connection from " << remote << " to local " << connection->getLocalEndpoint() << "\n";
        connection->onPacket(onPacketReceived);
        connection->onSocketError(onConnectionError);
        pendingSocket = std::move(connection); //get ownership
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); //wait for socket to be opened/ready
    // connect to the tcp server
    auto sockClient = sosimple::createTCPClient({"lo", 0}, {"lo", 5100});
    sockClient->onPacket(onPacketReceived);
    sockClient->onSocketError(onConnectionError);
    // wait for the callbacks to give us a connection
    pendingSocket.wait_for(std::chrono::milliseconds(100));
    if (!pendingSocket) {
        std::cout << " Connection failed!" << std::endl;
        return;
    }

    std::cout << " Sending message" << std::endl;
    std::string msg = "Hello World";
    std::vector<uint8_t> payload{ msg.begin(), msg.end() };
    sockClient->send(payload, {});

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

fun
main(int argc, char** argv) -> int
{
    utils_test();
    udp_test();
    tcp_test();
}
