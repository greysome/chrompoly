/*
 * Copyright (C) 2023-2023 Way Yan Win
 * This code is under the MIT License.
 */
#include "array.h"
#include "matrix.c"

bool graph_changed = false;
bool stop_flag = false;
int cur_submap_count = 0;
int cur_matrix_rows_count = 0;

typedef struct {
  int start_idx;
  int end_idx;
} Edge;
array_def(Edge, edge);

typedef array_int Vertex;
array_def(Vertex, vertex);

typedef struct {
  array_vertex vertices;
  array_edge edges;
} Submap;
array_def(Submap, submap);

bool int_eq(int a, int b) { return a == b; }

bool vertex_eq(Vertex v1, Vertex v2) { return array_eq(&v1, &v2, int_eq); }

bool edge_eq(Edge e1, Edge e2) {
  return e1.start_idx == e2.start_idx && e1.end_idx == e2.end_idx;
}

bool submap_eq(Submap s1, Submap s2) {
  return array_eq(&s1.vertices, &s2.vertices, vertex_eq) &&
         array_eq(&s1.edges, &s2.edges, edge_eq);
}

// Convenient global variable for recursively generating all submaps using
// direct_submaps()
array_submap _all_submaps;

int cmp(const void *a, const void *b) {
  int x = *((int *)a);
  int y = *((int *)b);
  if (x > y)
    return 1;
  if (x < y)
    return -1;
  return 0;
}

array_submap get_direct_submaps(Submap *submap) {
  array_submap result;
  array_init(&result);

  array_foreach(&submap->edges, Edge edge) {
    int i = edge.start_idx;
    int j = edge.end_idx;
    // Make sure i < j
    if (i > j) {
      int k = i;
      i = j;
      j = k;
    }

    array_vertex vertices;
    array_init(&vertices);

    array_enumerate(&submap->vertices, k, Vertex _) {
      if (k == i) {
        Vertex v1 = array_at(&submap->vertices, i);
        Vertex v2 = array_at(&submap->vertices, j);
        Vertex identified;
        array_init(&identified);
        array_foreach(&v1, int i) { array_add(&identified, i); }
        array_foreach(&v2, int i) { array_add(&identified, i); }
        qsort(identified.elems, identified.size, sizeof(int), cmp);
        array_add(&vertices, identified);
      } else if (k == j) {
        continue;
      } else {
        Vertex src = array_at(&submap->vertices, k);
        Vertex dest;
        array_init(&dest);
        // Copy over src to dest
        array_foreach(&src, int i) { array_add(&dest, i); }
        array_add(&vertices, dest);
      }
    }

    array_edge edges;
    array_init(&edges);

    array_foreach(&submap->edges, Edge _edge) {
      int k = _edge.start_idx;
      int l = _edge.end_idx;
      if ((k == i && l == j) || (k == j && l == i)) {
        continue;
      }
      if (k == i || k == j) {
        if (l > j) {
          l -= 1;
        }
        array_add(&edges, ((l < i) ? (Edge){l, i} : (Edge){i, l}));
        continue;
      } else if (l == i || l == j) {
        if (k > j) {
          k -= 1;
        }
        array_add(&edges, ((k < i) ? (Edge){k, i} : (Edge){i, k}));
        continue;
      }
      if (k > j)
        k -= 1;
      if (l > j)
        l -= 1;
      array_add(&edges, ((k < l) ? (Edge){k, l} : (Edge){l, k}));
    }

    array_add(&result, ((Submap){vertices, edges}));
  }
  return result;
}

bool in_submap_array(array_submap *submaps, Submap *submap) {
  array_foreach(submaps, Submap s) {
    if (submap_eq(s, *submap))
      return true;
  }
  return false;
}

void _handle(Submap *submap) {
  if (graph_changed || stop_flag) {
    stop_flag = true;
    return;
  }

  bool inside = in_submap_array(&_all_submaps, submap);
  if (inside) {
    return;
  }

  array_submap direct_submaps = get_direct_submaps(submap);
  if (array_size(&submap->vertices) > 1) {
    array_foreach(&direct_submaps, Submap direct_submap) {
      _handle(&direct_submap);
    }
  }
  cur_submap_count++;
  //idebug(cur_submap_count);
  array_add(&_all_submaps, ((Submap){submap->vertices, submap->edges}));
}

array_submap get_all_submaps(Submap *submap) {
  free_submaps(&_all_submaps);
  array_init(&_all_submaps);
  _handle(submap);
  if (stop_flag)
    printf("aborted early\n");
  stop_flag = false;
  cur_submap_count = 0;
  return _all_submaps;
}

Submap from_graph(int n, array_edge edges) {
  array_vertex vertices;
  array_init(&vertices);

  for (int i = 0; i < n; i++) {
    Vertex v;
    array_init(&v);
    array_add(&v, i);
    array_add(&vertices, v);
  }

  array_edge edges_copy;
  array_init(&edges_copy);
  array_foreach(&edges, Edge edge) { array_add(&edges_copy, edge); }

  return (Submap){vertices, edges_copy};
}

void free_submap(Submap *submap) {
  // Free vertices
  array_enumerate(&submap->vertices, i, Vertex _) {
    array_term(&submap->vertices.elems[i]);
  }
  array_term(&submap->vertices);
  array_term(&submap->edges);
}

void free_submaps(array_submap *submaps) {
  array_enumerate(submaps, i, Submap _) { free_submap(&submaps->elems[i]); }
  array_term(submaps);
}

/* Find vertex of s containing the number n, and return its index. */
int _find_node_idx(Submap *submap, int n) {
  array_enumerate(&submap->vertices, i, Vertex v) {
    if (array_contains(&v, n, int)) {
      return i;
    }
  }
  return -1;
}

bool submap_ge(Submap *s1, Submap *s2) {
  array_foreach(&s1->vertices, Vertex v) {
    int idx;
    array_enumerate(&v, i, int n) {
      int found_idx = _find_node_idx(&s2->vertices, n);
      if (i == 0)
        idx = found_idx;
      else if (found_idx != idx)
        return false;
    }
  }
  return true;
}

WYMatrix get_ordering_matrix(array_submap *submaps) {
  int size = array_size(submaps);
  WYMatrix M = matrix_init(size);
  array_enumerate(submaps, i, Submap s1) {
    cur_matrix_rows_count++;
    if (graph_changed || stop_flag) {
      stop_flag = false;
      cur_submap_count = 0;
      cur_matrix_rows_count = 0;
      return M;
    }
    array_enumerate(submaps, j, Submap s2) {
      bool x = submap_ge(&s2, &s1);
      if (i <= j && x)
        M.entries[i][j] = 1;
      else
        M.entries[i][j] = 0;
    }
  }
  cur_matrix_rows_count = 0;
  return M;
}

/* Compute the last column of the inverse of M.
This corresponds to the sequence
(mobius(submap, G) | submap in submaps, G is largest submap) */
array_int get_mobius_of_column(WYMatrix M) {
  array_int unknowns;
  array_init(&unknowns);
  for (int j = M.size - 1; j >= 0; j--) {
    array_add(&unknowns, 0);
  }

  int unknown;
  for (int j = M.size - 1; j >= 0; j--) {
    if (j == M.size - 1)
      unknown = 1;
    else {
      unknown = 0;
      for (int k = 0; k < M.size; k++) {
        unknown += array_at(&unknowns, k) * M.entries[j][k];
      }
      unknown = -unknown;
    }
    array_at(&unknowns, j) += unknown;
  }
  return unknowns;
}

array_int get_chromatic_polynomial(int n, array_submap *submaps, WYMatrix M) {
  array_int P; // Chromatic polynomial stored as an array of coeffs
  array_init(&P);
  for (int i = 0; i < n; i++) {
    array_add(&P, 0); // The size of this array is the no. of nodes in the original graph
  }
  array_int mobiuses = get_mobius_of_column(M);
  array_enumerate(&mobiuses, i, int mobius) {
    Submap submap = array_at(submaps, i);
    array_at(&P, array_size(&submap.vertices)-1) += mobius;
  }
  array_term(&mobiuses);
  return P;
}