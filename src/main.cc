#include <sync_bench.h>
#include <iostream>
#include <cassert>
#include <fstream>

void write_results(sync_bench::bench_t type, uint32_t nthreads, double throughput)
{
        std::ofstream result_file;
        
        if (type == sync_bench::SPINLOCK) {
                result_file.open("spinlock.txt", std::ios::app | std::ios::out);
        } else if (type == sync_bench::LATCH_FREE) {
                result_file.open("latchfree.txt", std::ios::app | std::ios::out);
        } else if (type == sync_bench::MCS_LOCK) {
                result_file.open("mcs.txt", std::ios::app | std::ios::out);
        } else if (type == sync_bench::PTHREAD_LOCK) {
                result_file.open("pthread.txt", std::ios::app | std::ios::out);
        } else {
                assert(false);
        }
        
        result_file << "threads:" << nthreads << " ";
        result_file << "throughput:" << throughput << " ";
        result_file << "\n";
        result_file.close();
}

/* 
 * For now, arg1 is the experiment type, arg2 is #threads, arg3 is 
 * spin_inside, and arg4 is spin_outside.  
 */   
int main(int argc, char **argv)
{
        assert(argc == 5);

        sync_bench::bench_args args;
        sync_bench::results result;
        sync_bench::bench *benchmark;
        
        args._type = (sync_bench::bench_t)atoi(argv[1]);
        assert(args._type >= sync_bench::SPINLOCK && 
               args._type <= sync_bench::PTHREAD_LOCK);
        
        args._ncpus = (uint32_t)atoi(argv[2]);        
        args._spin_inside = (uint64_t)atoi(argv[3]);
        args._spin_outside = (uint64_t)atoi(argv[4]);
        
        benchmark = new sync_bench::bench(args);
        result = benchmark->execute();
        std::cout << "Throughput: " << result._throughput << "\n";
        write_results(args._type, args._ncpus, result._throughput);
        return 0;
}
