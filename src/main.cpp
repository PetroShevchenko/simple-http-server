#include <iostream>
#include <chrono>
#include <logger.hpp>
#include <thread>
#include <signal.h>
#include "tcp_thread.hpp"
#include "http_server.hpp"

using namespace std;
using namespace tslogger;
using namespace http;

static HttpServer *serverPtr = nullptr;

static void stop_server(void *obj)
{
    if (obj)
        ((HttpServer *)obj)->stop();
}

static void signal_handler(int signo)
{
    if (signo == SIGINT)
    {
        cerr << "There was cought SIGINT\n";
        if (serverPtr == nullptr)
            exit(0);
        stop_server(serverPtr);
    }
}

int main()
{
    std::error_code ec;
    const char *root = "/home/petro/logs/";
    const char *logFileName = "simple-http-server.log";

    if (signal(SIGINT, signal_handler) == SIG_ERR)
    {
        std::cerr << "Unable to set SIGINT handler\n";
        exit(1);
    }

    Handler logHandler(root, DEBUG, std::clog, ec);
    if (ec.value())
    {
        std::cerr << ec.message() << "\n";
        exit(1);
    }

    std::jthread handlerThread([&](){
        while(true) {
            logHandler.process();
        }
    });
    handlerThread.detach();

    Logger logger(
        logHandler.get_queue_ptr(),
        logFileName,
        FLAGS_OUTPUT_TO_ALL
    );

    HttpServer server(
                    "/var/www/embedded.net.ua",
                    8080,
                    true,
                    10,
                    logHandler,
                    logFileName,
                    true,
                    ec
                );
    if (ec.value()) {
        logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, ec.message().c_str());
        exit(1);
    }

    serverPtr = &server;

    // this is a thread to join all client threads with closed connections
    std::jthread removerThread([&](){
        std::error_code ec;
        while(true) {
            server.thread_remover(ec);
            if (ec.value()) {
                logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, ec.message().c_str());
                ec.clear();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });
    removerThread.detach();

    while (server.is_running())
    {
        Connection conn;
        logger << "Main thread, run accept()\n";
        ec.clear();
        server.accept(conn, ec);
        if (ec.value()) {
            logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, ec.message().c_str());
            continue;
        }
        server.new_thread(std::move(conn));
    }

    logger << "Program terminated\n";
    exit(0);
}
