**ak_cpu_benchmark.cpp**

This is a CPU performance benchmarking program. It tests CPU performance by executing a task known as 'pointer chasing'. The program should be compiled before the first use for target architecture. 

gcc -o cpu_bench cpu_bench.cpp -Ofast -lpthread -lstdc++

The program takes two arguments: 
1. The total number of seconds to run the benchmark.
2. The interval in seconds at which to report the number of transactions processed by each thread.

Each thread generates an array of 100M integers, which is shuffled using the Fisher-Yates algorithm. The main task involves a pointer chasing loop, executed 1000 times per iteration. After each iteration, one transaction is added to the transaction counter. The thread then yields to the OS, allowing other loads to run in parallel.

Source Code Explanation:
The `memoryBoundTask` function is the core function that each thread executes. It generates an array of 100M integers, shuffles it, pins the thread to a core, and then performs iterations of pointer chasing. After each 1000 iterations, it increments the transaction count and yields to the OS.
The `printCsvHeaders` and `printCsvRow` functions are used for reporting. They print the elapsed time and the number of transactions processed by each thread in a CSV format.
The `runBenchmark` function creates a number of threads equal to the number of hardware threads available on the system. It then starts each thread with the `memoryBoundTask` function. It also prints the CSV headers and rows at the specified report interval.
The `main` function checks if the required arguments are provided, and if so, it calls the `runBenchmark` function with these arguments. If the required arguments are not provided, it prints a usage message.

The load itself consists of a pointer chasing loop. Here's the main loop in ASM
L39:
        cdqe
        lea     rax, [r15+rax*4]
        mov     eax, DWORD PTR [rax]
        sub     edx, 1
        jne     .L39

Normal number of transactions per vCPU in AWS: 8000 – 10000 per second# benchmarks

**ak_web_benchmark.c**

This program is designed to test the performance of a network, with a particular focus on the Linux TCP stack. It operates in two modes: server and client. 

    In server mode, the program opens a specified number of sockets (-t) on ports starting from the base port # 10,000. If a port is already in use, the program will display an error but will not terminate. If this occurs, it is recommended to stop the program and find another base port that allows for the opening of -t sequential ports. The server mode does not record any statistics. 

    In client mode, the program creates a specified number of connections (-t) to the server, using server ports starting from 10,000. The client mode records and outputs the following statistics every second: total number of threads running, total number of operations performed, and average number of operations per thread. 

    The program does not terminate on its own and must be manually stopped using Ctrl-C. When the client is stopped, the server continues to run and can be used for more tests.

    To compile the program, use the following command: gcc -o web_bench ak_web_bench.c -lpthread 

    Usage:
    Server mode: ./web_bench -s -t <num_threads>
    Client mode: ./web_bench -c <server_ip> -t <num_threads>

    The load on the server and the client from this benchmark is very much the same. So if your server VM's CPU/Network are less powerful, you won't get proper results on the client VM.
