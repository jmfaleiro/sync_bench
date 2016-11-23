#include <log_latency.h>
#include <util.h>
#include <unistd.h>

volatile uint64_t log_bench::s_log_tail = 0;

uint64_t log_bench::get_elapsed(log_bench **threads, int ncpus)
{
        int i;
        uint64_t elapsed = 0;

        for (i = 0; i < ncpus; ++i) 
                elapsed += threads[i]->get_txn_count();
        return elapsed;
}

double log_bench::do_benchmark(uint64_t txn_length, uint64_t log_latency)
{
        int ncpus = 80, i;
        log_bench *threads[ncpus];
        uint64_t prev_elapsed, cur_elapsed;
        double throughput;

        /* Setup threads */
        for (i = 0; i < ncpus; ++i) {
                threads[i] = new log_bench(i, txn_length, log_latency);
                threads[i]->run();
        }        
        
        /* Run for 5 seconds */
        barrier();
        sleep(5);
        barrier();
        prev_elapsed = get_elapsed(threads, ncpus);

        /* Run for 10 seconds */
        barrier();
        sleep(10);
        barrier();
        cur_elapsed = get_elapsed(threads, ncpus);
        
        throughput = (double)(cur_elapsed - prev_elapsed) / 10.0;
        return throughput;
}

log_bench::log_bench(int cpu, uint64_t txn_length, 
                     uint64_t log_latency) 
        : runnable(cpu)
{
        _txn_counter = 0;
        _txn_length = txn_length;
        _log_latency = log_latency;
        _cpu = cpu;
}

/* Simulate a transaction which executes for "cycles" */
void log_bench::transaction(uint64_t cycles)
{
        uint64_t i;
        for (i = 0; i < cycles; ++i) 
                single_work();
}

/* Simulate logging by incrementing a counter */
void log_bench::log()
{
        fetch_and_increment(&s_log_tail);
}

void log_bench::do_thread_benchmark()
{
        uint64_t prev_log, cur_time;
        prev_log = rdtsc();
        while (true) {
                transaction(_txn_length);
                cur_time = rdtsc();
                if (cur_time - prev_log > _log_latency) {
                        log();
                        prev_log = cur_time;
                }
                fetch_and_increment(&_txn_counter);
        }
}

uint64_t log_bench::get_txn_count()
{
        return _txn_counter;
}

void log_bench::init()
{
}

void log_bench::start_working()
{
        do_thread_benchmark();
}
