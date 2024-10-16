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

static void         renderdashboard();
static void         rendernewtask();

static void         entries_da_init(entries_da* da);
static void         entries_da_resize(entries_da* da, int32_t new_cap);
static void         entries_da_push(entries_da* da, todo_entry* entry);  
static void         entries_da_remove_i(entries_da* da, uint32_t i); 
static void         entries_da_free(entries_da* da); 

static int          compare_entry_priority(const void* a, const void* b);
static void         sort_entries_by_priority(entries_da* da);

static char*        get_command_output(const char* cmd);

static void         serialize_todo_entry(FILE* file, todo_entry* entry);
static void         serialize_todo_list(const char* filename, entries_da* da);
static todo_entry*  deserialize_todo_entry(FILE* file);
static void         deserialize_todo_list(const char* filename, entries_da* da);

static void         print_requires_argument(const char* option, uint32_t numargs);
static void         str_to_lower(char* str);

static state s;

void 
resizecb(GLFWwindow* win, int32_t w, int32_t h) {
  s.winw = w;
  s.winh = h;
  lf_resize_display(w, h);
  glViewport(0, 0, w, h);
}

void 
rendertopbar() {
  // Title
  lf_push_font(&s.titlefont);
  {
    LfUIElementProps props = lf_get_theme().text_props;
    lf_push_style_props(props);
    lf_text("Your To Do");
    lf_pop_style_props();
  }
  lf_pop_font();

// Button
  {
    const char* text = "New Task";
    const float width = 150.0f;

    // UI Properties
    LfUIElementProps props = lf_get_theme().button_props;
    props.border_width = 0.0f;
    props.margin_top = 0.0f;
    props.color = SECONDARY_COLOR;
    props.corner_radius = 4.0f;
    lf_push_style_props(props);
    lf_set_ptr_x_absolute(s.winw - width - GLOBAL_MARGIN * 2.0f);

    // Button
    lf_set_line_should_overflow(false); 
    if(lf_button_fixed("New Task", width, -1) == LF_CLICKED) {
      s.crnt_tab = TAB_NEW_TASK;
    }
    lf_set_line_should_overflow(true);

    lf_pop_style_props();
  }
}

