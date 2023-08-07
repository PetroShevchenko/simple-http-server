#include <cstdint>
#include <cstring>
#include <thread>
#include "tcp_server.hpp"
#include "http_error.hpp"
#include "utils.hpp"

using namespace tslogger;

namespace http
{

TcpServer::TcpServer(
        int port,
        bool ipv4,
        const unsigned int maxClients,
        tslogger::Handler &handler,
        const char *logFileName,
        bool logToStdout,
        std::error_code &ec
        ):
    m_running{false},
    m_port{port},
    m_maxClients{maxClients},
    m_conn{},
    m_threads{},
    m_remove{},
    m_mutex{},
    m_logger{handler.get_queue_ptr(), logFileName, logToStdout ? FLAGS_OUTPUT_TO_ALL : FLAGS_OUTPUT_TO_FILE_ONLY}
{
    ENTER();
    int sockfd = ::socket(ipv4 ? AF_INET : AF_INET6, SOCK_STREAM, 0);
    if (sockfd == -1) {
        ec = make_error_code(HttpStatus::HTTP_ERR_CREATE_SOCKET);
        LOG_E(ec.message().c_str());
        EXIT();
        return;
    }
    uint16_t _port = htons((uint16_t)port);
    if (ipv4)
    {
        // start inintialization by zero values
        memset(&m_conn.serv.addr, 0, sizeof(m_conn.serv.addr));
        // initialization of server address structure
        m_conn.serv.addr.sin_family = AF_INET;
        m_conn.serv.addr.sin_addr.s_addr = INADDR_ANY;
        m_conn.serv.addr.sin_port = _port;
    }
    else {
        memset(&m_conn.serv.addr6, 0, sizeof(m_conn.serv.addr6));
        m_conn.serv.addr6.sin6_family = AF_INET6;
        m_conn.serv.addr6.sin6_addr = in6addr_any;
        m_conn.serv.addr6.sin6_port = _port;
    }
    // bind an empty socket to serv.addr(6) structure
    if (::bind(sockfd, ipv4 ? (struct sockaddr *)&m_conn.serv.addr : (struct sockaddr *)&m_conn.serv.addr6,
                 ipv4 ? sizeof(m_conn.serv.addr) : sizeof(m_conn.serv.addr6))==-1) {
        LOG_D("sockfd = %d\n", sockfd);
        LOG_D("errno = %d\n", errno);
        ec = make_error_code(HttpStatus::HTTP_ERR_SOCKET_NOT_BOUND);
        close(sockfd);
        LOG_E(ec.message().c_str());
        EXIT();
        return;
    }
    // listen to an incomming connection
    if (::listen(sockfd, maxClients) == -1) {
        ec = make_error_code(HttpStatus::HTTP_ERR_LISTEN_TO_SOCKET);
        LOG_D("sockfd = %d\n", sockfd);
        LOG_D("maxClients = %d\n", maxClients);
        close(sockfd);
        LOG_D("errno = %d\n", errno);
        LOG_E("%s\n",ec.message().c_str());
        EXIT();
        return;
    }
    m_conn.ipv4 = ipv4;
    m_conn.sockfd = sockfd;
    unsigned int maxThreads = get_max_threads(MAX_BUFFER_SIZE);
    if (maxClients > maxThreads) {
        m_maxClients = maxThreads;
    }
    start();
    EXIT();
}

TcpServer::~TcpServer()
{
    close(m_conn.sockfd);
}

void TcpServer::operator()()
{
    tslogger::Logger logger(m_logger.queue_ptr(), m_logger.filename(), m_logger.flags());
    logger.log(DEBUG, "%s:%d <<< Entering\n", __FILE__, __LINE__);
    std::error_code ec;
    //TcpThread current = get_thread(std::this_thread::get_id(), ec);
    Connection conn;
    get_connection(conn, std::this_thread::get_id(), ec);
    if (ec.value()) {
        logger.log(ERROR, "%s\n", ec.message().c_str());
        logger.log(DEBUG, "%s:%d >>> Exiting\n", __FILE__, __LINE__);
        return;
    }

    int status;
    fd_set rd;
    struct timeval tv;
    time_t timeout = 1;// wait 1 second

    logger.log(INFO, "New client thread has been started\n");

    while(is_running())
    {
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        //std::cout << "conn.sockfd: " << conn.sockfd << "\n";
        FD_ZERO(&rd);
        FD_SET(conn.sockfd, &rd);

        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        status = select(FD_SETSIZE, &rd, NULL, NULL, &tv);
        //std::cout << "status: " << status << "\n";
        if (status == -1)
        {
            ec = make_system_error(errno);
            logger.log(ERROR, "%s:%d %s\n",  __FILE__, __LINE__, ec.message().c_str());
            break;            
        }
        else if (status)
        {
            ssize_t received, sent;
            if (FD_ISSET(conn.sockfd, &rd))
            {
                //log_connection(logger, conn);
                incoming_handler(conn, logger, ec);
                if (ec.value()) {
                    logger.log(ERROR, "%s:%d %s\n",  __FILE__, __LINE__, ec.message().c_str());
                    break;
                }
            }
        }
        else {
            //logger.flags(FLAGS_OUTPUT_TO_STREAM_ONLY);
            //logger.log(INFO, "No data within %d seconds\n", timeout);
            //logger.flags(FLAGS_OUTPUT_TO_ALL);
        }
    }
    ::close(conn.sockfd);
    ec.clear();
    add_thread_to_remove(std::this_thread::get_id());
    if (ec.value()) {
        logger.log(ERROR, "%s:%d %s\n",  __FILE__, __LINE__, ec.message());
    }
    logger.log(DEBUG, "%s:%d >>> Exiting\n", __FILE__, __LINE__);
}

void TcpServer::accept(Connection &conn, std::error_code &ec)
{
    ENTER();
    int accept_fd;
    socklen_t addr_len;
    struct sockaddr *client_addr;
    //std::cout << "m_conn.ipv4 : " << m_conn.ipv4 << "\n";
    if (m_conn.ipv4) {
        addr_len = sizeof(conn.client.addr);
        client_addr = reinterpret_cast<struct sockaddr *>(&conn.client.addr);
    }
    else {
        addr_len = sizeof(conn.client.addr6);
        client_addr = reinterpret_cast<struct sockaddr *>(&conn.client.addr6);
    }
    memset(client_addr, 0, addr_len);
    //std::cout << "m_conn.sockfd : " << m_conn.sockfd << "\n";

    // accept shouldn't block the thread, select is used to achieve this purpose
    int status;
    fd_set rd;
    struct timeval tv;
    time_t timeout = 60;// wait for 60 seconds
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    FD_ZERO(&rd);
    FD_SET(m_conn.sockfd, &rd);

    status = select(FD_SETSIZE, &rd, NULL, NULL, &tv);
    //std::cout << "select status = " << status << "\n";
    if (status == -1) {
        ec = make_system_error(errno);
        LOG_E("%s:%d %s\n", __FILE__, __LINE__, ec.message().c_str());
        EXIT();
        return;
    }
    else if (status == 0) {
        //m_logger.flags(FLAGS_OUTPUT_TO_STREAM_ONLY);
        //LOG_I("No incoming connection within %d seconds\n", timeout);
        ec = make_error_code(HttpStatus::HTTP_ERR_TIMEOUT);
        EXIT();
        //m_logger.flags(FLAGS_OUTPUT_TO_ALL);
        return;
    }
    //std::cout << "status = " << status << "\n";
    //std::cout << "enter accept()\n";

    accept_fd = ::accept(m_conn.sockfd, client_addr, &addr_len);
    if (accept_fd == -1) {
        ec = make_system_error(errno);
        LOG_E("%s:%d %s\n", __FILE__, __LINE__, ec.message().c_str());
        EXIT();
        return;
    }
    //std::cout << "exit accept()\n";

    conn.sockfd = accept_fd;
    conn.ipv4 = m_conn.ipv4;
    EXIT();
}

void TcpServer::new_thread(Connection &&conn)
{
    ENTER();
    LOG_I("Creating of a new client thread...\n");
    log_connection(m_logger, static_cast<const Connection&>(conn));

    std::shared_ptr<TcpThread> newThreadPtr = std::make_shared<TcpThread>(
        [this](){this->operator()();},
        std::move(conn)
        );
    set_thread(newThreadPtr);

    LOG_I("A new thread has been created\n");
    EXIT();
}

void TcpServer::incoming_handler(
                        const Connection &conn,
                        tslogger::Logger &logger,
                        std::error_code &ec
                    )
{
    ssize_t received, sent;
    CharBuffer buffer;
    memset(buffer.data(), 0, buffer.size());
    received = ::recv(conn.sockfd, buffer.data(), buffer.size(), MSG_DONTWAIT);
    if (received > 0) {
        logger.log(INFO, "<-- %d bytes received\n", received);
        if (sent = ::send(conn.sockfd, buffer.data(), received, MSG_DONTWAIT) == -1) {
            ec = make_system_error(errno);
            logger.log(ERROR, "%s\n", ec.message().c_str());
            ec.clear();
        }
        else {
            logger.log(INFO, "--> %d bytes sent\n", received);
        }
    }
    else {
        ec = make_error_code(HttpStatus::HTTP_ERR_CLOSED_CONNECTION);
    }
}

void log_connection(tslogger::Logger &logger, const Connection &conn)
{
    logger.log(DEBUG, "-----------------------\n");
    logger.log(DEBUG, "ipv4: %d\n", conn.ipv4);
    if (conn.ipv4) {
        logger.log(DEBUG, "client.addr.sin_port: %d\n", conn.client.addr.sin_port);
        logger.log(DEBUG, "client.addr.sin_addr: %d\n", conn.client.addr.sin_addr);
    }
    else {
        //TODO
    }
    logger.log(DEBUG, "sockfd: %d\n", conn.sockfd);
    logger.log(DEBUG, "-----------------------\n");
}

}// namespace http
