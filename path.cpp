// ================================================================
//  path.cpp — 路徑預設資料與建置函式
// ================================================================
#include "path.h"
#include <cstring>

// ══════════════════════════════════════════════════════════════════
//  5 條預設路線
// ══════════════════════════════════════════════════════════════════
const PathPreset PATH_PRESETS[5] = {
    { 13, {-1,3,3,9,9,5,5,13,13,17,17,11,11},
           { 8,8,2,2,5,5,12,12, 5, 5,14,14, 8}, "S 型迂迴"  },
    { 11, {-1,2,2,6,6,12,12,18,18,11,11},
           { 8,8,1,1,9, 9, 2, 2, 8, 8, 8},      "上方長驅"  },
    { 11, {-1,4,4,8,8,14,14,19,19,11,11},
           { 8,8,14,14,6,6,15,15, 8, 8, 8},      "下方包抄"  },
    {  5, {-1,5,5,11,11},
           { 8,8,4, 4, 8},                        "中央突破"  },
    { 15, {-1,2,2,8,8,4,4,10,10,16,16,13,13,11,11},
           { 8,8,3,3,10,10,5,5,14,14, 4, 4, 8, 8, 8}, "Z 型纏繞"},
};

// ── 全域路徑狀態 ─────────────────────────────────────────────────
int                   CURRENT_PATH_IDX  = 0;
int                   CURRENT_PATH_IDX2 = 1;
const PathPreset*     CUR_PRESET  = &PATH_PRESETS[0];
const PathPreset*     CUR_PRESET2 = &PATH_PRESETS[1];
std::vector<PathCell> PATH_CELLS;
std::vector<PathCell> PATH_CELLS2;
bool IS_PATH [24][20] = {};
bool IS_PATH2[24][20] = {};

// ══════════════════════════════════════════════════════════════════
//  BuildPath  —  建置主路徑
// ══════════════════════════════════════════════════════════════════
void BuildPath(int presetIdx) {
    CURRENT_PATH_IDX = presetIdx % 5;
    CUR_PRESET = &PATH_PRESETS[CURRENT_PATH_IDX];

    PATH_CELLS.clear();
    memset(IS_PATH, 0, sizeof(IS_PATH));

    for (int wi = 0; wi + 1 < CUR_PRESET->count; wi++) {
        int x0 = CUR_PRESET->wx[wi],   y0 = CUR_PRESET->wy[wi];
        int x1 = CUR_PRESET->wx[wi+1], y1 = CUR_PRESET->wy[wi+1];
        int dx = (x1 > x0) ? 1 : (x1 < x0) ? -1 : 0;
        int dy = (y1 > y0) ? 1 : (y1 < y0) ? -1 : 0;
        int cx = x0, cy = y0;

        while (cx != x1 || cy != y1) {
            if (cx >= 0 && cx < COLS && cy >= 0 && cy < ROWS) {
                bool notDup = PATH_CELLS.empty() ||
                              PATH_CELLS.back().gx != cx ||
                              PATH_CELLS.back().gy != cy;
                if (notDup) {
                    IS_PATH[cx][cy] = true;
                    PATH_CELLS.push_back({cx, cy});
                }
            }
            cx += dx;
            cy += dy;
        }
    }

    IS_PATH[CPU_GX][CPU_GY] = true;
}

// ══════════════════════════════════════════════════════════════════
//  BuildDualPath  —  建置副路徑
// ══════════════════════════════════════════════════════════════════
void BuildDualPath(int presetIdx) {
    CURRENT_PATH_IDX2 = presetIdx % 5;
    CUR_PRESET2 = &PATH_PRESETS[CURRENT_PATH_IDX2];

    PATH_CELLS2.clear();
    memset(IS_PATH2, 0, sizeof(IS_PATH2));

    for (int wi = 0; wi + 1 < CUR_PRESET2->count; wi++) {
        int x0 = CUR_PRESET2->wx[wi],   y0 = CUR_PRESET2->wy[wi];
        int x1 = CUR_PRESET2->wx[wi+1], y1 = CUR_PRESET2->wy[wi+1];
        int dx = (x1 > x0) ? 1 : (x1 < x0) ? -1 : 0;
        int dy = (y1 > y0) ? 1 : (y1 < y0) ? -1 : 0;
        int cx = x0, cy = y0;

        while (cx != x1 || cy != y1) {
            if (cx >= 0 && cx < COLS && cy >= 0 && cy < ROWS) {
                bool notDup = PATH_CELLS2.empty() ||
                              PATH_CELLS2.back().gx != cx ||
                              PATH_CELLS2.back().gy != cy;
                if (notDup) {
                    IS_PATH2[cx][cy] = true;
                    PATH_CELLS2.push_back({cx, cy});
                }
            }
            cx += dx;
            cy += dy;
        }
    }

    IS_PATH2[CPU_GX][CPU_GY] = true;
}
