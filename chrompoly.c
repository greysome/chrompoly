/*
 * Copyright (C) 2023-2023 Way Yan Win
 * This code is under the MIT License.
 */
#include "array.h"
#include "debug.h"
#include "raylib/raylib.h"
#include "submap.c"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <threads.h>
#include <unistd.h>

#define OVERLAY_START_Y 0.8 * screen_height
#define OVERLAY_COLOR ((Color){0xC1, 0xD4, 0xD4, 0x99})
#define BG_COLOR ((Color){0xE1, 0xF4, 0xFA, 0xFF})
#define NODE_SELECT_COLOR ORANGE
#define NODE_DESELECT_COLOR BLACK

#define NODE_SIZE 20.0
#define PLOP_ANIMATION_FRAMES 30.0
#define DISAPPEAR_ANIMATION_FRAMES 30.0

#define QUARTIC_EASE(x) (1.0 - pow(1.0 - x, 4))

/* A vertex but with extra fields for rendering purposes */
typedef struct {
  int x;
  int y;
  bool selected;
  bool deleted;
  int plop_animation_timer;
  int disappear_animation_timer;
} Node;
array_def(Node, node);

array_node nodes;
array_edge edges;

/* Count number of nodes not marked deleted */
unsigned int active_node_count(array_node *nodes) {
  unsigned int count = 0;
  array_foreach(nodes, Node node) {
    if (!node.deleted)
      count++;
  }
  return count;
}

/* Get the index of the ith active node in nodes */
unsigned int get_ith_active_node_idx(array_node *nodes, unsigned int i) {
  array_enumerate(nodes, j, Node node) {
    if (!node.deleted)
      i--;
    if (i == -1)
      return j;
  }
}

void deselect_all_nodes(array_node *nodes) {
  array_enumerate(nodes, i, Node node) { array_at(nodes, i).selected = false; }
}

/*
 * Returns the first node in `nodes` that is positioned at most NODE_SIZE
 * distance away from the coordinates (x,y),
 * or -1 if no such node was found.
 */
int get_node_idx_at_coords(array_node *nodes, int x, int y) {
  array_enumerate(nodes, i, Node node) {
    int dx = node.x - x;
    int dy = node.y - y;
    if (!node.deleted && dx * dx + dy * dy < NODE_SIZE * NODE_SIZE) {
      return i;
    }
  }
  return -1;
}

void draw_node_at_idx(array_node *nodes, unsigned int i) {
  Node node = array_at(nodes, i);
  Color color;
  float size;

  // Determine color and size based on node state and animation timers
  if (node.deleted) {
    color = NODE_SELECT_COLOR;
    size =
        NODE_SIZE * (1.0 - QUARTIC_EASE(1.0 - node.disappear_animation_timer /
                                                  PLOP_ANIMATION_FRAMES));
  } else {
    color = node.selected ? NODE_SELECT_COLOR : NODE_DESELECT_COLOR;
    size = NODE_SIZE * QUARTIC_EASE(1.0 - node.plop_animation_timer /
                                              PLOP_ANIMATION_FRAMES);
  }

  // Draw shadow when dragging selected node
  if (node.selected && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    DrawCircle(node.x + 5, node.y + 5, size, (Color){0, 0, 0, 125});
  }
  // Draw the node itself
  DrawCircle(node.x, node.y, size, color);

  // Update animation timers
  if (node.plop_animation_timer > 0) {
    array_at(nodes, i).plop_animation_timer--;
  }
  if (node.deleted && node.disappear_animation_timer > 0) {
    array_at(nodes, i).disappear_animation_timer--;
  }
}

/*
 * Adds edge if the pair (selected_idx, end_idx) does not appear in an
 * existing edge.
 */
void add_edge(array_edge *edges, unsigned int start_idx, unsigned int end_idx) {
  Edge new_edge = {.start_idx = start_idx, .end_idx = end_idx};
  if (array_size(edges) == 0) {
    array_add(edges, new_edge);
    return;
  }
  array_foreach(edges, Edge edge) {
    if ((edge.start_idx == start_idx && edge.end_idx == end_idx) ||
        (edge.start_idx == end_idx && edge.end_idx == start_idx)) {
      return;
    }
  }
  array_add(edges, new_edge);
}

void draw_edge_at_idx(array_node *nodes, array_edge *edges, unsigned int i) {
  // The argument i ranges through all active nodes. Convert this to an index
  // ranging over all nodes, including those marked deleted.
  Edge edge = array_at(edges, i);
  Node start = array_at(nodes, get_ith_active_node_idx(nodes, edge.start_idx));
  Node end = array_at(nodes, get_ith_active_node_idx(nodes, edge.end_idx));
  DrawLineEx((Vector2){start.x, start.y}, (Vector2){end.x, end.y}, 4.0,
             DARKGRAY);
}

mtx_t graph_changed_mutex;
mtx_t nodes_mutex;
array_submap all_submaps;
array_int chromatic_polynomial;
bool is_running = true;
int output[1000]; // list of codepoints
int output_len = 0;

void polynomial_print(array_int *P) {
  for (int i = array_size(P) - 1; i >= 0; i--) {
    int x = array_at(P, i);
    if (x > 0)
      printf("+ %dx^%d ", x, i + 1);
    else if (x < 0)
      printf("- %dx^%d ", -x, i + 1);
  }
  printf("\n");
}

int get_ascii_codepoint(char c) {
  if ('a' <= c && c <= 'z')
    return 0x61 + (c - 'a');
  else if ('A' <= c && c <= 'Z')
    return 0x41 + (c - 'A');
  else if ('0' <= c && c <= '9')
    return 0x30 + (c - '0');
  else if (c == ' ')
    return 0x20;
  else if (c == '/')
    return 0x2f;
  else if (c == '+')
    return 0x2b;
  else if (c == '-')
    return 0x2d;
}

int get_superscript_codepoint(int digit) {
  if (digit == 0)
    return 0x2070;
  else if (digit == 1)
    return 0xb9;
  else if (digit == 2 || digit == 3)
    return 0xb2 + (digit - 2);
  else
    return 0x2074 + (digit - 4);
}

void add_ascii_char_to_output(char c) {
  output[output_len++] = get_ascii_codepoint(c);
}

void add_ascii_string_to_output(char *s, int n) {
  for (int i = 0; i < n; i++)
    add_ascii_char_to_output(s[i]);
}

void add_number_to_output(int n) {
  int d = num_digits(n);
  for (int i = d - 1; i >= 0; i--) {
    int div = ten_pow(i);
    output[output_len++] = 0x30 + n / div; // 0-9
    n %= div;
  }
}

void add_superscript_number_to_output(int n) {
  int d = num_digits(n);
  for (int i = d - 1; i >= 0; i--) {
    int div = ten_pow(i);
    // Superscript 0-9
    output[output_len++] = get_superscript_codepoint(n / div);
    n %= div;
  }
}

void set_output_to_loading() {
  memset(output, 0, sizeof(output));
  output_len = 0;
  add_ascii_string_to_output("...", 3);
}

void set_output_to_cur_submap_count() {
  memset(output, 0, sizeof(output));
  output_len = 0;
  // "Found <n> submaps"
  add_ascii_string_to_output("Found ", 6);
  add_number_to_output(cur_submap_count);
  add_ascii_string_to_output(" submaps", 8);
}

void set_output_to_cur_matrix_rows_count() {
  memset(output, 0, sizeof(output));
  output_len = 0;
  // "Processing <n>/<total> submaps"
  add_ascii_string_to_output("Processing ", 11);
  add_number_to_output(cur_matrix_rows_count);
  add_ascii_char_to_output('/');
  add_number_to_output(array_size(&all_submaps));
  add_ascii_string_to_output(" submaps", 8);
}

int ten_pow(int n) {
  int res = 1;
  for (int i = 0; i < n; i++)
    res *= 10;
  return res;
}

int num_digits(int n) {
  int i = 0;
  while (n >= 1) {
    i++;
    n /= 10;
  }
  return i;
}

void set_output_to_polynomial(array_int *P) {
  memset(output, 0, sizeof(output));
  output_len = 0;

  int degree; // The largest power of x that divides P
  array_enumerate(P, power, int coeff) {
    if (coeff != 0) {
      degree = power + 1;
      break;
    }
  }

  for (int power = array_size(P); power >= 1; power--) {
    int coeff = array_at(P, power - 1);
    // Sign
    if (coeff > 0 && power < array_size(P) - 1)
      add_ascii_char_to_output('+');
    else if (coeff < 0)
      add_ascii_char_to_output('-');

    // Coefficient
    if (coeff < 0)
      coeff = -coeff;
    if (coeff != 0 && coeff != 1)
      add_number_to_output(coeff);

    // x^n
    if (power >= degree) {
      add_ascii_char_to_output('x');
      if (power > 1)
        add_superscript_number_to_output(power);
    }
  }
}

int calculate(void *arg) {
  Submap submap;
  WYMatrix M;
  array_init(&all_submaps);
  array_init(&chromatic_polynomial);

  set_output_to_loading();

  while (is_running) {
    if (!graph_changed)
      continue;

    // No need to free all_submaps here, because it is already
    // done in get_all_submaps
    array_term(&chromatic_polynomial);

    mtx_lock(&graph_changed_mutex);
    graph_changed = false;
    mtx_unlock(&graph_changed_mutex);

    memset(output, 0, sizeof(output));
    set_output_to_loading();

    // The actual calculation
    mtx_lock(&nodes_mutex);
    int n = active_node_count(&nodes);
    mtx_unlock(&nodes_mutex);
    if (n == 0)
      continue;
    submap = from_graph(n, edges);
    all_submaps = get_all_submaps(&submap);
    matrix_free(M);
    M = get_ordering_matrix(&all_submaps);
    // matrix_print(M);
    chromatic_polynomial = get_chromatic_polynomial(n, &all_submaps, M);
    // polynomial_print(&chromatic_polynomial);

    set_output_to_polynomial(&chromatic_polynomial);
  }
  return 0;
}

int main() {
  int screen_width = 1000;
  int screen_height = 600;
  int mouse_x, mouse_y, mouse_dx, mouse_dy;
  // -1 means that no node is selected
  int selected_idx = -1;
  const int codepoints[] = {
      0x61,   0x62,   0x63, 0x64,   0x65,   0x66,   0x67,   0x68,   0x69,
      0x6a,   0x6b,   0x6c, 0x6d,   0x6e,   0x6f,   0x70,   0x71,   0x72,
      0x73,   0x74,   0x75, 0x76,   0x77,   0x78,   0x79,   0x41,   0x42,
      0x43,   0x44,   0x45, 0x46,   0x47,   0x48,   0x49,   0x4a,   0x4b,
      0x4c,   0x4d,   0x4e, 0x4f,   0x50,   0x51,   0x52,   0x53,   0x54,
      0x55,   0x56,   0x57, 0x58,   0x59,   0x30,   0x31,   0x32,   0x33,
      0x34,   0x35,   0x36, 0x37,   0x38,   0x39,   0x20,   0x2e,   0x2f,
      0x3a,   0xb2,   0xb3, 0x2070, 0x00B9, 0x2074, 0x2075, 0x2076, 0x2077,
      0x2078, 0x2079, 0x2d, 0x2b};

  array_init(&nodes);
  array_init(&edges);
  bool edging = false; // Is user currently creating an edge by dragging?

  InitWindow(screen_width, screen_height, "wygraph");
  SetTargetFPS(60);

  Font font = LoadFontEx("resources/Rubik-Regular.ttf", 24, codepoints,
                         sizeof(codepoints) / sizeof(int));

  thrd_t calculation_thread;
  if (thrd_create(&calculation_thread, calculate, NULL) != thrd_success) {
    printf("Failed to create thread\n");
    return 1;
  }

  while (!WindowShouldClose()) {
    screen_width = GetScreenWidth();
    screen_height = GetScreenHeight();
    mouse_x = GetMouseX();
    mouse_y = GetMouseY();
    mouse_dx = GetMouseDelta().x;
    mouse_dy = GetMouseDelta().y;

    // If node is marked deleted and its disappear animation has completed,
    // delete it from memory.
    array_enumerate(&nodes, i, Node node) {
      if (node.deleted && node.disappear_animation_timer <= 0) {
        array_del(&nodes, i);
        if (selected_idx > i)
          selected_idx--;
        i--;
      }
    }
    // Deselect all nodes
    if (IsKeyPressed(KEY_Z)) {
      deselect_all_nodes(&nodes);
      selected_idx = -1;
    }
    // Mark node as deleted and start disappear animation timer
    // (node is only removed from memory when the animation finishes)
    if (IsKeyPressed(KEY_X) && selected_idx >= 0) {
      array_at(&nodes, selected_idx).deleted = true;
      array_at(&nodes, selected_idx).disappear_animation_timer =
          DISAPPEAR_ANIMATION_FRAMES;

      array_enumerate(&edges, i, Edge edge) {
        // Remove edges to/from node marked deleted
        if (edge.start_idx == selected_idx || edge.end_idx == selected_idx) {
          mtx_lock(&nodes_mutex);
          array_del(&edges, i);
          mtx_unlock(&nodes_mutex);
          i--;
          continue;
        }
      }
      array_enumerate(&edges, i, Edge edge) {
        if (edge.start_idx > selected_idx)
          array_at(&edges, i).start_idx--;
        if (edge.end_idx > selected_idx)
          array_at(&edges, i).end_idx--;
      }

      // Deselect all nodes
      deselect_all_nodes(&nodes);
      selected_idx = -1;

      // Indicate to whole program that graph has changed, so that threads
      // can redo computation
      mtx_lock(&graph_changed_mutex);
      graph_changed = true;
      mtx_unlock(&graph_changed_mutex);
    }
    // Left click creates a new node or selects an existing one
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      deselect_all_nodes(&nodes);
      selected_idx = get_node_idx_at_coords(&nodes, mouse_x, mouse_y);
      if (selected_idx >= 0) {
        // selected_idx = get_ith_active_node_idx(&nodes, selected_idx);
        array_at(&nodes, selected_idx).selected = true;
      } else {
        Node new_node = {.x = mouse_x,
                         .y = mouse_y,
                         .selected = true,
                         .deleted = false,
                         .plop_animation_timer = PLOP_ANIMATION_FRAMES,
                         .disappear_animation_timer = 0};
        array_add(&nodes, new_node);
        selected_idx = array_size(&nodes) - 1;
        mtx_lock(&graph_changed_mutex);
        graph_changed = true;
        mtx_unlock(&graph_changed_mutex);
      }
    }
    // Hold left click to drag selected node
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      if (selected_idx >= 0) {
        array_at(&nodes, selected_idx).x += mouse_dx;
        array_at(&nodes, selected_idx).y += mouse_dy;
      }
    }
    // Start creating edge
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && selected_idx >= 0) {
      edging = true;
    }
    // If user deselected node while creating edge
    // (e.g. by pressing 'z' or deleting it),
    // cancel edge.
    if (edging && selected_idx < 0) {
      edging = false;
    }
    // Create new edge if the end point is at another node,
    // otherwise cancel the edge.
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && edging) {
      int end_idx = get_node_idx_at_coords(&nodes, mouse_x, mouse_y);
      if (end_idx >= 0 && selected_idx != end_idx) {
        add_edge(&edges, selected_idx, end_idx);
        mtx_lock(&graph_changed_mutex);
        graph_changed = true;
        mtx_unlock(&graph_changed_mutex);
      }
      edging = false;
    }

    BeginDrawing();

    ClearBackground(BG_COLOR);

    // Draw the preview edge that user is dragging around
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && edging) {
      Node selected_node = array_at(&nodes, selected_idx);
      DrawLine(selected_node.x, selected_node.y, mouse_x, mouse_y, DARKGRAY);
    }
    // Draw edges
    array_enumerate(&edges, i, Edge edge) {
      draw_edge_at_idx(&nodes, &edges, i);
    }
    // Draw deselected nodes first
    array_enumerate(&nodes, i, Node node) {
      if (i != selected_idx) {
        draw_node_at_idx(&nodes, i);
      }
    }
    // Draw selected node on top
    if (selected_idx >= 0) {
      draw_node_at_idx(&nodes, selected_idx);
    }

    DrawRectangle(0, OVERLAY_START_Y, screen_width, screen_height, OVERLAY_COLOR);
    DrawTextEx(font,
               "Chromatic polynomial:", (Vector2){10, OVERLAY_START_Y+10},
               24.0, 0.0, BLACK);
    if (cur_submap_count > 0 && cur_matrix_rows_count == 0)
      set_output_to_cur_submap_count();
    else if (cur_matrix_rows_count > 0)
      set_output_to_cur_matrix_rows_count();
    Vector2 CP_text_dims = MeasureTextEx(font, "Chromatic polynomial:", 24.0, 0.0);
    DrawTextCodepoints(font, output, output_len,
                       (Vector2){CP_text_dims.x+20, OVERLAY_START_Y+10}, 24.0, 0.0,
                       DARKGRAY);

    EndDrawing();
  }

  is_running = false;
  if (thrd_join(calculation_thread, NULL) != thrd_success) {
    printf("Error joining thread\n");
    return 1;
  }
  free_submaps(&all_submaps);
  array_term(&chromatic_polynomial);

  CloseWindow();
  return 0;
}