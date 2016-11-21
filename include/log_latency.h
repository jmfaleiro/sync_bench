#ifndef 	LOG_LATENCY_H_
#define 	LOG_LATENCY_H_

#include <runnable.h>

class log_bench : public runnable {
 private:
        static volatile uint64_t 	s_log_tail;  
      
        volatile uint64_t 		_txn_counter;
        uint64_t 			_txn_length;
        uint64_t 			_log_latency;
        
        void do_thread_benchmark();
        void log();
        void transaction(uint64_t cycles);
        static uint64_t get_elapsed(log_bench **threads, int ncpus);

 protected:
        virtual void start_working();
        virtual void init();

 public:
        log_bench(int cpu, uint64_t txn_length, uint64_t log_latency);
        uint64_t get_txn_count();
        
        static double do_benchmark(uint64_t txn_length, uint64_t log_latency);
};

#endif 		// LOG_LATENCY_H_
