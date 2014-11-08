/*
 * Author: Tiwei Bie (btw@mail.ustc.edu.cn)
 */

#ifndef MAPLE_IO_THREAD_H
#define MAPLE_IO_THREAD_H

#if defined(__linux__) || defined(__linux)
#include <sched.h>
#include <pthread.h>

#ifndef CPU_ZERO
#define CPU_ZERO(cpusetp)       __CPU_ZERO_S(sizeof (cpu_set_t), cpusetp)
#define CPU_SET(cpu, cpusetp)   __CPU_SET_S(cpu, sizeof (cpu_set_t), cpusetp)
#endif
extern int pthread_setaffinity_np(pthread_t __th, size_t __cpusetsize,
                                  __const cpu_set_t *__cpuset);
extern int pthread_setname_np(pthread_t thread, const char *name);

#define cpuset_t	cpu_set_t

#elif defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/cpuset.h>
#include <pthread.h>
#include <pthread_np.h>
#else
#error "Unsupported platform"
#endif

#include <stdio.h>

static inline int bind_cpu(int cpuid)
{
	cpuset_t mask;

	CPU_ZERO(&mask);
	CPU_SET(cpuid, &mask);

	if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) != 0) {
		perror("pthread_setaffinity_np");
		return 1;
	}

	return (0);
}

/* ========================================================================= */

static inline void thread_setname(const char *name)
{
#ifdef __FreeBSD__
        pthread_set_name_np(pthread_self(), name);
#elif defined(__linux__)
        pthread_setname_np(pthread_self(), name);
#endif
}

#endif
