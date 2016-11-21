#ifndef 	LOG_LATENCY_H_
#define 	LOG_LATENCY_H_

#include <runnable.h>

class log_bench : public runnable {
 private:
        volatile uint64_t 	_txn_counter;
        volatile uint64_t 	*_log_tail;        
        uint64_t 		_txn_length;
        uint64_t 		_log_latency;
        
        void do_benchmark();
        void log();
        void transaction(uint64_t cycles);

 protected:
        virtual void start_working();
        virtual void init();

 public:
        log_bench(int cpu, volatile uint64_t *log_tail, uint64_t txn_length, 
                  uint64_t log_latency);
        uint64_t get_txn_count();
};

#endif 		// LOG_LATENCY_H_
