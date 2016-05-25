#ifndef                 RUNNABLE_HH_
#define                 RUNNABLE_HH_

#include <pthread.h>
#include <cassert>
#include <iostream>

class runnable {

private:
        void rand_init();

protected:
        volatile uint64_t 		_start_signal;
        int                             _cpu_number;
        pthread_t                       _thread;
        uint64_t                        _pthreadId;
        uint32_t 			_guid_counter;
        struct random_data 		*_rand_state;

        virtual void start_working() = 0;
        virtual void init() = 0;  
        static void* bootstrap(void *arg);        

public:    
        runnable(int cpu_number);
        void run();
        void wait_init();
        virtual int gen_random();
        virtual uint64_t gen_guid();
};

#endif          //  RUNNABLE_HH_
