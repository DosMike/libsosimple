#include <sosimple.hpp>
#include "platforms_internal.hpp"

auto sosimple::WINGLUE_FormatMessage(int errorno) -> char const*
{
    static char message[512]={};

    if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorno, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            message, sizeof(message), NULL))
        return message;

    return nullptr;
}
auto sosimple::WINGLUE_ErrnoName(int errorno) -> char const*
{
    switch(errorno) {
        case 6: return "WSA_INVALID_HANDLE";
        case 8: return "WSA_NOT_ENOUGH_MEMORY";
        case 87: return "WSA_INVALID_PARAMETER";
        case 995: return "WSA_OPERATION_ABORTED";
        case 996: return "WSA_IO_INCOMPLETE";
        case 997: return "WSA_IO_PENDING";
        case 10004: return "WSAEINTR";
        case 10009: return "WSAEBADF";
        case 10013: return "WSAEACCES";
        case 10014: return "WSAEFAULT";
        case 10022: return "WSAEINVAL";
        case 10024: return "WSAEMFILE";
        case 10035: return "WSAEWOULDBLOCK";
        case 10036: return "WSAEINPROGRESS";
        case 10037: return "WSAEALREADY";
        case 10038: return "WSAENOTSOCK";
        case 10039: return "WSAEDESTADDRREQ";
        case 10040: return "WSAEMSGSIZE";
        case 10041: return "WSAEPROTOTYPE";
        case 10042: return "WSAENOPROTOOPT";
        case 10043: return "WSAEPROTONOSUPPORT";
        case 10044: return "WSAESOCKTNOSUPPORT";
        case 10045: return "WSAEOPNOTSUPP";
        case 10046: return "WSAEPFNOSUPPORT";
        case 10047: return "WSAEAFNOSUPPORT";
        case 10048: return "WSAEADDRINUSE";
        case 10049: return "WSAEADDRNOTAVAIL";
        case 10050: return "WSAENETDOWN";
        case 10051: return "WSAENETUNREACH";
        case 10052: return "WSAENETRESET";
        case 10053: return "WSAECONNABORTED";
        case 10054: return "WSAECONNRESET";
        case 10055: return "WSAENOBUFS";
        case 10056: return "WSAEISCONN";
        case 10057: return "WSAENOTCONN";
        case 10058: return "WSAESHUTDOWN";
        case 10059: return "WSAETOOMANYREFS";
        case 10060: return "WSAETIMEDOUT";
        case 10061: return "WSAECONNREFUSED";
        case 10062: return "WSAELOOP";
        case 10063: return "WSAENAMETOOLONG";
        case 10064: return "WSAEHOSTDOWN";
        case 10065: return "WSAEHOSTUNREACH";
        case 10066: return "WSAENOTEMPTY";
        case 10067: return "WSAEPROCLIM";
        case 10068: return "WSAEUSERS";
        case 10069: return "WSAEDQUOT";
        case 10070: return "WSAESTALE";
        case 10071: return "WSAEREMOTE";
        case 10091: return "WSASYSNOTREADY";
        case 10092: return "WSAVERNOTSUPPORTED";
        case 10093: return "WSANOTINITIALISED";
        case 10101: return "WSAEDISCON";
        case 10102: return "WSAENOMORE";
        case 10103: return "WSAECANCELLED";
        case 10104: return "WSAEINVALIDPROCTABLE";
        case 10105: return "WSAEINVALIDPROVIDER";
        case 10106: return "WSAEPROVIDERFAILEDINIT";
        case 10107: return "WSASYSCALLFAILURE";
        case 10108: return "WSASERVICE_NOT_FOUND";
        case 10109: return "WSATYPE_NOT_FOUND";
        case 10110: return "WSA_E_NO_MORE";
        case 10111: return "WSA_E_CANCELLED";
        case 10112: return "WSAEREFUSED";
        case 11001: return "WSAHOST_NOT_FOUND";
        case 11002: return "WSATRY_AGAIN";
        case 11003: return "WSANO_RECOVERY";
        case 11004: return "WSANO_DATA";
        case 11005: return "WSA_QOS_RECEIVERS";
        case 11006: return "WSA_QOS_SENDERS";
        case 11007: return "WSA_QOS_NO_SENDERS";
        case 11008: return "WSA_QOS_NO_RECEIVERS";
        case 11009: return "WSA_QOS_REQUEST_CONFIRMED";
        case 11010: return "WSA_QOS_ADMISSION_FAILURE";
        case 11011: return "WSA_QOS_POLICY_FAILURE";
        case 11012: return "WSA_QOS_BAD_STYLE";
        case 11013: return "WSA_QOS_BAD_OBJECT";
        case 11014: return "WSA_QOS_TRAFFIC_CTRL_ERROR";
        case 11015: return "WSA_QOS_GENERIC_ERROR";
        case 11016: return "WSA_QOS_ESERVICETYPE";
        case 11017: return "WSA_QOS_EFLOWSPEC";
        case 11018: return "WSA_QOS_EPROVSPECBUF";
        case 11019: return "WSA_QOS_EFILTERSTYLE";
        case 11020: return "WSA_QOS_EFILTERTYPE";
        case 11021: return "WSA_QOS_EFILTERCOUNT";
        case 11022: return "WSA_QOS_EOBJLENGTH";
        case 11023: return "WSA_QOS_EFLOWCOUNT";
        case 11024: return "WSA_QOS_EUNKOWNPSOBJ";
        case 11025: return "WSA_QOS_EPOLICYOBJ";
        case 11026: return "WSA_QOS_EFLOWDESC";
        case 11027: return "WSA_QOS_EPSFLOWSPEC";
        case 11028: return "WSA_QOS_EPSFILTERSPEC";
        case 11029: return "WSA_QOS_ESDMODEOBJ";
        case 11030: return "WSA_QOS_ESHAPERATEOBJ";
        case 11031: return "WSA_QOS_RESERVED_PETYPE";
        default: return nullptr;
    }
}


auto
sosimple::WINGLUE_APIInit() -> void
{
    // Note: there is no cleanup/WSAShutdown
    // For a static library that would be required to be called by the user upon application termination
    // while for dynamic libraries the unload callback could be used. The only time where this would
    // maybe make sense would be, that the dynamic library was loaded over and over again, increasing
    // the Setup call counter. Anyways the OS will clean up after us when the application terminates,
    // so this is wasted effort in my opinion. KISS

    // guard to init no more than once
    // Warning: this can still be called multiple times if linked into the end user application via different libraries
    static bool once{false};
    if (once) return;
    else once = true;

    // initialize WSA
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (result != 0)
        throw new sosimple::socket_error(sosimple::SocketError::Generic, "WSAStartup failed with code "+std::to_string(result));
}
