# Coroutine-based Concurrent C++ Job System
* Heap Free
* Lock-free
* Wait-free? (maybe; idk)

This repository contains a job system. It is essentially a library which is supposed to aid in multithreaded programming. **EXPERIMENTAL AND NOT READY FOR PRODUCTION USE**.

## A note on stability and performance

This project is currently in the very early stages. It has been tested on an arm64 CPU (with a
memory model weaker than x86) and seems to work - it doesn't segfault after many hours of
running. Still, as it always goes with lock-free code, it is likely that some synchronization
bugs are still present. Currently, the executable runs a test which calculates the overhead
of launching and waiting for a job compared to a function call. It seems to be around `20ns` per call on the test CPU (Apple M1 Pro). This is less than the latency of a fetch from main memory, which is the performance goal of the project.

## Usage

Simply declare functions as jobs, by making them return a `job<ReturnType, IsNoexcept>`.
```cpp
#include <job.h>
job<Renderer> createRenderer();
```
Start running jobs with `executor::run(main_job_func)`. The executor will start running the `main_job`. Newly created jobs will be executed on multiple threads, if they don't depend on each other. `run` will return when the `main_job` will return.
```cpp
job<int> app_main;
...
executor::initialize(num_threads);
int exit_code = executor::run(app_main);
//          app_main, NOT: app_main() ^
executor::destroy();
```


Launch other jobs to run asynchronously and wait for them when their results are needed.

```cpp
job<Renderer> createRenderer()
{
    auto window_job = createWindow(); // start a job...
    // ... do other things ...
    Window window = co_await window_job; // ... and get its result

    auto config = co_await asyncReadConfigFile(); // or launch and wait immediately
    // ...

    // you can wait for multiple jobs at once
    auto [gfx_pipeline, rtx_pipeline, swap_chain] = co_await when_all(
        createGraphicsPipeline(),
        createRaytracingPipeline(),
        createSwapChain(),
    );
}
```

## Implementation

### Thread specific buffers
When a job is launched, it does not start executing immediately.
It is appended to a small thread-specific local ring-buffer queue using `executor::push`.
Then control is returned to parent job.

When a job returns or is suspended at a `co_await` expression, a new job is selected using `executor::pop`. If the thread-specific buffer isn't empty, the job is selected from there.
If the buffer is empty, the buffer is first refilled from a global job queue.

When a job is about to start, it first publishes any pending jobs from the local
ring-buffer to the global queue using `executor::publish`. This must be done at
a job entry because of job lifetimes. If the jobs were published after getting `push`ed,
it would be possible for a different thread to pick up a job and start executing it
before it finished running on the publisher thread.

The queues are small enough to be allocated up front, when the executor is initialized, so
`push`ing and `pop`ping jobs doesn't cause any allocations. By default they have a capacity
of 256 jobs each. This eliminates the use of moduli in their implementation. Instead the
ring buffer iterators are simply `unsigned char`s which wrap around automatically. A limitation
of this is that a job can have at most 249 dependencies. It is not 256 because there can be
already at most 7 jobs in the queue (assuming a 64-byte cache line, read below).

### The global MPMC queue

The global queue is a multi-producer multi-consumer lock-free queue. Jobs are `push`ed to it
and `pop`ped from it in cache-line sized blocks. This is because the [MOESI](https://en.wikipedia.org/wiki/MOESI_protocol) protocol operates
at a cache line granularity. By disallowing reads or writes to the queue spanning
cache line boundaries, it is guaranteed that at most two threads will be ever
contending for a piece of the queue data - the writer and the reader.

When a thread wants to `enqueue` a batch of jobs to the global queue, it is given
a write iterator, which tells is where it can put the jobs. The write iterator is
simply an index into the queue's underlying data array. The write iterator is obtained from a
monotonically increasing atomic counter. No two writers can receive the same iterator. This eliminates all race conditions between writers.

When a thread wants to `dequeue` a batch of jobs from the global queue, it is given
a read iterator. It is very similar to the write iterator. It is also monotonically
increasing, so it ensures that a given batch of jobs will be read by only one thread.

Understanding how the reader and the writer synchronize with each other on a given cache line
requires some explanation. The global queue doesn't store the job objects (of type `job`).
The jobs are implemented in terms of C++20 coroutines. In C++20 a coroutine is associated with
two objects: the _coroutine promise_ and the _coroutine return object_. The promise is named
as such because it resembles the classical functional programming
[_Promise_](https://en.wikipedia.org/wiki/Futures_and_promises).
Still, in C++ the promise object also functions as a sort of
coroutine manager. It is, in essence, a handle that _a coroutine can use to refer to itself_.
Meanwhile, the return object is a handle to the coroutine, which _its caller can use to refer to the coroutine_.

The global queue stores pointers to the job's coroutine's promise, aka `promise_base*`.
Assuming that a size of a pointer is 8 bytes and that a cache line is 64 bytes, the
queue's array looks like this:
```cpp
union cache_line
{
    promise_base* data[8];
    struct
    {
        char bytes[63];
        std::atomic<char> sync;
    };
};
cache_line buffer[ARRAY_SIZE / sizeof(promise_base*)];
```
In code, this is implemented more generically, but the above illustration
is a good exposition, of how it actually works.

Access to a cache line is synchronized via an atomic variable stored in the last byte
of each cache line. It works on the assumption that the system is little-endian and
that the high-order byte of a pointer is unused. This is a valid assumption for both x86_64 and arm64 which this library targets.

The reader thread busy-waits on the atomic until its highest-order _bit_ is set. That
bit is set by the writer thread when it finishes writing to the queue. The writer
uses release memory semantics, while the reader thread uses acquire semantics, so the
access to the entire cache line is synchronized thanks to this atomic.

### Inter-job synchronization

The only synchronization primitive used is an atomic counter. This idea stems from the
famous [GDC talk](https://www.gdcvault.com/play/1022187/Parallelizing-the-Naughty-Dog-Engine)
on the implementation of a job system using fibers in the Naughty Dog's game engine.

The common promise type of the jobs (`promise_base`) is actually a derived type from
`mpsc_list`, a multi-producer single-consumer list. This list stores the jobs dependent
on the current job. It is a lock-less linked list implemented using atomic operations.
Each node stores a pointer to the dependent's promise and the next node. Interestingly,
this linked list does not use any dynamic memory allocation.

When a job `co_await`s a (possibly 1-sized) set of dependency jobs, it does a few things.
Firstly its promise sets its own internal atomic counter to the number of dependency jobs.
Then, it allocates a dependency-count-sized array of `notifier` objects. The `notifier` type
is the type of the linked list's node. The created `notifier`s all point to the job
being suspended. They do not have a next node.

Then, the job goes through each of its dependency jobs and tries to append the corresponding
`notifier` to the dependency's list. If that dependency has already completed, this operation
fails. This is because when a job returns, it sets its list's head to a special sentinel value.
If the dependency has already completed (for example, on a different thread), then the
suspending job simply decrements its own atomic counter. If the dependency hasn't completed yet, it appends the `notifier` to that dependency's list.
It does so using a [CAS loop](https://en.wikipedia.org/wiki/Compare-and-swap).

After having gone through each of the dependencies, the suspending job checks how many
of its dependencies have already completed. If all of them have, then it doesn't suspend
and immediately continues execution. This isn't just an optimization. This is necessary
for the job system to function properly. This is because this job system _does not_ have
a _suspended job_ queue. The job system only has a _ready job_ queue. Suspended jobs are
stored only inside their dependencies' linked lists. Hence, if a job would suspend, while
not having any dependencies, it would never get resumed.

When a job returns, it traverses its linked list of dependents. Firstly, it sets the list's
head to the special sentinel value. Then, it goes through all of the jobs, atomically
decrementing their atomic counters. The decrement is a [RMW operation](https://en.wikipedia.org/wiki/Read–modify–write), so the job reads the counter's previous value. If it is one,
then it knows that it is the last dependency to complete for that job, and it `push`es it
to the job queue.

### Coroutine frame allocation

As described above, none of the data structures used by the job system use locks nor
allocate memory. Still, how is it possible for the jobs themselves to not need heap
allocations? This happens thanks to
[HALO](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0981r0.html).
It is a C++ compiler optimization which allows allocation of a coroutine on the stack, if
the compiler knows its lifetime. To take advantage of it, I had to introduce some
intermediary types. The `job<T>` object is the sole owner of a coroutine. It destroys
it according to RAII. This is why a separate type `future<T>`, which doesn't have access
to the job's `std::coroutine_handle`, is used to get the value returned from the job.
This shows the compiler that although the job system and other threads have access to
a job, they are not able to destroy it. The lifetime of the coroutine is local to the
function launching it.

Currently, LLVM Clang is the only compiler which implements HALO. Other compilers are
supported, but on each job launch, they allocate heap memory. It is planned to
optimize the performance on these compilers, by overloading the `promise`s `operator new` to
allocate a launched job within the parent job's coroutine frame (which would have scratch
space inside).
