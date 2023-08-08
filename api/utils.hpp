#ifndef _UTILS_HPP
#define _UTILS_HPP
#include "tcp_server.hpp"
#include "http_error.hpp"
#include <logger.hpp>

unsigned long long get_total_system_memory();
unsigned int get_total_cpu_cores();
unsigned int get_max_threads(unsigned long maxBufLenPerThread);
size_t get_file_size(std::filesystem::path &filename, std::error_code &ec);
size_t compress_file(
			std::filesystem::path &filename,
			tslogger::Logger &logger,
			http::Buffer<char, http::MAX_BUFFER_SIZE> &out,
			std::error_code &ec
		);

#endif
