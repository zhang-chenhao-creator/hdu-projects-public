/******************************************************************************
 * 贪吃蛇拓展版（C 语言 / Windows 控制台版）
 * ============================================================================
 * 必做四项功能：
 *   (一) 自撞 → 截尾（不死亡）
 *   (二) 障碍物 → 撞到死亡
 *   (三) 多种食物 → 不同生长速度与分数
 *   (四) 积分排行榜 → 文件持久化保存历次成绩
 * 创新功能：
 *   (五) 冲击波技能 → 吃 * 冲击波果获得次数，按 F 释放，
 *                     以蛇头为中心摧毁半径内的障碍物，每个 +20 分
 *
 * 关键技术：双屏幕缓冲区（Double Buffering）
 * ----------------------------------------------------------------------------
 *   传统单缓冲方式每帧"擦了再写"会导致严重闪烁。本程序使用 Windows 提供
 *   的 CreateConsoleScreenBuffer 创建两个屏幕缓冲区，绘制在后台缓冲，
 *   完成后用 SetConsoleActiveScreenBuffer 瞬间切换显示，完全消除闪烁。
 *
 * 编译环境：Windows + 任意 C 编译器（gcc / Visual Studio / Dev-C++）
 * 编译命令：gcc snake_game.c -o snake_game.exe
 * 运行：    snake_game.exe
 *
 * 操作说明：
 *   方向键 / WASD ：控制移动方向
 *   空格          ：暂停 / 继续
 *   F             ：释放冲击波技能
 *   R             ：重新开始
 *   ESC           ：退出游戏
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <conio.h>      /* _kbhit, _getch */
#include <windows.h>    /* Sleep, 控制台 API */

/* ==========================================================================
 *                              游戏配置常量
 * ========================================================================== */
#define MAP_WIDTH       40      /* 地图宽度（列数）*/
#define MAP_HEIGHT      20      /* 地图高度（行数）*/
#define MAX_SNAKE_LEN   800     /* 蛇身最大节数 */
#define MAX_OBSTACLES   30      /* 障碍物最大数量 */
#define INIT_OBSTACLES  14      /* 初始障碍物数量 */
#define BASE_SPEED      130     /* 基础刷新间隔（毫秒），值越小蛇越快 */
#define SHOCKWAVE_R     4       /* 冲击波作用半径（格）*/
#define MAX_LEADERBOARD 10      /* 排行榜最多保存条数 */
#define LEADERBOARD_FILE "leaderboard.dat"  /* 排行榜文件名 */

/* 屏幕缓冲区大小（必须能容纳整个游戏画面 + 信息面板）*/
#define SCREEN_WIDTH    120
#define SCREEN_HEIGHT   35

/* 控制台颜色常量（Windows API 颜色码）*/
#define COLOR_DEFAULT   7       /* 灰白 */
#define COLOR_SNAKE     10      /* 亮绿 */
#define COLOR_HEAD      14      /* 亮黄 */
#define COLOR_WALL      8       /* 暗灰 */
#define COLOR_OBSTACLE  6       /* 棕色 */
#define COLOR_NORMAL    12      /* 亮红（普通苹果）*/
#define COLOR_GOLDEN    14      /* 亮黄（金苹果）*/
#define COLOR_SPEED     11      /* 亮青（加速果）*/
#define COLOR_DIAMOND   3       /* 青色（钻石）*/
#define COLOR_POISON    13      /* 亮紫（毒蘑菇）*/
#define COLOR_SHOCK     12      /* 亮红（冲击波果，用 * 区分）*/
#define COLOR_INFO      11      /* 亮青（信息文字）*/
#define COLOR_TITLE     14      /* 亮黄（标题）*/

/* ==========================================================================
 *        要求(三)：食物类型枚举与配置表
 * ----------------------------------------------------------------------------
 *   每种食物有不同的：显示符号、颜色、生长节数、分数、生成权重、特效
 * ========================================================================== */
typedef enum {
    FOOD_NORMAL = 0,    /* @ 普通苹果 */
    FOOD_GOLDEN,        /* $ 金苹果   */
    FOOD_SPEED,         /* > 加速果   */
    FOOD_DIAMOND,       /* & 钻石     */
    FOOD_POISON,        /* % 毒蘑菇   */
    FOOD_SHOCKWAVE,     /* * 冲击波果 */
    FOOD_TYPE_COUNT
} FoodType;

typedef struct {
    char  symbol;       /* 显示字符 */
    int   color;        /* 控制台颜色 */
    int   growth;       /* 吃下后增加的节数（可为负）*/
    int   score;        /* 吃下后获得的分数（可为负）*/
    int   weight;       /* 随机生成权重 */
    char  name[20];     /* 名称（用于侧栏显示）*/
    int   effect;       /* 特效：0=无 1=加速 2=冲击波 */
} FoodConfig;

/* 食物配置表（按枚举顺序）*/
static const FoodConfig FOOD_TABLE[FOOD_TYPE_COUNT] = {
    /*  符号 颜色          生长 分数 权重  名称            特效 */
    { '@',  COLOR_NORMAL,    1,  10,  50, "普通苹果",       0 },
    { '$',  COLOR_GOLDEN,    3,  30,  18, "金苹果",         0 },
    { '>',  COLOR_SPEED,     1,  20,  15, "加速果",         1 },
    { '&',  COLOR_DIAMOND,   2,  50,   7, "钻石",           0 },
    { '%',  COLOR_POISON,   -2, -10,  10, "毒蘑菇",         0 },
    { '*',  COLOR_SHOCK,     1,  15,   8, "冲击波果",       2 }
};

/* ==========================================================================
 *                          数据结构定义
 * ========================================================================== */

typedef struct { int x; int y; } Point;

typedef struct {
    Point body[MAX_SNAKE_LEN];  /* 蛇身节段，body[0] 为蛇头 */
    int   length;
    int   growthPending;        /* 待执行的生长量 */
    Point direction;            /* 当前移动方向 */
    Point nextDirection;        /* 下一帧的方向（输入缓冲）*/
} Snake;

typedef struct { Point pos; FoodType type; } Food;

typedef struct {
    char name[16];
    int  score;
    char date[16];
} ScoreEntry;

/* ==========================================================================
 *                          全局游戏状态
 * ========================================================================== */
static Snake  g_snake;
static Food   g_food;
static Point  g_obstacles[MAX_OBSTACLES];
static int    g_obstacleCount = 0;
static int    g_score = 0;
static int    g_isPaused = 0;
static int    g_isGameOver = 0;
static int    g_speedBoostTicks = 0;
static int    g_currentDelay = BASE_SPEED;
static int    g_shockwaveCharges = 0;
static char   g_gameOverReason[64] = "";
static char   g_floatingMsg[64] = "";
static int    g_floatingMsgLife = 0;

/* ==========================================================================
 *               核心抗闪烁机制：双屏幕缓冲区
 * ----------------------------------------------------------------------------
 *   原理：
 *     - 维护两个屏幕缓冲区 g_hGame1 和 g_hGame2
 *     - 一个正在显示给用户（前台），另一个在内存中绘制（后台）
 *     - render() 在后台缓冲完整绘制后，调用 SetConsoleActiveScreenBuffer
 *       瞬间切换显示，用户永远看不到"绘制中间状态"，因此完全无闪烁
 *     - 菜单/排行榜等静态界面用 g_hMenu（即原 stdout），仍可用 printf
 * ========================================================================== */
static HANDLE g_hMenu  = NULL;      /* 菜单缓冲（原 stdout）*/
static HANDLE g_hGame1 = NULL;      /* 游戏缓冲 1 */
static HANDLE g_hGame2 = NULL;      /* 游戏缓冲 2 */
static HANDLE g_hBack  = NULL;      /* 当前后台缓冲（用于绘制）*/

/* 创建一个屏幕缓冲区并设置好大小、光标隐藏 */
HANDLE createGameBuffer(void) {
    HANDLE h = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CONSOLE_TEXTMODE_BUFFER,
        NULL
    );
    if (h == INVALID_HANDLE_VALUE) return NULL;

    /* 设置缓冲区尺寸 */
    COORD size = { SCREEN_WIDTH, SCREEN_HEIGHT };
    SetConsoleScreenBufferSize(h, size);

    /* 隐藏光标 */
    CONSOLE_CURSOR_INFO ci;
    ci.dwSize   = 1;
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(h, &ci);

    return h;
}

/* 程序启动时调用一次：初始化双缓冲机制 */
void initDoubleBuffer(void) {
    g_hMenu  = GetStdHandle(STD_OUTPUT_HANDLE);
    g_hGame1 = createGameBuffer();
    g_hGame2 = createGameBuffer();
    g_hBack  = g_hGame1;        /* 初始后台缓冲为 game1 */

    /* 给菜单缓冲也隐藏光标 */
    CONSOLE_CURSOR_INFO ci;
    ci.dwSize   = 1;
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(g_hMenu, &ci);
}

/* 程序退出时调用：释放游戏缓冲 */
void cleanupDoubleBuffer(void) {
    /* 切回菜单缓冲再释放，避免 active buffer 被关闭 */
    SetConsoleActiveScreenBuffer(g_hMenu);
    if (g_hGame1) CloseHandle(g_hGame1);
    if (g_hGame2) CloseHandle(g_hGame2);
}

/* 进入游戏模式：把当前后台缓冲切换为活动显示 */
void enterGameMode(void) {
    SetConsoleActiveScreenBuffer(g_hBack);
}

/* 退出游戏模式：切回菜单缓冲（用 printf 显示菜单）*/
void exitGameMode(void) {
    SetConsoleActiveScreenBuffer(g_hMenu);
}

/* 清空指定缓冲区：填充空格 + 默认颜色 */
void clearBuffer(HANDLE h) {
    COORD origin = {0, 0};
    DWORD written;
    DWORD size = SCREEN_WIDTH * SCREEN_HEIGHT;
    FillConsoleOutputCharacterA(h, ' ', size, origin, &written);
    FillConsoleOutputAttribute (h, COLOR_DEFAULT, size, origin, &written);
}

/*
 * 双缓冲核心函数：在指定 (x,y) 位置写一个字符串（带颜色）
 *   - 不依赖光标位置，避免 putchar 的开销
 *   - 直接用 WriteConsoleOutputCharacterA / WriteConsoleOutputAttribute
 *     写到 back buffer，速度比 printf+gotoxy 快很多
 *   - 中文按 GBK 双字节编码，会自动占两个屏幕格子
 */
void writeAt(int x, int y, const char *str, int color) {
    if (str == NULL || *str == '\0') return;
    COORD pos = { (SHORT)x, (SHORT)y };
    DWORD written;
    int len = (int)strlen(str);
    if (len <= 0) return;

    /* 准备颜色属性数组 */
    static WORD attrs[256];
    int i, n = (len > 256) ? 256 : len;
    for (i = 0; i < n; i++) attrs[i] = (WORD)color;

    WriteConsoleOutputAttribute(g_hBack, attrs, n, pos, &written);
    WriteConsoleOutputCharacterA(g_hBack, str, len, pos, &written);
}

/*
 * 翻转双缓冲：把当前 back buffer 切换为显示，然后切换 back 指针
 *   关键点：SetConsoleActiveScreenBuffer 是原子操作，瞬间生效，
 *   用户从看到旧画面 → 看到新画面只需要一次屏幕刷新，绝无闪烁。
 */
void flipBuffer(void) {
    SetConsoleActiveScreenBuffer(g_hBack);
    /* 切换 back：下次绘制到另一个缓冲 */
    g_hBack = (g_hBack == g_hGame1) ? g_hGame2 : g_hGame1;
}

/* ==========================================================================
 *                       要求(一)：蛇的相关操作
 * ========================================================================== */

void initSnake(void) {
    g_snake.length = 3;
    g_snake.body[0].x = 6; g_snake.body[0].y = MAP_HEIGHT / 2;
    g_snake.body[1].x = 5; g_snake.body[1].y = MAP_HEIGHT / 2;
    g_snake.body[2].x = 4; g_snake.body[2].y = MAP_HEIGHT / 2;
    g_snake.growthPending = 0;
    g_snake.direction.x     = 1; g_snake.direction.y     = 0;
    g_snake.nextDirection.x = 1; g_snake.nextDirection.y = 0;
}

void moveSnake(void) {
    int i;
    Point newHead;
    newHead.x = g_snake.body[0].x + g_snake.direction.x;
    newHead.y = g_snake.body[0].y + g_snake.direction.y;

    for (i = g_snake.length; i > 0; i--) {
        g_snake.body[i] = g_snake.body[i - 1];
    }
    g_snake.body[0] = newHead;
    g_snake.length++;

    if (g_snake.growthPending > 0) {
        g_snake.growthPending--;
    } else {
        g_snake.length--;
        if (g_snake.growthPending < 0 && g_snake.length > 1) {
            g_snake.length--;
            g_snake.growthPending++;
        }
    }
}

int checkSelfCollision(void) {
    int i;
    for (i = 1; i < g_snake.length; i++) {
        if (g_snake.body[i].x == g_snake.body[0].x &&
            g_snake.body[i].y == g_snake.body[0].y) return i;
    }
    return -1;
}

int cutTail(int index) {
    int cutLen = g_snake.length - index;
    g_snake.length = index;
    return cutLen;
}

/* ==========================================================================
 *               要求(二)：障碍物生成与判定
 * ========================================================================== */

int isOnSnake(int x, int y) {
    int i;
    for (i = 0; i < g_snake.length; i++) {
        if (g_snake.body[i].x == x && g_snake.body[i].y == y) return 1;
    }
    return 0;
}

int isOnObstacle(int x, int y) {
    int i;
    for (i = 0; i < g_obstacleCount; i++) {
        if (g_obstacles[i].x == x && g_obstacles[i].y == y) return 1;
    }
    return 0;
}

void generateObstacles(int count) {
    int attempt = 0;
    g_obstacleCount = 0;
    while (g_obstacleCount < count && attempt < 1000) {
        int x = rand() % (MAP_WIDTH  - 2) + 1;
        int y = rand() % (MAP_HEIGHT - 2) + 1;
        attempt++;
        if (x < 12 && y >= MAP_HEIGHT/2 - 1 && y <= MAP_HEIGHT/2 + 1) continue;
        if (isOnObstacle(x, y)) continue;
        g_obstacles[g_obstacleCount].x = x;
        g_obstacles[g_obstacleCount].y = y;
        g_obstacleCount++;
    }
}

/* ==========================================================================
 *                      要求(三)：食物生成
 * ========================================================================== */

FoodType pickFoodType(void) {
    int total = 0, r, i;
    for (i = 0; i < FOOD_TYPE_COUNT; i++) total += FOOD_TABLE[i].weight;
    r = rand() % total;
    for (i = 0; i < FOOD_TYPE_COUNT; i++) {
        r -= FOOD_TABLE[i].weight;
        if (r < 0) return (FoodType)i;
    }
    return FOOD_NORMAL;
}

void spawnFood(void) {
    int attempt = 0;
    do {
        g_food.pos.x = rand() % (MAP_WIDTH  - 2) + 1;
        g_food.pos.y = rand() % (MAP_HEIGHT - 2) + 1;
        attempt++;
    } while ((isOnSnake(g_food.pos.x, g_food.pos.y) ||
              isOnObstacle(g_food.pos.x, g_food.pos.y)) && attempt < 500);
    g_food.type = pickFoodType();
}

/* ==========================================================================
 *           创新功能：冲击波技能释放
 * ========================================================================== */
void activateShockwave(void) {
    int hx, hy, r2, i, j, destroyed = 0;
    if (g_shockwaveCharges <= 0 || g_isPaused || g_isGameOver) return;

    g_shockwaveCharges--;
    hx = g_snake.body[0].x;
    hy = g_snake.body[0].y;
    r2 = SHOCKWAVE_R * SHOCKWAVE_R;

    j = 0;
    for (i = 0; i < g_obstacleCount; i++) {
        int dx = g_obstacles[i].x - hx;
        int dy = g_obstacles[i].y - hy;
        if (dx * dx + dy * dy <= r2) {
            destroyed++;
        } else {
            g_obstacles[j++] = g_obstacles[i];
        }
    }
    g_obstacleCount = j;
    g_score += destroyed * 20;

    if (destroyed > 0) {
        sprintf(g_floatingMsg, "*** 冲击波！摧毁 %d 个障碍 +%d 分 ***",
                destroyed, destroyed * 20);
    } else {
        strcpy(g_floatingMsg, "*** 冲击波释放（未命中）***");
    }
    g_floatingMsgLife = 8;
}

/* ==========================================================================
 *               要求(四)：积分排行榜
 * ========================================================================== */

int loadLeaderboard(ScoreEntry *entries) {
    FILE *fp;
    int count = 0;
    fp = fopen(LEADERBOARD_FILE, "rb");
    if (fp == NULL) return 0;
    count = (int)fread(entries, sizeof(ScoreEntry), MAX_LEADERBOARD, fp);
    fclose(fp);
    return count;
}

void writeLeaderboard(ScoreEntry *entries, int count) {
    FILE *fp = fopen(LEADERBOARD_FILE, "wb");
    if (fp == NULL) return;
    fwrite(entries, sizeof(ScoreEntry), count, fp);
    fclose(fp);
}

int compareScore(const void *a, const void *b) {
    return ((const ScoreEntry *)b)->score - ((const ScoreEntry *)a)->score;
}

void saveScore(const char *name, int score) {
    ScoreEntry entries[MAX_LEADERBOARD + 1];
    int count = loadLeaderboard(entries);
    time_t now;
    struct tm *t;

    strncpy(entries[count].name, name, 15);
    entries[count].name[15] = '\0';
    entries[count].score = score;
    time(&now);
    t = localtime(&now);
    sprintf(entries[count].date, "%04d-%02d-%02d",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    count++;

    qsort(entries, count, sizeof(ScoreEntry), compareScore);
    if (count > MAX_LEADERBOARD) count = MAX_LEADERBOARD;
    writeLeaderboard(entries, count);
}

/* 显示排行榜（在菜单缓冲区，用普通 printf）*/
void showLeaderboard(void) {
    ScoreEntry entries[MAX_LEADERBOARD];
    int count = loadLeaderboard(entries);
    int i;

    system("cls");
    SetConsoleTextAttribute(g_hMenu, COLOR_TITLE);
    printf("\n  ========== 历史排行榜 TOP %d ==========\n\n", MAX_LEADERBOARD);
    SetConsoleTextAttribute(g_hMenu, COLOR_DEFAULT);

    if (count == 0) {
        printf("  暂无记录，快去挑战第一名吧！\n\n");
    } else {
        printf("  排名   玩家名         分数      日期\n");
        printf("  ----------------------------------------\n");
        for (i = 0; i < count; i++) {
            const char *medal = "    ";
            if (i == 0) medal = " [1]";
            else if (i == 1) medal = " [2]";
            else if (i == 2) medal = " [3]";
            SetConsoleTextAttribute(g_hMenu, i < 3 ? COLOR_GOLDEN : COLOR_DEFAULT);
            printf("  %s   %-12s   %5d    %s\n",
                   medal, entries[i].name, entries[i].score, entries[i].date);
        }
        SetConsoleTextAttribute(g_hMenu, COLOR_DEFAULT);
    }
    printf("\n  按任意键返回主菜单...\n");
    _getch();
}

/* ==========================================================================
 *                 渲染游戏画面（双缓冲版本）
 * ----------------------------------------------------------------------------
 *   流程：
 *     1. clearBuffer(g_hBack)  - 清空后台缓冲
 *     2. 用 writeAt() 把所有元素绘制到后台缓冲（无可见效果）
 *     3. flipBuffer()          - 瞬间切换显示，零闪烁
 * ========================================================================== */
void render(void) {
    int x, y, i, line;
    char buf[128];
    int panelX = MAP_WIDTH * 2 + 4;

    /* === 第一步：清空后台缓冲 === */
    clearBuffer(g_hBack);

    /* === 第二步：在后台缓冲绘制所有内容 === */

    /* 边框 */
    for (x = 0; x < MAP_WIDTH; x++) {
        writeAt(x * 2, 0,             "##", COLOR_WALL);
        writeAt(x * 2, MAP_HEIGHT-1,  "##", COLOR_WALL);
    }
    for (y = 0; y < MAP_HEIGHT; y++) {
        writeAt(0,             y, "##", COLOR_WALL);
        writeAt((MAP_WIDTH-1)*2, y, "##", COLOR_WALL);
    }

    /* 障碍物 */
    for (i = 0; i < g_obstacleCount; i++) {
        writeAt(g_obstacles[i].x * 2, g_obstacles[i].y, "##", COLOR_OBSTACLE);
    }

    /* 食物 */
    char foodStr[3];
    foodStr[0] = FOOD_TABLE[g_food.type].symbol;
    foodStr[1] = '\0';
    writeAt(g_food.pos.x * 2, g_food.pos.y, foodStr,
            FOOD_TABLE[g_food.type].color);

    /* 蛇身（先尾后头，蛇头压在最上层）*/
    for (i = g_snake.length - 1; i >= 1; i--) {
        writeAt(g_snake.body[i].x * 2, g_snake.body[i].y, "o", COLOR_SNAKE);
    }
    /* 蛇头用更亮的颜色和不同符号 */
    writeAt(g_snake.body[0].x * 2, g_snake.body[0].y, "O", COLOR_HEAD);

    /* === 右侧信息面板 === */
    line = 1;

    writeAt(panelX, line++, " === 贪吃蛇拓展版 ===", COLOR_TITLE);
    line++;

    sprintf(buf, " 当前得分 : %-10d", g_score);
    writeAt(panelX, line++, buf, COLOR_GOLDEN);

    sprintf(buf, " 蛇身长度 : %-10d", g_snake.length);
    writeAt(panelX, line++, buf, COLOR_INFO);

    sprintf(buf, " 移动速度 : %.1fx       ",
            (float)BASE_SPEED / g_currentDelay);
    writeAt(panelX, line++, buf, COLOR_INFO);

    sprintf(buf, " 冲击波   : %-10d", g_shockwaveCharges);
    writeAt(panelX, line++, buf, COLOR_SHOCK);

    sprintf(buf, " 状态     : %-10s",
            g_isPaused ? "暂停" : (g_isGameOver ? "结束" : "运行中"));
    writeAt(panelX, line++, buf, COLOR_INFO);

    line++;
    writeAt(panelX, line++, " --- 食物图鉴 ---", COLOR_TITLE);
    for (i = 0; i < FOOD_TYPE_COUNT; i++) {
        const char *effectStr = "";
        if (FOOD_TABLE[i].effect == 1) effectStr = " 加速";
        else if (FOOD_TABLE[i].effect == 2) effectStr = " +冲击波";
        sprintf(buf, " %c %-8s %+3d分 %+2d节%s",
                FOOD_TABLE[i].symbol,
                FOOD_TABLE[i].name,
                FOOD_TABLE[i].score,
                FOOD_TABLE[i].growth,
                effectStr);
        writeAt(panelX, line++, buf, FOOD_TABLE[i].color);
    }

    line++;
    writeAt(panelX, line++, " --- 操作说明 ---", COLOR_TITLE);
    writeAt(panelX, line++, " WASD/方向键 : 移动",   COLOR_DEFAULT);
    writeAt(panelX, line++, " 空格        : 暂停",   COLOR_DEFAULT);
    writeAt(panelX, line++, " F           : 冲击波", COLOR_DEFAULT);
    writeAt(panelX, line++, " R           : 重开",   COLOR_DEFAULT);
    writeAt(panelX, line++, " ESC         : 退出",   COLOR_DEFAULT);

    /* 飘字提示（地图下方）*/
    if (g_floatingMsgLife > 0) {
        sprintf(buf, "  >> %s", g_floatingMsg);
        writeAt(0, MAP_HEIGHT + 1, buf, COLOR_GOLDEN);
        g_floatingMsgLife--;
    }

    /* 暂停提示 */
    if (g_isPaused) {
        writeAt(MAP_WIDTH - 5, MAP_HEIGHT / 2,
                "  [ 暂停中，按空格继续 ]  ", COLOR_TITLE);
    }

    /* === 第三步：原子切换显示，零闪烁 === */
    flipBuffer();
}

/* ==========================================================================
 *                          键盘输入处理
 * ========================================================================== */
void handleInput(void) {
    if (!_kbhit()) return;
    int ch = _getch();

    /* 方向键是双字节序列 */
    if (ch == 224 || ch == 0) {
        ch = _getch();
        switch (ch) {
            case 72: ch = 'w'; break;
            case 80: ch = 's'; break;
            case 75: ch = 'a'; break;
            case 77: ch = 'd'; break;
            default: return;
        }
    }

    if (ch >= 'A' && ch <= 'Z') ch += 32;

    switch (ch) {
        case 'w':
            if (g_snake.direction.y != 1) {
                g_snake.nextDirection.x = 0; g_snake.nextDirection.y = -1;
            }
            break;
        case 's':
            if (g_snake.direction.y != -1) {
                g_snake.nextDirection.x = 0; g_snake.nextDirection.y = 1;
            }
            break;
        case 'a':
            if (g_snake.direction.x != 1) {
                g_snake.nextDirection.x = -1; g_snake.nextDirection.y = 0;
            }
            break;
        case 'd':
            if (g_snake.direction.x != -1) {
                g_snake.nextDirection.x = 1; g_snake.nextDirection.y = 0;
            }
            break;
        case ' ':
            if (!g_isGameOver) g_isPaused = !g_isPaused;
            break;
        case 'f':
            activateShockwave();
            break;
        case 'r':
            g_isGameOver = 1;
            strcpy(g_gameOverReason, "玩家主动重开");
            break;
        case 27:
            g_isGameOver = 2;
            strcpy(g_gameOverReason, "玩家退出");
            break;
    }
}

/* ==========================================================================
 *                          主游戏循环
 * ========================================================================== */
void gameLoop(void) {
    Point head;
    int collisionIdx, cutLen;

    /* 第一帧立即渲染（避免空白等待）*/
    render();

    while (!g_isGameOver) {
        handleInput();

        if (g_isPaused) {
            render();
            Sleep(50);
            continue;
        }

        /* 1. 应用方向输入 */
        g_snake.direction = g_snake.nextDirection;
        moveSnake();
        head = g_snake.body[0];

        /* 2. 撞墙判定（死亡）*/
        if (head.x <= 0 || head.x >= MAP_WIDTH - 1 ||
            head.y <= 0 || head.y >= MAP_HEIGHT - 1) {
            strcpy(g_gameOverReason, "撞到墙壁");
            g_isGameOver = 1;
            break;
        }

        /* 3. 要求(二)：撞障碍物（死亡）*/
        if (isOnObstacle(head.x, head.y)) {
            strcpy(g_gameOverReason, "撞到障碍物");
            g_isGameOver = 1;
            break;
        }

        /* 4. 要求(一)：自撞 → 截尾 */
        collisionIdx = checkSelfCollision();
        if (collisionIdx > 0) {
            cutLen = cutTail(collisionIdx);
            sprintf(g_floatingMsg, "!!! 自撞截尾，损失 %d 节 !!!", cutLen);
            g_floatingMsgLife = 8;
            if (g_snake.length < 2) {
                strcpy(g_gameOverReason, "蛇身被截光");
                g_isGameOver = 1;
                break;
            }
        }

        /* 5. 要求(三)：吃食物，按类型获得不同效果 */
        if (head.x == g_food.pos.x && head.y == g_food.pos.y) {
            const FoodConfig *cfg = &FOOD_TABLE[g_food.type];
            g_snake.growthPending += cfg->growth;
            g_score += cfg->score;
            if (g_score < 0) g_score = 0;

            sprintf(g_floatingMsg, ">>> 吃到[%s] %+d分 %+d节 <<<",
                    cfg->name, cfg->score, cfg->growth);
            g_floatingMsgLife = 8;

            if (cfg->effect == 1) {
                g_speedBoostTicks = 60;
                g_currentDelay = (int)(BASE_SPEED / 1.8);
            } else if (cfg->effect == 2) {
                g_shockwaveCharges++;
            }

            if (g_snake.length + g_snake.growthPending < 1) {
                strcpy(g_gameOverReason, "中毒身亡");
                g_isGameOver = 1;
                break;
            }
            spawnFood();
        }

        /* 6. 加速效果倒计时 */
        if (g_speedBoostTicks > 0) {
            g_speedBoostTicks--;
            if (g_speedBoostTicks == 0) g_currentDelay = BASE_SPEED;
        }

        render();
        Sleep(g_currentDelay);
    }
}

/* ==========================================================================
 *                          游戏初始化
 * ========================================================================== */
void initGame(void) {
    g_score = 0;
    g_isPaused = 0;
    g_isGameOver = 0;
    g_speedBoostTicks = 0;
    g_currentDelay = BASE_SPEED;
    g_shockwaveCharges = 0;
    g_floatingMsgLife = 0;
    g_gameOverReason[0] = '\0';

    initSnake();
    generateObstacles(INIT_OBSTACLES);
    spawnFood();
}

/* ==========================================================================
 *                       游戏结束处理
 * ========================================================================== */
void onGameOver(void) {
    char name[16];
    int  i;

    system("cls");
    SetConsoleTextAttribute(g_hMenu, COLOR_TITLE);
    printf("\n\n");
    printf("  ###################################\n");
    printf("  #          游 戏 结 束            #\n");
    printf("  ###################################\n\n");

    SetConsoleTextAttribute(g_hMenu, COLOR_INFO);
    printf("  本局得分 : ");
    SetConsoleTextAttribute(g_hMenu, COLOR_GOLDEN);
    printf("%d\n", g_score);

    SetConsoleTextAttribute(g_hMenu, COLOR_INFO);
    printf("  蛇身长度 : ");
    SetConsoleTextAttribute(g_hMenu, COLOR_DEFAULT);
    printf("%d\n", g_snake.length);

    SetConsoleTextAttribute(g_hMenu, COLOR_INFO);
    printf("  结束原因 : ");
    SetConsoleTextAttribute(g_hMenu, COLOR_NORMAL);
    printf("%s\n\n", g_gameOverReason);

    SetConsoleTextAttribute(g_hMenu, COLOR_DEFAULT);
    printf("  请输入您的名字保存成绩（直接回车使用 \"匿名玩家\"）：\n  > ");

    while (_kbhit()) _getch();

    /* 临时显示光标，方便用户输入 */
    CONSOLE_CURSOR_INFO ci;
    ci.dwSize = 1;
    ci.bVisible = TRUE;
    SetConsoleCursorInfo(g_hMenu, &ci);

    if (fgets(name, sizeof(name), stdin) == NULL) {
        strcpy(name, "匿名玩家");
    } else {
        for (i = 0; name[i]; i++) {
            if (name[i] == '\n' || name[i] == '\r') { name[i] = '\0'; break; }
        }
        if (name[0] == '\0') strcpy(name, "匿名玩家");
    }

    /* 重新隐藏光标 */
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(g_hMenu, &ci);

    saveScore(name, g_score);
    printf("\n  成绩已保存！按任意键查看排行榜...\n");
    _getch();
    showLeaderboard();
}

/* ==========================================================================
 *                          主菜单
 * ========================================================================== */
int showMainMenu(void) {
    int ch;
    system("cls");
    SetConsoleTextAttribute(g_hMenu, COLOR_TITLE);
    printf("\n\n");
    printf("    ##############################################\n");
    printf("    #                                            #\n");
    printf("    #          贪 吃 蛇  ·  拓 展 版             #\n");
    printf("    #                                            #\n");
    printf("    #          C 语言 / Windows 控制台版         #\n");
    printf("    #                                            #\n");
    printf("    ##############################################\n\n");

    SetConsoleTextAttribute(g_hMenu, COLOR_INFO);
    printf("    [1] 开始新游戏\n");
    printf("    [2] 查看排行榜\n");
    printf("    [3] 游戏说明\n");
    printf("    [0] 退出游戏\n\n");

    SetConsoleTextAttribute(g_hMenu, COLOR_DEFAULT);
    printf("    请选择: ");

    ch = _getch();
    return ch;
}

void showHelp(void) {
    int i;
    system("cls");
    SetConsoleTextAttribute(g_hMenu, COLOR_TITLE);
    printf("\n  ========== 游戏说明 ==========\n\n");

    SetConsoleTextAttribute(g_hMenu, COLOR_INFO); printf("  操作方式：\n");
    SetConsoleTextAttribute(g_hMenu, COLOR_DEFAULT);
    printf("    WASD 或方向键 - 控制蛇移动\n");
    printf("    空格         - 暂停 / 继续\n");
    printf("    F            - 释放冲击波（消耗 1 次）\n");
    printf("    R            - 重新开始本局\n");
    printf("    ESC          - 退出当前游戏\n\n");

    SetConsoleTextAttribute(g_hMenu, COLOR_INFO); printf("  游戏规则：\n");
    SetConsoleTextAttribute(g_hMenu, COLOR_DEFAULT);
    printf("    1. 撞到墙壁或障碍物会死亡\n");
    printf("    2. 自撞不会死亡，但会被截掉撞击点之后的尾巴\n");
    printf("    3. 不同食物有不同的效果（看右侧食物图鉴）\n");
    printf("    4. 吃到 * 冲击波果会获得 1 次冲击波技能\n");
    printf("    5. 释放冲击波可摧毁周围障碍物，每个 +20 分\n\n");

    SetConsoleTextAttribute(g_hMenu, COLOR_INFO); printf("  食物类型：\n");
    for (i = 0; i < FOOD_TYPE_COUNT; i++) {
        printf("    ");
        SetConsoleTextAttribute(g_hMenu, FOOD_TABLE[i].color);
        printf("%c ", FOOD_TABLE[i].symbol);
        SetConsoleTextAttribute(g_hMenu, COLOR_DEFAULT);
        printf("%-8s 分数:%+3d  生长:%+2d 节",
               FOOD_TABLE[i].name,
               FOOD_TABLE[i].score,
               FOOD_TABLE[i].growth);
        if (FOOD_TABLE[i].effect == 1) printf("  (+加速效果)");
        else if (FOOD_TABLE[i].effect == 2) printf("  (+1 冲击波)");
        printf("\n");
    }

    printf("\n  按任意键返回主菜单...\n");
    _getch();
}

/* ==========================================================================
 *                              主函数
 * ========================================================================== */
int main(void) {
    int choice;

    SetConsoleOutputCP(936);
    SetConsoleTitle("贪吃蛇拓展版 - C 语言版");

    /* 初始化双缓冲机制（程序启动时一次性创建两个屏幕缓冲）*/
    initDoubleBuffer();

    srand((unsigned int)time(NULL));

    while (1) {
        choice = showMainMenu();
        switch (choice) {
            case '1':
                initGame();
                enterGameMode();    /* 切换到游戏缓冲区显示 */
                gameLoop();
                exitGameMode();     /* 切回菜单缓冲区 */
                if (g_isGameOver == 1 &&
                    strcmp(g_gameOverReason, "玩家主动重开") != 0) {
                    onGameOver();
                }
                break;
            case '2':
                showLeaderboard();
                break;
            case '3':
                showHelp();
                break;
            case '0':
            case 27:
                cleanupDoubleBuffer();
                system("cls");
                SetConsoleTextAttribute(g_hMenu, COLOR_TITLE);
                printf("\n\n  感谢游玩，再见！\n\n");
                SetConsoleTextAttribute(g_hMenu, COLOR_DEFAULT);
                return 0;
            default:
                break;
        }
    }
    return 0;
}
