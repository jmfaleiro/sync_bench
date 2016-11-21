#include <log_latency.h>

log_bench::log_bench(int cpu, volatile uint64_t *log_tail, uint64_t txn_length, 
                     uint64_t log_latency) 
        : runnable(cpu)
{
        _txn_counter = 0;
        _log_tail = log_tail;
        _txn_length = txn_length;
        _log_latency = log_latency;
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
        fetch_and_increment(_log_tail);
}

void log_bench::do_benchmark()
{
        uint64_t prev_log, cur_time;
        prev_log = rdtsc();
        while (true) {
                transaction(_cycles);
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
        do_benchmark();
}
