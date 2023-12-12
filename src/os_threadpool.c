// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

os_task_t *create_task(void (*action)(void *), void *arg, void (*destroy_arg)(void *))
{
	os_task_t *t = malloc(sizeof(*t));

	t->action = action;
	t->argument = arg;
	t->destroy_arg = destroy_arg;

	return t;
}

void destroy_task(os_task_t *t)
{
	if (t->destroy_arg != NULL)
		t->destroy_arg(t->argument);
	free(t);
}

void enqueue_task(os_threadpool_t *tp, os_task_t *t)
{
	assert(tp != NULL);
	assert(t != NULL);

	pthread_mutex_lock(&tp->mutex);
	list_add_tail(&tp->head, &t->list);
	pthread_cond_signal(&tp->condition);
	pthread_mutex_unlock(&tp->mutex);
}

os_task_t *dequeue_task(os_threadpool_t *tp)
{
	os_task_t *t;

	pthread_mutex_lock(&tp->mutex);

	while (list_empty(&tp->head) && !tp->stop_flag)
		pthread_cond_wait(&tp->condition, &tp->mutex);

	if (!list_empty(&tp->head)) {
		t = list_entry(tp->head.next, os_task_t, list);
		list_del(&t->list);
	} else {
		t = NULL;
	}

	pthread_mutex_unlock(&tp->mutex);

	return t;
}

static void *thread_loop_function(void *arg)
{
	os_threadpool_t *tp = (os_threadpool_t *)arg;

	while (1) {
		os_task_t *t;

		t = dequeue_task(tp);
		if (t == NULL)
			break;

		t->action(t->argument);
		destroy_task(t);
	}

	return NULL;
}

void wait_for_completion(os_threadpool_t *tp)
{
	pthread_mutex_lock(&tp->mutex);
	pthread_cond_broadcast(&tp->condition);
	tp->stop_flag = 1;
	pthread_mutex_unlock(&tp->mutex);

	for (unsigned int i = 0; i < tp->num_threads; i++)
		pthread_join(tp->threads[i], NULL);
}

os_threadpool_t *create_threadpool(unsigned int num_threads)
{
	os_threadpool_t *tp = malloc(sizeof(*tp));

	list_init(&tp->head);

	pthread_mutex_init(&tp->mutex, NULL);
	pthread_cond_init(&tp->condition, NULL);

	tp->num_threads = num_threads;
	tp->threads = malloc(num_threads * sizeof(pthread_t));
	tp->stop_flag = 0;

	for (unsigned int i = 0; i < num_threads; i++)
		pthread_create(&tp->threads[i], NULL, thread_loop_function, (void *)tp);

	return tp;
}

void destroy_threadpool(os_threadpool_t *tp)
{
	os_list_node_t *n, *p;

	pthread_mutex_lock(&tp->mutex);
	tp->stop_flag = 1;
	pthread_cond_broadcast(&tp->condition);
	pthread_mutex_unlock(&tp->mutex);

	list_for_each_safe(n, p, &tp->head) {
		list_del(n);
		destroy_task(list_entry(n, os_task_t, list));
	}

	free(tp->threads);
	free(tp);
}
