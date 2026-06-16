#include "raylib.h"
#include "constants.h"
#include "globals.h"
#include "font.h"
#include "path.h"
#include "game.h"
#include "logic.h"
#include "draw.h"
#include "input.h"

int main() {
    const int WIN_W = 1280, WIN_H = 720;
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(WIN_W, WIN_H, "邏輯閘防禦戰 v3.0 + AI Edition");
    SetExitKey(KEY_NULL);

    int mon = GetCurrentMonitor();
    int mw  = GetMonitorWidth(mon), mh = GetMonitorHeight(mon);
    SetWindowPosition((mw - WIN_W) / 2, (mh - WIN_H) / 2);

    LoadChineseFont();
    BuildPath(0);
#if !defined(NDEBUG)
    if (!ValidatePathSafety()) {
        TraceLog(LOG_WARNING, "Path safety validation failed");
    }
#endif

    bool borderlessFS = false;
    int  savedW = WIN_W, savedH = WIN_H;
    int  savedX = (mw - WIN_W) / 2, savedY = (mh - WIN_H) / 2;

    RenderTexture2D target = LoadRenderTexture(VIRT_W, VIRT_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    SetTargetFPS(60);

    Game G;
    G.InitStars();
    G.highScoreMgr.LoadFromFile();
    G.phase = Game::MENU;

    while (!WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), .05f);

        // F11 切換無邊框全螢幕
        if (IsKeyPressed(KEY_F11)) {
            if (!borderlessFS) {
                savedW = GetScreenWidth();  savedH = GetScreenHeight();
                savedX = (int)GetWindowPosition().x;
                savedY = (int)GetWindowPosition().y;
                mon = GetCurrentMonitor();
                mw  = GetMonitorWidth(mon); mh = GetMonitorHeight(mon);
                SetWindowState(FLAG_WINDOW_UNDECORATED);
                SetWindowSize(mw, mh);
                SetWindowPosition(0, 0);
                borderlessFS = true;
            } else {
                ClearWindowState(FLAG_WINDOW_UNDECORATED);
                SetWindowSize(savedW, savedH);
                SetWindowPosition(savedX, savedY);
                borderlessFS = false;
            }
        }

        // 縮放與偏移
        int wW = GetScreenWidth(), wH = GetScreenHeight();
        float sx = (float)wW / VIRT_W, sy = (float)wH / VIRT_H;
        gScale = (sx < sy) ? sx : sy;
        gOffX  = (wW - VIRT_W * gScale) * .5f;
        gOffY  = (wH - VIRT_H * gScale) * .5f;

        G.HandleInput();
        G.Update(dt);

        Vector2 vmp = VirtualMouse();
        int hgx = -1, hgy = -1;
        ScreenToGrid(G, vmp, hgx, hgy);

        // 畫面震動
        float shakeX = 0, shakeY = 0;
        if (G.shakeT > 0) {
            float sp = G.shakePow * (G.shakeT * 3.f);
            std::uniform_real_distribution<float> sd(-sp, sp);
            shakeX = sd(G.rng);
            shakeY = sd(G.rng);
        }

        // 繪製到虛擬解析度
        BeginTextureMode(target);
        ClearBackground(BG);

        if (G.phase == Game::MENU) {
            DrawMenu(G);
            if (G.showHelp) DrawHelp();
        } else if (G.phase == Game::TUTORIAL_SELECT) {
            DrawTutorialSelect(G);
            if (G.showHelp) DrawHelp();
        } else {
            DrawStars(G);
            gShkX = shakeX; gShkY = shakeY;

            BeginScissorMode(PANEL_L, TOPBAR_H, MAP_W, MAP_H);
            DrawPath(G);
            DrawThreatMap(G);
            DrawConnections(G);
            DrawPulses(G);
            for (auto& t : G.towers) DrawTower(G, t, t.id == G.selectedId);
            DrawAIHints(G);
            DrawGhostTower(G, hgx, hgy);
            DrawEnemies(G);
            DrawBullets(G);
            DrawParticles(G);
            DrawFloats(G);
            EndScissorMode();

            gShkX = 0; gShkY = 0;
            DrawLeftPanel(G);
            DrawRightPanel(G);
            DrawTopBar(G);
            DrawBotBar(G);

            if (G.tutorial.active) DrawTutorialOverlay(G);
            if (G.phase == Game::GAMEOVER) DrawGameOver(G);
            if (G.showHelp) DrawHelp();
        }

        EndTextureMode();

        // 縮放輸出到真實視窗
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(
            target.texture,
            { 0.f, 0.f, (float)VIRT_W, -(float)VIRT_H },
            { gOffX, gOffY, VIRT_W * gScale, VIRT_H * gScale },
            { 0.f, 0.f }, 0.f, WHITE
        );
        EndDrawing();
    }

    UnloadRenderTexture(target);
    if (gFontLoaded) UnloadFont(gFont);
    CloseWindow();
    return 0;
}
