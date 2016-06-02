#include <util.h>
#include <sync_bench.h>
#include <sys/time.h>

using namespace sync_bench;

/* Initialize static fields */
pthread_mutex_t bench_runnable::_mutex = PTHREAD_MUTEX_INITIALIZER;
void* bench_runnable::_location = NULL;

static timespec diff_time(timespec end, timespec start)
{
        timespec temp;
        if ((end.tv_nsec - start.tv_nsec) < 0) {
                temp.tv_sec = end.tv_sec - start.tv_sec - 1;
                temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
        } else {
                temp.tv_sec = end.tv_sec-start.tv_sec;
                temp.tv_nsec = end.tv_nsec-start.tv_nsec;
        }
        return temp;
}

bench::bench(bench_args args)
{
        _args = args;
        _count = 0;
        _count_mutex = PTHREAD_MUTEX_INITIALIZER;
        _count_cond = PTHREAD_COND_INITIALIZER;
        _runnables = bench_runnable::create_runnables(args, 
                                                      &_count, 
                                                      &_count_mutex, 
                                                      &_count_cond);
}

void bench::wait_runnables()
{
        pthread_mutex_lock(&_count_mutex);
        while (_count != _args._ncpus)
                pthread_cond_wait(&_count_cond, &_count_mutex);
        pthread_mutex_unlock(&_count_mutex);
}

void bench::do_run(uint64_t iterations, uint64_t **latencies, double *throughput)
{
        uint32_t i;
        timespec start_time, end_time;
        double elapsed_milli;
        
        _count = 0;
        for (i = 0; i < _args._ncpus; ++i) 
                _runnables[i]->prepare(iterations);        
        wait_runnables();

        barrier();
        clock_gettime(CLOCK_REALTIME, &start_time);
        barrier();

        _count = 0;
        for (i = 0; i < _args._ncpus; ++i)
                _runnables[i]->start();
        wait_runnables();

        barrier();
        clock_gettime(CLOCK_REALTIME, &end_time);
        barrier();

        for (i = 0; i < _args._ncpus; ++i)
                latencies[i] = _runnables[i]->get_latency();
        
        end_time = diff_time(end_time, start_time);
        elapsed_milli = end_time.tv_sec + (1.0*end_time.tv_nsec)/1000000000.0;
        std::cout << elapsed_milli << "\n";
        *throughput = (1.0*iterations*_args._ncpus)/elapsed_milli;
}

results bench::execute()
{
        uint64_t **latencies;
        double throughput;
        results ret;

        latencies = (uint64_t**)malloc(sizeof(uint64_t*)*_args._ncpus);
        memset(latencies, 0x0, sizeof(uint64_t*)*_args._ncpus);
        
        if (_args._ncpus > 80 && _args._type == MCS_LOCK) {
                do_run(1000, latencies, &throughput);
        } else {        
                do_run(1000, latencies, &throughput);
                do_run(100000, latencies, &throughput);
        }
        ret._throughput = throughput;
        ret._latency = latencies;
        return ret;
}

void bench_runnable::start_working()
{
        while (true) {                
                /* Wait for a command from the master thread */
                pthread_mutex_lock(&_local_mutex);
                while (_state == IDLE)
                        pthread_cond_wait(&_local_cond, &_local_mutex);
                pthread_mutex_unlock(&_local_mutex);
                
                /* The only legal commands at this point are QUIT and SETUP */
                if (_state == QUIT)
                        break;
                else if (_state == SETUP)
                        do_iteration();
                else
                        assert(false);
        }
}

void bench_runnable::init()
{
}

bench_runnable::bench_runnable(int cpu) : runnable(cpu)
{
}

void bench_runnable::critical_section()
{
        ONE();
        //        uint32_t i;
        //        for (i = 0; i < _args._bench_args._spin_inside; ++i) 
        //                single_work();
}

bench_runnable** bench_runnable::create_runnables(bench_args args, 
                                                  uint64_t *count, 
                                                  pthread_mutex_t *count_mutex,
                                                  pthread_cond_t *count_cond)
{
        uint32_t i;
        bench_runnable **ret;
        runnable_args run_args;
        int total_cpus;

        /* Every runnable has the same argument */
        run_args._bench_args = args;
        run_args._count = count;
        run_args._mutex = count_mutex;
        run_args._cond = count_cond;

        ret = (bench_runnable**)malloc(sizeof(bench_runnable*)*args._ncpus);
        memset(ret, 0x0, sizeof(bench_runnable*)*args._ncpus);
        bench_runnable::_location = malloc(4*CACHE_LINE);
        memset(bench_runnable::_location, 0x0, 4*CACHE_LINE);
        assert(*((uint64_t*)bench_runnable::_location) == 0);
        bench_runnable::_mutex = PTHREAD_MUTEX_INITIALIZER;
        
        total_cpus = numa_num_configured_cpus();

        for (i = 0; i < args._ncpus; ++i) {
                switch (args._type) {
                case SPINLOCK:
                        ret[i] = new spinlock_runnable(i % total_cpus, 
                                                       bench_runnable::_location);
                        break;
                case LATCH_FREE:
                        ret[i] = new latchfree_runnable(i % total_cpus, 
                                                        bench_runnable::_location);
                        break;
                case MCS_LOCK:
                        ret[i]  = new mcs_runnable(i % total_cpus, 
                                                   bench_runnable::_location);
                        break;
                case PTHREAD_LOCK:
                        ret[i] = new pthread_runnable(i % total_cpus, 
                                                      &bench_runnable::_mutex);
                        break;
                default:
                        assert(false);
                };                
                
                ret[i]->_args = run_args;
                ret[i]->_local_mutex = PTHREAD_MUTEX_INITIALIZER;
                ret[i]->_local_cond = PTHREAD_COND_INITIALIZER;
                ret[i]->_state = IDLE;
                ret[i]->_latencies = NULL;
                ret[i]->_iterations = 0;
                ret[i]->run();
        }
        return ret;
}

/* Runs on the master thread */
void bench_runnable::prepare(uint64_t iterations)
{
        _iterations = iterations;
        signal_runnable(SETUP);
}

void bench_runnable::start()
{
        signal_runnable(EXEC);
}

void bench_runnable::kill()
{
        signal_runnable(QUIT);
}

void bench_runnable::signal_master()
{
        pthread_mutex_lock(_args._mutex);
        assert(*_args._count < _args._bench_args._ncpus);
        *_args._count += 1;
        if (*_args._count == _args._bench_args._ncpus)
                pthread_cond_signal(_args._cond);
        pthread_mutex_unlock(_args._mutex);
}

void bench_runnable::signal_runnable(uint64_t cmd)
{
        pthread_mutex_lock(&_local_mutex);
        _state = cmd;
        pthread_cond_signal(&_local_cond);
        pthread_mutex_unlock(&_local_mutex);
}

/* Runs on a worker thread */
void bench_runnable::do_iteration()
{
        //        uint64_t state;

        /* Setup state to run the iteration */
        setup_iteration();
        signal_master();

        /* Wait to be signaled to run */
        pthread_mutex_lock(&_local_mutex);
        while (_state == READY)
                pthread_cond_wait(&_local_cond, &_local_mutex);
        pthread_mutex_unlock(&_local_mutex);
        
        /* Do the benchmark */
        bench_iteration();
        signal_master();
}

void bench_runnable::do_spin(__attribute__((unused)) uint64_t duration)
{
        DDDDDDDDDD_ONE();
        //        uint64_t i;
        //        for (i = 0; i < duration; ++i) 
        //                single_work();
}

/* Runs on the master thread. Signals worker threads to start */
void bench_runnable::setup_iteration()
{
        assert(_state == SETUP);
        _latencies = (uint64_t*)malloc(sizeof(uint64_t)*_iterations);
        memset(_latencies, 0x0, sizeof(uint64_t)*_iterations);
        _state = READY;
}

/* Runs on the worker thread. Do a benchmark run */
void bench_runnable::bench_iteration()
{
        assert(_state == EXEC);
        uint64_t i, time_start, time_end;

        for (i = 0; i < _iterations; ++i) {
                do_spin(_args._bench_args._spin_outside);

                barrier();
                time_start = rdtsc();
                barrier();
                
                do_critical_section();
                //                std::cout << "Done!\n";
                barrier();
                time_end = rdtsc();
                barrier();
                _latencies[i] = time_end - time_start;
        }
        xchgq(&_state, IDLE);
}

uint64_t* bench_runnable::get_latency()
{
        uint64_t *ret;
        
        pthread_mutex_lock(&_local_mutex);
        assert(_state == IDLE);
        ret = _latencies;
        pthread_mutex_unlock(&_local_mutex);
        return ret;
}

spinlock_runnable::spinlock_runnable(int cpu, void *location) 
        : bench_runnable(cpu)
{
        _location = (volatile uint64_t*)location;
        assert(*_location == 0);
}

void spinlock_runnable::do_critical_section()
{
        lock(_location);
        critical_section();
        unlock(_location);
}

latchfree_runnable::latchfree_runnable(int cpu, void *location)
        : bench_runnable(cpu)
{
        _location = (volatile uint64_t*)location;
}

void latchfree_runnable::do_critical_section()
{
        uint64_t begin_ctr;
        
        while (true) {
                barrier();
                begin_ctr = *_location;
                barrier();
                
                critical_section();
                
                if (cmp_and_swap(_location, begin_ctr, begin_ctr+1))
                        break;
        }
}

pthread_runnable::pthread_runnable(int cpu, pthread_mutex_t *mutex)
        : bench_runnable(cpu)
{
        _mutex = mutex;
}

void pthread_runnable::do_critical_section()
{
        pthread_mutex_lock(_mutex);
        critical_section();
        pthread_mutex_unlock(_mutex);
}

mcs_runnable::mcs_runnable(int cpu, void *location)
        : bench_runnable(cpu)
{
        _lock = (mcs_struct*)alloc_mem(sizeof(mcs_struct), cpu);
        _lock->_is_held = false;
        _lock->_next = NULL;
        _lock->_tail_ptr = (volatile mcs_struct**)location;
}

void mcs_runnable::do_critical_section()
{
        mcs_mgr::lock(_lock);
        critical_section();
        mcs_mgr::unlock(_lock);
}
