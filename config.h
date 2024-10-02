#pragma once

#define WIN_INIT_W 1280
#define WIN_INIT_H 720

#define BG_COLOR (LfColor){6, 6, 6, 255}

#define FONT "/usr/share/todo/fonts/inter.ttf"
#define FONT_BOLD "/usr/share/todo/fonts/inter-bold.ttf"

#define TODO_DATA_DIR getenv("HOME")
#define TODO_DATA_FILE ".tododata"

#define BACK_ICON "/usr/share/todo/icons/back.png"
#define REMOVE_ICON "/usr/share/todo/icons/remove.png"
#define RAISE_ICON "/usr/share/todo/icons/raise.png"

#define GLOBAL_MARGIN 25.0f
#define INPUT_BUF_SIZE 512