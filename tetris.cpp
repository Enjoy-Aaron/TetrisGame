#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <string>
#include <vector>

// ============================================================
// 一、布局与尺寸常量
// ============================================================
static constexpr int BOARD_W = 16;      // 棋盘列数
static constexpr int BOARD_H = 20;      // 棋盘行数
static constexpr int CELL = 30;         // 每格像素大小
static constexpr int BOARD_X = 25;      // 棋盘左上角 x
static constexpr int BOARD_Y = 25;      // 棋盘左上角 y
static constexpr int PANEL_X = 550;     // 右侧信息栏起点 x
static constexpr int WIN_W = 800;       // 窗口宽
static constexpr int WIN_H = 650;       // 窗口高

static constexpr int RANK_SIZE = 5;     // 排行榜显示几条
static constexpr const char* SCORE_FILE = "high_score.txt";  // 最高分文件
static constexpr const char* RANK_FILE = "leaderboard.txt";  // 排行榜文件

// ============================================================
// 二、颜色（RGBA）
// ============================================================
static const Color kBg       = {18, 18, 24, 255};       //很深的蓝灰色，整个窗口的底色
static const Color kBoardBg  = {11, 11, 16, 255};       //比底色更暗的近黑色，棋盘内部的背景
static const Color kGrid     = {30, 30, 40, 255};       //很暗的灰蓝，棋盘上画格子用的细线
static const Color kText     = {226, 226, 232, 255};    //接近纯白的亮灰，主要正文和数字
static const Color kTextDim  = {128, 128, 144, 255};    //中灰色，次要提示/说明文字
static const Color kGold     = {240, 200, 70, 255};     //金黄色，强调色
static const Color kSelect   = {52, 84, 158, 255};      //中等蓝色，菜单按钮被选中时的高亮底色

// 形状编号：0=T 1=J 2=L 3=S 4=Z 5=O 6=I，下标直接对应颜色
static const Color kShapeColor[7] = {
    {176, 80, 225, 255},   // T型 紫色
    {52, 92, 232, 255},    // J型 蓝色
    {238, 142, 38, 255},   // L型 橙色
    {60, 185, 80, 255},    // S型 绿色
    {228, 68, 72, 255},    // Z型 红色
    {232, 205, 55, 255},   // O型 黄色
    {45, 198, 218, 255}    // I型 青色
};

// 把颜色按倍数调亮/调暗（用于方块的高光和暗边）
static Color Shade(Color c, float f)    // f>1.0 调亮，f<1.0 调暗
{
    return Color{
        (unsigned char)std::clamp((int)(c.r * f), 0, 255),
        (unsigned char)std::clamp((int)(c.g * f), 0, 255),
        (unsigned char)std::clamp((int)(c.b * f), 0, 255),
        c.a};
}

// 画一个带立体高光的小方块
static void DrawCell(int x, int y, int sz, Color c)
{
    DrawRectangle(x, y, sz, sz, c);     // 方块主体
    int b = sz >= 25 ? 3 : 2;           // 高光和暗边的宽度
    DrawRectangle(x + 1, y + 1, sz - 2, b, Shade(c, 1.35f));            // 顶部高光
    DrawRectangle(x + 1, y + 1, b, sz - 2, Shade(c, 1.35f));            // 左侧高光
    DrawRectangle(x + 1, y + sz - 1 - b, sz - 2, b, Shade(c, 0.55f));   // 底部暗边
    DrawRectangle(x + sz - 1 - b, y + 1, b, sz - 2, Shade(c, 0.55f));   // 右侧暗边
}

// 画落点影子（半透明填充 + 描边）
static void DrawGhostCell(int x, int y, int sz, Color c)
{
    DrawRectangle(x + 1, y + 1, sz - 2, sz - 2, Color{c.r, c.g, c.b, 40});      // 半透明填充  
    DrawRectangleLinesEx(Rectangle{x + 1.0f, y + 1.0f, sz - 2.0f, sz - 2.0f},   
                         1.5f, Color{c.r, c.g, c.b, 150});                      // 半透明描边
}

// ============================================================
// 三、中文字体加载
// ============================================================
// 所有界面会用到的中文文案，程序启动时从中提取需要的汉字码点加载字体
static const char* kUiTexts[] = {
    "俄罗斯方块",
    "开始游戏", "排行榜", "退出游戏",
    "↑↓ 选择，回车确认，也可点击鼠标",
    "选择难度", "模式", "简单", "普通", "困难",
    "适合新手，下落较慢", "标准速度", "高速挑战",
    "↑↓ 选择，回车确认，Esc 返回",
    "第 ", " 名    ", " 分", "---",
    "按任意键或点击鼠标返回",
    "下一个", "当前分数", "最高纪录", "当前等级", "消除行数",
    "← → 移动", "↓ 加速", "↑/空格 旋转", "P 暂停", "R 重开", "Esc 菜单",
    "难度：",
    "游戏暂停", "按 P 继续", "Esc 返回主菜单",
    "游戏结束", "本局得分：", "成绩已记入排行榜",
    "R 重新开始", "Esc 返回主菜单"};

// 从 kUiTexts 里解析出所有字符的 Unicode 码点（含 ASCII 和箭头）
static std::vector<int> CollectCodePoints()
{
    std::set<int> s;
    for (int c = 32; c <= 126; ++c) s.insert(c);
    static const int extra[] = {
        0x2190, 0x2191, 0x2192, 0x2193,   // ← ↑ → ↓
        0xFF0C, 0xFF1A,                    // ， ：
        0x2014};                           // —
    for (int c : extra) s.insert(c);

    // 手动按 UTF-8 规则解析每个汉字的码点
    for (const char* t : kUiTexts)
    {
        const unsigned char* p = (const unsigned char*)t;
        while (*p)
        {
            int cp = 0;
            if (*p < 0x80) { cp = *p; ++p; }
            else if ((*p & 0xE0) == 0xC0)
            {
                cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
                p += 2;
            }
            else if ((*p & 0xF0) == 0xE0)
            {
                cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
                p += 3;
            }
            else if ((*p & 0xF8) == 0xF0)
            {
                cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
                     ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
                p += 4;
            }
            else ++p;
            s.insert(cp);
        }
    }
    return std::vector<int>(s.begin(), s.end());
}

static Font fontUI;   // 普通字号字体
static Font fontBig;  // 大标题字体

// 水平居中绘制一段文字，cx 是中心点 x
static void TextCenter(Font f, const char* text, float cx, float y, float size, Color c)
{
    Vector2 m = MeasureTextEx(f, text, size, 1.0f);
    DrawTextEx(f, text, Vector2{cx - m.x / 2, y}, size, 1.0f, c);
}

// ============================================================
// 四、方块形状表（三维数组 piece[7][4][4]）
//    piece[形状编号][旋转形态][行][列] = 1 表示该格有方块
// ============================================================
static int g_piece[7][4][4][4];

// 初始化 28 个方块矩阵：先手工填 0 号形态，再用旋转公式生成其余 3 种
static void InitPieces()
{
    for (int sh = 0; sh < 7; ++sh)
        for (int f = 0; f < 4; ++f)
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    g_piece[sh][f][i][j] = 0;

    // T 形
    for (int i = 0; i <= 2; ++i) g_piece[0][0][1][i] = 1;
    g_piece[0][0][2][1] = 1;

    // J 形和 L 形
    for (int i = 1; i <= 3; ++i)
    {
        g_piece[1][0][i][1] = 1;
        g_piece[2][0][i][2] = 1;
    }
    g_piece[1][0][3][2] = 1;
    g_piece[2][0][3][1] = 1;

    // S、Z、O 形
    for (int i = 0; i <= 1; ++i)
    {
        g_piece[3][0][1][i] = 1;
        g_piece[3][0][2][i + 1] = 1;
        g_piece[4][0][1][i + 1] = 1;
        g_piece[4][0][2][i] = 1;
        g_piece[5][0][1][i + 1] = 1;
        g_piece[5][0][2][i + 1] = 1;
    }

    // I 形
    for (int i = 0; i <= 3; ++i) g_piece[6][0][1][i] = 1;

    // 顺时针旋转 90°：新矩阵[i][j] = 旧矩阵[3-j][i]
    for (int sh = 0; sh < 7; ++sh)
        for (int f = 0; f < 3; ++f)
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    g_piece[sh][f + 1][i][j] = g_piece[sh][f][3 - j][i];
}

// ============================================================
// 五、棋盘（二维数组 cell[20][16]）
//    cell[y][x] = -1 空格；0~6 表示该格被哪种形状的方块占据
// ============================================================
static int cell[BOARD_H][BOARD_W];

// 清空棋盘，全部置为 -1
static void resetBoard()
{
    for (int i = 0; i < BOARD_H; ++i)
        for (int j = 0; j < BOARD_W; ++j)
            cell[i][j] = -1;
}

// 判断方块放在 (bx, by) 处是否合法：不越界、不撞已有方块
// by < 0 时上方越界放行（方块刚从顶部出生时一半在屏外）
static bool legal(int shape, int form, int bx, int by)
{
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            if (g_piece[shape][form][i][j] != 1) continue;
            int x = bx + j;
            int y = by + i;
            if (x < 0 || x >= BOARD_W || y >= BOARD_H) return false;
            if (y < 0) continue;
            if (cell[y][x] != -1) return false;
        }
    }
    return true;
}

// 把活动方块写进棋盘（落地锁定时调用），记录它的形状编号
static void lockPiece(int shape, int form, int bx, int by)
{
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (g_piece[shape][form][i][j] == 1)
            {
                int y = by + i, x = bx + j;
                if (y >= 0 && y < BOARD_H && x >= 0 && x < BOARD_W)
                    cell[y][x] = shape;
            }
}

// 找出所有满行，返回行号列表
static std::vector<int> fullRows()
{
    std::vector<int> rows;
    for (int y = 0; y < BOARD_H; ++y)
    {
        bool full = true;
        for (int x = 0; x < BOARD_W; ++x)
            if (cell[y][x] == -1) { full = false; break; }
        if (full) rows.push_back(y);
    }
    return rows;
}

// 删除指定满行：先标记哪些行满，再从下往上只把非满行压下来，顶部补空
// 这样连续多行同时消除时不会漏
static void removeRows(const std::vector<int>& rows)
{
    std::vector<bool> full(BOARD_H, false);
    for (int r : rows) full[r] = true;

    int write = BOARD_H - 1;
    for (int y = BOARD_H - 1; y >= 0; --y)
    {
        if (full[y]) continue;
        for (int x = 0; x < BOARD_W; ++x)
            cell[write][x] = cell[y][x];
        --write;
    }
    for (; write >= 0; --write)
        for (int x = 0; x < BOARD_W; ++x)
            cell[write][x] = -1;
}

// ============================================================
// 六、难度表（结构体数组）
// ============================================================
struct Difficulty
{
    const char* name;       // 难度名
    const char* desc;       // 一句话说明
    float initial;          // 初始下落间隔（秒）
    float step;            // 每升一级间隔减少多少
    float minimum;          // 间隔下限
    int linesPerLevel;      // 消多少行升一级
};

static const Difficulty kDiff[3] = {
    {"简单", "适合新手，下落较慢", 0.280f, 0.020f, 0.080f, 15},
    {"普通", "标准速度",           0.140f, 0.010f, 0.040f, 10},
    {"困难", "高速挑战",           0.093f, 0.007f, 0.027f, 8}};

// ============================================================
// 七、排行榜（数组 scores[5] + 排序）
// ============================================================
static int scores[RANK_SIZE];

// 从文件读排行榜，读不到就填 0
static void loadRanks()
{
    std::ifstream in(RANK_FILE);
    for (int i = 0; i < RANK_SIZE; ++i)
        if (!(in >> scores[i])) scores[i] = 0;
}

// 把排行榜写回文件
static void saveRanks()
{
    std::ofstream out(RANK_FILE);
    for (int i = 0; i < RANK_SIZE; ++i) out << scores[i] << '\n';
}

// 插入一个新分数：合并进旧分数、降序排序、取前 5 名
static void addScore(int score)
{
    std::vector<int> all;
    for (int i = 0; i < RANK_SIZE; ++i) all.push_back(scores[i]);
    all.push_back(score);
    std::sort(all.begin(), all.end(), std::greater<int>());
    for (int i = 0; i < RANK_SIZE; ++i) scores[i] = all[i];
}

// ============================================================
// 八、方块生成队列（数组实现的环形队列）
//    作用：方块按 FIFO 顺序依次出现，队头取走一个，队尾补一个随机块
// ============================================================
struct PiecePair
{
    int shape;
    int form;
};

static const int Q_CAP = 8;            // 队列容量
static PiecePair pieceQueue[Q_CAP];   // 队列存储
static int qHead = 0;                // 队头下标
static int qTail = 0;               // 队尾下标
static int qCount = 0;             // 当前元素个数

// 清空队列并预填三个随机块（接下来要依次出现的 3 个方块）
static void qInit()
{
    qHead = qTail = qCount = 0;
    for (int i = 0; i < 3; ++i)
    {
        pieceQueue[qTail] = {GetRandomValue(0, 6), GetRandomValue(0, 3)};
        qTail = (qTail + 1) % Q_CAP;
        ++qCount;
    }
}

// 队尾入队一个随机方块
static void qPushRandom()
{
    pieceQueue[qTail] = {GetRandomValue(0, 6), GetRandomValue(0, 3)};
    qTail = (qTail + 1) % Q_CAP;
    ++qCount;
}

// 队头出队并返回该方块，同时队尾补一个随机块（保持队列长度）
static PiecePair qPop()
{
    PiecePair p = pieceQueue[qHead];
    qHead = (qHead + 1) % Q_CAP;
    --qCount;
    qPushRandom();
    return p;
}

// 只看队头（即"下一个方块"），不出队
static PiecePair qFront()
{
    return pieceQueue[qHead];
}

// ============================================================
// 九、游戏状态（全局变量）
// ============================================================
enum Phase
{
    PH_MENU,        // 主菜单
    PH_DIFFICULTY,  // 难度选择
    PH_RANK,        // 排行榜
    PH_PLAYING,     // 游戏中
    PH_PAUSED,      // 暂停
    PH_CLEARING,    // 消行闪烁动画中
    PH_GAMEOVER     // 游戏结束
};

// 前向声明（有些函数互相调用，先声明再定义）
static void spawn();
static void endGame();
static void updateMenu();
static void chooseMenu(int i);
static void updateDifficulty();
static void updateRank();
static void updatePaused();
static void updateOver();
static void updatePlaying(float dt);
static void updateClearing(float dt);
static void drawMenu();
static void drawDifficulty();
static void drawRank();
static void drawGame();
static void drawPause();
static void drawGameOverScreen();
static void drawPanel();
static void statBlock(float x, int y, const char* label, int value);
static void drawNext(Rectangle pv);

static Phase phase = PH_MENU;   // 当前界面
static bool wantExit = false;   // 是否退出程序

static int menuSel = 0;   // 菜单选中项
static int diffSel = 1;   // 难度选中项（默认普通）

// 正在下落的方块
static int shape = 0, form = 0, px = 3, py = -1;
static int diffIdx = 1;   // 当前难度下标

// 计分
static int level = 1;
static int totalLines = 0;
static int score = 0;
static int high = 0;

// 计时器（单位：秒）
static float dropTimer = 0.0f;   // 自然下落累计时间
static float repL = 0.0f, repR = 0.0f, repD = 0.0f;  // 左右软降长按重复计时
static bool holdL = false, holdR = false, holdD = false;  // 对应键是否按住

// 消行动画
static std::vector<int> clearing;  // 正在闪烁的行号
static float clearTimer = 0.0f;

// 菜单/难度按钮的可点击区域（鼠标检测用）
static Rectangle menuBounds[3];
static Rectangle diffBounds[3];

// 计算当前下落间隔：随等级加快，但不低于下限
static float dropInterval()
{
    const Difficulty& d = kDiff[diffIdx];
    return std::max(d.minimum, d.initial - (level - 1) * d.step);
}

// 保存最高分到文件
static void saveHigh()
{
    std::ofstream out(SCORE_FILE);
    if (out) out << high;
}

// 开始一局新游戏：重置所有数据、初始化队列、生成第一个方块
static void startGame(int idx)
{
    diffIdx = idx;
    resetBoard();
    score = 0;
    level = 1;
    totalLines = 0;
    dropTimer = 0.0f;
    holdL = holdR = holdD = false;
    repL = repR = repD = 0.0f;
    clearing.clear();
    qInit();
    spawn();
    phase = PH_PLAYING;
}

// 从队列取出新方块放到顶部，位置不合法说明堆到顶了，游戏结束
static void spawn()
{
    PiecePair cur = qPop();
    shape = cur.shape;
    form = cur.form;
    px = 3;
    py = -1;
    dropTimer = 0.0f;
    if (!legal(shape, form, px, py)) endGame();
}

// 游戏结束：写入排行榜、更新最高分、切到结束界面
static void endGame()
{
    addScore(score);
    saveRanks();
    high = std::max(high, score);
    saveHigh();
    phase = PH_GAMEOVER;
}

// 尝试把方块平移 (dx, dy)，合法就走，返回是否成功
static bool moveBy(int dx, int dy)
{
    if (!legal(shape, form, px + dx, py + dy)) return false;
    px += dx;
    py += dy;
    return true;
}

// 旋转：先原地转，不行就左右踢墙（简单的 wall kick）
static void rotate()
{
    int nf = (form + 1) % 4;
    static const int ox[] = {0, -1, 1, -2, 2};
    for (int oy = 0; oy >= -1; --oy)
        for (int k = 0; k < 5; ++k)
            if (legal(shape, nf, px + ox[k], py + oy))
            {
                px += ox[k];
                py += oy;
                form = nf;
                return;
            }
}

// 方块落地：写入棋盘，如果有满行就进消行动画状态，否则直接生成下一个
static void beginLock()
{
    lockPiece(shape, form, px, py);
    std::vector<int> rows = fullRows();
    if (!rows.empty())
    {
        clearing = rows;
        clearTimer = 0.0f;
        phase = PH_CLEARING;
    }
    else spawn();
}

// 方块向下走一格，走不动就落地
static void stepDown()
{
    if (!moveBy(0, 1)) beginLock();
}

// 计算影子位置：从当前 y 一直往下试，直到不合法
static int shadowY()
{
    int sy = py;
    while (legal(shape, form, px, sy + 1)) ++sy;
    return sy;
}

// 消完 n 行后更新分数和等级
static void addLines(int n)
{
    if (n <= 0) return;
    score += n * 10;
    totalLines += n;
    level = totalLines / kDiff[diffIdx].linesPerLevel + 1;
    high = std::max(high, score);
}

// ============================================================
// 十、更新逻辑（每帧调用，按当前状态分发）
// ============================================================
static void update(float dt)
{
    switch (phase)
    {
    case PH_MENU:       updateMenu(); break;
    case PH_DIFFICULTY: updateDifficulty(); break;
    case PH_RANK:       updateRank(); break;
    case PH_PLAYING:    updatePlaying(dt); break;
    case PH_PAUSED:     updatePaused(); break;
    case PH_CLEARING:   updateClearing(dt); break;
    case PH_GAMEOVER:   updateOver(); break;
    }
}

// 主菜单：鼠标悬停切换选中、上下键选择、回车确认、Esc 退出
static void updateMenu()
{
    Vector2 m = GetMousePosition();
    for (int i = 0; i < 3; ++i)
        if (CheckCollisionPointRec(m, menuBounds[i])) menuSel = i;
    if (IsKeyPressed(KEY_UP) && menuSel > 0) --menuSel;
    if (IsKeyPressed(KEY_DOWN) && menuSel < 2) ++menuSel;
    if (IsKeyPressed(KEY_ENTER)) chooseMenu(menuSel);
    if (IsKeyPressed(KEY_ESCAPE)) wantExit = true;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        for (int i = 0; i < 3; ++i)
            if (CheckCollisionPointRec(m, menuBounds[i])) chooseMenu(i);
}

// 菜单选项被选中后做什么
static void chooseMenu(int i)
{
    if (i == 0) phase = PH_DIFFICULTY;
    else if (i == 1) phase = PH_RANK;
    else wantExit = true;
}

// 难度选择界面
static void updateDifficulty()
{
    Vector2 m = GetMousePosition();
    for (int i = 0; i < 3; ++i)
        if (CheckCollisionPointRec(m, diffBounds[i])) diffSel = i;
    if (IsKeyPressed(KEY_UP) && diffSel > 0) --diffSel;
    if (IsKeyPressed(KEY_DOWN) && diffSel < 2) ++diffSel;
    if (IsKeyPressed(KEY_ENTER)) startGame(diffSel);
    if (IsKeyPressed(KEY_ESCAPE)) phase = PH_MENU;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        for (int i = 0; i < 3; ++i)
            if (CheckCollisionPointRec(m, diffBounds[i])) startGame(i);
}

// 排行榜：任意键或点击返回主菜单
static void updateRank()
{
    if (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        phase = PH_MENU;
}

// 暂停界面：P 继续，Esc 回主菜单
static void updatePaused()
{
    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_S)) phase = PH_PLAYING;
    if (IsKeyPressed(KEY_ESCAPE))
    {
        saveHigh();
        phase = PH_MENU;
    }
}

// 结束界面：R 重开，回车/Esc/点击回主菜单
static void updateOver()
{
    if (IsKeyPressed(KEY_R)) startGame(diffIdx);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) phase = PH_MENU;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) phase = PH_MENU;
}

// 游戏中：处理按键、长按重复、自然下落
static void updatePlaying(float dt)
{
    if (IsKeyPressed(KEY_ESCAPE)) { saveHigh(); phase = PH_MENU; return; }
    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_S)) { phase = PH_PAUSED; return; }
    if (IsKeyPressed(KEY_R)) { startGame(diffIdx); return; }

    // 左移（带长按加速）
    if (IsKeyPressed(KEY_LEFT)) { moveBy(-1, 0); holdL = true; repL = 0.0f; }
    if (IsKeyReleased(KEY_LEFT)) holdL = false;
    if (holdL && IsKeyDown(KEY_LEFT))
    {
        repL += dt;
        if (repL >= 0.16f) { moveBy(-1, 0); repL -= 0.05f; }
    }

    // 右移
    if (IsKeyPressed(KEY_RIGHT)) { moveBy(1, 0); holdR = true; repR = 0.0f; }
    if (IsKeyReleased(KEY_RIGHT)) holdR = false;
    if (holdR && IsKeyDown(KEY_RIGHT))
    {
        repR += dt;
        if (repR >= 0.16f) { moveBy(1, 0); repR -= 0.05f; }
    }

    // 软降（加速下落）
    if (IsKeyPressed(KEY_DOWN)) { stepDown(); holdD = true; repD = 0.0f; }
    if (IsKeyReleased(KEY_DOWN)) holdD = false;
    if (holdD && IsKeyDown(KEY_DOWN))
    {
        repD += dt;
        if (repD >= 0.05f) { stepDown(); repD -= 0.05f; }
    }

    // 旋转（↑ 或空格）
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_SPACE)) rotate();

    if (phase != PH_PLAYING) return;

    // 按时间自动下落
    dropTimer += dt;
    if (dropTimer >= dropInterval())
    {
        dropTimer = 0.0f;
        stepDown();
    }
}

// 消行动画：播完 0.42 秒后真正删行、加分、生成下一个方块
static void updateClearing(float dt)
{
    clearTimer += dt;
    if (clearTimer >= 0.42f)
    {
        removeRows(clearing);
        addLines((int)clearing.size());
        clearing.clear();
        spawn();
        if (phase == PH_CLEARING) phase = PH_PLAYING;
    }
}

// ============================================================
// 十一、绘制（每帧调用）
// ============================================================

// 总入口：先画背景，再按当前状态画对应界面
static void draw()
{
    DrawRectangle(0, 0, WIN_W, WIN_H, kBg);
    switch (phase)
    {
    case PH_MENU: drawMenu(); break;
    case PH_DIFFICULTY: drawDifficulty(); break;
    case PH_RANK: drawRank(); break;
    default:
        drawGame();
        if (phase == PH_PAUSED) drawPause();
        if (phase == PH_GAMEOVER) drawGameOverScreen();
        break;
    }
}

// 主菜单
static void drawMenu()
{
    TextCenter(fontBig, "俄罗斯方块", WIN_W / 2.0f, 110, 60, kGold);
    const char* items[3] = {"开始游戏", "排行榜", "退出游戏"};
    for (int i = 0; i < 3; ++i)
    {
        Rectangle b = Rectangle{(WIN_W - 260) / 2.0f, 260 + i * 70.0f, 260, 56};
        menuBounds[i] = b;
        bool on = i == menuSel;
        if (on)
        {
            DrawRectangleRounded(b, 0.3f, 8, kSelect);
            DrawRectangleRoundedLinesEx(b, 0.3f, 8, 2.0f, Shade(kSelect, 1.4f));
        }
        TextCenter(fontUI, items[i], WIN_W / 2.0f, b.y + 12, 28, on ? WHITE : kTextDim);
    }
    TextCenter(fontUI, "↑↓ 选择，回车确认，也可点击鼠标", WIN_W / 2.0f, 590, 18, kTextDim);
}

// 难度选择
static void drawDifficulty()
{
    TextCenter(fontBig, "选择难度", WIN_W / 2.0f, 80, 46, kGold);
    for (int i = 0; i < 3; ++i)
    {
        Rectangle b = Rectangle{(WIN_W - 380) / 2.0f, 200 + i * 100.0f, 380, 78};
        diffBounds[i] = b;
        bool on = i == diffSel;
        if (on)
        {
            DrawRectangleRounded(b, 0.18f, 8, kSelect);
            DrawRectangleRoundedLinesEx(b, 0.18f, 8, 2.0f, Shade(kSelect, 1.4f));
        }
        std::string name = std::string(kDiff[i].name) + "模式";
        TextCenter(fontUI, name.c_str(), WIN_W / 2.0f, b.y + 10, 26, on ? WHITE : kText);
        TextCenter(fontUI, kDiff[i].desc, WIN_W / 2.0f, b.y + 44, 17, on ? kText : kTextDim);
    }
    TextCenter(fontUI, "↑↓ 选择，回车确认，Esc 返回", WIN_W / 2.0f, 600, 18, kTextDim);
}

// 排行榜
static void drawRank()
{
    TextCenter(fontBig, "排行榜", WIN_W / 2.0f, 65, 46, kGold);
    for (int i = 0; i < RANK_SIZE; ++i)
    {
        float y = 155 + i * 80.0f;
        if (scores[i] > 0)
        {
            std::string s = "第 " + std::to_string(i + 1) + " 名    " +
                            std::to_string(scores[i]) + " 分";
            TextCenter(fontUI, s.c_str(), WIN_W / 2.0f, y, 28, i == 0 ? kGold : kText);
        }
        else
        {
            std::string s = "第 " + std::to_string(i + 1) + " 名    ---";
            TextCenter(fontUI, s.c_str(), WIN_W / 2.0f, y, 28, kTextDim);
        }
    }
    TextCenter(fontUI, "按任意键或点击鼠标返回", WIN_W / 2.0f, 600, 18, kTextDim);
}

// 游戏主画面：棋盘、已落方块、影子、活动方块、右侧信息栏
static void drawGame()
{
    // 棋盘外框和背景
    DrawRectangle(BOARD_X - 4, BOARD_Y - 4, BOARD_W * CELL + 8, BOARD_H * CELL + 8,
                  Color{42, 42, 56, 255});
    DrawRectangle(BOARD_X, BOARD_Y, BOARD_W * CELL, BOARD_H * CELL, kBoardBg);
    for (int x = 1; x < BOARD_W; ++x)
        DrawRectangle(BOARD_X + x * CELL, BOARD_Y, 1, BOARD_H * CELL, kGrid);
    for (int y = 1; y < BOARD_H; ++y)
        DrawRectangle(BOARD_X, BOARD_Y + y * CELL, BOARD_W * CELL, 1, kGrid);

    // 已锁定的方块
    for (int y = 0; y < BOARD_H; ++y)
    {
        for (int x = 0; x < BOARD_W; ++x)
        {
            if (cell[y][x] == -1) continue;
            Color c = kShapeColor[cell[y][x]];
            // 消行闪烁：正在删除的行每隔一小段变白
            if (phase == PH_CLEARING &&
                std::find(clearing.begin(), clearing.end(), y) != clearing.end())
            {
                if (std::fmod(clearTimer, 0.14f) < 0.07f) c = WHITE;
            }
            DrawCell(BOARD_X + x * CELL, BOARD_Y + y * CELL, CELL, c);
        }
    }

    // 活动方块和它的影子
    if (phase == PH_PLAYING || phase == PH_PAUSED)
    {
        int sy = shadowY();
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
            {
                if (g_piece[shape][form][i][j] != 1) continue;
                DrawGhostCell(BOARD_X + (px + j) * CELL,
                              BOARD_Y + (sy + i) * CELL, CELL, kShapeColor[shape]);
            }
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
            {
                if (g_piece[shape][form][i][j] != 1) continue;
                int by = py + i;
                if (by < 0) continue;
                DrawCell(BOARD_X + (px + j) * CELL,
                         BOARD_Y + by * CELL, CELL, kShapeColor[shape]);
            }
    }

    drawPanel();
}

// 右侧信息栏：下一个方块、分数、操作说明
static void drawPanel()
{
    float x = (float)PANEL_X;

    DrawTextEx(fontUI, "下一个", Vector2{x, 18}, 22, 1.0f, kText);

    // 预览框加高到能竖排显示 3 个方块
    Rectangle pv = Rectangle{x + 5, 48, 190, 195};
    DrawRectangleRounded(pv, 0.08f, 8, Color{26, 26, 36, 255});
    DrawRectangleRoundedLinesEx(pv, 0.08f, 8, 2.0f, Color{52, 52, 70, 255});
    drawNext(pv);

    statBlock(x, 260, "当前分数", score);
    statBlock(x, 325, "最高纪录", high);
    statBlock(x, 390, "当前等级", level);
    statBlock(x, 455, "消除行数", totalLines);

    const char* help[] = {"← → 移动", "↓ 加速", "↑/空格 旋转",
                          "P 暂停", "R 重开", "Esc 菜单"};
    for (int i = 0; i < 6; ++i)
        DrawTextEx(fontUI, help[i], Vector2{x, 505.0f + i * 20.0f}, 17, 1.0f, kTextDim);

    std::string d = "难度：" + std::string(kDiff[diffIdx].name);
    DrawTextEx(fontUI, d.c_str(), Vector2{x, 625}, 19, 1.0f, kGold);
}

// 画一个"标签 + 数值"的小块（分数、等级等）
static void statBlock(float x, int y, const char* label, int value)
{
    DrawTextEx(fontUI, label, Vector2{x, (float)y}, 18, 1.0f, kTextDim);
    DrawTextEx(fontUI, std::to_string(value).c_str(),
               Vector2{x, (float)y + 24}, 26, 1.0f, kText);
}

// 在预览框 pv 里竖排画队列前 3 个方块（接下来要依次出现的 3 个）
static void drawNext(Rectangle pv)
{
    const int gs = 18;                    // 每个小格像素
    float slotH = pv.height / 3.0f;       // 3 个方块平分高度
    for (int k = 0; k < 3; ++k)
    {
        // 从队头开始依次取 3 个（环形队列下标取模）
        int idx = (qHead + k) % Q_CAP;
        int sh = pieceQueue[idx].shape;
        int fm = pieceQueue[idx].form;

        // 算出这个方块的实际包围盒
        int minR = 4, maxR = -1, minC = 4, maxC = -1;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (g_piece[sh][fm][i][j] == 1)
                {
                    minR = std::min(minR, i);
                    maxR = std::max(maxR, i);
                    minC = std::min(minC, j);
                    maxC = std::max(maxC, j);
                }
        float w = (maxC - minC + 1) * gs;
        float h = (maxR - minR + 1) * gs;
        // 水平居中、垂直落在自己的槽位里
        float ox = pv.x + (pv.width - w) / 2;
        float oy = pv.y + k * slotH + (slotH - h) / 2;
        for (int i = minR; i <= maxR; ++i)
            for (int j = minC; j <= maxC; ++j)
                if (g_piece[sh][fm][i][j] == 1)
                    DrawCell((int)(ox + (j - minC) * gs),
                             (int)(oy + (i - minR) * gs), gs, kShapeColor[sh]);
    }
}

// 暂停遮罩
static void drawPause()
{
    DrawRectangle(0, 0, WIN_W, WIN_H, Color{0, 0, 0, 160});
    TextCenter(fontBig, "游戏暂停", WIN_W / 2.0f, 250, 52, kText);
    TextCenter(fontUI, "按 P 继续", WIN_W / 2.0f, 330, 22, kText);
    TextCenter(fontUI, "Esc 返回主菜单", WIN_W / 2.0f, 375, 20, kTextDim);
}

// 游戏结束遮罩
static void drawGameOverScreen()
{
    DrawRectangle(0, 0, WIN_W, WIN_H, Color{0, 0, 0, 170});
    TextCenter(fontBig, "游戏结束", WIN_W / 2.0f, 170, 52, Color{235, 95, 95, 255});
    std::string s = "本局得分：" + std::to_string(score);
    TextCenter(fontUI, s.c_str(), WIN_W / 2.0f, 260, 28, kText);
    TextCenter(fontUI, "成绩已记入排行榜", WIN_W / 2.0f, 310, 19, kTextDim);
    TextCenter(fontUI, "R 重新开始", WIN_W / 2.0f, 400, 22, kText);
    TextCenter(fontUI, "Esc 返回主菜单", WIN_W / 2.0f, 445, 20, kTextDim);
}

// ============================================================
// 十二、主程序入口
// ============================================================
int main()
{
    InitWindow(WIN_W, WIN_H, "俄罗斯方块");
    SetTargetFPS(60);

    InitPieces();

    // 加载中文字体（注意：Raylib 对 msyh.ttc 支持有问题，用黑体 ttf）
    std::vector<int> cp = CollectCodePoints();
    const char* fontPath = "C:\\Windows\\Fonts\\simhei.ttf";
    fontUI = LoadFontEx(fontPath, 26, cp.data(), (int)cp.size());
    fontBig = LoadFontEx(fontPath, 56, cp.data(), (int)cp.size());
    if (fontUI.texture.id == 0 || fontBig.texture.id == 0)
    {
        fontUI = GetFontDefault();
        fontBig = GetFontDefault();
    }

    // 读最高分和排行榜
    std::ifstream in(SCORE_FILE);
    int h = 0;
    if (in >> h) high = std::max(0, h);
    loadRanks();

    // 主循环
    while (!WindowShouldClose() && !wantExit)
    {
        float dt = GetFrameTime();
        update(dt);
        BeginDrawing();
        draw();
        EndDrawing();
    }

    saveHigh();
    UnloadFont(fontUI);
    UnloadFont(fontBig);
    CloseWindow();
    return 0;
}
