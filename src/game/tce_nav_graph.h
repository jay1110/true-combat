#ifndef TCE_NAV_GRAPH_H
#define TCE_NAV_GRAPH_H
#include <stdlib.h>
#include <string.h>
/* Current accepted directed edges only. Iterative Kosaraju avoids recursion
 * on the native16K graph; grouping is search selection, never physics proof. */
typedef struct { int from, to; } tce_navGraphEdge;
static int TCE_NavStrongComponents(int nodes, int edgeCount, const unsigned char *active, const tce_navGraphEdge *edges, int *component)
{
	int           *memory, *head, *reverse, *stack, *cursor, *order, *next, *previous;
	unsigned char *seen; int i, e, count = 0, group = 0;
	if (nodes < 1 || nodes > 16384 || edgeCount < 0 || edgeCount > 262144 || !active || !component || (edgeCount && !edges))
	{
		return -1;
	}
	memory = (int *)malloc((size_t)(5 * nodes + 2 * edgeCount) * sizeof(int)); seen = (unsigned char *)calloc((size_t)nodes, 1);
	if (!memory || !seen)
	{
		free(memory); free(seen); return -1;
	}
	head = memory; reverse = head + nodes; stack = reverse + nodes; cursor = stack + nodes; order = cursor + nodes;
	next = order + nodes; previous = next + edgeCount;
	for (i = 0; i < nodes; i++)
	{
		head[i] = reverse[i] = component[i] = -1;
	}
	for (e = 0; e < edgeCount; e++)
	{
		int a = edges[e].from, b = edges[e].to;
		if (a < 0 || b < 0 || a >= nodes || b >= nodes)
		{
			free(memory); free(seen); return -1;
		}
		if (!active[a] || !active[b])
		{
			next[e] = previous[e] = -1; continue;
		}
		next[e] = head[a]; head[a] = e; previous[e] = reverse[b]; reverse[b] = e;
	}
	for (i = 0; i < nodes; i++)
		if (active[i] && !seen[i])
		{
			int top = 0; stack[0] = i; cursor[0] = head[i]; seen[i] = 1;
			while (top >= 0)
			{
				int v = stack[top]; e = cursor[top];
				if (e < 0)
				{
					order[count++] = v; top--; continue;
				}
				cursor[top] = next[e];
				if (!seen[edges[e].to])
				{
					int b = edges[e].to; seen[b] = 1; top++; stack[top] = b; cursor[top] = head[b];
				}
			}
		}
	for (i = count - 1; i >= 0; i--)
		if (component[order[i]] < 0)
		{
			int top = 0; stack[0] = order[i]; component[order[i]] = group;
			while (top >= 0)
			{
				int v = stack[top--];
				for (e = reverse[v]; e >= 0; e = previous[e])
					if (component[edges[e].from] < 0)
					{
						int a = edges[e].from; component[a] = group; stack[++top] = a;
					}
			}
			group++;
		}
	free(memory); free(seen); return group;
}
#endif
