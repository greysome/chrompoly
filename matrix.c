/*
 * Copyright (C) 2023-2023 Way Yan Win
 * This code is under the MIT License.
 */
#include <stdlib.h>
#include <string.h>

// Square matrix
typedef struct {
  int size;
  int **entries;
} WYMatrix; // Prevent naming conflict with Raylib

WYMatrix matrix_init(int size) {
  if (size == 0)
    return (WYMatrix){0, NULL};
  int **entries = malloc(size * sizeof(int *));
  for (int i = 0; i < size; i++) {
    entries[i] = malloc(size * sizeof(int));
  }
  return (WYMatrix){size, entries};
}

void matrix_free(WYMatrix M) {
  if (M.size == 0)
    return;
  for (int i = 0; i < M.size; i++)
    free(M.entries[i]);
  free(M.entries);
}

void matrix_print(WYMatrix M) {
  for (int i = 0; i < M.size; i++) {
    for (int j = 0; j < M.size; j++) {
      printf("%d ", M.entries[i][j]);
    }
    printf("\n");
  }
}