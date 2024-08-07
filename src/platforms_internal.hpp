#if !defined SOSIMPLE_PLATFORMS_INTERNAL_HPP
#define SOSIMPLE_PLATFORMS_INTERNAL_HPP

// some replacers for GNU apis around errno, error name and description

namespace sosimple {

#if defined _WIN32
    // impl in windows_compat.cpp

    auto
    WINGLUE_FormatMessage(int errorno) -> char const*;
    auto
    WINGLUE_ErrnoName(int errorno) -> char const*;

    #define GNU_STRERRORNAME_NP(x) sosimple::WINGLUE_ErrnoName(x)
    #define GNU_STRERRORDESC_NP(x) sosimple::WINGLUE_FormatMessage(x)

    auto
    WINGLUE_APIInit() -> void;

    #define SOSIMPLE_SOCKET_INIT sosimple::WINGLUE_APIInit()

#else

    #define GNU_STRERRORNAME_NP(x) ::strerrorname_np(x)
    #define GNU_STRERRORDESC_NP(x) ::strerrordesc_np(x)

    #define SOSIMPLE_SOCKET_INIT while(false)

#endif

}

#endif