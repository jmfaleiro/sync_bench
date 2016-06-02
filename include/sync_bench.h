#ifndef 	SYNC_BENCH_H_
#define 	SYNC_BENCH_H_

#include <runnable.h>
#include <mcs.h>
#include <pthread.h>

#define ONE() 			single_work()
#define D_ONE()			ONE(); ONE()
#define DD_ONE()		D_ONE(); 		D_ONE()
#define DDD_ONE()		DD_ONE(); 		DD_ONE()
#define DDDD_ONE()		DDD_ONE(); 		DDD_ONE()
#define DDDDD_ONE() 		DDDD_ONE(); 		DDDD_ONE()
#define DDDDDD_ONE()		DDDDD_ONE(); 		DDDDD_ONE()
#define DDDDDDD_ONE()		DDDDDD_ONE(); 		DDDDDD_ONE()
#define DDDDDDDD_ONE()  	DDDDDDD_ONE(); 		DDDDDDD_ONE()
#define DDDDDDDDD_ONE()  	DDDDDDDD_ONE(); 	DDDDDDDD_ONE()
#define DDDDDDDDDD_ONE()  	DDDDDDDDD_ONE(); 	DDDDDDDDD_ONE()
#define DDDDDDDDDDD_ONE()  	DDDDDDDDDD_ONE(); 	DDDDDDDDDD_ONE()
#define DDDDDDDDDDDD_ONE()  	DDDDDDDDDDD_ONE(); 	DDDDDDDDDDD_ONE()
#define DDDDDDDDDDDDD_ONE()  	DDDDDDDDDDDD_ONE(); 	DDDDDDDDDDDD_ONE()
#define DDDDDDDDDDDDDD_ONE()  	DDDDDDDDDDDDD_ONE(); 	DDDDDDDDDDDDD_ONE()
#define DDDDDDDDDDDDDDD_ONE()  	DDDDDDDDDDDDDD_ONE(); 	DDDDDDDDDDDDDD_ONE()


namespace sync_bench {

        enum bench_t {
                SPINLOCK = 0,
                LATCH_FREE = 1,
                MCS_LOCK = 2,
                PTHREAD_LOCK = 3,
        };

        struct bench_args {
                bench_t 	_type;
                uint64_t 	_spin_outside;
                uint64_t 	_spin_inside;
                uint32_t 	_ncpus;
        };

        struct results {
                double 		_throughput;	/* Experiment throughput */
                uint64_t 	**_latency;	/* One array for each thread */
        };

        struct runnable_args {
                bench_args 		_bench_args;
                uint64_t 		*_count;
                pthread_mutex_t 	*_mutex;
                pthread_cond_t 		*_cond;
        };

        class bench_runnable : public runnable {
        private:
                /* Disable constructors */
                bench_runnable();
                bench_runnable(const bench_runnable&);

        protected:
                enum run_state_t {
                        IDLE,
                        SETUP,
                        READY,
                        EXEC,
                        QUIT,
                };

                bench_runnable(int cpu);
                static void 		*_location;
                static pthread_mutex_t	_mutex;
                static void do_spin(uint64_t duration);

                runnable_args 		_args;
                pthread_mutex_t 	_local_mutex;
                pthread_cond_t 		_local_cond;
                volatile uint64_t	_state;
                uint64_t 		*_latencies;
                volatile uint64_t	_iterations;
                
                virtual void do_critical_section() = 0;

                void start_working();
                void init();
                void do_iteration();
                void bench_iteration();
                void signal_runnable(uint64_t cmd);
                void signal_master();
                void setup_iteration();
                void critical_section();

        public:
                static bench_runnable** create_runnables(bench_args args, 
                                                         uint64_t *count, 
                                                         pthread_mutex_t *count_mutex,
                                                         pthread_cond_t *count_cond);

                void prepare(uint64_t iterations);
                void start();
                void kill();
                uint64_t* get_latency();                
        };

        class spinlock_runnable : public bench_runnable {
        private:
                volatile uint64_t 	*_location;

        protected:
                void do_critical_section();
                
        public:
                spinlock_runnable(int cpu, void *location);
        };

        class latchfree_runnable : public bench_runnable {
        private:
                volatile uint64_t 	*_location;
                uint64_t 		_read_value;
                
        protected:
                void do_critical_section();

        public:
                latchfree_runnable(int cpu, void *location);
        };

        class mcs_runnable : public bench_runnable {
        private:
                mcs_struct 	*_lock;
                
        protected:
                void do_critical_section();

        public:
                mcs_runnable(int cpu, void *location);
        };

        class pthread_runnable : public bench_runnable {
        private:
                pthread_mutex_t 	*_mutex;
                
        protected:
                void do_critical_section();

        public:
                pthread_runnable(int cpu, pthread_mutex_t *mutex);
        };

        class bench {
        private:        
                bench_args 		_args;
                bench_runnable 		**_runnables;
                //                uint64_t 		_value;
                uint64_t 		_count;
                pthread_mutex_t 	_count_mutex;
                pthread_cond_t 		_count_cond;
                
                void do_run(uint64_t iterations, uint64_t **latencies, double *throughput);
                void signal_runnables(uint64_t cmd);
                void wait_runnables();

        public:
                bench(bench_args args);
                results execute();        
        };
};

#endif 		// SYNC_BENCH_H_
