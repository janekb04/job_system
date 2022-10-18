#ifndef SET_AFFINITY_H
#define SET_AFFINITY_H

bool set_this_thread_affinity(int cpu);
bool set_this_process_priority_high();

#endif
