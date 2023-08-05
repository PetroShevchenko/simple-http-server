#include "http_error.hpp"
#include <string>

namespace
{

struct HttpErrorCategory : public std::error_category
{
    const char* name() const noexcept override;
    std::string message(int ev) const override;
};

const char* HttpErrorCategory::name() const noexcept
{ return "http"; }

std::string HttpErrorCategory::message(int ev) const
{
    switch((HttpStatus)ev)
    {
        case HttpStatus::HTTP_OK:
            return "Success";
        case HttpStatus::HTTP_ERR_CREATE_SOCKET:
            return "Failed to create socket";
        case HttpStatus::HTTP_ERR_SOCKET_NOT_BOUND:
            return "Failed to bind socket";
        case HttpStatus::HTTP_ERR_LISTEN_TO_SOCKET:
            return "Failed to listen socket";
        case HttpStatus::HTTP_ERR_ACCEPT_CONNECTION:
            return "Failed to accept incoming conection";
        case HttpStatus::HTTP_ERR_THREAD_NOT_FOUND:
            return "There is no such thread id in the thread list";
        case HttpStatus::HTTP_ERR_CLOSED_CONNECTION:
            return "The connection has been closed by the peer";
        case HttpStatus::HTTP_ERR_NOT_DIRECTORY:
            return "This should be a directory";
        case HttpStatus::HTTP_ERR_BAD_REQUEST:
            return "400 Bad Request";
        case HttpStatus::HTTP_ERR_NOT_IMPLEMENTED:
            return "501 Not Implemented";
        case HttpStatus::HTTP_ERR_FILE_NOT_FOUND:
            return "404 Not Found";
        case HttpStatus::HTTP_ERR_FORBIDDEN:
            return "403 Forbidden";
        case HttpStatus::HTTP_ERR_TIMEOUT:
            return "Timeout expired";
    }
    return "Unknown error";
}

const HttpErrorCategory theHttpErrorCategory {};

} // namespace

namespace http 
{ 
std::error_code make_error_code (HttpStatus e)
{
    return {static_cast<int>(e), theHttpErrorCategory};
}

std::error_code make_system_error (int e)
{
    return {e, std::generic_category()};
}

} // namespace
