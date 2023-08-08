#include "http_server.hpp"
#include "utils.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace tslogger;

namespace http
{

const char *HEADER_SEPARATOR = "\r\n";
const char *CONTENT_SEPARATOR = "\r\n\r\n";

const char *RESPONSE_HEADER_TEMPLATE = "HTTP/1.1 %s\r\n"\
"Server: simple-http-server\r\n"\
"Content-Encoding: %s\r\n"\
"Content-Length: %08d\r\n"\
"Content-Type: %s\r\n"\
"\r\n";

const char *ERROR_PAGE_TEMPLATE = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"\
"<html>"\
"<head>"\
"\t<title>%s</title>"\
"</head>"\
"<body>"\
"\t<h1>%s</h1>"\
"\t<p>%s</p>"\
"</body>"\
"</html>";


const RequestHandler::fsa_state_handler_ptr RequestHandler::s_fsa_state_handler[] = {
    parse_incomming_http_pdu,
    handle_get_request,
    done
};

void RequestHandler::parse_incomming_http_pdu(void *data)
{
	((RequestHandler *)data)->parse_incomming_http_pdu();
}

void RequestHandler::handle_get_request(void *data)
{
	((RequestHandler *)data)->handle_get_request();
}

void RequestHandler::done(void *data)
{
	((RequestHandler *)data)->done();
}

static const char *cmd2str(Command cmd)
{
	switch(cmd)
	{
	case OPTIONS:
		return "OPTIONS";
	case GET:
		return "GET";
	case HEAD:
		return "HEAD";
	case POST:
		return "POST";
	case PUT:
		return "PUT";
	case DELETE:
		return "DELETE";
	case TRACE:
		return "TRACE";
	case CONNECT:
		return "CONNECT";
	default:
		return "Unknown";
	}
}

static const char *content_type2str(ContentType t)
{
	switch(t)
	{
	case TEXT_HTML:
		return "text/html";
	case TEXT_CSS:
		return "text/css";
	case TEXT_JS:
		return "text/javascript";
	case IMAGE_PNG:
		return "image/png";
	case IMAGE_JPEG:
		return "image/jpeg";
	case IMAGE_ICON:
		return "image/vnd.microsoft.icon";
	default:
		return "application/octet-stream";
	}
}

static ContentType file_extention2content_type(const char *extension)
{
	size_t el = strlen(extension);
	std::map<std::string, ContentType> ctm = { 
		{".html",TEXT_HTML}, {".css",TEXT_CSS}, {".js",TEXT_JS},
		{".png",IMAGE_PNG}, {".jpeg",IMAGE_JPEG}, {".jpg",IMAGE_JPEG}, {".ico",IMAGE_ICON}
	};
	for (const auto& [key, value] : ctm)
	{
		if (strncmp(key.c_str(), extension, el) == 0)
			return value;
	}
	return UNKNOWN;
}

static void parse_command(const char *line, Request &rqst, std::error_code &ec)
{
	for (Command cmd : { OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE, CONNECT })
	{
		const char *cmdStr = cmd2str(cmd);
		if (strncmp(line, cmdStr, strlen(cmdStr)) == 0)
		{
			rqst.cmd = cmd;
			return;
		}
	}
	ec = make_error_code(HttpStatus::HTTP_ERR_BAD_REQUEST);
} 

static void parse_uri(const char *line, Request &rqst, std::error_code &ec)
{
	if (strlen(line) > 2000) // if uri is longer than 2000 characters, there is an error
	{
		ec = make_error_code(HttpStatus::HTTP_ERR_BAD_REQUEST);
		return;
	}
	rqst.uri = line;
}

static void parse_version(const char *line, Request &rqst, std::error_code &ec)
{
	if (strlen(line) > strlen("HTTP/1.1"))
	{
		ec = make_error_code(HttpStatus::HTTP_ERR_BAD_REQUEST);
		return;
	}
	rqst.version = line;
}

void RequestHandler::parse_incomming_http_pdu()
{
	m_logger.log(DEBUG, "%s:%d <<< Entering\n", __FILE__, __LINE__);
	char request [m_offset];
	memcpy (request, m_buffer.data(), m_offset);
	// split to header and content

	char *token = strstr(request, CONTENT_SEPARATOR);
	if (token != NULL)
	{
		token += strlen(CONTENT_SEPARATOR);
		m_request.content = token; // copy content of the request
	}
	token = strtok(request, HEADER_SEPARATOR);
	if (token == NULL)
	{
		m_ec = make_error_code(HttpStatus::HTTP_ERR_BAD_REQUEST);
		m_logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, m_ec.message().c_str());
		m_fsaState = FSA_STATE_DONE;
		return;
	}
	// parse header

	typedef void (*header_parser_t)(const char *line, Request &rqst, std::error_code &ec);
	header_parser_t parsers[] = {
		parse_command,
		parse_uri,
		parse_version,
		nullptr
	};
	token = strtok(request, " ");
	int state = 0;
	while(token != NULL && parsers[state] != nullptr)
	{
		parsers[state++](token, m_request, m_ec);
		if (m_ec.value())
		{
			m_fsaState = FSA_STATE_DONE;
			m_logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, m_ec.message().c_str());
			return;			
		}
		token = strtok(NULL, " ");
	}
	m_logger.log(DEBUG, "m_request.uri = %s\n", m_request.uri.c_str());
	// parse command

	if (m_request.cmd != GET)
	{
		m_ec = make_error_code(HttpStatus::HTTP_ERR_NOT_IMPLEMENTED);
		m_logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, m_ec.message().c_str());
		m_fsaState = FSA_STATE_DONE;
		return;	
	}
	m_fsaState = FSA_STATE_HANDLE_GET_REQUEST;
	m_logger.log(DEBUG, "%s:%d >>> Exiting\n", __FILE__, __LINE__);
}

static void create_header(std::filesystem::path &filePath, size_t contentSize, char (&out)[256])
{
    const char *ct = content_type2str( file_extention2content_type( filePath.extension().string().c_str() ) );
    sprintf(out, RESPONSE_HEADER_TEMPLATE, "200 OK", "deflate", contentSize, ct);
}

void RequestHandler::handle_get_request()
{
	m_logger.log(DEBUG, "%s:%d <<< Entering\n", __FILE__, __LINE__);
	m_fsaState = FSA_STATE_DONE;

	m_logger.log(DEBUG, "%s:%d %s\n", __FILE__, __LINE__, m_root.string().c_str());
	
	if (m_request.uri != "/" && m_request.uri[0] == '/') {
		m_request.uri.erase(0,1);
	}

	std::filesystem::path filePath = (m_request.uri == "/") ? (m_root / std::filesystem::path("index.html")) : (m_root / std::filesystem::path(m_request.uri));
    m_logger.log(DEBUG, "%s:%d %s\n", __FILE__, __LINE__, filePath.string().c_str());

    if (std::filesystem::is_directory(filePath)) {
		m_ec = make_error_code(HttpStatus::HTTP_ERR_NOT_IMPLEMENTED);
		m_logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, m_ec.message().c_str());
		return;
    }
    // content size is required here only to calculate a header size
	size_t contentSize = MAX_BUFFER_SIZE;
    char header[256];
    create_header(filePath, contentSize, header);

	m_buffer.clear();
	// copy the header to the output buffer
    strncpy(m_buffer.data(), header, strlen(header));
	m_offset = strlen(header);
	m_buffer.offset(m_offset);
	// compress the requested file
	contentSize = compress_file(filePath, m_logger, m_buffer, m_ec);
	if (m_ec.value()) {
		m_logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, m_ec.message().c_str());
		return;
	}

	// create header again due to the Content-Lenght has been changed
	create_header(filePath, contentSize, header);
	strncpy(m_buffer.data(), header, strlen(header));
	m_offset = strlen(header);

	m_offset += contentSize;
	m_logger.log(DEBUG, "%s:%d >>> Exiting\n", __FILE__, __LINE__);
}

static void create_error_content(std::error_code &ec, std::string &out)
{
	char header[256];
	char html[256];
	sprintf(html, ERROR_PAGE_TEMPLATE, ec.message().c_str(), ec.message().c_str(), ec.message().c_str());
	sprintf(header, RESPONSE_HEADER_TEMPLATE, ec.message().c_str(), "deflate", strlen(html), "text/html");
	out = header;
	out += html;
}

void RequestHandler::done()
{
	m_logger.log(DEBUG, "%s:%d <<< Entering\n", __FILE__, __LINE__);
	m_processing = false;
	if (m_ec.value())
	{
		std::string answer;
		create_error_content(m_ec, answer);
		memcpy(m_buffer.data(), answer.c_str(), answer.size());
		m_offset = answer.size();
	}
	m_logger.log(DEBUG, "%s:%d >>> Exiting\n", __FILE__, __LINE__);
}

HttpServer::HttpServer(
		const char *root,
		int port,
		bool ipv4,
		const unsigned int maxClients,
		tslogger::Handler &handler,
		const char *logFileName,
		bool logToStdout,
		std::error_code &ec
	):
	TcpServer(
		port,
		ipv4,
		maxClients,
		handler,
		logFileName,
		logToStdout,
		ec
	),
	m_root{root}
{
	if (ec.value())
		return;
	if (!std::filesystem::is_directory(m_root)) {
        ec = make_error_code(HttpStatus::HTTP_ERR_NOT_DIRECTORY);
    }
}

void HttpServer::incoming_handler(
			const Connection &conn,
			tslogger::Logger &logger,
			std::error_code &ec
		)
{
	RequestHandler rh(logger, m_root);
	ssize_t received, sent;
	socklen_t addr_len;
	struct sockaddr *client_addr;
	Connection inconn = conn;
	if (conn.ipv4) {
		addr_len = sizeof(inconn.client.addr);
		client_addr = reinterpret_cast<struct sockaddr *>(&inconn.client.addr);
	}
	else {
		addr_len = sizeof(inconn.client.addr6);
		client_addr = reinterpret_cast<struct sockaddr *>(&inconn.client.addr6);
	}
	memset(client_addr, 0, addr_len);

	received = ::recvfrom(conn.sockfd, rh.buffer().data(), rh.buffer().size(), MSG_DONTWAIT, client_addr, &addr_len);
	if (received > 0) {
		logger.log(INFO, "<-- %d bytes received\n", received);
		rh.offset(received);
		rh.process();
		//log_connection(logger, static_cast<const Connection&>(inconn));
		if (sent = ::sendto(conn.sockfd, rh.buffer().data(), rh.offset(), MSG_DONTWAIT, client_addr, addr_len) == -1) {
			ec = make_system_error(errno);
			logger.log(ERROR, "%s\n", ec.message().c_str());
			ec.clear();
		}
		else {
			logger.log(INFO, "--> %d bytes sent\n", rh.offset());
		}
	}
	else {
		ec = make_error_code(HttpStatus::HTTP_ERR_CLOSED_CONNECTION);
	}
}

}
