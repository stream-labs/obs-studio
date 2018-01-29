/*
* Copyright (c) 2017 Zachary Lund <zachary.lund@streamlabs.com>
*
* Based on Michael Dirks (aka Xaymar) threaded memcpy tests
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "threading.h"
#include "bmem.h"

#if defined(_WIN32)
	#include <Windows.h>
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
	#include <unistd.h>
#endif

/* Prevents WinAPI redefinition down the road. */
#define NOMINMAX

#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

/* TODO: Will need adjustment */
#define MAX_THREAD_COUNT 32

struct memcpy_thread_work {
	void* from;
	void* to;
	size_t size;
	os_sem_t *semaphore;
	size_t block_size;
	struct memcpy_thread_work *next;
	struct memcpy_thread_work *prev;
};

/* Static global data */
const size_t block_size = 64 * 1024;

struct memcpy_environment {
	/* We don't know the size of pthread_t
	 * but it's usually represented by a 
	 * pointer or an integer. 
	 
	 * MAX_THREAD_COUNT should be a multiple of 8 
	 * so threads don't thrash the cache alignment 
	 * of the rest of the structure. */
	pthread_t threads[MAX_THREAD_COUNT];
	int thread_count;
	struct memcpy_thread_work *work_queue;
	struct memcpy_thread_work *work_queue_last;
	pthread_mutex_t work_queue_mutex;
	pthread_cond_t work_queue_cond;
	int running;
};

static unsigned optimal_thread_count()
{
	unsigned long result;
#if defined(_SC_NPROCESSORS_ONLN)
	result = sysconf(_SC_NPROCESSORS_ONLN);
	
	if (result < 0)
		return 1;
#elif defined(_WIN32)
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	result = info.dwNumberOfProcessors;
#else
	result = MAX_THREAD_COUNT / 8;
#endif
	result /= 2;

	if (result == 0) return 1;

	if (result > MAX_THREAD_COUNT)
		result = MAX_THREAD_COUNT;

	return result;
}

static void *start_memcpy_thread(void* context)
{
	struct memcpy_environment *env = (struct memcpy_environment*)context;
	struct memcpy_thread_work *work;

	os_sem_t* semaphore;
	void *from, *to;
	size_t size;

	for (;;) {
		pthread_mutex_lock(&env->work_queue_mutex);

		while (!env->work_queue && env->running) {
			pthread_cond_wait(&env->work_queue_cond, &env->work_queue_mutex);
		}

		if (!env->running) {
			pthread_mutex_unlock(&env->work_queue_mutex);
			break;
		}

		work = env->work_queue;
		from = work->from;
		to = work->to;
		size = work->block_size;
		semaphore = work->semaphore;

		if (work->size > size) {
			work->from = ((uint8_t*)work->from) + size;
			work->to = ((uint8_t*)work->to) + size;
			work->size -= size;
		}
		else {
			if (env->work_queue->next != NULL) {
				env->work_queue = env->work_queue->next;
				env->work_queue->prev = NULL;
			} else {
				env->work_queue = NULL;
				env->work_queue_last = NULL;
			}
		}

		pthread_mutex_unlock(&env->work_queue_mutex);

		memcpy((uint8_t*)to, (uint8_t*)from, size);

		/* Notify the calling thread that this thread is done working */
		os_sem_post(work->semaphore);
	}

	return 0;
}

/* Not thread safe but only needs to be called once for all threads */
struct memcpy_environment *init_threaded_memcpy_pool(int threads)
{
	struct memcpy_environment *env = (struct memcpy_environment*)
		bmalloc(sizeof(struct memcpy_environment));

	if (!threads)
		env->thread_count = optimal_thread_count();
	else
		env->thread_count = threads;

	env->work_queue = NULL;
	env->running = true;
	pthread_cond_init(&env->work_queue_cond, NULL);
	pthread_mutex_init(&env->work_queue_mutex, NULL);

	if (env->thread_count == 1)
		return env;

	for (int i = 0; i < env->thread_count; ++i) {
		int error = pthread_create(
			&env->threads[i],
			NULL,
			start_memcpy_thread,
			(void*)env);

		if (error) {
			/* This can only really happen in two cases:
			 * The first one being permissions for setting policy. 
			 * The second being lack of resources. 
			 * We can't really help either case so just fallback to
			 * a single thread */
			for (int k = 0; k < i; ++k) {
				pthread_cancel(env->threads[k]);
			}

			env->thread_count = 1;
			return env;
		}
	}

	return env;
}

void destroy_threaded_memcpy_pool(struct memcpy_environment *env)
{
	pthread_mutex_lock(&env->work_queue_mutex);
	env->running = false;
	pthread_mutex_unlock(&env->work_queue_mutex);

	if (env->thread_count == 1) {
		goto finish;
	}

	for (int i = 0; i < env->thread_count; ++i) {
		/* Since we don't know which thread wakes up, wake them all up. */
		pthread_cond_broadcast(&env->work_queue_cond);
		pthread_join(env->threads[i], NULL);
	}

finish:
	pthread_cond_destroy(&env->work_queue_cond);
	pthread_mutex_destroy(&env->work_queue_mutex);
	bfree(env);
}

void threaded_memcpy(void *destination, void *source, size_t size, struct memcpy_environment *env)
{
	if (env->thread_count == 1 ||
		size <= block_size * 2) 
	{
		memcpy(destination, source, size);
		return;
	}

	struct memcpy_thread_work work;
	os_sem_t *finish_signal;

	size_t blocks =
		min(max(size, block_size) / block_size, env->thread_count);

	os_sem_init(&finish_signal, 0);

	work.block_size = size / blocks;
	size_t block_size_rem = size - (work.block_size * blocks);
	work.size = size - block_size_rem;
	work.to = (uint8_t*)destination + block_size_rem;
	work.from = (uint8_t*)source + block_size_rem;
	work.semaphore = finish_signal;
	work.next = NULL;
	work.prev = NULL;

	pthread_mutex_lock(&env->work_queue_mutex);

	if (env->work_queue == NULL) {
		env->work_queue = &work;
		env->work_queue_last = &work;
	} else {
		work.prev = env->work_queue_last;
		work.prev->next = &work;
		env->work_queue_last = &work;
	}

	for (int i = 0; i < blocks; ++i)
		pthread_cond_signal(&env->work_queue_cond);

	pthread_mutex_unlock(&env->work_queue_mutex);

	/* Copy the remainder while we wait */
	memcpy(destination, source, block_size_rem);

	/* Wait for a signal from each job */
	for (int i = 0; i < blocks; ++i) {
		os_sem_wait(finish_signal);
	}

	pthread_mutex_lock(&env->work_queue_mutex);

	if (work.prev)
		work.prev->next = work.next;

	pthread_mutex_unlock(&env->work_queue_mutex);

	os_sem_destroy(finish_signal);
}