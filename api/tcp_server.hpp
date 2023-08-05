#ifndef _TCP_SERVER_HPP
#define _TCP_SERVER_HPP
#include <map>
#include <mutex>
#include <atomic>
#include <memory>
#include <logger.hpp>
#include "http_error.hpp"
#include "tcp_connection.hpp"
#include "tcp_thread.hpp"

namespace http
{

enum {
	MAX_BUFFER_SIZE = 10485760, // 10 Mb
};

template <typename T, std::size_t N>
class Buffer {
public:
	Buffer()
	: m_offset{0}
	{
		m_data = new T [N];
		m_size = N;
	}
	~Buffer()
	{
		delete [] m_data;
	}
	Buffer(const Buffer&) = delete;
	Buffer(Buffer&&) = delete;
	Buffer &operator=(const Buffer&) = delete;
	Buffer &operator=(Buffer&&) = delete;

public:
	T *data()
	{
		return m_data;
	}

	const std::size_t size() const
	{
		return static_cast<const std::size_t>(m_size);
	}

	void size(const std::size_t value)
	{
		m_size = value;
	}

	const std::size_t offset() const
	{
		return static_cast<const std::size_t>(m_offset);
	}

	bool offset(const std::size_t value)
	{
		m_offset = value > m_size ? m_offset : value;
		return value <= m_size; 
	}

private:
	T *m_data;
	std::size_t m_size;
	std::size_t m_offset;	
};

typedef Buffer<char, MAX_BUFFER_SIZE> CharBuffer;

class TcpServer {
public:
	TcpServer(
			int port,
			bool ipv4,
			const unsigned int maxClients,
			tslogger::Handler &handler,
			const char *logFileName,
			bool logToStdout,
			std::error_code &ec
			);
	virtual ~TcpServer();

	TcpServer(const TcpServer&) = delete;
	TcpServer(TcpServer &&) = delete;
	TcpServer &operator=(const TcpServer &) = delete;
	TcpServer &operator=(TcpServer &&) = delete;

	void operator()();

	void accept(Connection &conn, std::error_code &ec);
	void new_thread(Connection &&conn);

	bool is_running() const
	{
		return m_running;
	}

	void start()
	{
		m_running = true;
	}

	void stop()
	{
		m_running = false;	
	}

	void get_connection(Connection &out, std::thread::id _id, std::error_code &ec)
	{
		std::lock_guard lg(m_mutex);
		std::map<std::thread::id, std::shared_ptr<TcpThread>>::iterator iter;
		iter = m_threads.find(_id);
		if (iter == m_threads.end()) {
			ec = make_error_code(HttpStatus::HTTP_ERR_THREAD_NOT_FOUND);
		}
		out =  iter->second.get()->conn();
	}

	void remove_thread(std::thread::id _id, std::error_code &ec)
	{
		std::lock_guard lg(m_mutex);
		std::map<std::thread::id, std::shared_ptr<TcpThread>>::iterator iter;
		iter = m_threads.find(_id);
		if (iter == m_threads.end()) {
			ec = make_error_code(HttpStatus::HTTP_ERR_THREAD_NOT_FOUND);
			return;
		}
		m_threads.erase(iter->first);
	}

	void set_thread(std::shared_ptr<TcpThread> threadPtr)
	{
		std::lock_guard lg(m_mutex);
  		m_threads.insert(std::pair<std::thread::id, std::shared_ptr<TcpThread>>(threadPtr.get()->get_id(),threadPtr));

		for(std::map<std::thread::id, std::shared_ptr<TcpThread>>::const_iterator it = m_threads.begin();
		    it != m_threads.end(); ++it)
		{
		    std::cout << it->first << " : " << it->second.get()->conn().sockfd << "\n";
		}
	}

protected:
	virtual void incoming_handler(
						const Connection &conn,
						tslogger::Logger &logger,
						std::error_code &ec
					);
protected:
	#define LOG_D(...) LOG(m_logger, tslogger::DEBUG, __VA_ARGS__)
	#define LOG_I(...) LOG(m_logger, tslogger::INFO, __VA_ARGS__)
	#define LOG_W(...) LOG(m_logger, tslogger::WARNING, __VA_ARGS__)
	#define LOG_E(...) LOG(m_logger, tslogger::ERROR, __VA_ARGS__)
	#define ENTER() ENTER_LOG(m_logger, tslogger::DEBUG)
	#define EXIT() EXIT_LOG(m_logger, tslogger::DEBUG)
	#define CODE_LINE(msg) LOG_D("> %s:%d %s: %s\n",  __FILE__,  __LINE__, __func__, msg)

private:
	std::atomic<bool> m_running;
	int m_port;
	unsigned int m_maxClients;
	Connection m_conn;
	std::map<std::thread::id, std::shared_ptr<TcpThread>> m_threads;
	std::mutex m_mutex;
	tslogger::Logger m_logger;
};

void log_connection(tslogger::Logger &logger, const Connection &conn);

}// namespace http


#endif
