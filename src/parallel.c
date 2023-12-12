// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS 4

int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
static pthread_mutex_t graph_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
	unsigned int index;
} graph_task_arg_t;

static void process_node(unsigned int idx)
{
	pthread_mutex_lock(&graph_mutex);

	if (graph->visited[idx] == NOT_VISITED) {
		graph->visited[idx] = PROCESSING;
		os_node_t *node = graph->nodes[idx];

		pthread_mutex_unlock(&graph_mutex);

		for (unsigned int i = 0; i < node->num_neighbours; i++)
			process_node(node->neighbours[i]);

		pthread_mutex_lock(&graph_mutex);
		graph->visited[idx] = DONE;
		sum += node->info;
		pthread_mutex_unlock(&graph_mutex);
	} else {
		pthread_mutex_unlock(&graph_mutex);
	}
}

static void graph_task(void *arg)
{
	graph_task_arg_t *task_arg = (graph_task_arg_t *)arg;

	process_node(task_arg->index);
	free(arg);
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	tp = create_threadpool(NUM_THREADS);

	pthread_mutex_init(&graph_mutex, NULL);

	enqueue_task(tp, create_task(graph_task, &(graph->nodes[0]->id), NULL));

	wait_for_completion(tp);
	destroy_threadpool(tp);

	pthread_mutex_destroy(&graph_mutex);

	printf("%d", sum);

	return 0;
}
