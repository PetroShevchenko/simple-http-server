#ifndef _TCP_CONNECTION_HPP
#define _TCP_CONNECTION_HPP
#include <string_view>
#include <cstring>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>

namespace http
{

typedef union
{
    struct sockaddr_in addr;
    struct sockaddr_in6 addr6;
} sockaddr_t;


struct Connection {
    bool ipv4;
    union {
    sockaddr_t serv;
    sockaddr_t client;
    };
    int sockfd;
    Connection() = default;
    ~Connection() = default;

    Connection &operator=(const Connection& other)
    {
		if (&other == this)
			return *this;
		ipv4 = other.ipv4;
		sockfd = other.sockfd;
		if (ipv4) {
			memcpy(&client.addr, &other.client.addr, sizeof(client.addr));
		}
		else {
			memcpy(&client.addr6, &other.client.addr6, sizeof(client.addr6));
		}
		return *this;   	
    }

    Connection(const Connection& other)
    {
    	operator=(other);
    }

    Connection &operator=(Connection&& other)
    {
		if (&other == this)
			return *this;
		std::swap(ipv4, other.ipv4);
		std::swap(sockfd, other.sockfd);
		if (ipv4) {
			memcpy(&client.addr, &other.client.addr, sizeof(client.addr));
			memset(&other.client.addr, 0, sizeof(client.addr));
		}
		else {
			memcpy(&client.addr6, &other.client.addr6, sizeof(client.addr6));
			memset(&other.client.addr6, 0, sizeof(client.addr6));
		}
		return *this;
    }
    Connection(Connection && other)
    {
    	operator=(std::move(other));
    } 
};

}// namespace http

#endif
