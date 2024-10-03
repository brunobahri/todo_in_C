#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <leif/leif.h>
#include <stdint.h>
#include <string.h>

#include "config.h"

typedef enum {
  FILTER_ALL = 0,
  FILTER_IN_PROGRESS,
  FILTER_COMPLETED,
  FILTER_LOW,
  FILTER_MEDIUM,
  FILTER_HIGH
} todo_filter;

typedef enum {
  TAB_DASHBOARD = 0,
  TAB_NEW_TASK
} tab;

typedef enum {
  PRIORITY_LOW = 0,
  PRIORITY_MEDIUM,
  PRIORITY_HIGH, 
  PRIORITY_COUNT
} entry_priority;

typedef struct {
  bool completed;
  char* desc, *date;

  entry_priority priority;
} todo_entry;

typedef struct {
  todo_entry** entries;
  uint32_t count, cap;
} entries_da;

typedef struct {
  GLFWwindow* win;
  int32_t winw, winh;

  todo_filter crnt_filter;
  tab crnt_tab;
  entries_da todo_entries;

  LfFont titlefont, smallfont;

  LfInputField new_task_input;
  char new_task_input_buf[INPUT_BUF_SIZE];
  LfTexture backicon, removeicon, raiseicon;

  FILE* serialization_file;

  char tododata_file[128];
} state;

static void         resizecb(GLFWwindow* win, int32_t w, int32_t h);
static void         rendertopbar();
static void         renderfilters();
static void         renderentries();

static void         initwin();
static void         initui();
static void         initentries();
static void         terminate();
