#include <runnable.h>
#include <util.h>
#include <cpuinfo.h>
#include <pthread.h>
#include <stdlib.h>

#define PRNG_BUFSZ 32

void runnable::rand_init()
{
        char *random_buf;
        
        _rand_state = (struct random_data*)malloc(sizeof(struct random_data));
        memset(_rand_state, 0x0, sizeof(struct random_data));
        random_buf = (char*)malloc(PRNG_BUFSZ);
        memset(random_buf, 0x0, PRNG_BUFSZ);
        initstate_r(random(), random_buf, PRNG_BUFSZ, _rand_state);
}

runnable::runnable(int cpu_number) {
        _start_signal = 0;
        _cpu_number = cpu_number;
        rand_init();
}

int runnable::gen_random()
{
        int ret;
        random_r(_rand_state, &ret);
        return ret;
}

void runnable::run() {
    assert(_start_signal == 0);
    
    // Kickstart the worker thread
    pthread_create(&_thread, NULL, bootstrap, this);
} 

void runnable::wait_init()
{
           while (!_start_signal)
                   ;
}

void* runnable::bootstrap(void *arg) {
        runnable *worker = (runnable*)arg;
    
        /* Pin the thread to a cpu */
        if (pin_thread((int)worker->_cpu_number) == -1) {
                std::cout << "Couldn't bind to a cpu!\n";
                exit(-1);
        }
    
        assert((uint32_t)worker->_thread != 0);    
        worker->init();
    
        /* Signal that we've initialized */
        fetch_and_increment(&worker->_start_signal);	
        
        /* Start the runnable thread */
        worker->start_working();
        return NULL;
}

uint64_t runnable::gen_guid()
{
        uint32_t temp;
        uint64_t ret;

        temp = _guid_counter;
        _guid_counter += 1;
        
        ret = (((uint64_t)temp) << 32) | _cpu_number;
        return ret;
}
