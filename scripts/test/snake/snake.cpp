/*
 * 终端贪吃蛇 —— Linux/macOS
 * 编译: g++ -std=c++17 -o snake snake.cpp
 * 运行: ./snake
 * 操控: WASD 或方向键，Q 退出
 */

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <random>
#include <termios.h>
#include <unistd.h>

// ── 地图参数 ─────────────────────────
static const int WIDTH  = 40;
static const int HEIGHT = 20;

// ── 方向 ────────────────────────────────
enum Dir { UP, DOWN, LEFT, RIGHT };
struct Pos { int x, y; };
static bool operator==(const Pos& a, const Pos& b) { return a.x == b.x && a.y == b.y; }

// ── 终端原始模式 ────────────────────────────
static struct termios origTermios;
static void disableRawMode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios); }
static void enableRawMode() {
    tcgetattr(STDIN_FILENO, &origTermios);
    atexit(disableRawMode);
    struct termios raw = origTermios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 1; // 100ms 超时
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// ─ 非阻塞读取按键 ──────────────────────────
static Dir readInput(Dir cur) {
    char buf[3];
    int n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return cur;

    // 方向键: ESC [ A/B/C/D
    if (n == 3 && buf[0] == 27 && buf[1] == '[') {
        switch (buf[2]) {
            case 'A': return cur != DOWN  ? UP    : cur;
            case 'B': return cur != UP    ? DOWN  : cur;
            case 'D': return cur != RIGHT ? LEFT  : cur;
            case 'C': return cur != LEFT  ? RIGHT : cur;
        }
    }
    // WASD
    if (n == 1) {
        switch (buf[0]) {
            case 'w': case 'W': return cur != DOWN  ? UP    : cur;
            case 's': case 'S': return cur != UP    ? DOWN  : cur;
            case 'a': case 'A': return cur != RIGHT ? LEFT  : cur;
            case 'd': case 'D': return cur != LEFT  ? RIGHT : cur;
            case 'q': case 'Q': exit(0);
        }
    }
    return cur;
}

// ── 随机食物 ────────────────────────────────
static Pos placeFood(const std::deque<Pos>& snake) {
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    Pos f;
    bool ok;
    do {
        f.x = 1 + rng() % (WIDTH  - 2);
        f.y = 1 + rng() % (HEIGHT - 2);
        ok = true;
        for (auto& s : snake) if (s == f) { ok = false; break; }
    } while (!ok);
    return f;
}

// ── 绘制 ────────────────────────────────
static void render(const std::deque<Pos>& snake, const Pos& food, int score) {
    // 光标归顶 + 清屏
    std::printf("\033[H\03[J");

    // 用二维数组标记
    char grid[HEIGHT][WIDTH + 1];
    std::memset(grid, ' ', sizeof(grid));
    for (int y = 0; y < HEIGHT; y++) grid[y][WIDTH] = '\0';

    // 边框
    for (int x = 0; x < WIDTH; x++)  { grid[0][x] = grid[HEIGHT-1][x] = '#'; }
    for (int y = 0; y < HEIGHT; y++) { grid[y][0] = grid[y][WIDTH-1] = '#'; }

    // 食物
    grid[food.y][food.x] = '*';

    // 蛇
    for (size_t i = 0; i < snake.size(); i++) {
        grid[snake[i].y][snake[i].x] = (i == 0) ? '@' : 'o';
    }

    for (int y = 0; y < HEIGHT; y++)
        std::printf("%s\n", grid[y]);
    std::printf("  Score: %d  |  WASD/方向键移动  Q退出\n", score);
}

// ── 主循环 ──────────────────────────
int main() {
    enableRawMode();

    std::deque<Pos> snake;
    snake.push_back({WIDTH / 2, HEIGHT / 2});
    Dir dir = RIGHT;
    Pos food = placeFood(snake);
    int score = 0;
    int speed = 150; // ms per tick

    while (true) {
        render(snake, food, score);
        usleep(speed * 1000);
        dir = readInput(dir);

        // 新头
        Pos head = snake.front();
        switch (dir) {
            case UP:    head.y--; break;
            case DOWN:  head.y++; break;
            case LEFT:  head.x--; break;
            case RIGHT: head.x++; break;
        }

        // 碰墙
        if (head.x <= 0 || head.x >= WIDTH - 1 || head.y <= 0 || head.y >= HEIGHT - 1)
            break;

        // 碰自身
        for (auto& s : snake)
            if (s == head) { goto done; }

        snake.push_front(head);

        if (head == food) {
            score += 10;
            food = placeFood(snake);
            if (speed > 50) speed -= 3; // 加速
        } else {
            snake.pop_back();
        }
    }

done:
    disableRawMode();
    std::printf("\n💀 Game Over!  Score: %d\n", score);
    return 0;
}

