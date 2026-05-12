#pragma once
// ================================================================
//  draw.h — 所有繪圖函式宣告
// ================================================================
#include "game.h"
#include "logic.h"   // 需要 SKILL_CD/SKILL_NAME 和 CalcPCTLayer

// ── 繪圖輔助 ─────────────────────────────────────────────────────
void DrawRoundBox(float x, float y, float w, float h, float r,
                  Color fill, Color border, float bw = 1.5f);
void DrawHex(Vector2 c, float r, Color fill, Color border);

struct RouteVisualTheme {
    Color       accent;
    Color       glow;
    Color       fill;
    Color       fillSoft;
    Color       text;
    const char* sideLabel;
    const char* shortLabel;
};

Color            LerpColor(Color a, Color b, float t);
RouteVisualTheme GetRouteTheme(const PathPreset& preset, int laneSlot = 0);

// ── 世界層 ───────────────────────────────────────────────────────
void DrawStars(Game& G);
void DrawPath(Game& G);
void DrawThreatMap(Game& G);
void DrawAIHints(Game& G);
void DrawConnections(Game& G);
void DrawPulses(Game& G);
void DrawTower(Game& G, Tower& t, bool sel);
void DrawGhostTower(Game& G, int gx, int gy);
void DrawEnemies(Game& G);
void DrawBullets(Game& G);
void DrawParticles(Game& G);
void DrawFloats(Game& G);

// ── UI 層 ────────────────────────────────────────────────────────
void DrawLeftPanel(Game& G);
void DrawRightPanel(Game& G);
void DrawTopBar(Game& G);
void DrawBotBar(Game& G);

// ── 畫面層 ───────────────────────────────────────────────────────
void DrawMenu(Game& G);
void DrawTutorialSelect(Game& G);
void DrawTutorialOverlay(Game& G);
void DrawHelp();
void DrawGameOver(Game& G);
