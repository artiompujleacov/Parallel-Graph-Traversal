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

#define NUM_THREADS		4


static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
static unsigned int contor;
pthread_mutex_t graph_mutex;
pthread_mutex_t process_mutex;
typedef struct {
	unsigned int idx;
} ProcessNodeTaskArgs;

static void process_neighbours(void *arg)
{
	ProcessNodeTaskArgs *task_arg = (ProcessNodeTaskArgs *)arg;

	pthread_mutex_lock(&graph_mutex);

	if (graph->visited[task_arg->idx] == NOT_VISITED) {
		sum += graph->nodes[task_arg->idx]->info;
		contor++;
		graph->visited[task_arg->idx] = DONE;
	}

	if (graph->num_edges == 0) {
		tp->done = 1;
		pthread_mutex_unlock(&graph_mutex);
		return;
	}

	for (unsigned int i = 0; i < graph->nodes[task_arg->idx]->num_neighbours; i++) {
		unsigned int neighbor_idx = graph->nodes[task_arg->idx]->neighbours[i];

		if (graph->visited[neighbor_idx] == NOT_VISITED) {
			contor++;
			if (contor >= graph->num_nodes)
				tp->done = 1;

			ProcessNodeTaskArgs *neighbor_task = malloc(sizeof(ProcessNodeTaskArgs));

			neighbor_task->idx = neighbor_idx;
			enqueue_task(tp, create_task(process_neighbours, neighbor_task, free));
		}
	}
	pthread_mutex_unlock(&graph_mutex);
}

static void process_node(unsigned int idx)
{
	pthread_mutex_lock(&process_mutex);
	ProcessNodeTaskArgs *task_arg = malloc(sizeof(ProcessNodeTaskArgs));

	task_arg->idx = idx;
	enqueue_task(tp, create_task(process_neighbours, task_arg, free));
	pthread_mutex_unlock(&process_mutex);
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

	pthread_mutex_init(&graph_mutex, NULL);
	pthread_mutex_init(&process_mutex, NULL);
	tp = create_threadpool(NUM_THREADS);
	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);
	pthread_mutex_destroy(&graph_mutex);
	pthread_mutex_destroy(&process_mutex);

	printf("%d", sum);

	return 0;
}
