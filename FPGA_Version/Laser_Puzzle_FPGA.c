/*--------------------------------------------
 |             Laser Puzzle Game             |
 |         Main Game Logic File (C)          |
 --------------------------------------------*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "background.h"
#include "bluenode.h"
#include "firewall.h"
#include "missionpassed.h"
#include "wasted.h"
/*----------------- Constants ----------------*/
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define TIMER_X 202
#define TIMER_Y 18
#define BLUENODE_WIDTH 11
#define BLUENODE_HEIGHT 11
#define NUM_MIRRORS 13
#define NUM_NODES 6
#define NUM_FIREWALLS 4
#define LASER_START_X 84
#define LASER_START_Y 81

#define BLACK_COLOR 0x0000
#define WHITE_COLOR 0xFFFF
#define LASER_COLOR 0x07FE

/*-------------- Global Variables ------------*/
volatile int *pixel_ctrl_ptr = (int *)0xFF203020;
volatile int *PS2_ptr = (int *)0xFF200100;
volatile int pixel_buffer_start;
short int Buffer1[SCREEN_HEIGHT][512];
short int Buffer2[SCREEN_HEIGHT][512];
volatile int frame_counter = 0;
volatile int seconds_left = 120;
int firewall_hit_counter[NUM_FIREWALLS] = {0};
int bluenode_hit_counter[NUM_NODES] = {0};
int firewall_destroyed = 0;
int bluenode_destroyed = 0;
int lives_left = 2;
int selected_mirror = 0;

/*------------- Type Definitions -------------*/
typedef struct {
  int x;
  int y;
  int angle;
} mirror_t;

typedef struct {
  int x;
  int y;
} node_t;

/*--------------- Game Objects ---------------*/
mirror_t mirrors[NUM_MIRRORS] = {
    {84, 175, 0},  {170, 175, 270}, {259, 175, 90}, {259, 108, 135},
    {259, 49, 45}, {180, 49, 135},  {180, 81, 90},  {157, 81, 0},
    {125, 81, 0},  {125, 195, 90},  {58, 195, 0},   {58, 140, 225},
    {58, 86, 135}};

node_t nodes[NUM_NODES] = {{180, 105}, {157, 65}, {73, 140},
                           {110, 81},  {58, 48},  {170, 138}};

node_t firewalls[NUM_FIREWALLS] = {
    {170, 194}, {235, 108}, {157, 100}, {71, 86}};

const uint8_t segment_patterns[10] = {
    0b0111111,  // 0
    0b0000110,  // 1
    0b1011011,  // 2
    0b1001111,  // 3
    0b1100110,  // 4
    0b1101101,  // 5
    0b1111101,  // 6
    0b0000111,  // 7
    0b1111111,  // 8
    0b1101111   // 9
};

const int segment_positions[7][4] = {{0, 0, 4, 0}, {4, 0, 4, 4}, {4, 4, 4, 8},
                                     {0, 8, 4, 8}, {0, 4, 0, 8}, {0, 0, 0, 4},
                                     {0, 4, 4, 4}};
/*----------- Function Declarations ----------*/
void plot_pixel(int x, int y, short int color);
void clear_screen();
void wait_for_vsync();
void draw_background();
void draw_mirror(int x, int y, int angle);
void read_ps2_keyboard();
void draw_timer();
void clear_timer_display();
void draw_segment(int x, int y, int segment, short int color);
void draw_digit(int x, int y, int num);
void convert_to_black_white();
void draw_char(int x, int y, char c, short int color);
void draw_wasted_text();
void handle_fail();
void handle_success();
void reset_game_state();
void draw_lives();

void simulate_laser();
int get_pixel_color(int x, int y);
int is_white(int color);
int is_black(int color);

void draw_bluenode(int x, int y);
void draw_firewall(int x, int y);

int main(void) {
  reset_game_state();
  *(pixel_ctrl_ptr + 1) = (int)&Buffer1;
  wait_for_vsync();
  pixel_buffer_start = *pixel_ctrl_ptr;
  clear_screen();

  *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
  pixel_buffer_start = *(pixel_ctrl_ptr + 1);
  clear_screen();

  while (1) {
    wait_for_vsync();
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);

    read_ps2_keyboard();

    frame_counter++;
    if (frame_counter >= 30) {
      frame_counter = 0;
      if (seconds_left > 0) {
        seconds_left--;
      }
    }

    draw_background();

    for (int i = 0; i < NUM_NODES; i++) {
      if (nodes[i].x >= 0 && nodes[i].y >= 0) {
        draw_bluenode(nodes[i].x - BLUENODE_WIDTH / 2,
                      nodes[i].y - BLUENODE_HEIGHT / 2);
      }
    }

    for (int i = 0; i < NUM_FIREWALLS; i++) {
      if (firewalls[i].x >= 0 && firewalls[i].y >= 0) {
        draw_firewall(firewalls[i].x - BLUENODE_WIDTH / 2,
                      firewalls[i].y - BLUENODE_HEIGHT / 2);
      }
    }

    for (int i = 0; i < NUM_MIRRORS; i++) {
      draw_mirror(mirrors[i].x, mirrors[i].y, mirrors[i].angle);
    }
    simulate_laser();
    draw_timer();
    draw_lives();

    if (seconds_left == 0) {
      wait_for_vsync();
      draw_timer();
      draw_lives();
      handle_fail();
      reset_game_state();
      clear_screen();
      draw_background();
      for (int i = 0; i < NUM_MIRRORS; i++) {
        draw_mirror(mirrors[i].x, mirrors[i].y, mirrors[i].angle);
      }
      for (int i = 0; i < NUM_NODES; i++) {
        if (nodes[i].x >= 0 && nodes[i].y >= 0)
          draw_bluenode(nodes[i].x - BLUENODE_WIDTH / 2,
                        nodes[i].y - BLUENODE_HEIGHT / 2);
      }
      for (int i = 0; i < NUM_FIREWALLS; i++) {
        if (firewalls[i].x >= 0 && firewalls[i].y >= 0)
          draw_firewall(firewalls[i].x - BLUENODE_WIDTH / 2,
                        firewalls[i].y - BLUENODE_HEIGHT / 2);
      }
      simulate_laser();
      draw_timer();
      draw_lives();
    }
  }
  return 0;
}

enum Direction { UP, DOWN, LEFT, RIGHT };
enum Direction get_reflected_direction(enum Direction dir, int angle) {
  switch (angle) {
    case 0:
    case 180:
      if (dir == LEFT) return RIGHT;
      if (dir == RIGHT) return LEFT;
      break;

    case 90:
    case 270:
      if (dir == UP) return DOWN;
      if (dir == DOWN) return UP;
      break;

    case 45:
      if (dir == DOWN) return LEFT;
      if (dir == RIGHT) return UP;
      if (dir == UP) return RIGHT;
      if (dir == LEFT) return DOWN;
      break;

    case 135:
      if (dir == DOWN) return RIGHT;
      if (dir == LEFT) return UP;
      if (dir == UP) return LEFT;
      if (dir == RIGHT) return DOWN;
      break;

    case 225:
      if (dir == DOWN) return LEFT;
      if (dir == RIGHT) return UP;
      if (dir == UP) return RIGHT;
      if (dir == LEFT) return DOWN;
      break;

    case 315:
      if (dir == DOWN) return RIGHT;
      if (dir == LEFT) return UP;
      if (dir == UP) return LEFT;
      if (dir == RIGHT) return DOWN;
      break;
  }
  return dir;
}

int is_point_on_mirror(int x, int y, mirror_t *mirror) {
  int dx = x - mirror->x;
  int dy = y - mirror->y;

  switch (mirror->angle) {
    case 0:
    case 180:
      return (abs(dx) <= 1) && (abs(dy) <= 5);
    case 90:
    case 270:
      return (abs(dy) <= 1) && (abs(dx) <= 5);
    case 45:
    case 225:
      return (abs(dx + dy) <= 1) && (abs(dx) <= 3) && (abs(dy) <= 3);
    case 135:
    case 315:
      return (abs(dx - dy) <= 1) && (abs(dx) <= 3) && (abs(dy) <= 3);
    default:
      return 0;
  }
}

int get_pixel_color(int x, int y) {
  volatile short int *addr =
      (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1));
  return *addr;
}

int is_white(int color) { return color == WHITE_COLOR; }

int is_black(int color) { return color == BLACK_COLOR; }

void simulate_laser() {
  int x = LASER_START_X;
  int y = LASER_START_Y;
  enum Direction dir = DOWN;
  int step = 0;

  while (step < 1000) {
    int nx = x, ny = y;
    switch (dir) {
      case UP:
        ny--;
        break;
      case DOWN:
        ny++;
        break;
      case LEFT:
        nx--;
        break;
      case RIGHT:
        nx++;
        break;
    }

    if (nx < 0 || nx >= SCREEN_WIDTH || ny < 0 || ny >= SCREEN_HEIGHT) break;

    int color = get_pixel_color(nx, ny);
    if (is_black(color)) break;

    if (color == 0x0800) {
      for (int i = 0; i < NUM_FIREWALLS; i++) {
        int fx = firewalls[i].x;
        int fy = firewalls[i].y;

        if (nx >= fx - BLUENODE_WIDTH / 2 && nx <= fx + BLUENODE_WIDTH / 2 &&
            ny >= fy - BLUENODE_HEIGHT / 2 && ny <= fy + BLUENODE_HEIGHT / 2) {
          firewall_hit_counter[i]++;
          if (firewall_hit_counter[i] == 45) {
            firewalls[i].x = -100;
            firewalls[i].y = -100;
            firewall_destroyed++;
            lives_left--;
            if (firewall_destroyed >= 2) {
              handle_fail();
            }
          }
          break;
        }
      }
      break;
    } else if (color == 0x0001) {
      for (int i = 0; i < NUM_NODES; i++) {
        int bx = nodes[i].x;
        int by = nodes[i].y;

        if (nx >= bx - BLUENODE_WIDTH / 2 && nx <= bx + BLUENODE_WIDTH / 2 &&
            ny >= by - BLUENODE_HEIGHT / 2 && ny <= by + BLUENODE_HEIGHT / 2) {
          bluenode_hit_counter[i]++;
          if (bluenode_hit_counter[i] == 45) {
            nodes[i].x = -100;
            nodes[i].y = -100;
            bluenode_destroyed++;

            if (bluenode_destroyed >= NUM_NODES) {
              handle_success();
            }
          }
          break;
        }
      }
      break;
    }

    if (color == 0xFFFF) {
      dir = get_reflected_direction(dir, 135);
    } else {
      int mirror_hit = -1;
      for (int i = 0; i < NUM_MIRRORS; i++) {
        int mx = mirrors[i].x;
        int my = mirrors[i].y;
        if (nx >= mx - 5 && nx <= mx + 5 && ny >= my - 5 && ny <= my + 5) {
          mirror_hit = i;
          break;
        }
      }
      if (mirror_hit != -1) {
        dir = get_reflected_direction(dir, mirrors[mirror_hit].angle);
      }
    }

    plot_pixel(x, y, LASER_COLOR);
    x = nx;
    y = ny;
    step++;
  }
}

void clear_timer_display() {
  for (int i = 0; i < 50; i++) {
    for (int j = 0; j < 13; j++) {
      plot_pixel(TIMER_X + i, TIMER_Y + j,
                 background[TIMER_Y + j][TIMER_X + i]);
    }
  }
}

void draw_timer() {
  int minutes = seconds_left / 60;
  int seconds = seconds_left % 60;

  clear_timer_display();

  draw_digit(TIMER_X + 8, TIMER_Y, minutes % 10);

  plot_pixel(TIMER_X + 16, TIMER_Y + 4, WHITE_COLOR);
  plot_pixel(TIMER_X + 16, TIMER_Y + 8, WHITE_COLOR);

  draw_digit(TIMER_X + 20, TIMER_Y, seconds / 10);
  draw_digit(TIMER_X + 28, TIMER_Y, seconds % 10);
}

void read_ps2_keyboard() {
  static int expecting_break = 0;
  while (*PS2_ptr & 0x8000) {
    int PS2_data = *PS2_ptr & 0xFF;

    if (PS2_data == 0xF0) {
      expecting_break = 1;
      continue;
    }

    if (expecting_break) {
      expecting_break = 0;
    } else {
      switch (PS2_data) {
        case 0x74:
          mirrors[selected_mirror].angle =
              (mirrors[selected_mirror].angle + 45) % 360;
          break;
        case 0x6B:
          mirrors[selected_mirror].angle =
              (mirrors[selected_mirror].angle - 45 + 360) % 360;
          break;
        case 0x75:
          selected_mirror = (selected_mirror - 1 + NUM_MIRRORS) % NUM_MIRRORS;
          break;
        case 0x72:
          selected_mirror = (selected_mirror + 1) % NUM_MIRRORS;
          break;
      }
    }
  }
}

void draw_segment(int x, int y, int segment, short int color) {
  int x1 = x + segment_positions[segment][0];
  int y1 = y + segment_positions[segment][1];
  int x2 = x + segment_positions[segment][2];
  int y2 = y + segment_positions[segment][3];

  if (x1 == x2) {
    for (int i = y1; i <= y2; i++) {
      plot_pixel(x1, i, color);
      plot_pixel(x1 + 1, i, color);
    }
  } else {
    for (int i = x1; i <= x2; i++) {
      plot_pixel(i, y1, color);
      plot_pixel(i, y1 + 1, color);
    }
  }
}

void draw_digit(int x, int y, int num) {
  uint8_t pattern = segment_patterns[num];
  for (int i = 0; i < 7; i++) {
    if (pattern & (1 << i)) {
      draw_segment(x, y, i, WHITE_COLOR);
    }
  }
}

void wait_for_vsync() {
  *pixel_ctrl_ptr = 1;
  while (*(pixel_ctrl_ptr + 3) & 1);
}

void clear_screen() {
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      plot_pixel(x, y, 0x0000);
    }
  }
}
void draw_background() {
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      plot_pixel(x, y, background[y][x]);
    }
  }
}

void plot_pixel(int x, int y, short int color) {
  volatile short int *one_pixel_address =
      (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1));
  *one_pixel_address = color;
}

void draw_mirror(int x, int y, int angle) {
  int center_x = x;
  int center_y = y;

  if (angle % 90 == 0) {
    if (angle == 0 || angle == 180) {
      for (int i = -5; i <= 5; i++) {
        plot_pixel(center_x - 1, center_y + i, 0xFFFF);
        plot_pixel(center_x, center_y + i, 0x7BEF);
        plot_pixel(center_x + 1, center_y + i, 0xFFFF);
      }
    } else if (angle == 90 || angle == 270) {
      for (int i = -5; i <= 5; i++) {
        plot_pixel(center_x + i, center_y - 1, 0xFFFF);
        plot_pixel(center_x + i, center_y, 0x7BEF);
        plot_pixel(center_x + i, center_y + 1, 0xFFFF);
      }
    }
  } else if (angle == 45 || angle == 225) {
    int dx = 1;
    int dy = -1;
    int length = 7;

    for (int i = -length / 2; i <= length / 2; i++) {
      plot_pixel(center_x + i * dx, center_y + i * dy, 0x7BEF);
    }

    for (int i = -length / 2; i <= length / 2 + 1; i++) {
      int px = center_x + i * dx;
      int py = center_y + i * dy;
      plot_pixel(px - dx, py - dy - 1, 0xFFFF);
      plot_pixel(px + dx - 1, py + dy + 2, 0xFFFF);
    }
  } else if (angle == 135 || angle == 315) {
    int dx = -1;
    int dy = -1;
    int length = 7;

    for (int i = -length / 2; i <= length / 2; i++) {
      plot_pixel(center_x + i * dx, center_y + i * dy, 0x7BEF);
    }

    for (int i = -length / 2; i <= length / 2 + 1; i++) {
      int px = center_x + i * dx;
      int py = center_y + i * dy;
      plot_pixel(px - dx, py - dy - 1, 0xFFFF);
      plot_pixel(px + dx + 1, py + dy + 2, 0xFFFF);
    }
  }
}

void handle_success() {
  *(pixel_ctrl_ptr + 1) = 0;

  for (int i = 0; i < 2; i++) {
    volatile short int *current_buffer =
        (i == 0) ? (short int *)Buffer1 : (short int *)Buffer2;

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
      for (int x = 0; x < SCREEN_WIDTH; x++) {
        short int color = current_buffer[y * 512 + x];
        int gray = ((color & 0xF800) >> 11) * 299 +
                   ((color & 0x07E0) >> 5) * 587 + (color & 0x001F) * 114;
        gray /= 1000;
        current_buffer[y * 512 + x] =
            ((gray >> 3) << 11) | ((gray >> 2) << 5) | (gray >> 3);
      }
    }

    int text_x = (SCREEN_WIDTH - 200) / 2;
    int text_y = (SCREEN_HEIGHT - 59) / 2;
    for (int row = 0; row < 59; row++) {
      for (int col = 0; col < 200; col++) {
        if (missionpassed[row][col] != 0x07E0) {
          current_buffer[(text_y + row) * 512 + (text_x + col)] =
              missionpassed[row][col];
        }
      }
    }
  }
  int restart = 0;
  reset_game_state();

  while (!restart) {
    *(pixel_ctrl_ptr + 1) = (int)Buffer1;
    wait_for_vsync();
    *(pixel_ctrl_ptr + 1) = (int)Buffer2;
    wait_for_vsync();

    while (*PS2_ptr & 0x8000) {
      int data = *PS2_ptr & 0xFF;
      static int expecting_break = 0;

      if (data == 0xF0) {
        expecting_break = 1;
      } else if (expecting_break) {
        expecting_break = 0;
        if (data == 0x75 || data == 0x72 || data == 0x6B || data == 0x74) {
          restart = 1;
        }
      }
    }
  }
}

void handle_fail() {
  *(pixel_ctrl_ptr + 1) = 0;

  for (int i = 0; i < 2; i++) {
    volatile short int *current_buffer =
        (i == 0) ? (short int *)Buffer1 : (short int *)Buffer2;

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
      for (int x = 0; x < SCREEN_WIDTH; x++) {
        short int color = current_buffer[y * 512 + x];
        int gray = ((color & 0xF800) >> 11) * 299 +
                   ((color & 0x07E0) >> 5) * 587 + (color & 0x001F) * 114;
        gray /= 1000;
        current_buffer[y * 512 + x] =
            ((gray >> 3) << 11) | ((gray >> 2) << 5) | (gray >> 3);
      }
    }

    int text_x = (SCREEN_WIDTH - 120) / 2;
    int text_y = (SCREEN_HEIGHT - 35) / 2;
    for (int row = 0; row < 35; row++) {
      for (int col = 0; col < 120; col++) {
        if (wasted[row][col] != 0x4389) {
          current_buffer[(text_y + row) * 512 + (text_x + col)] =
              wasted[row][col];
        }
      }
    }
  }
  reset_game_state();
  int restart = 0;

  while (!restart) {
    *(pixel_ctrl_ptr + 1) = (int)Buffer1;
    wait_for_vsync();
    *(pixel_ctrl_ptr + 1) = (int)Buffer2;
    wait_for_vsync();

    while (*PS2_ptr & 0x8000) {
      int data = *PS2_ptr & 0xFF;
      static int expecting_break = 0;

      if (data == 0xF0) {
        expecting_break = 1;
      } else if (expecting_break) {
        expecting_break = 0;
        if (data == 0x75 || data == 0x72 || data == 0x6B || data == 0x74) {
          restart = 1;
        }
      }
    }
  }
}

void convert_to_black_white() {
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      volatile short int *pixel_addr =
          (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1));

      short int color = *pixel_addr;

      int r = (color >> 11) & 0x1F;
      int g = (color >> 5) & 0x3F;
      int b = (color) & 0x1F;
      int gray = (r * 299 + g * 587 + b * 114) / 1000;

      short int gray_color = ((gray * 31 / 255) << 11) |
                             ((gray * 63 / 255) << 5) | ((gray * 31 / 255));
      *pixel_addr = gray_color;
    }
  }
}

void draw_bluenode(int x, int y) {
  for (int row = 0; row < BLUENODE_HEIGHT; row++) {
    for (int col = 0; col < BLUENODE_WIDTH; col++) {
      uint16_t color = bluenode[row][col];
      if (color != 0x07E0) {
        plot_pixel(x + col, y + row, color);
      }
    }
  }
}

void draw_firewall(int x, int y) {
  for (int row = 0; row < BLUENODE_HEIGHT; row++) {
    for (int col = 0; col < BLUENODE_WIDTH; col++) {
      uint16_t color = firewall[row][col];
      if (color != 0x07E0) {
        plot_pixel(x + col, y + row, color);
      }
    }
  }
}

void draw_lives() {
  int x = 265;
  int y = 18;
  for (int dx = 0; dx < 6; dx++) {
    for (int dy = 0; dy < 10; dy++) {
      plot_pixel(x + dx, y + dy, background[y + dy][x + dx]);
    }
  }
  draw_digit(x, y, lives_left);
}

void reset_game_state() {
  seconds_left = 120;
  frame_counter = 0;
  selected_mirror = 0;
  lives_left = 2;
  firewall_destroyed = 0;
  bluenode_destroyed = 0;
  for (int i = 0; i < NUM_FIREWALLS; i++) firewall_hit_counter[i] = 0;
  for (int i = 0; i < NUM_NODES; i++) bluenode_hit_counter[i] = 0;
  const node_t firewall_init[NUM_FIREWALLS] = {
      {170, 194}, {235, 108}, {157, 100}, {71, 86}};
  for (int i = 0; i < NUM_FIREWALLS; i++) {
    firewalls[i] = firewall_init[i];
    firewall_hit_counter[i] = 0;
  }

  nodes[0].x = 180;
  nodes[0].y = 105;
  nodes[1].x = 157;
  nodes[1].y = 65;
  nodes[2].x = 73;
  nodes[2].y = 140;
  nodes[3].x = 110;
  nodes[3].y = 81;
  nodes[4].x = 58;
  nodes[4].y = 48;
  nodes[5].x = 170;
  nodes[5].y = 138;

  mirrors[0].angle = 0;
  mirrors[1].angle = 270;
  mirrors[2].angle = 90;
  mirrors[3].angle = 135;
  mirrors[4].angle = 45;
  mirrors[5].angle = 135;
  mirrors[6].angle = 90;
  mirrors[7].angle = 0;
  mirrors[8].angle = 0;
  mirrors[9].angle = 90;
  mirrors[10].angle = 0;
  mirrors[11].angle = 225;
  mirrors[12].angle = 135;
}
