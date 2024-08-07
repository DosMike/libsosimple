#if !defined SOSIMPLE_UTILITIES_HPP
#define SOSIMPLE_UTILITIES_HPP

#include "sosimple/exports.hpp"

#include <set>
#include <stdexcept>
#include <functional>

namespace sosimple {
    /** general error enum for this library */
    enum class SOSIMPLE_API SocketError {
        OK, ///< error is probably uninitialized
        Generic, ///< other unspecified error
        Permission, ///< application has no permission
        Configuration, ///< failed to configure options on socket
        Bind, ///< address protected, already in use or no free ephemeral port (port was 0)
        Timeout, ///< rw timed out after 10s, connection is probably gone
        BrokenPipe, ///< remote closed connection, or fd was closed/invalidated
        NoMemory, ///< somewhere ran out of memory
        Listen, ///< error listening to connections
        HandleLimit, ///< used all file handles
    };

    /** socket error. use errcode() to check what happened */
    class SOSIMPLE_API socket_error : public std::runtime_error {
        SocketError error;
    public:
        inline
        socket_error(SocketError error, char const* const message) throw()
        : error(error), std::runtime_error(message) {}
        inline
        socket_error(SocketError error, const std::string& message) throw()
        : error(error), std::runtime_error(message) {}

        inline auto
        errcode() -> SocketError
        { return error; }
    };

    namespace utility::ip {
        /** get names of all IP network interfaces */
        auto SOSIMPLE_API
        listInterfaces() -> std::set<std::string>;

        /** validate IP string or look up interface name, returns ip addr string for inet_pton */
        auto SOSIMPLE_API
        fromString(const std::string& name) -> std::string;
    }

    /** this is a container utilizing life time management to implement an "on exit" callback
     * for the current scope. Think of it as the "finally" clause in try-catch blocks, with the
     * only difference, that you'll have to create the on_exit instance first and wrap it like this:
     * <code>
     * { // try-catch context
     *   on_exit finally([&]{ ... });
     *   try { ... } catch (...) { ... }
     * }
     * </code>
     */
    class SOSIMPLE_API on_exit {
        std::function<void()> fun;
    public:
        inline
        on_exit(std::function<void()> function)
        : fun(function) {}

        inline
        ~on_exit()
        { fun(); }
    };
}

#endif
