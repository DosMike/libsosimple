#if !defined SOSIMPLE_NETHEADERS_HPP
#define SOSIMPLE_NETHEADERS_HPP

// common bunch of network, socket and other headers that i dont want to repeat over and over

//std
#include <cstdint>
#include <string>
#include <stdexcept>
#include <memory>
#include <functional>

//posix
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <poll.h>

/// Socket type defined for clarity
/// (FDs are int on loonix, "SOCKET" alias type on windoof)
typedef int socket_t;

#endif
