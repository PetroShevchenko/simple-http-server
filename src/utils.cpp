#include "utils.hpp"
#include <thread>
#include <unistd.h>

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
