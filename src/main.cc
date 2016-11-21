#include <sync_bench.h>
#include <iostream>
#include <cassert>
#include <fstream>
#include <algorithm>
#include <vector>
#include <log_latency.h>

bool LATENCY_EXP;

void write_latency(sync_bench::latency_result latency, std::ofstream &result_file)
{
        double diff, cur;
        uint64_t i;

        diff = 1.0 / ((double)latency._iters);
        cur = 0.0;
        for (i = 0; i < latency._iters; ++i) {
                result_file << cur << " " << latency._latency[i] << "\n";
                cur += diff;
        }
}

std::vector<uint64_t>* avg_sequence(std::vector<uint64_t> *values)
{
        uint64_t i, cur_len;
        std::vector<uint64_t> *seq_lens;

        seq_lens = new std::vector<uint64_t>();
        cur_len = 0;
        
        for (i = 1; i < values->size(); ++i) {
                assert((*values)[i] >= (*values)[i-1]);
                if ((*values)[i] - (*values)[i-1] != 1) {
                        seq_lens->push_back(cur_len);
                        cur_len = 0;
                } else {
                        cur_len += 1;
                }
        }
        
        return seq_lens;
}

std::vector<uint64_t>** fairness_by_numa(sync_bench::fairness_result *res, uint32_t nthreads,
                                    uint32_t *ngroups)
{
        assert(nthreads == 80);
        
        uint32_t i, j, nnuma, numa_threads, numa_node;
        std::vector<uint64_t> **ret;

        nnuma = 8;
        numa_threads = 10;
        *ngroups = nnuma;

        ret = (std::vector<uint64_t>**)zmalloc(sizeof(std::vector<uint64_t>*)*nnuma);
        for (i = 0; i < nnuma; ++i) 
                ret[i] = new std::vector<uint64_t>();
        
        for (i = 0; i < nthreads; ++i) {
                numa_node = i / numa_threads;
                for (j = 0; j < res[i]._iters; ++j) {
                        ret[numa_node]->push_back(res[i]._vals[j]);
                }
        }

        for (i = 0; i < nnuma; ++i)
                std::sort(ret[i]->begin(), ret[i]->end());
        
        return ret;
}

void write_fairness_cdf(std::vector<uint64_t> &results, const char *filename)
{
        std::ofstream result_file;
        double diff, cur;
        uint32_t i;
        
        result_file.open(filename, std::ios::trunc | std::ios::out);
        std::sort(results.begin(), results.end());        
        diff = 1.0 / double(results.size());
        cur = 0.0;
        for (i = 0; i < results.size(); ++i) {
                result_file << cur << " " << results[i] << "\n";
                cur += diff;
        }
        result_file.close();
}

void do_fairness(sync_bench::fairness_result *res, uint32_t nthreads)
{
        std::vector<uint64_t> **grouped_numa, **seq, to_write;
        uint32_t i, j, ngroups;
        
        grouped_numa = fairness_by_numa(res, nthreads, &ngroups);
        seq = (std::vector<uint64_t>**)zmalloc(sizeof(std::vector<uint64_t>*)*(ngroups));

        for (i = 0; i < ngroups; ++i) {
                seq[i] = avg_sequence(grouped_numa[i]);
                delete(grouped_numa[i]);
        }
        free(grouped_numa);
        
        for (i = 0; i < ngroups; ++i) {
                for (j = 0; j < seq[i]->size(); ++j) {
                        to_write.push_back((*seq[i])[j]);
                }
                delete(seq[i]);
        }
        free(seq);

        write_fairness_cdf(to_write, "fairness_cdf.txt");
}

void write_results(sync_bench::bench_t type, uint32_t nthreads,
                   uint32_t serial_section,
                   uint32_t parallel_section,
                   double throughput, 
                   __attribute__((unused)) sync_bench::latency_result latency)
{
        std::ofstream result_file;
        
        if (type == sync_bench::SPINLOCK) {
                result_file.open("spinlock.txt", std::ios::app | std::ios::out);
                result_file << "spinlock ";
        } else if (type == sync_bench::LATCH_FREE) {
                result_file.open("latchfree.txt", std::ios::app | std::ios::out);
                result_file << "latchfree ";
        } else if (type == sync_bench::MCS_LOCK) {
                result_file.open("mcs.txt", std::ios::app | std::ios::out);
                result_file << "mcs ";
        } else if (type == sync_bench::PTHREAD_LOCK) {
                result_file.open("pthread.txt", std::ios::app | std::ios::out);
                result_file << "pthread ";
        } else {
                assert(false);
        }
        
        result_file << "threads:" << nthreads << " ";
        result_file << "throughput:" << throughput << " ";
        result_file << "parallel:" << parallel_section << " ";
        result_file << "serial:" << serial_section << " ";
        result_file << "\n";
        result_file.close();

        if (LATENCY_EXP) {
                if (type == sync_bench::SPINLOCK) {
                        result_file.open("spinlock_latency.txt", std::ios::app | std::ios::out);
                } else if (type == sync_bench::LATCH_FREE) {
                        result_file.open("latchfree_latency.txt", std::ios::app | std::ios::out);
                } else if (type == sync_bench::MCS_LOCK) {
                        result_file.open("mcs_latency.txt", std::ios::app | std::ios::out);
                } else if (type == sync_bench::PTHREAD_LOCK) {
                        result_file.open("pthread_latency.txt", std::ios:: app | std::ios::out);
                } else {
                        assert(false);
                }
        
                write_latency(latency, result_file);
                result_file.close();        
        }
}


sync_bench::latency_result avg_latency(uint32_t ncpus, sync_bench::latency_result *res)
{
        sync_bench::latency_result ret_latency;
        uint64_t i, iters[ncpus], acc[ncpus], cur, j;

        ret_latency._iters = 0;
        for (i = 0; i < ncpus; ++i) {
                iters[i] = (res[i]._iters > (1<<20))? (1<<20) : res[i]._iters;
                if (i == 0)
                        acc[0] = iters[0];
                else
                        acc[i] = iters[i] + acc[i-1];
                
                ret_latency._iters += iters[i];
        }

        ret_latency._latency = (uint64_t*)malloc(sizeof(uint64_t)*ret_latency._iters);
        memset(ret_latency._latency, 0x0, sizeof(uint64_t)*ret_latency._iters);
        
        cur = 0;
        for (i = 0; i < ncpus; ++i) {
                for (j = 0; j < iters[i]; ++j) {
                        ret_latency._latency[cur+j] = res[i]._latency[j];
                }
                cur += iters[i];
        }
        std::sort(ret_latency._latency, &ret_latency._latency[ret_latency._iters]);
        return ret_latency;
}

/* 
 * For now, arg1 is the experiment type, arg2 is #threads, arg3 is 
 * spin_inside, and arg4 is spin_outside.  
 */   
void do_sync_bench(int argc, char **argv)
{
        assert(argc == 6);

        sync_bench::bench_args args;
        sync_bench::results result;
        sync_bench::bench *benchmark;
        sync_bench::latency_result latency;
        
        args._type = (sync_bench::bench_t)atoi(argv[1]);
        
        assert(args._type >= sync_bench::SPINLOCK && 
               args._type <= sync_bench::PTHREAD_LOCK);

        args._ncpus = (uint32_t)atoi(argv[2]);        
        args._spin_inside = (uint64_t)atoi(argv[3]);
        args._spin_outside = (uint64_t)atoi(argv[4]);
        LATENCY_EXP = ((uint32_t)atoi(argv[5]) > 0);

        benchmark = new sync_bench::bench(args);
        result = benchmark->execute();
        std::cout << "Throughput: " << result._throughput << "\n";
        
        if (LATENCY_EXP)
                latency = avg_latency(args._ncpus, result._latencies);

        do_fairness(result._fairness, args._ncpus);
        write_results(args._type, args._ncpus, args._spin_inside,
                      args._spin_outside,
                      result._throughput,
                      latency);
}

void write_logging_results(uint64_t txn_length, uint64_t log_latency, 
                           double throughput)
{
        std::ofstream result_file;
        result_file.open("logging.txt", std::ios::app | std::ios::out);
        result_file << "logging ";        
        result_file << "txn_length:" << txn_length << " ";
        result_file << "log_latency:" << log_latency << " ";
        result_file << "throughput:" << throughput << "\n";
        result_file.close();
}

void do_logging(int argc, char **argv)
{
        assert(argc == 4);
        
        uint64_t txn_length, log_latency;
        double throughput;
        txn_length = (uint64_t)atoi(argv[2]);
        log_latency = (uint64_t)atoi(argv[3]);
        
        throughput = log_bench::do_benchmark(txn_length, log_latency);
        write_logging_results(txn_length, log_latency, throughput);
}

int main(int argc, char **argv)
{
        assert(argc >= 2);
        sync_bench::bench_t type;
        type = (sync_bench::bench_t)atoi(argv[1]);
        
        if (type >= sync_bench::SPINLOCK && 
            type <= sync_bench::PTHREAD_LOCK) 
                do_sync_bench(argc, argv);
        else if (type == sync_bench::LOGGING) 
                do_logging(argc, argv);
        else 
                assert(false);
        return 0;
}
