--- Page 18/22 ---
17 
该难度开局 
} 
  
// 排行榜界面：按任意键或点击鼠标，返回主菜单 
static void updateRank() 
{ 
    if (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) 
        phase = PH_MENU; 
} 
  
// 暂停界面：按 P 或 S 继续游戏；按 Esc 保存最高分并返回主菜单 
static void updatePaused() 
{ 
    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_S)) phase = PH_PLAYING;  // 继续 
    if (IsKeyPressed(KEY_ESCAPE)) 
    { 
        saveHigh();             // 回主菜单前先保存最高分 
        phase = PH_MENU; 
    } 
} 
  
// 游戏结束界面：R 用同一难度重新开始；回车 / Esc / 点击鼠标返回主菜单 
static void updateOver() 
{ 
    if (IsKeyPressed(KEY_R)) startGame(diffIdx);                       // 重新开始 
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) phase = PH_MENU;  // 回
菜单 
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) phase = PH_MENU;      // 点击回菜单 
} 
  
// 绘制总入口：每帧先铺一层背景底色，再根据当前状态画对应界面 
static void draw() 
{ 
    DrawRectangle(0, 0, WIN_W, WIN_H, kBg);   // 用背景色铺满整个窗口（同时清掉上一帧） 
    switch (phase) 
    { 
    case PH_MENU: drawMenu(); break;          // 主菜单 
    case PH_DIFFICULTY: drawDifficulty(); break;  // 难度选择 
    case PH_RANK: drawRank(); break;          // 排行榜 
    default: 
        // 游戏中 / 暂停 / 消行 / 结束 都以游戏画面为底 
        drawGame(); 
        if (phase == PH_PAUSED) drawPause();          // 暂停时在游戏画面上盖一层遮罩 
        if (phase == PH_GAMEOVER) drawGameOverScreen();  // 结束时盖结束遮罩

--- Page 19/22 ---
18 
        break; 
    } 
} 
  
// 绘制主菜单 
static void drawMenu() 
{ 
    TextCenter(fontBig, "俄罗斯方块", WIN_W / 2.0f, 110, 60, kGold);  // 大标题 
    const char* items[3] = {"开始游戏", "排行榜", "退出游戏"};         // 三个菜单项 
    for (int i = 0; i < 3; ++i) 
    { 
        // 计算第 i 个按钮的矩形（宽 260、高 56，水平居中，纵向间距 70） 
        Rectangle b = Rectangle{(WIN_W - 260) / 2.0f, 260 + i * 70.0f, 260, 56}; 
        menuBounds[i] = b;        // 记录按钮区域，供鼠标悬停/点击检测 
        bool on = i == menuSel;   // 是否为当前选中项 
        if (on) 
        { 
            // 选中：画蓝色圆角底 + 一圈更亮的描边 
            DrawRectangleRounded(b, 0.3f, 8, kSelect); 
            DrawRectangleRoundedLinesEx(b, 0.3f, 8, 2.0f, Shade(kSelect, 1.4f)); 
        } 
        // 选中项文字用白色，未选中用暗灰色 
        TextCenter(fontUI, items[i], WIN_W / 2.0f, b.y + 12, 28, on ? WHITE : 
kTextDim); 
    } 
    TextCenter(fontUI, "↑↓ 选择，回车确认，也可点击鼠标", WIN_W / 2.0f, 590, 18, 
kTextDim);  // 底部操作提示 
} 
  
// 绘制难度选择界面 
static void drawDifficulty() 
{ 
    TextCenter(fontBig, "选择难度", WIN_W / 2.0f, 80, 46, kGold);  // 标题 
    for (int i = 0; i < 3; ++i) 
    { 
        // 每个难度按钮：宽 380、高 78，纵向间距 100 
        Rectangle b = Rectangle{(WIN_W - 380) / 2.0f, 200 + i * 100.0f, 380, 78}; 
        diffBounds[i] = b;       // 记录可点击区域 
        bool on = i == diffSel;  // 是否被选中 
        if (on) 
        { 
            DrawRectangleRounded(b, 0.18f, 8, kSelect);                       // 选中
蓝底 
            DrawRectangleRoundedLinesEx(b, 0.18f, 8, 2.0f, Shade(kSelect, 1.4f));  //

--- Page 20/22 ---
19 
亮描边 
        } 
        std::string name = std::string(kDiff[i].name) + "模式";  // 拼出"简单模式"等
文字 
        TextCenter(fontUI, name.c_str(), WIN_W / 2.0f, b.y + 10, 26, on ? WHITE : 
kText);  // 难度名 
        TextCenter(fontUI, kDiff[i].desc, WIN_W / 2.0f, b.y + 44, 17, on ? kText : 
kTextDim);  // 说明 
    } 
    TextCenter(fontUI, "↑↓ 选择，回车确认，Esc 返回", WIN_W / 2.0f, 600, 18, kTextDim);  
// 底部提示 
} 
  
// 绘制排行榜 
static void drawRank() 
{ 
    TextCenter(fontBig, "排行榜", WIN_W / 2.0f, 65, 46, kGold);  // 标题 
    for (int i = 0; i < RANK_SIZE; ++i) 
    { 
        float y = 155 + i * 80.0f;    // 每条记录的纵向位置，间距 80 
        if (scores[i] > 0) 
        { 
            // 有成绩：拼出"第 X 名    YYY 分" 
            std::string s = "第 " + std::to_string(i + 1) + " 名    " + 
                            std::to_string(scores[i]) + " 分"; 
            TextCenter(fontUI, s.c_str(), WIN_W / 2.0f, y, 28, i == 0 ? kGold : kText);  
// 第1 名用金色 
        } 
        else 
        { 
            // 无成绩：显示"第 X 名    ---"，用暗灰色 
            std::string s = "第 " + std::to_string(i + 1) + " 名    ---"; 
            TextCenter(fontUI, s.c_str(), WIN_W / 2.0f, y, 28, kTextDim); 
        } 
    } 
    TextCenter(fontUI, "按任意键或点击鼠标返回", WIN_W / 2.0f, 600, 18, kTextDim);  // 
底部返回提示 
} 
  
// 绘制暂停遮罩：半透明黑层 + 居中提示文字 
static void drawPause() 
{ 
    DrawRectangle(0, 0, WIN_W, WIN_H, Color{0, 0, 0, 160});  // 全屏半透明黑色
（alpha=160）

--- Page 21/22 ---
20 
    TextCenter(fontBig, "游戏暂停", WIN_W / 2.0f, 250, 52, kText); 
    TextCenter(fontUI, "按 P 继续", WIN_W / 2.0f, 330, 22, kText); 
    TextCenter(fontUI, "Esc 返回主菜单", WIN_W / 2.0f, 375, 20, kTextDim); 
} 
  
// 绘制游戏结束遮罩 
static void drawGameOverScreen() 
{ 
    DrawRectangle(0, 0, WIN_W, WIN_H, Color{0, 0, 0, 170});  // 半透明黑色遮罩 
    TextCenter(fontBig, "游戏结束", WIN_W / 2.0f, 170, 52, Color{235, 95, 95, 255});  
// 红色标题 
    std::string s = "本局得分：" + std::to_string(score);  // 拼出本局得分 
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
    // ---- 初始化窗口与画面 ---- 
    InitWindow(WIN_W, WIN_H, "俄罗斯方块");  // 创建 800×650 的窗口，标题"俄罗斯方块" 
    SetTargetFPS(60);                        // 目标帧率 60 FPS（游戏运行流畅度） 
  
    InitPieces();                            // 初始化 7 种形状的全部旋转矩阵 
  
    // ---- 加载中文字体 ---- 
    // 注意：Raylib 对微软雅黑 msyh.ttc（TTC 集合）支持有问题，这里改用黑体 simhei.ttf 
    std::vector<int> cp = CollectCodePoints();                       // 收集界面所需全
部码点 
    const char* fontPath = "C:\\Windows\\Fonts\\simhei.ttf";        // Windows 自带
黑体路径 
    fontUI = LoadFontEx(fontPath, 26, cp.data(), (int)cp.size());   // 加载 26 号普
通字体 
    fontBig = LoadFontEx(fontPath, 56, cp.data(), (int)cp.size());  // 加载 56 号大
字体 
    if (fontUI.texture.id == 0 || fontBig.texture.id == 0) 
    { 
        // 字体加载失败（纹理 id 为 0）：退回 raylib 默认字体，避免程序崩溃 
        fontUI = GetFontDefault(); 
        fontBig = GetFontDefault();