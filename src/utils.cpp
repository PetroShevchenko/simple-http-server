#include "utils.hpp"
#include "http_error.hpp"
#include "tcp_server.hpp"
#include <thread>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <zlib.h>

using namespace http;
using namespace tslogger;

unsigned long long get_total_system_memory()
{
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

unsigned int get_total_cpu_cores()
{
    return std::thread::hardware_concurrency();
}


static unsigned int get_max_threads_by_cpu_cores()
{
    unsigned int cores = get_total_cpu_cores();
    return cores ? 2 * cores : 10;
}

static unsigned int get_max_threads_by_memory_limit(unsigned long maxBufLen)
{
    return static_cast<unsigned int>(get_total_system_memory() / static_cast<unsigned long long>(maxBufLen));
}

unsigned int get_max_threads(unsigned long maxBufLenPerThread)
{
    unsigned int maxThreadsByCpuCores = get_max_threads_by_cpu_cores();
    unsigned int maxThreadsByMemoryLimit = get_max_threads_by_memory_limit(maxBufLenPerThread);
    return maxThreadsByCpuCores < maxThreadsByMemoryLimit ? maxThreadsByCpuCores : maxThreadsByMemoryLimit;
}

size_t get_file_size(std::filesystem::path &filename, std::error_code &ec)
{
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        ec = make_error_code(HttpStatus::HTTP_ERR_FILE_NOT_FOUND);
        return 0;
    }

    ifs.seekg(0, std::ios::end);
    size_t contentSize = ifs.tellg();
    ifs.seekg(0);
    ifs.close();
    return contentSize;
}

size_t compress_file(
            std::filesystem::path &filename,
            tslogger::Logger &logger,
            http::Buffer<char, http::MAX_BUFFER_SIZE> &out,
            std::error_code &ec
        )
{
    size_t chunkSize = 4096;
    const int compressionLevel = 9;
    size_t total = 0;
    z_stream stream = {0};
    if (deflateInit(&stream, compressionLevel) != Z_OK)
    {
        ec = make_error_code(HttpStatus::HTTP_ERR_INTERNAL_SERVER_ERROR);
        logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, ec.message().c_str());
        return total;
    }

    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        ec = make_error_code(HttpStatus::HTTP_ERR_FILE_NOT_FOUND);
        deflateEnd(&stream);
        logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, ec.message().c_str());
        return total;
    }
    ifs.seekg(0, std::ios::end);
    size_t contentSize = ifs.tellg();
    ifs.seekg(0);

    if (contentSize < chunkSize) {
        chunkSize = contentSize;
    }

    char *inbuff = new char [chunkSize];
    char *outbuff = new char [chunkSize];

    if (out.offset() + contentSize > out.size()) {
        ec = make_error_code(HttpStatus::HTTP_ERR_INTERNAL_SERVER_ERROR);
        logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, ec.message().c_str());
        goto error_exit;
    }

    int flush;
    do {
        if (!ifs.read(inbuff, chunkSize) && !ifs.eof()) {
            ec = make_error_code(HttpStatus::HTTP_ERR_INTERNAL_SERVER_ERROR);
            logger.log(ERROR, "%s:%d %s\n", __FILE__, __LINE__, ec.message().c_str());
            goto error_exit;
        }
        stream.avail_in = chunkSize;
        flush = ifs.eof() ? Z_FINISH : Z_NO_FLUSH;
        stream.next_in = reinterpret_cast<uint8_t *>(inbuff);

        do {
            stream.avail_out = chunkSize;
            stream.next_out = reinterpret_cast<uint8_t *>(outbuff);
            deflate(&stream, flush);

            size_t compressed = chunkSize - stream.avail_out;
            total += compressed;

            memcpy(out.data() + out.offset(), outbuff, compressed);
            out.offset(out.offset() + compressed);
        } while (stream.avail_out == 0);

    } while (flush != Z_FINISH);

error_exit:
    deflateEnd(&stream);
    ifs.close();
    delete [] inbuff;
    delete [] outbuff;
    return total;
}
