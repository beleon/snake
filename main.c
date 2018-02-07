#include <stdlib.h>
#include <curses.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#define ASSERT(N, X) {if (X != OK) { \
  fprintf(stderr, "%s call returned error: %d\n", N, X); \
  end_game(&game, timer); \
  exit(1);}}

#define DRAW() prefresh(board, 0, 0, BOARD_Y_OFF, BOARD_X_OFF, BOARD_HEIGHT + BOARD_Y_OFF, BOARD_WIDTH + BOARD_X_OFF); \
  prefresh(score_board, 0, 0, SCORE_BOARD_Y_OFF, SCORE_BOARD_X_OFF, BOARD_HEIGHT + SCORE_BOARD_Y_OFF, SCORE_BOARD_WIDTH + SCORE_BOARD_X_OFF)


#define MAX_DIR_MEMORY 10
#define BOARD_HEIGHT 15
#define BOARD_WIDTH 30
#define BOARD_X_OFF 8
#define BOARD_Y_OFF 1
#define SCORE_BOARD_WIDTH 20
#define SCORE_BOARD_HEIGHT 10
#define SCORE_BOARD_X_OFF BOARD_WIDTH + BOARD_X_OFF * 2
#define SCORE_BOARD_Y_OFF BOARD_Y_OFF + 1
#define DEFAULT_SNAKE_BODY_ARRAY_SIZE 100

bool walk = FALSE;

typedef struct {
  int x;
  int y;
} point;

point S_DOWN = {0, 1};
point S_UP = {0, -1};
point S_LEFT = {-1, 0};
point S_RIGHT = {1, 0};
point S_NO_DIR = {0, 0};

typedef struct {
  point *body;
  int __b_size;
  int __b_array_size;
  int __b_begin;
  point facing;
} snake;

typedef struct {
  int board_width;
  int board_height;
  int speed;
  int next_speed;
  bool paused;
  point food;
  snake snk;
  bool dead;
  point dir_memory[MAX_DIR_MEMORY];
  int __dm_size;
  int __dm_begin;
  uint64_t score;
} snk_game;

void init_board(WINDOW *board);
snk_game init_game();
void render_snake(WINDOW *board, snake *snk, bool remove);
void end_game(snk_game *game, struct itimerval *timer);
snake init_snake();
point to_dir(int key);
void add_dir(snk_game *game, point dir);
void walk_snake(snk_game *game);
void snake_go(snake *snk);
point point_add(point a, point b);
void handle_sigalrm(int sig_count);
point gen_food();
void render_food(WINDOW *board, snk_game *game);
int rand_range(int upper_bound);
struct itimerval *start_alarm();
void pause_alarm(struct itimerval *it);
void resume_alarm(struct itimerval *it);
void stop_alarm(struct itimerval *it);
void set_alarm_speed(struct itimerval *it, int speed);
void set_alarm_handler();
bool changed_dir(point a, point b);
void render_score(WINDOW *score_board, uint64_t score);
void render_speed(WINDOW *score_board, uint64_t score);
void init_score_board(WINDOW *score_board);
void render_num(WINDOW *wd, uint64_t, int x_off, int y_off);
bool point_eq(point a, point b);

int main(void) {
  srand(time(NULL));
  set_alarm_handler();
  snk_game game = init_game();
  struct itimerval *timer = start_alarm();
  set_alarm_speed(timer, game.speed);
  WINDOW *stdscr = initscr();
  WINDOW *board = newpad(BOARD_HEIGHT, BOARD_WIDTH);
  WINDOW *score_board = newpad(SCORE_BOARD_HEIGHT, SCORE_BOARD_WIDTH);
  ASSERT("cbreak", cbreak());
  ASSERT("noecho", noecho());
  ASSERT("nonl", nonl());
  ASSERT("initrflush", intrflush(stdscr, FALSE));
  ASSERT("keypad", keypad(stdscr, TRUE));
  wrefresh(stdscr);
  init_board(board);
  init_score_board(score_board);
  render_snake(board, &game.snk, FALSE);
  render_food(board, &game);
  render_score(score_board, game.score);
  render_speed(score_board, game.next_speed);
  wmove(stdscr, LINES - 1, COLS - 1); //get cursor out of the way;
  DRAW();
  int ch;
  do {
    if (walk && !game.dead) {
      walk = FALSE;
      render_snake(board, &game.snk, TRUE);
      walk_snake(&game);
      render_snake(board, &game.snk, FALSE);
      render_food(board, &game);
      render_score(score_board, game.score);
      wmove(stdscr, LINES - 1, COLS - 1); //get cursor out of the way;
      DRAW();
    }
    errno = 0;
    ch = wgetch(stdscr);
    if (errno == 0 && ch != ERR) {
      if (ch == 'r') {
        free(game.snk.body);
        int next_speed = game.next_speed;
        game = init_game();
        game.speed = next_speed;
        game.next_speed = next_speed;
        set_alarm_speed(timer, game.speed);
        delwin(board);
        delwin(score_board);
        board = newpad(BOARD_HEIGHT, BOARD_WIDTH);
        score_board = newpad(SCORE_BOARD_HEIGHT, SCORE_BOARD_WIDTH);
        init_board(board);
        init_score_board(score_board);
        render_snake(board, &game.snk, FALSE);
        render_score(score_board, game.score);
        render_speed(score_board, game.next_speed);
        render_food(board, &game);
        wmove(stdscr, LINES - 1, COLS - 1); //get cursor out of the way;
        DRAW();
      } else if (ch == 'p') {
        if (game.paused){
          resume_alarm(timer);
        } else {
          pause_alarm(timer);
        }
        game.paused = !game.paused;
      } else if (ch == '+' || ch == '-') {
        if (ch == '+') {
          game.next_speed++;
        } else if (game.next_speed > 1) {
          game.next_speed--;
        }
        delwin(score_board);
        score_board = newpad(SCORE_BOARD_HEIGHT, SCORE_BOARD_WIDTH);
        init_score_board(score_board);
        render_score(score_board, game.score);
        render_speed(score_board, game.next_speed);
        wmove(stdscr, LINES - 1, COLS - 1); //get cursor out of the way;
        DRAW();
      } else if (!game.dead) {
        add_dir(&game, to_dir(ch));
      }
    }
  } while (ch != 'q');
  ASSERT("endwin", endwin());
  end_game(&game, timer);
  return 0;
}

void init_board(WINDOW *board) {
    wmove(board, 0, 0);
    for (size_t i = 0; i < BOARD_WIDTH; i++) {
      waddch(board, '_');
    }
    wmove(board, BOARD_HEIGHT - 1, 0);
    for (size_t i = 0; i < BOARD_WIDTH; i++) {
      waddch(board, 'T');
    }
    for (size_t i = 0; i < BOARD_HEIGHT - 2; i++) {
      wmove(board, i + 1, 0);
      waddch(board, '\\');
      wmove(board, i + 1, BOARD_WIDTH - 1);
      waddch(board, '/');
    }
}

void end_game(snk_game *game, struct itimerval *timer) {
  free(game->snk.body);
  stop_alarm(timer);
}

void handle_sigalrm(int sig_count) {
  walk = TRUE;
}

snk_game init_game() {
  snk_game game;
  game.board_width = BOARD_WIDTH;
  game.board_height = BOARD_HEIGHT;
  game.speed = 5;
  game.next_speed = game.speed;
  game.paused = FALSE;
  game.snk = init_snake();
  game.dead = FALSE;
  game.__dm_size = 0;
  game.__dm_begin = 0;
  game.score = 0;
  game.food = gen_food(&game);
  return game;
}

snake init_snake() {
  snake snk;
  snk.body = (point*)malloc(sizeof(point) * DEFAULT_SNAKE_BODY_ARRAY_SIZE);
  point pos = {0, BOARD_HEIGHT - 3};
  snk.body[0] = pos;
  pos.x++;
  snk.body[1] = pos;
  pos.x++;
  snk.body[2] = pos;
  snk.__b_array_size = DEFAULT_SNAKE_BODY_ARRAY_SIZE;
  snk.__b_size = 3;
  snk.__b_begin = 0;
  snk.facing = S_RIGHT;
  return snk;
}

void render_snake(WINDOW *board, snake *snk, bool remove) {
  for (size_t i = 0; i < snk->__b_size; i++) {
    point pos = snk->body[(snk->__b_begin + i) % snk->__b_array_size];
    wmove(board, pos.y + 1, pos.x + 1);
    waddch(board, remove ? ' ' : i == snk->__b_size - 1 ? '0' : '#');
  }
}

point to_dir(int key) {
  point dir = S_NO_DIR;
  switch (key) {
    case KEY_LEFT:
    case 'h': dir = S_LEFT; break;
    case KEY_DOWN:
    case 'j': dir = S_DOWN; break;
    case KEY_UP:
    case 'k': dir = S_UP; break;
    case KEY_RIGHT:
    case 'l': dir = S_RIGHT; break;
  }
  return dir;
}

void add_dir(snk_game *game, point dir) {
  if (game->__dm_size < MAX_DIR_MEMORY
      && !point_eq(dir, S_NO_DIR)
      && ((game->__dm_size == 0 && changed_dir(dir, game->snk.facing))
         || (game->__dm_size > 0 && changed_dir(dir, game->dir_memory[(game->__dm_begin + game->__dm_size - 1) % MAX_DIR_MEMORY])))) {
    game->dir_memory[(game->__dm_begin + game->__dm_size) % MAX_DIR_MEMORY] = dir;
    game->__dm_size++;
  }
}

void snake_go(snake *snk) {
  snk->body[(snk->__b_begin + snk->__b_size) % snk->__b_array_size] = point_add(
    snk->body[(snk->__b_begin + snk->__b_size - 1) % snk->__b_array_size],
    snk->facing
  );
  snk->__b_begin = (snk->__b_begin + 1) % snk->__b_array_size;
}

int rand_range(int upper_bound) {
  return ((rand() - 1) / (double)RAND_MAX) * upper_bound;
}

int compare_int(const void *int1, const void *int2) {
  return *((int*)int1) - *((int*)int2);
}

point gen_food(snk_game *game) {
  int w = BOARD_WIDTH - 2;
  int h = BOARD_HEIGHT - 2;
  int len = game->snk.__b_size;
  int tile_count = w * h - len;
  int used_tiles[len];
  for (size_t i = 0; i < len; i++) {
    point p = game->snk.body[(game->snk.__b_begin + i) % game->snk.__b_array_size];
    used_tiles[i] = p.y * w + p.x + 1;
  }
  qsort((void*)used_tiles, len, sizeof(int), compare_int);
  int pick = rand_range(tile_count) + 1;
  for (size_t i = 0; i < len; i++) {
    if (used_tiles[i] <= pick) {
      pick++;
    } else {
      break;
    }
  }
  point food;
  food.x = (pick - 1) % w;
  food.y = (pick - 1) / w;
  return food;
}

void render_food(WINDOW *board, snk_game *game) {
  wmove(board, game->food.y + 1, game->food.x + 1);
  waddch(board, '*');
}

void walk_snake(snk_game *game) {
  snake *snk = &game->snk;
  if (game->__dm_size > 0) {
    game->snk.facing = game->dir_memory[game->__dm_begin];
    game->__dm_size--;
    game->__dm_begin = (game->__dm_begin + 1) % MAX_DIR_MEMORY;
  }
  snake_go(snk);
  point head = game->snk.body[(snk->__b_begin + snk->__b_size - 1) % snk->__b_array_size];
  if (head.x < 0 || head.y < 0 || head.x > BOARD_WIDTH - 3 || head.y > BOARD_HEIGHT - 3) {
    game->dead = true;
  } else {
    for (size_t i = 0; i < snk->__b_size - 1; i++) {
      point segment = snk->body[(snk->__b_begin + i) % snk->__b_array_size];
      if (segment.x == head.x && segment.y == head.y) {
        game->dead = true;
        break;
      }
    }
  }
  if (head.x == game->food.x && head.y == game->food.y) {
    snk->__b_begin = (snk->__b_begin + snk->__b_array_size - 1) % snk->__b_array_size;
    snk->__b_size++;
    if (snk->__b_size == snk->__b_array_size) {
      point *old_body = snk->body;
      int newsize = 2 * snk->__b_array_size;
      snk->body = (point*)malloc(sizeof(point) * newsize);
      memcpy(snk->body, old_body + snk->__b_begin, (snk->__b_array_size - snk->__b_begin) * sizeof(point));
      memcpy(snk->body + (snk->__b_array_size - snk->__b_begin), old_body, snk->__b_begin * sizeof(point));
      free(old_body);
      snk->__b_begin = 0;
      snk->__b_array_size = newsize;
    }
    game->score += game->speed;
    game->food = gen_food(game);
  }
}

point point_add(point a, point b) {
  a.x += b.x;
  a.y += b.y;
  return a;
}

struct itimerval *start_alarm() {
  struct timeval tv = {0, 0};
  struct itimerval *itp = (struct itimerval*)malloc(sizeof(struct itimerval));
  struct itimerval it = {tv, tv};
  *itp = it;
  setitimer(ITIMER_REAL, itp, NULL);
  return itp;
}

void pause_alarm(struct itimerval *it) {
  struct timeval tv = {0, 0};
  it->it_value = tv;
  setitimer(ITIMER_REAL, it, NULL);
}

void resume_alarm(struct itimerval *it) {
  it->it_value = it->it_interval;
  setitimer(ITIMER_REAL, it, NULL);
}

void stop_alarm(struct itimerval *it) {
  struct timeval tv = {0, 0};
  it->it_value = tv;
  setitimer(ITIMER_REAL, it, NULL);
  free(it);
}

void set_alarm_speed(struct itimerval *it, int speed) {
  struct timeval tv;
  if (speed == 1) {
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  } else {
    tv.tv_sec = 0;
    tv.tv_usec = 1000000 / speed;
  }
  it->it_value = tv;
  it->it_interval = tv;
  setitimer(ITIMER_REAL, it, NULL);
}

void set_alarm_handler() {
  struct sigaction sa = {};
  sa.sa_handler = handle_sigalrm;
  sigaction(SIGALRM, &sa, NULL);
}

bool changed_dir(point a, point b) {
  return !((a.x == b.x && a.y == b.y)
    || (a.x != 0 && a.x == -b.x)
    || (a.y != 0 && a.y == -b.y));
}

void render_num(WINDOW *wd, uint64_t number, int x_off, int y_off) {
  if (number == 0) {
      wmove(wd, y_off, x_off);
      waddch(wd, '0');
  } else {
    while (number > 0) {
      wmove(wd, y_off, x_off);
      x_off--;
      waddch(wd, '0' + (number % 10));
      number /= 10;
    }
  }
}

void render_score(WINDOW *score_board, uint64_t score) {
  render_num(score_board, score, SCORE_BOARD_WIDTH - 1, 0);
}

void render_speed(WINDOW *score_board, uint64_t speed) {
  render_num(score_board, speed, SCORE_BOARD_WIDTH - 1, 2);
}

void init_score_board(WINDOW * score_board) {
  wmove(score_board, 0, 0);
  waddstr(score_board, "Score:");
  wmove(score_board, 2, 0);
  waddstr(score_board, "Speed:");
}

bool point_eq(point a, point b) {
  return a.x == b.x && a.y == b.y;
}
