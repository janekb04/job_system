# Coroutine-based Concurrent C++ Job System
* **New-Free** (no dynamic memory allocations)
* **Lock-free** (guaranteed system-wide progress)
* Almost **Wait-free** (see below)

This repository contains a job system. It is essentially a library which is supposed to aid in multithreaded programming. **EXPERIMENTAL AND NOT READY FOR PRODUCTION USE**.

## A note on stability and performance

This project is currently in the very early stages. It has been tested on an arm64 CPU (with a
memory model weaker than x86) and seems to work - it doesn't segfault after many hours of
running. Still, as it always goes with lock-free code, it is likely that some synchronization
bugs are still present. Currently, the executable runs a test, which calculates the overhead
of launching and waiting for a job compared to a function call. It seems to be around `20ns` per call on the test CPU (Apple M1 Pro). This is less than the latency of a fetch from main memory, which is the performance goal of the project.

## Usage

Simply declare functions as jobs, by making them return a `job<ReturnType, IsNoexcept>`.
```cpp
#include <job.h>
job<Renderer> createRenderer();
```
Start running jobs with `executor::run(main_job_func)`. The executor will start running the `main_job`. Newly created jobs will be executed on multiple threads, if they don't depend on each other. `run` will return when the `main_job` will return.
```cpp
job<int> app_main();
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
Then, control is returned to its parent job.

When a job returns or is suspended at a `co_await` expression, a new job is selected using `executor::pop`. If the thread-specific buffer isn't empty, the job is selected from there.
If the buffer is empty, the buffer is first refilled from a global job queue.

When a job is about to start, it first publishes any pending jobs from the local
ring-buffer to the global queue using `executor::publish`. This must be done at
a job entry because of job lifetimes. If the jobs were published after getting `push`ed,
it would be possible for a different thread to pick up a job and start executing it
before it finished running on the publisher thread.

The queues are small enough to be allocated up front, when the executor is initialized, so
`push`ing and `pop`ping jobs doesn't cause any allocations. By default, they have a capacity
of 256 jobs each. This eliminates the use of moduli in their implementation. Instead, the
ring buffer iterators are simply `uint8_t`s, which wrap around automatically. A limitation
of this is that a job can have at most 249 dependencies. It is not 256 because there can be
already at most 7 jobs in the queue (assuming a 64-byte cache line; read below).

### The global MPMC queue

The global queue is a multi-producer multi-consumer lock-free queue. Jobs are `push`ed to it
and `pop`ped from it in cache-line sized blocks. This is because the [MOESI](https://en.wikipedia.org/wiki/MOESI_protocol) protocol operates
at a cache line granularity. By disallowing reads or writes to the queue spanning across
cache line boundaries, it is guaranteed that at most two threads will ever be
contending for a piece of the queue data - *the writer* and *the reader*.

When a thread wants to `enqueue` a batch of jobs to the global queue, it is given
a *write iterator*, which tells it where it can put the jobs. The write iterator is
simply an index into the queue's underlying data array. The write iterator is obtained from a
monotonically increasing atomic counter. No two writers can receive the same iterator. This eliminates all race conditions between writers.

When a thread wants to `dequeue` a batch of jobs from the global queue, it is given
a *read iterator*. It is very similar to the write iterator. It is also monotonically
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
is a good exposition of how it actually works.

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
Firstly, its promise sets its own internal atomic counter to the number of dependency jobs.
Then, it allocates (on the stack) a dependency-count-sized array of `notifier` objects. The `notifier` type
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

## Lock/Wait-free-ness

A lock-free system is a system with guaranteed system-wide progress, while a wait-free system is a system with guaranteed per-thread progress. Basically, consider a system that has `N` threads. If it is lock-free, it is guaranteed that always at least one thread is working (it is not blocked). If it is wait-free, it is guaranteed that always all `N` threads will be working.

There exist only three waits in the job system:
1. Threads contending on a depedency list of certain job - ie. threads trying to add a dependency to a job.
2. A reader thread waiting on a cacheline for a writer thread to begin writing to it.
3. A reader thread waiting on a cacheline that is currently being written to by a writer thread.

### Threads contending on a job dependency list

The first wait is a real wait. If multiple threads try to add a dependency to the same job at once, they will have to do it one by one. This means that, in general, the job system technically isn't lock free. Still, I feel comfortable with calling the job system almost wait-free, as this CAS loop isn't a central part of the job system. This situation has a very low probability of happening, and only, if the job system is used in a certain way. In order for this to happen, at least two independently scheduled jobs would need to have access to a job's future and they would both need to add a dependecy to it, for example by waiting on it, and they would have to do that at once. The vast majority of jobs are single-waiter - they have only a single dependent. It is rather rare to schedule a job and then pass its future to other jobs, so they could wait on it.

### Reader waiting for there being any work to do

Regarding the second wait, it occurs rather frequently. Still, I argue that after thinking about it, you can realize that it does not qualify to be a real wait. This wait essentially means that a reader thread cannot progress because there are not jobs on its cacheline and noone is writing to that cacheline. Well then, why is noone writing to that cacheline? The answer is that either there are no ready threads at all or there are ready threads, but have or are being written to other cachelines. To me, the first option isn't a real wait, as what the reader is waiting for is for there to be work to be done. It seems rather logical to me that all systems have to wait until they are given work to do. The second wait is actually impossible. Below, I prove that a writer is guaranteed to unblock a reader (if one exists) when writing jobs. In other words, it is impossible for a writer to write a batch of jobs somewhere, when no reader is waiting, while readers are waiting somewhere else.

Firstly, let's introduce some definitions.

>  * Let `R` be the reader iterator and `W` be the writer iterator.
>  * A given cache-line can be in one of six states:
>       * Not having been written to (state `1**`)
>           * and a reader is currently blocked on it (state `1R*`)
>              * and a writer is currently writing to it (state `1RW`)
>              * and no writer is currently writing to it (state `1R-`)
>           * and no reader is currently blocked on it (`1-*`)
>              * and a writer is currently writing to it (state `1-W`)
>              * and no writer is currently writing to it (state `1--`)
>       * Written to but not yet read (state `2`)
>       * Written and read (state `3`)

Now, let's prove that enqueuing new jobs is guaranteed to unblock a blocked reader thread (if one exists):

>  * Because `W` is monotonically increasing, when a writer writes a new batch of jobs, it is guaranteed to write them to a `1R-` or `1--` cacheline. The line will then enter either the `1RW` or `1-W` state. After the write is complete, the cacheline will become `2`.
>  * Lemma 1: if there exists a `1R-` cacheline, an enqueue operation cannot happen on a `1--` cacheline
>       1. By contradiction: let's assume that there exists a `1R-` cacheline and a thread writes to a `1--` cacheline
>       2. Let `A` be the index (value of the iterator) of the cacheline in `1R-`.
>       3. Let `B` be the index (value of the iterator) of the cacheline in `1--`.
>       4. Recall that `R` is monotonically increasing.
>       5. Because there is a reader with an index `A`, there must also have been readers
for all indices smaller than `A`. This means that all cachelines with an index smaller than `A` must be `1R-`, `1RW` or `3`.
>       6. Hence, `B > A` as a cacheline with an index smaller than `A` cannot be `1--`.
>       7. Recall that `W` is also monotonically increasing.
>       8. Because there is a writer with an index `B`, there must also have been writers for all indices smaller than `B`. This means that all cachelines with an index smaller than `B` must be `1RW`, `1-W`, `2`, or `3`.
>       9. From (vi.) `B > A`. Yet from (viii.) `A` cannot be in `1R-`. Contradiction.
>  * The above implies that if there exists a `1R-` cacheline, a writer must write to a `1R-` cacheline. This means that if there exist blocked reader threads, one of them is guaranteed to unblock as soon as a batch of jobs will be enqueued by any thread.

Ok, so we proved that a writer will never "not notice" a reader and leave it blocked. But what about the other way around? Could a reader not notice a ready (written to) cacheline and instead block on an unready (not written to) cacheline? Well, no. Here's the proof.

>  * Because `R` is monotonically increasing, when a reader reads a new batch of jobs, it is guaranteed to read the from a `1-W`, `1--` or `2` cacheline. The line will then enter either the `1RW` or `1R-` state or stay `2`. After the read is complete, the cacheline will become `3`. Here, we consider a reader reading from a not written to cacheline - a reader reading from a `1--` cacheline.
>  * Lemma 2: if there exists a `2` cacheline, a dequeue operation cannot happen on a `1--` cacheline.
>       1. By contradiction: let's assume that there exists a `2` cacheline and a thread is about to read from a `1--` cacheline.
>       2. Let `A` be the index of the cacheline in `2`.
>       3. Let `B` be the index of the cacheline in `1--`. Because the reader is about to read from this cacheline `R = B`.
>       4. Recall that `W` is monotonically increasing.
>       5. Because cacheline `A` has been written to, there must also have been writers for all indices smaller than `A`. This means that all cachelines with an index smaller than `A` must be `1RW`, `1-W` or `2`.
>       6. Hence, `B > A` as a cacheline with an index smaller than `A` cannot be `1--`.
>       7. Recall that `W` is also monotonically increasing.
>       8. Because a reader is about to read from cacheline `B`, there must have been readers for all indices smaller than `B`. This means that all cachelines with an index smaller than `B` must be `1RW`, `1R-`, or `3`.
>       9. From (vi.) `B > A`. Yet from (viii.) `A` cannot be in `2`. Contradiction.
> * The above implies that if there exists a `2` cacheline, a reader must read from a `2` cacheline or a `1-W` cacheline. This means that if there exist any jobs to be done, a reader is guaranteed to "pick them up" as soon as it will become available, unless it will read a `1-W` cacheline.

Regarding reading from a cacheline that is currently being written to (state `1-W`), see the section below.

### Reader waiting for a writer to write `64 bytes`

Regarding the third wait, it is also relatively unlikely. However, in addition to that, I'd like to explain a bit more about why I don't believe that these two waits make the job system cease to deserve a "wait-free-ness" badge.

It is the OS that gives CPU time to processes. It can arbitrarly pause any thread and then resume it at some other point in time. This means that a thread can be in three states: it can be _working_, _blocked_ or _sleeping_. At time `T`, a thread is said to be working if it is currently running on the CPU and performing "useful" work (a spinlock doesn't count). A sleeping thread isn't currently running on the CPU. It is a thread that will immediately become a working thread or a blocked thread, if resumed by the OS, regardless of any circumstances. Usually, when talking about lock-free-ness or wait-free-ness, sleeping threads are disregarded. That is because if we took them into account, no system could, in general, be lock-free. After all, the OS could put the entire process to sleep at once, in which case the requirement of lock-free-ness wouldn't be fulfilled. As such, when looking at a particular thread, we assume that it never sleeps.

However, we don't assume that the system overall doesn't sleep. To give an example, when considering a reader thread, we assume it to never sleep. Let's say that at some point in time it begins waiting for a writer thread to finish writing to a cacheline. You could think that the reader thread isn't waiting at all, as, after all, writing a single cacheline is nearly instantaneous, and if we know that the writer has already began writing to the cacheline, it will complete nearly immediately, and hence the reader thread will unblock nearly immediately. Still, this doesn't technically count as wait-free-ness. After all, although from the point of view of the reader, it never sleeps, this doesn't hold for other threads. Other threads can be put to sleep or resumed at any point in time. This means that the reader is blocked on the writer and the reader-writer system is not wait-free. It is lock-free, however, because from the point of view of the writer, it never sleeps, and it isn't waiting on anything, so it is always making progress. As soon as the reader unblocks, the system will become wait-free.

In reality, lock-free `!=` lock-free. Consider two systems. In one system the main thread is blocked until all workers complete some background computation. In the other system, a reader thread is blocked on a writer thread until it writes `64 bytes`. Although from a theoretical standpoint, these systems are both only lock-free, the second system is clearly "more" lock-free than the first one. I even dare to call it "almost wait-free" because the waiting time is equal to the time it takes to write `64 bytes` by the writer thread plus the amount of time the writer spends sleeping. In reality, this time will be negligible. This is why I consider this system to be "almost wait-free".






