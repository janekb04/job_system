#if __linux__
#    include <pthread.h>
#    include <sys/resource.h>
#    include <sys/time.h>
bool set_this_thread_affinity(int cpu)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    int result =
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    return result == 0;
}
bool set_this_process_priority_high()
{
    int result = setpriority(PRIO_PROCESS, 0, -20);
    if (result != 0)
    {
        rlimit limit;
        if (getrlimit(RLIMIT_NICE, &limit) != 0)
        {
            return false;
        }
        int priority_limit = 20 - limit.rlim_cur;
        result             = setpriority(PRIO_PROCESS, 0, priority_limit);
    }
    return result == 0;
}
#elif __APPLE__
#    include <mach/mach.h>
#    include <mach/thread_act.h>
#    include <mach/thread_policy.h>
bool set_this_thread_affinity(int cpu)
{
    thread_affinity_policy_data_t policy = { cpu };
    kern_return_t                 result = thread_policy_set(
        mach_thread_self(),
        THREAD_AFFINITY_POLICY,
        reinterpret_cast<thread_policy_t>(&policy),
        THREAD_AFFINITY_POLICY_COUNT);
    return result == KERN_SUCCESS;
}
bool set_this_process_priority_high()
{
    thread_extended_policy_data_t policy1;
    policy1.timeshare = 0;
    kern_return_t result =
        thread_policy_set(mach_thread_self(), THREAD_EXTENDED_POLICY, reinterpret_cast<thread_policy_t>(&policy1), THREAD_EXTENDED_POLICY_COUNT);
    if (result != KERN_SUCCESS)
    {
        return false;
    }
    thread_precedence_policy_data_t policy2;
    policy2.importance = 63;
    result             = thread_policy_set(mach_thread_self(), THREAD_PRECEDENCE_POLICY, reinterpret_cast<thread_policy_t>(&policy2), THREAD_PRECEDENCE_POLICY_COUNT);
    return result == KERN_SUCCESS;
}
#elif defined(_WIN64)
#    include <Windows.h>
bool set_this_thread_affinity(int cpu)
{
    int result = SetThreadAffinityMask(GetCurrentThread() 1 << cpu);
    return result != 0;
}
bool set_this_process_priority_high()
{
    int result = SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    return result != 0;
}
#else
#    error
#endif
