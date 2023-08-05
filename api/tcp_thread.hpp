#ifndef _THREAD_HPP
#define _THREAD_HPP
#include <thread>
#include <sys/socket.h>
#include <iostream>
#include "tcp_connection.hpp"

namespace http
{

class TcpThread : public std::jthread {
public:

	template <class Function>
	TcpThread(Function&& f, Connection &&conn)://explicit 
		std::jthread(f),
		m_conn{std::move(conn)}
	{
		std::cout << "TcpThread()\n";
	}

	virtual ~TcpThread()
	{
		std::cout << "~TcpThread()\n";
		if (m_conn.sockfd > 0)
			::close(m_conn.sockfd);
	}

	TcpThread &operator=(const TcpThread &other)
	{
		if (&other == this)
			return *this;
		m_conn = other.m_conn;
		return *this;
	}

	TcpThread &operator=(TcpThread &&other)
	{
		if (&other == this)
			return *this;
		m_conn = std::move(other.m_conn);
		return *this;
	}

	TcpThread(const TcpThread& other)
	{
		operator=(other);
	}

	TcpThread(TcpThread &&other)
	{
		operator=(std::move(other));
	}

	const Connection &conn() const
	{
		return static_cast<const Connection&>(m_conn);
	}

	Connection &conn()
	{
		return m_conn;
	}

private:
	Connection m_conn;
};


} // namespace http

#endif
