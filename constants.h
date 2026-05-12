#pragma once
// ================================================================
//  constants.h — 佈局常數、列舉、顏色主題、TowerDef 資料
// ================================================================
#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ── 虛擬解析度與地圖佈局常數 ────────────────────────────────────
constexpr int VIRT_W   = 1920;
constexpr int VIRT_H   = 1080;
constexpr int PANEL_L  = 330;
constexpr int PANEL_R  = 400;
constexpr int TOPBAR_H = 70;
constexpr int BOTBAR_H = 52;
constexpr int MAP_W    = VIRT_W - PANEL_L - PANEL_R;
constexpr int MAP_H    = VIRT_H - TOPBAR_H - BOTBAR_H;
constexpr int CELL     = 52;
constexpr int COLS     = MAP_W / CELL;
constexpr int ROWS     = MAP_H / CELL;
constexpr int CPU_GX   = 11;
constexpr int CPU_GY   = 8;
constexpr int MAX_LANES = 6;

// ── 左側 UI 控制項幾何（繪製與點擊判定共用）──────────────────────
constexpr int LEFT_CTRL_X       = 10;
constexpr int LEFT_CTRL_W       = PANEL_L - 20;
constexpr int LEFT_TOWER_BTN_H  = 58;
constexpr int LEFT_TOWER_BTN_GAP = 7;
constexpr int LEFT_WAVE_BTN_H   = 54;
constexpr int TRAIN_CARD_H      = 72;
constexpr int TRAIN_CARD_GAP    = 14;

// ── 列舉型別 ─────────────────────────────────────────────────────
enum class TType { SENSOR, PERCEPTRON, AND, OR, XOR, CANNON, CPU, NAND, NONE };
enum class EType { BASIC, FAST, ARMORED, ELITE, BOSS };
enum class BossState { CHARGE, EVADE, RAMPAGE };

// ── 顏色主題（內部連結，各 TU 各有一份，無問題）────────────────
static const Color BG          = {  3,   7,  14, 255};
static const Color COL_PATH    = { 10,  18,  30, 255};
static const Color COL_SENSOR  = { 34, 211, 238, 255};
static const Color COL_PERC    = { 74, 222, 128, 255};
static const Color COL_AND     = {250, 204,  21, 255};
static const Color COL_OR      = {251, 146,  60, 255};
static const Color COL_XOR     = {232, 121, 249, 255};
static const Color COL_CANNON  = {248, 113, 113, 255};
static const Color COL_CPU     = {100, 220, 255, 255};
static const Color COL_VIRUS   = {255,  51, 102, 255};
static const Color COL_BOSS    = {180,  50, 255, 255};
static const Color COL_ARMORED = {140, 190, 255, 255};
static const Color COL_FAST    = {255, 200,  50, 255};
static const Color COL_ELITE   = {255, 100, 200, 255};
static const Color COL_STAR    = {255, 220,  70, 255};
static const Color COL_AI      = {  0, 255, 200, 255};
static const Color COL_THREAT  = {255,  80,  30, 255};
static const Color COL_NAND    = {180, 255,  80, 255};
static const Color PANEL_BG    = {  4,   9,  18, 255};
static const Color PANEL_BD    = {  0, 170,  85,  45};

// ── 小工具函式 ───────────────────────────────────────────────────
inline Color AlphaOf(Color c, int a) {
    c.a = (unsigned char)std::max(0, std::min(255, a));
    return c;
}
inline float Sigmoid(float x)        { return 1.f / (1.f + expf(-x)); }
inline float SigmoidDeriv(float y)   { return y * (1.f - y); }
inline float Dist(Vector2 a, Vector2 b) { return Vector2Distance(a, b); }

// ── TowerDef 資料結構 ─────────────────────────────────────────────
struct TierInfo {
    int         cost;
    const char* name;
    const char* ability;
};

struct TowerDef {
    const char* label;
    const char* sym;
    Color       col;
    int         baseCost;
    const char* desc;
    TierInfo    tiers[3];
};

// 定義在 tower_data.cpp，此處宣告
extern TowerDef TDEFS[8];

inline TowerDef& TDef(TType t) { return TDEFS[(int)t]; }
