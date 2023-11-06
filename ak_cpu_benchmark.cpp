// This is a CPU performance benchmarking program. It tests CPU performance by executing a task known as 'pointer chasing'. The program should be compiled before the first use for target architecture. 

// gcc -o cpu_bench cpu_bench.cpp -Ofast -lpthread -lstdc++

// The program takes two arguments: 
// 1. The total number of seconds to run the benchmark.
// 2. The interval in seconds at which to report the number of transactions processed by each thread.

// Each thread generates an array of 100M integers, which is shuffled using the Fisher-Yates algorithm. The main task involves a pointer chasing loop, executed 1000 times per iteration. After each iteration, one transaction is added to the transaction counter. The thread then yields to the OS, allowing other loads to run in parallel.

// Source Code Explanation:
// The `memoryBoundTask` function is the core function that each thread executes. It generates an array of 100M integers, shuffles it, pins the thread to a core, and then performs iterations of pointer chasing. After each 1000 iterations, it increments the transaction count and yields to the OS.
// The `printCsvHeaders` and `printCsvRow` functions are used for reporting. They print the elapsed time and the number of transactions processed by each thread in a CSV format.
// The `runBenchmark` function creates a number of threads equal to the number of hardware threads available on the system. It then starts each thread with the `memoryBoundTask` function. It also prints the CSV headers and rows at the specified report interval.
// The `main` function checks if the required arguments are provided, and if so, it calls the `runBenchmark` function with these arguments. If the required arguments are not provided, it prints a usage message.

// The load itself consists of a pointer chasing loop. Here's the main loop in ASM
// L39:
//         cdqe
//         lea     rax, [r15+rax*4]
//         mov     eax, DWORD PTR [rax]
//         sub     edx, 1
//         jne     .L39

// Normal number of transactions per vCPU in AWS: 8000 – 10000 per second

#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <ctime>
#include <cstdlib>
#include <random>
#include <pthread.h>
#include <sched.h>
#include <pthread.h>


std::atomic<uint64_t> globalTransactionCount(0);
std::mutex outputMutex;

void memoryBoundTask(int numSeconds, std::atomic<uint64_t> &localTransactionCount, unsigned int seed, int core_id) {
    const int SIZE = 100000000;
    const int MAX_RANDOM = SIZE;
    volatile int* numbers = new int[SIZE];
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<int> distribution(0,MAX_RANDOM);

    // Generate a permutation of the integers 0 to SIZE - 1
    for (int i = 0; i < SIZE; ++i) {
        numbers[i] = i;
    }

    // Fisher-Yates shuffle
    for (int i = SIZE - 1; i > 0; --i) {
        std::uniform_int_distribution<int> distribution(0, i);
        std::swap(numbers[i], numbers[distribution(generator)]);
    }

    // Pin thread to core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    auto end_time = std::chrono::system_clock::now() + std::chrono::seconds(numSeconds);

    while (std::chrono::system_clock::now() < end_time) {
        // Perform iterations of pointer chasing
        int currentIndex = distribution(generator);
        for (int i = 0; i < 1000; i++) {
            currentIndex = numbers[currentIndex];
        }

        // transactions++;
        localTransactionCount++;
        globalTransactionCount++;
        sched_yield();
    }
    delete[] numbers;
}

void printCsvHeaders(int numThreads) {
    std::cout << "Elapsed,";
    for (int i = 0; i < numThreads; ++i) {
        std::cout << "Thread_" << (i);
        if (i != numThreads - 1) {
            std::cout << ",";
        }
    }
    std::cout << std::endl;
}

void printCsvRow(int elapsed, int numThreads, std::vector<std::atomic<uint64_t>> &localTransactionCounts, std::vector<uint64_t> &lastLocalTransactionCounts) {
    std::cout << elapsed << ",";
    for (int i = 0; i < numThreads; ++i) {
        uint64_t currentLocalTransactionCount = localTransactionCounts[i].load();
        std::cout << (currentLocalTransactionCount - lastLocalTransactionCounts[i]);
        if (i != numThreads - 1) {
            std::cout << ",";
        }
        lastLocalTransactionCounts[i] = currentLocalTransactionCount;
    }
    std::cout << std::endl;
}

void runBenchmark(int totalSeconds, int reportIntervalSeconds) {
    int numThreads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::vector<std::atomic<uint64_t>> localTransactionCounts(numThreads);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(memoryBoundTask, totalSeconds, std::ref(localTransactionCounts[i]), time(0) + i, i);
    }

    uint64_t lastGlobalTransactionCount = 0;
    std::vector<uint64_t> lastLocalTransactionCounts(numThreads, 0);

    printCsvHeaders(numThreads);

    for (int elapsed = reportIntervalSeconds; elapsed <= totalSeconds; elapsed += reportIntervalSeconds) {
        std::this_thread::sleep_for(std::chrono::seconds(reportIntervalSeconds));
        std::lock_guard<std::mutex> lock(outputMutex);

        printCsvRow(elapsed, numThreads, localTransactionCounts, lastLocalTransactionCounts);
    }

    for (auto &t : threads) {
        t.join();
    }

    std::cout << "Final Report:" << std::endl;
    std::cout << "Total Transactions: " << globalTransactionCount.load() << std::endl;
    for (int i = 0; i < numThreads; ++i) {
        std::cout << "  Total Transactions (Thread " << i+1 << "): " << localTransactionCounts[i].load() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: ./program <totalSeconds> <reportIntervalSeconds>" << std::endl;
        std::cout << "totalSeconds: The total duration for which the program should run." << std::endl;
        std::cout << "reportIntervalSeconds: The interval in seconds at which the program should report the number of transactions processed by each thread." << std::endl;
        return 1;
    }

    int totalSeconds = std::stoi(argv[1]);
    int reportIntervalSeconds = std::stoi(argv[2]);

    runBenchmark(totalSeconds, reportIntervalSeconds);

    return 0;
}

