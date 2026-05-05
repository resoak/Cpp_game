// ================================================================
//  path.cpp — 路徑預設資料與建置函式
// ================================================================
#include "path.h"
#include <cstring>
#include <initializer_list>

static PathPreset MakePreset(int count,
                             std::initializer_list<int> xs,
                             std::initializer_list<int> ys,
                             const char* name,
                             PathEntrySide entrySide,
                             int family,
                             int dualWeight) {
    PathPreset preset{};
    preset.count = count;
    int i = 0;
    for (int x : xs) preset.wx[i++] = x;
    i = 0;
    for (int y : ys) preset.wy[i++] = y;
    preset.name       = name;
    preset.entrySide  = entrySide;
    preset.family     = family;
    preset.dualWeight = dualWeight;
    return preset;
}

// ══════════════════════════════════════════════════════════════════
//  作者預設路線
// ══════════════════════════════════════════════════════════════════
const PathPreset PATH_PRESETS[] = {
    MakePreset(13, {-1,3,3,9,9,5,5,13,13,17,17,11,11}, {8,8,2,2,5,5,12,12,5,5,14,14,8}, "S 型迂迴", PathEntrySide::LEFT,   0, 4),
    MakePreset(11, {-1,2,2,6,6,12,12,18,18,11,11},     {8,8,1,1,9,9,2,2,8,8,8},          "上方長驅", PathEntrySide::LEFT,   1, 3),
    MakePreset(11, {-1,4,4,8,8,14,14,19,19,11,11},     {8,8,14,14,6,6,15,15,8,8,8},      "下方包抄", PathEntrySide::LEFT,   2, 3),
    MakePreset(5,  {-1,5,5,11,11},                      {8,8,4,4,8},                      "中央突破", PathEntrySide::LEFT,   3, 1),
    MakePreset(15, {-1,2,2,8,8,4,4,10,10,16,16,13,13,11,11}, {8,8,3,3,10,10,5,5,14,14,4,4,8,8,8}, "Z 型纏繞", PathEntrySide::LEFT, 4, 4),
    MakePreset(9,  {11,11,6,6,15,15,10,10,11},         {-1,3,3,9,9,13,13,8,8},           "天井折返", PathEntrySide::TOP,    5, 4),
    MakePreset(9,  {18,18,9,9,14,14,7,7,11},           {-1,2,2,11,11,5,5,8,8},           "北側橫切", PathEntrySide::TOP,    6, 3),
    MakePreset(9,  {24,20,20,14,14,18,18,11,11},       {8,8,3,3,12,12,15,15,8},          "東線鉤擊", PathEntrySide::RIGHT,  7, 4),
    MakePreset(9,  {24,19,19,13,13,9,9,12,11},         {5,5,14,14,6,6,11,11,8},          "右側夾攻", PathEntrySide::RIGHT,  8, 3),
    MakePreset(9,  {11,11,4,4,17,17,12,12,11},         {20,15,15,9,9,4,4,8,8},           "南線迴轉", PathEntrySide::BOTTOM, 9, 4),
    MakePreset(9,  {18,18,8,8,14,14,10,10,11},         {20,14,14,5,5,11,11,8,8},         "底部穿插", PathEntrySide::BOTTOM, 10, 3),
};

const int PATH_PRESET_COUNT = (int)(sizeof(PATH_PRESETS) / sizeof(PATH_PRESETS[0]));

// ── 全域路徑狀態 ─────────────────────────────────────────────────
std::array<int, MAX_LANES> CURRENT_PATH_IDX = {{0, 1, 2, 3, 4, 5}};
std::array<const PathPreset*, MAX_LANES> CUR_PRESET = {{
    &PATH_PRESETS[0], &PATH_PRESETS[1], &PATH_PRESETS[2],
    &PATH_PRESETS[3], &PATH_PRESETS[4], &PATH_PRESETS[5]
}};
std::array<std::vector<PathCell>, MAX_LANES> PATH_CELLS;
bool IS_PATH[MAX_LANES][24][20] = {};

static int NormalizeLaneSlot(int laneSlot) {
    if (laneSlot < 0) return 0;
    if (laneSlot >= MAX_LANES) return MAX_LANES - 1;
    return laneSlot;
}

static void BuildLaneCells(const PathPreset& preset,
                           std::vector<PathCell>& cells,
                           bool mask[24][20]) {
    cells.clear();
    memset(mask, 0, sizeof(bool) * 24 * 20);

    for (int wi = 0; wi + 1 < preset.count; wi++) {
        int x0 = preset.wx[wi],   y0 = preset.wy[wi];
        int x1 = preset.wx[wi+1], y1 = preset.wy[wi+1];
        int dx = (x1 > x0) ? 1 : (x1 < x0) ? -1 : 0;
        int dy = (y1 > y0) ? 1 : (y1 < y0) ? -1 : 0;
        int cx = x0, cy = y0;

        while (cx != x1 || cy != y1) {
            if (cx >= 0 && cx < COLS && cy >= 0 && cy < ROWS) {
                bool notDup = cells.empty() ||
                              cells.back().gx != cx ||
                              cells.back().gy != cy;
                if (notDup) {
                    mask[cx][cy] = true;
                    cells.push_back({cx, cy});
                }
            }
            cx += dx;
            cy += dy;
        }
    }

    mask[CPU_GX][CPU_GY] = true;
}

int NormalizePathPresetIdx(int presetIdx) {
    if (PATH_PRESET_COUNT <= 0) return 0;
    int idx = presetIdx % PATH_PRESET_COUNT;
    return (idx < 0) ? (idx + PATH_PRESET_COUNT) : idx;
}

const PathPreset& GetPathPreset(int presetIdx) {
    return PATH_PRESETS[(size_t)NormalizePathPresetIdx(presetIdx)];
}

int GetActiveLanePresetIdx(int laneSlot) {
    return CURRENT_PATH_IDX[NormalizeLaneSlot(laneSlot)];
}

const PathPreset& GetActiveLanePreset(int laneSlot) {
    return *CUR_PRESET[NormalizeLaneSlot(laneSlot)];
}

const std::vector<PathCell>& GetActiveLaneCells(int laneSlot) {
    return PATH_CELLS[NormalizeLaneSlot(laneSlot)];
}

bool IsActiveLaneCell(int laneSlot, int gx, int gy) {
    if (gx < 0 || gx >= COLS || gy < 0 || gy >= ROWS) return false;
    return IS_PATH[NormalizeLaneSlot(laneSlot)][gx][gy];
}

bool IsAnyActivePathCell(int gx, int gy, int activeLaneCount) {
    if (gx < 0 || gx >= COLS || gy < 0 || gy >= ROWS) return false;
    int laneCount = std::max(1, std::min(MAX_LANES, activeLaneCount));
    for (int lane = 0; lane < laneCount; lane++) {
        if (IsActiveLaneCell(lane, gx, gy)) return true;
    }
    return false;
}

std::vector<PathCell> BuildPresetPathCells(int presetIdx) {
    std::vector<PathCell> cells;
    bool mask[24][20] = {};
    BuildLaneCells(GetPathPreset(presetIdx), cells, mask);
    return cells;
}

void SetActiveLanePreset(int laneSlot, int presetIdx) {
    int lane = NormalizeLaneSlot(laneSlot);
    int normalized = NormalizePathPresetIdx(presetIdx);
    const PathPreset& preset = GetPathPreset(normalized);

    CURRENT_PATH_IDX[lane] = normalized;
    CUR_PRESET[lane] = &preset;
    BuildLaneCells(preset, PATH_CELLS[lane], IS_PATH[lane]);
}

// ══════════════════════════════════════════════════════════════════
//  BuildPath  —  建置主路徑
// ══════════════════════════════════════════════════════════════════
void BuildPath(int presetIdx) {
    SetActiveLanePreset(0, presetIdx);
}

// ══════════════════════════════════════════════════════════════════
//  BuildDualPath  —  建置副路徑
// ══════════════════════════════════════════════════════════════════
void BuildDualPath(int presetIdx) {
    SetActiveLanePreset(1, presetIdx);
}
