#ifndef _HTTP_SERVER_HPP
#define _HTTP_SERVER_HPP
#include "tcp_server.hpp"
#include "http_error.hpp"
#include <cstring>
#include <string>
#include <filesystem>

namespace http
{

enum Command
{
	OPTIONS = 0,
	GET,
	HEAD,
	POST,
	PUT,
	DELETE,
	TRACE,
	CONNECT,
};

struct Request
{
	Command cmd;
	std::string uri;
	std::string version;
	std::string content;
};

enum ContentType
{
	TEXT_HTML = 0,
	TEXT_CSS,
	TEXT_JS,
	IMAGE_PNG,
	IMAGE_JPEG,
	IMAGE_ICON,
	UNKNOWN,
};

class RequestHandler
{
    enum FsaState
    {
        FSA_STATE_PARSE_INCOMMING_HTTP_PDU = 0,
        FSA_STATE_HANDLE_GET_REQUEST,
        FSA_STATE_DONE
    };
    #define FSA_STATE_DEFAULT FSA_STATE_PARSE_INCOMMING_HTTP_PDU

public:
	RequestHandler(tslogger::Logger &logger, std::filesystem::path &root)
	: m_buffer{},
	  m_offset{0},
	  m_fsaState{FSA_STATE_DEFAULT},
	  m_processing{false},
	  m_request{},
	  m_ec{},
	  m_logger{logger},
	  m_root{root}
	{
		clear();
	}
	~RequestHandler()
	{}

public:
	size_t offset() const
	{
		return m_offset;
	}

	void offset(size_t value)
	{
		m_offset = value;
	}

	CharBuffer &buffer()
	{
		return m_buffer;
	}

	void clear()
	{
		memset(m_buffer.data(), 0, m_buffer.size());
		m_offset = 0;
	}

	void process()
	{
	    m_fsaState = FSA_STATE_DEFAULT;
    	m_processing = true;
	    while(m_processing)
    	{
        	if (s_fsa_state_handler [m_fsaState])
            	s_fsa_state_handler [m_fsaState](this);
    	}	
	}

private:
    static void parse_incomming_http_pdu(void *data);
    static void handle_get_request(void *data);
    static void done(void *data);
    
    typedef void (*fsa_state_handler_ptr)(void *);
    static const fsa_state_handler_ptr s_fsa_state_handler[];

private:
    void parse_incomming_http_pdu();
    void handle_get_request();
    void done();

private:
	CharBuffer m_buffer;
	size_t m_offset;
	FsaState m_fsaState;
	bool m_processing;

private:
	Request m_request;
    std::error_code m_ec;
    tslogger::Logger &m_logger;
    std::filesystem::path &m_root;
};

class HttpServer : public TcpServer
{
public:
	HttpServer(
			const char *root,
			int port,
			bool ipv4,
			const unsigned int maxClients,
			tslogger::Handler &handler,
			const char *logFileName,
			bool logToStdout,
			std::error_code &ec
		);
	~HttpServer()
	{}

private:
	void incoming_handler(
				const Connection &conn,
				tslogger::Logger &logger,
				std::error_code &ec
			) override;

private:
	std::filesystem::path m_root;
};

}// namespace http

#endif

