#include <util.h>
#include <sync_bench.h>
#include <sys/time.h>
#include <unistd.h>

using namespace sync_bench;

extern bool LATENCY_EXP;

/* Initialize static fields */
pthread_mutex_t bench_runnable::_mutex = PTHREAD_MUTEX_INITIALIZER;
void* bench_runnable::_location = NULL;
uint64_t bench_runnable::_fairness_counter = 0;

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

uint64_t bench::count_exec()
{
        uint32_t i;
        uint64_t exec;
        
        exec = 0;
        for (i = 0; i < _args._ncpus; ++i) 
                exec += _runnables[i]->get_exec();
        return exec;
}

void bench::do_run(uint64_t iterations, latency_result *latencies,
                   fairness_result *fairness,
                   double *throughput)
{
        uint64_t num_exec;
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
        //        wait_runnables();
        sleep(30);
        
        num_exec = count_exec();

        barrier();
        clock_gettime(CLOCK_REALTIME, &end_time);
        barrier();

        for (i = 0; i < _args._ncpus; ++i) {
                latencies[i] = _runnables[i]->get_latency();
                fairness[i] = _runnables[i]->get_fairness();
        }
        
        end_time = diff_time(end_time, start_time);
        elapsed_milli = end_time.tv_sec + (1.0*end_time.tv_nsec)/1000000000.0;
        *throughput = (1.0*num_exec)/elapsed_milli;
}

results bench::execute()
{
        double throughput;
        results ret;
        latency_result *latency_res;
        fairness_result *fairness_res;

        latency_res = (latency_result*)zmalloc(sizeof(latency_result)*_args._ncpus);
        fairness_res = (fairness_result*)zmalloc(sizeof(fairness_result)*_args._ncpus);
                
        //        memset(latency_res, 0x0, sizeof(latency_result)*_args._ncpus);
        
        if (_args._ncpus > 80 && _args._type == MCS_LOCK) {
                do_run(1000, latency_res, fairness_res, &throughput);
        } else {        
                //                do_run(1000, latencies, &throughput);
                do_run(100000, latency_res, fairness_res, &throughput);
        }
        ret._throughput = throughput;
        ret._latencies = latency_res;
        ret._fairness = fairness_res;
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
        size_t alloc_sz;
        
        _cpu = cpu;
        alloc_sz = sizeof(uint64_t)*(((uint64_t)1) << 26);
        _owned_slots = (uint64_t*)alloc_mem(alloc_sz, cpu);
        memset(_owned_slots, 0x0, alloc_sz);
}

void bench_runnable::critical_section()
{
        //        DDDDDDDDDD_ONE();

        //        DDDDDDDDD_ONE();
        uint32_t i;
        for (i = 0; i < _args._bench_args._spin_inside; ++i) 
                single_work();
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
                
                //                ret[i]->_cpu = i;
                //                assert(ret[i]->_cpu == i);
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

uint64_t bench_runnable::get_exec()
{
        uint64_t ret;
        
        barrier();
        ret = _iterations;
        barrier();
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

         uint64_t i;
 
         for (i = 0; i < duration; ++i) 
                 single_work();
}

/* Runs on the master thread. Signals worker threads to start */
void bench_runnable::setup_iteration()
{
        assert(_state == SETUP);
        _sz = (1<<20);
        _latencies = (uint64_t*)malloc(sizeof(uint64_t)*_sz);
        memset(_latencies, 0x0, sizeof(uint64_t)*_sz);
        _state = READY;
}

/* Runs on the worker thread. Do a benchmark run */
void bench_runnable::bench_iteration()
{
        assert(_state == EXEC);
        uint64_t i;
        volatile uint64_t time_start, time_end;
        _iterations = 0;
        i = 0;
        while (true) {

                //                do_spin(_args._bench_args._spin_outside);
                
                if (LATENCY_EXP) {
                        barrier();
                        time_start = rdtsc();
                        barrier();
                }

                // do_critical_section();
                _owned_slots[_iterations] = do_critical_section();
                
                if (LATENCY_EXP) {
                        barrier();
                        time_end = rdtsc();
                        barrier();

                        _latencies[i % _sz] = time_end - time_start;
                        i += 1;              
                }

                fetch_and_increment(&_iterations);
        }
        xchgq(&_state, IDLE);
}

fairness_result bench_runnable::get_fairness()
{
        fairness_result ret;
        ret._vals = _owned_slots;
        ret._iters = _iterations;
        return ret;
}

latency_result bench_runnable::get_latency()
{
        latency_result ret;
        
        ret._latency = _latencies;
        ret._iters = _iterations;
        return ret;
        //        pthread_mutex_lock(&_local_mutex);
        //        assert(_state == IDLE);
        //        ret = _latencies;
        //        pthread_mutex_unlock(&_local_mutex);
        //        return ret;
}

spinlock_runnable::spinlock_runnable(int cpu, void *location) 
        : bench_runnable(cpu)
{
        _location = (volatile uint64_t*)location;
        assert(*_location == 0);
}

uint32_t spinlock_runnable::do_critical_section()
{
        lock(_location);
        critical_section();
        unlock(_location);
        return 0;
}

latchfree_runnable::latchfree_runnable(int cpu, void *location)
        : bench_runnable(cpu)
{
        _location = (volatile uint64_t*)location;
        
}

uint32_t latchfree_runnable::do_critical_section()
{
        volatile uint64_t begin_ctr, end_ctr;
        
        while (true) {
                barrier();
                begin_ctr = *_location;
                barrier();
                
                critical_section();
                
                barrier();
                end_ctr = *_location;
                barrier();
                if (begin_ctr == end_ctr && cmp_and_swap(_location, begin_ctr, begin_ctr+1)) {
                        break;
                }                 
        }
        return 0;
}

pthread_runnable::pthread_runnable(int cpu, pthread_mutex_t *mutex)
        : bench_runnable(cpu)
{
        _mutex = mutex;
}

uint32_t pthread_runnable::do_critical_section()
{
        pthread_mutex_lock(_mutex);
        critical_section();
        pthread_mutex_unlock(_mutex);
        
        return 0;
}

mcs_runnable::mcs_runnable(int cpu, void *location)
        : bench_runnable(cpu)
{
        _lock = (mcs_struct*)alloc_mem(sizeof(mcs_struct), cpu);
        _lock->_is_held = false;
        _lock->_next = NULL;
        _lock->_tail_ptr = (volatile mcs_struct**)location;
}

uint32_t mcs_runnable::do_critical_section()
{
        uint32_t ret;
        
        mcs_mgr::lock(_lock);
        ret = _fairness_counter++;
        //        critical_section();
        mcs_mgr::unlock(_lock);
        return ret;
}
