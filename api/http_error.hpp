#ifndef _HTTP_ERROR_HPP
#define _HTTP_ERROR_HPP
#include <system_error>

enum class HttpStatus
{
    HTTP_OK = 0,
    HTTP_ERR_CREATE_SOCKET,
    HTTP_ERR_SOCKET_NOT_BOUND,
    HTTP_ERR_LISTEN_TO_SOCKET,
    HTTP_ERR_ACCEPT_CONNECTION,
    HTTP_ERR_THREAD_NOT_FOUND,
    HTTP_ERR_CLOSED_CONNECTION,
    HTTP_ERR_NOT_DIRECTORY,
    HTTP_ERR_BAD_REQUEST,
    HTTP_ERR_NOT_IMPLEMENTED,
    HTTP_ERR_FILE_NOT_FOUND,
    HTTP_ERR_FORBIDDEN,
    HTTP_ERR_TIMEOUT,
};

namespace std
{
template<> struct is_error_condition_enum<HttpStatus> : public true_type {};
}

namespace http
{
std::error_code make_error_code (HttpStatus e);
std::error_code make_system_error (int e);
}

#endif
