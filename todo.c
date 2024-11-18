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

void 
renderfilters() {
  // Filters 
  uint32_t itemcount = 6;
  static const char* items[] = {
    "ALL", "IN PROGRESS", "COMPLETED", "LOW", "MEDIUM", "HIGH"
  };

  // UI Properties
  LfUIElementProps props = lf_get_theme().button_props;
  props.margin_left = 10.0f;
  props.margin_right = 10.0f;
  props.margin_top = 20.0f;
  props.padding = 10.0f;
  props.border_width = 0.0f;
  props.color = LF_NO_COLOR;
  props.corner_radius = 8.0f;
  props.text_color = LF_WHITE;

  lf_push_font(&s.smallfont);

  // Calculating width
  float width = 0.0f;
  {
    float ptrx_before = lf_get_ptr_x();
    lf_push_style_props(props);
    lf_set_cull_end_x(s.winw);
    lf_set_cull_end_y(s.winh);
    lf_set_no_render(true);
    for(uint32_t i = 0; i < itemcount; i++) {
      lf_button(items[i]);
    }
    lf_unset_cull_end_x();
    lf_unset_cull_end_y();
    lf_set_no_render(false);
    width = lf_get_ptr_x() - ptrx_before - props.margin_right - props.padding;
  }

  lf_set_ptr_x_absolute(s.winw - width - GLOBAL_MARGIN);

  // Rendering the filter items
  lf_set_line_should_overflow(false);
  for(uint32_t i = 0; i < itemcount; i++) {
    // If the filter is currently selected, render a 
    // box around it to indicate selection.
    if(s.crnt_filter == (uint32_t)i) {
      props.color = (LfColor){255, 255, 255, 50};
    } else {
      props.color = LF_NO_COLOR;
    }
    // Rendering the button
    lf_push_style_props(props);
    if(lf_button(items[i]) == LF_CLICKED) {
      s.crnt_filter = i;
    }
    lf_pop_style_props();
    }
  // Popping props
  lf_set_line_should_overflow(true);
  lf_pop_style_props();
  lf_pop_font();
}

void 
renderentries() {
  lf_div_begin(((vec2s){lf_get_ptr_x(), lf_get_ptr_y()}), 
               ((vec2s){(s.winw - lf_get_ptr_x()) - GLOBAL_MARGIN, (s.winh - lf_get_ptr_y()) - GLOBAL_MARGIN}), 
               true);
  uint32_t renderedcount = 0;
  for(uint32_t i = 0; i < s.todo_entries.count; i++) {
    todo_entry* entry = s.todo_entries.entries[i];
    // Filtering the entries
    if(s.crnt_filter == FILTER_COMPLETED && !entry->completed) continue;
    if(s.crnt_filter == FILTER_IN_PROGRESS && entry->completed) continue;
    if(s.crnt_filter == FILTER_LOW && entry->priority != PRIORITY_LOW) continue;
    if(s.crnt_filter == FILTER_MEDIUM && entry->priority != PRIORITY_MEDIUM) continue;
    if(s.crnt_filter == FILTER_HIGH && entry->priority != PRIORITY_HIGH) continue;

     {
      float ptry_before = lf_get_ptr_y();
      float priority_size = 15.0f;
      lf_set_ptr_y_absolute(lf_get_ptr_y() + priority_size);
      lf_set_ptr_x_absolute(lf_get_ptr_x() + 5.0f);
      bool clicked_priority = lf_hovered((vec2s){lf_get_ptr_x(), lf_get_ptr_y()}, (vec2s){priority_size, priority_size}) &&
                              lf_mouse_button_went_down(GLFW_MOUSE_BUTTON_LEFT);
      if(clicked_priority) {
        if(entry->priority + 1 >= PRIORITY_COUNT) {
          entry->priority = 0;
        } else {
          entry->priority++;
        }
        sort_entries_by_priority(&s.todo_entries);
      }
      switch (entry->priority) {
        case PRIORITY_LOW: {
          lf_rect(priority_size, priority_size, (LfColor){76, 175, 80, 255}, 4.0f);
          break;
        }
        case PRIORITY_MEDIUM: {
          lf_rect(priority_size, priority_size, (LfColor){255, 235, 59, 255}, 4.0f);
          break;
        }
        case PRIORITY_HIGH: {
          lf_rect(priority_size, priority_size, (LfColor){244, 67, 54, 255}, 4.0f);
          break;
        }
        default:
          break;
      }
      lf_set_ptr_y_absolute(ptry_before);
    }
    {
      LfUIElementProps props = lf_get_theme().button_props;
      props.color = LF_NO_COLOR;
      props.border_width = 0.0f; props.padding = 0.0f; props.margin_top = 13; props.margin_left = 10.0f;
      lf_push_style_props(props);
      if(lf_image_button(((LfTexture){.id = s.removeicon.id, .width = 20, .height = 20})) == LF_CLICKED) {
        entries_da_remove_i(&s.todo_entries, i);
        serialize_todo_list(s.tododata_file, &s.todo_entries);
      }
      lf_pop_style_props();
    }
    {
      uint32_t texw = 15, texh = 8;
      lf_set_ptr_x_absolute(s.winw - GLOBAL_MARGIN - texw);
      lf_set_line_should_overflow(false);
      LfUIElementProps props = lf_get_theme().button_props;
      props.color = LF_NO_COLOR;
      props.border_width = 0.0f; props.padding = 0.0f; props.margin_left = 0.0f; props.margin_right = 0.0f;
      lf_push_style_props(props);
      lf_set_image_color((LfColor){120, 120, 120, 255});
      if(lf_image_button(((LfTexture){.id = s.raiseicon.id, .width = texw, .height = texh})) == LF_CLICKED) {
        todo_entry* tmp = s.todo_entries.entries[0];
        s.todo_entries.entries[0] = entry;
        s.todo_entries.entries[i] = tmp;
        serialize_todo_list(s.tododata_file, &s.todo_entries);
      }
      lf_unset_image_color();
      lf_set_line_should_overflow(true);
      lf_pop_style_props();
    }

    lf_next_line();
    renderedcount++;
  }
  if(!renderedcount) {
    lf_text("There is nothing here.");
  }
   
   lf_div_end();
}

void 
initwin() {
  // Initialize GLFW
  glfwInit();

  // Setting base window width
  s.winw = WIN_INIT_W;
  s.winh = WIN_INIT_H;
  // Creating & initializing window and UI library
  s.win = glfwCreateWindow(s.winw, s.winh, "todo", NULL, NULL);
  glfwMakeContextCurrent(s.win);
  glfwSetFramebufferSizeCallback(s.win, resizecb);
  lf_init_glfw(s.winw, s.winh, s.win);
}

void
initui() {
  // Initializing fonts
  s.titlefont = lf_load_font(FONT_BOLD, 40);
  s.smallfont = lf_load_font(FONT, 20);

  s.crnt_filter = FILTER_ALL;

  // Initializing base theme
  LfTheme theme = lf_get_theme();
  theme.div_props.color = LF_NO_COLOR;
  lf_free_font(&theme.font);
  theme.font = lf_load_font(FONT, 24);
  theme.scrollbar_props.corner_radius = 2;
  theme.scrollbar_props.color = lf_color_brightness(BG_COLOR, 3.0);
  theme.div_smooth_scroll = SMOOTH_SCROLL;
  lf_set_theme(theme);

  // Initializing retained state
  memset(s.new_task_input_buf, 0, INPUT_BUF_SIZE);
  s.new_task_input = (LfInputField){
    .width = 400,
    .buf = s.new_task_input_buf,
    .buf_size = INPUT_BUF_SIZE,
    .placeholder = (char*)"What is there to do?"
  };

  s.backicon = lf_load_texture(BACK_ICON, true, LF_TEX_FILTER_LINEAR);
  s.removeicon = lf_load_texture(REMOVE_ICON, true, LF_TEX_FILTER_LINEAR);
  s.raiseicon = lf_load_texture(RAISE_ICON, true, LF_TEX_FILTER_LINEAR);
  initentries();
}

void
  initentries() {
    strcat(s.tododata_file, TODO_DATA_DIR);
    strcat(s.tododata_file, "/");
    strcat(s.tododata_file, TODO_DATA_FILE);

    entries_da_init(&s.todo_entries);
    deserialize_todo_list(s.tododata_file, &s.todo_entries);
  }

void 
terminate() {
  // Terminate UI library
  lf_terminate();

  // Freeing allocated resources
  lf_free_font(&s.smallfont);
  lf_free_font(&s.titlefont);
  entries_da_free(&s.todo_entries); 

  // Terminate Windowing
  glfwDestroyWindow(s.win);
   glfwTerminate();
}

void 
renderdashboard() {
  rendertopbar();
  lf_next_line();
  renderfilters();
  lf_next_line();
  renderentries();
}

void 
rendernewtask() {
  // Title
  lf_push_font(&s.titlefont);
  {
    LfUIElementProps props = lf_get_theme().text_props;
    props.margin_bottom = 15.0f;
    lf_push_style_props(props);
    lf_text("Add a new Task");
    lf_pop_style_props();
    lf_pop_font();
  }

  lf_next_line();

  // Description input field 
  {
    lf_push_font(&s.smallfont);
    lf_text("Description");
    lf_pop_font();

    lf_next_line();
    LfUIElementProps props = lf_get_theme().inputfield_props;
    props.padding = 15;]
    props.border_width = 0;