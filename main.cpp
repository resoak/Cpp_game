// ================================================================
//  Logic Gate Defense  v3.0 + AI Edition
// ================================================================

#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

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

// ── 螢幕縮放與震動全域變數 ──────────────────────────────────────
static float gScale = 1.f;
static float gOffX  = 0.f;
static float gOffY  = 0.f;
static float gShkX  = 0.f;
static float gShkY  = 0.f;

// 將實體滑鼠座標換算回虛擬座標
inline Vector2 VirtualMouse() {
    Vector2 m = GetMousePosition();
    return { (m.x - gOffX) / gScale, (m.y - gOffY) / gScale };
}

// ── 中文字型載入 ─────────────────────────────────────────────────
static Font gFont;
static bool gFontLoaded = false;

static void LoadChineseFont() {
    // 建立字碼點清單（ASCII + 中文 + 標點 + 符號）
    static std::vector<int> cp;
    cp.clear();
    for (int i = 32;     i < 127;     i++) cp.push_back(i);
    for (int i = 0x4E00; i <= 0x9FFF; i++) cp.push_back(i);
    for (int i = 0x3000; i <= 0x303F; i++) cp.push_back(i);
    for (int i = 0xFF01; i <= 0xFF60; i++) cp.push_back(i);
    for (int i = 0x2000; i <= 0x206F; i++) cp.push_back(i);
    for (int i = 0x2190; i <= 0x21FF; i++) cp.push_back(i);
    for (int i = 0x25A0; i <= 0x25FF; i++) cp.push_back(i);

    // 按優先順序嘗試各作業系統的字型路徑
    const char* paths[] = {
        "font.ttf", "font.ttc", "font.otf",
        "C:/Windows/Fonts/msjh.ttc",
        "C:/Windows/Fonts/kaiu.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        nullptr
    };

    for (int i = 0; paths[i]; i++) {
        FILE* f = fopen(paths[i], "rb");
        if (!f) continue;
        fclose(f);

        gFont = LoadFontEx(paths[i], 60, cp.data(), (int)cp.size());
        if (gFont.texture.id != 0) {
            gFontLoaded = true;
            SetTextureFilter(gFont.texture, TEXTURE_FILTER_BILINEAR);
            break;
        }
    }

    if (!gFontLoaded) {
        gFont = GetFontDefault();
    }
}

// ── 字型大小常數 ─────────────────────────────────────────────────
constexpr float FS_TINY  = 20.f;
constexpr float FS_SMALL = 24.f;
constexpr float FS_MED   = 30.f;
constexpr float FS_LARGE = 36.f;
constexpr float FS_TITLE = 44.f;
constexpr float FS_BIG   = 60.f;

// ── 文字繪製工具函式 ─────────────────────────────────────────────
// MCN  : 量測文字寬度
// DTX  : 左上角對齊繪製
// DTC  : 水平置中繪製
inline float MCN(const char* t, float sz) {
    return MeasureTextEx(gFont, t, sz, 0).x;
}
inline void DTX(const char* t, float x, float y, float sz, Color c) {
    DrawTextEx(gFont, t, {x, y}, sz, 0, c);
}
inline void DTC(const char* t, int x, int y, float sz, Color c) {
    DTX(t, (float)x - MCN(t, sz) * 0.5f, (float)y - sz * 0.5f, sz, c);
}

// ── 顏色主題 ─────────────────────────────────────────────────────
const Color BG          = {  3,   7,  14, 255};
const Color COL_PATH    = { 10,  18,  30, 255};
const Color COL_SENSOR  = { 34, 211, 238, 255};
const Color COL_PERC    = { 74, 222, 128, 255};
const Color COL_AND     = {250, 204,  21, 255};
const Color COL_OR      = {251, 146,  60, 255};
const Color COL_XOR     = {232, 121, 249, 255};
const Color COL_CANNON  = {248, 113, 113, 255};
const Color COL_CPU     = {100, 220, 255, 255};
const Color COL_VIRUS   = {255,  51, 102, 255};
const Color COL_BOSS    = {180,  50, 255, 255};
const Color COL_ARMORED = {140, 190, 255, 255};
const Color COL_FAST    = {255, 200,  50, 255};
const Color COL_ELITE   = {255, 100, 200, 255};
const Color COL_STAR    = {255, 220,  70, 255};
const Color COL_AI      = {  0, 255, 200, 255};
const Color COL_THREAT  = {255,  80,  30, 255};
const Color COL_NAND    = {180, 255,  80, 255};
const Color PANEL_BG    = {  4,   9,  18, 255};
const Color PANEL_BD    = {  0, 170,  85,  45};

// ── 小工具函式 ───────────────────────────────────────────────────
inline Color AlphaOf(Color c, int a) {
    c.a = (unsigned char)std::max(0, std::min(255, a));
    return c;
}
inline float Sigmoid(float x) {
    return 1.f / (1.f + expf(-x));
}
inline float SigmoidDeriv(float y) {
    return y * (1.f - y);
}
inline float Dist(Vector2 a, Vector2 b) {
    return Vector2Distance(a, b);
}

// ── 列舉型別 ─────────────────────────────────────────────────────
enum class TType { SENSOR, PERCEPTRON, AND, OR, XOR, CANNON, CPU, NAND, NONE };
enum class EType { BASIC, FAST, ARMORED, ELITE, BOSS };

enum class BossState {
    CHARGE,   // 普通前進
    EVADE,    // 偵測威脅後加速閃避
    RAMPAGE   // HP < 30% 永久暴走
};

// ══════════════════════════════════════════════════════════════════
//  感知器學習器（Delta Rule）
// ══════════════════════════════════════════════════════════════════
struct PerceptronLearner {
    float accInput1{0.f};
    float accInput2{0.f};
    float accOutput{0.f};
    int   samples{0};
    float lastLoss{0.f};
    float lrDecay{1.f};     // 學習率衰減係數

    // 累積一個樣本（每幀由 SampleSignal 呼叫）
    void Accumulate(float in1, float in2, float out) {
        accInput1 += in1;
        accInput2 += in2;
        accOutput += out;
        samples++;
    }

    // 清空累積緩衝
    void Reset() {
        accInput1 = accInput2 = accOutput = 0.f;
        samples = 0;
    }

    // 波次結束後執行一步梯度更新
    void Update(float target, float& w1, float& w2, float& bias, float baseLR = 0.15f) {
        if (samples == 0) return;

        float i1  = accInput1 / samples;
        float i2  = accInput2 / samples;
        float out = accOutput / samples;
        float lr  = baseLR * lrDecay;

        float error = target - out;
        float delta = error * SigmoidDeriv(out);

        w1   += lr * delta * i1;
        w2   += lr * delta * i2;
        bias += lr * delta;

        // 防止權重爆炸，限制在 [-2, 2]
        w1   = std::max(-2.f, std::min(2.f, w1));
        w2   = std::max(-2.f, std::min(2.f, w2));
        bias = std::max(-2.f, std::min(2.f, bias));

        lastLoss  = error * error;
        lrDecay  *= 0.92f;
        Reset();
    }
};

// ══════════════════════════════════════════════════════════════════
//  威脅熱圖（記錄各格擊殺密度）
// ══════════════════════════════════════════════════════════════════
struct ThreatMap {
    float cell[24][20]{};

    void AddKill(int gx, int gy, float value = 1.f) {
        if (gx >= 0 && gx < 24 && gy >= 0 && gy < 20) {
            cell[gx][gy] += value;
        }
    }

    float Get(int gx, int gy) const {
        if (gx >= 0 && gx < 24 && gy >= 0 && gy < 20) {
            return cell[gx][gy];
        }
        return 0.f;
    }

    float GetMax() const {
        float mx = 0.f;
        for (int x = 0; x < 24; x++) {
            for (int y = 0; y < 20; y++) {
                mx = std::max(mx, cell[x][y]);
            }
        }
        return mx;
    }

    // 每波結束後衰減，讓舊資料逐漸消失
    void Decay(float factor = 0.85f) {
        for (int x = 0; x < 24; x++) {
            for (int y = 0; y < 20; y++) {
                cell[x][y] *= factor;
            }
        }
    }
};

// AI 顧問提示（顯示在地圖上的建議格子）
struct AIHint {
    int         gx, gy;
    TType       suggest;
    float       score;
    std::string reason;
    float       flashT{0.f};
};

// ── 塔定義資料 ───────────────────────────────────────────────────
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

static TowerDef TDEFS[] = {
    {
        "感測器", "SEN", Color{34, 211, 238, 255}, 40,
        "偵測病毒距離→輸出0~1",
        {
            {  0, "基礎偵測", "範圍4.5格"},
            { 80, "強化偵測", "範圍6格+偵測快速"},
            {160, "全域偵測", "範圍8格+隱形"}
        }
    },
    {
        "感知器", "PCT", Color{74, 222, 128, 255}, 90,
        "加權求和+Sigmoid（AI可學習w1,w2,bias）",
        {
            {  0, "單層神經元", "0~1連續輸出"},
            {100, "雙層神經元", "訊號強度x1.4"},
            {200, "自適應網路", "訊號強度x2.0"}
        }
    },
    {
        "AND閘", "AND", Color{250, 204, 21, 255}, 60,
        "全輸入>0.5才輸出1(對裝甲+50%)",
        {
            {  0, "基礎AND", "裝甲傷害+50%"},
            { 80, "增強AND", "傷害乘數x1.5"},
            {160, "連鎖AND", "觸發鄰近AND"}
        }
    },
    {
        "OR閘", "OR", Color{251, 146, 60, 255}, 60,
        "任一>0.5輸出1(對雜兵最佳)",
        {
            {  0, "基礎OR", "廣域觸發"},
            { 80, "快速OR", "砲塔冷卻-30%"},
            {160, "脈衝OR", "爆發連射x2"}
        }
    },
    {
        "XOR閘", "XOR", Color{232, 121, 249, 255}, 60,
        "恰好一個>0.5(對精英+100%)",
        {
            {  0, "基礎XOR", "精英傷害+100%"},
            { 80, "精準XOR", "20%暴擊率"},
            {160, "標記XOR", "敵人受傷+50%"}
        }
    },
    {
        "砲塔", "GUN", Color{248, 113, 113, 255}, 80,
        "訊號>0.5→自動開火",
        {
            {  0, "基礎砲塔", "36傷 射程7格"},
            {120, "強化砲塔", "60傷 射程9格"},
            {240, "爆破砲塔", "100傷+爆炸"}
        }
    },
    {
        "CPU", "CPU", Color{100, 220, 255, 255}, 0,
        "你的核心，保護它！",
        {
            {0, "", ""},
            {0, "", ""},
            {0, "", ""}
        }
    },
    {
        "NAND", "NND", Color{180, 255, 80, 255}, 70,
        "非AND：只要一個輸入≤0.5就輸出1（後期反制裝甲）",
        {
            {  0, "基礎NAND", "護甲減免-20%"},
            { 90, "強化NAND", "護甲減免-40%"},
            {180, "破甲NAND", "護甲歸零+標記"}
        }
    }
};

inline TowerDef& TDef(TType t) {
    return TDEFS[(int)t];
}

// ══════════════════════════════════════════════════════════════════
//  路徑系統
// ══════════════════════════════════════════════════════════════════
struct PathCell {
    int gx, gy;
};

struct PathPreset {
    int         count;
    int         wx[16];
    int         wy[16];
    const char* name;
};

static const PathPreset PATH_PRESETS[] = {
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

static int                  CURRENT_PATH_IDX  = 0;
static int                  CURRENT_PATH_IDX2 = 1;
static const PathPreset*    CUR_PRESET  = &PATH_PRESETS[0];
static const PathPreset*    CUR_PRESET2 = &PATH_PRESETS[1];
static std::vector<PathCell> PATH_CELLS;
static std::vector<PathCell> PATH_CELLS2;
static bool IS_PATH [24][20] = {};
static bool IS_PATH2[24][20] = {};

// 根據 PathPreset 把格子填入 PATH_CELLS / IS_PATH
static void BuildPath(int presetIdx = 0) {
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

static void BuildDualPath(int presetIdx) {
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

// ══════════════════════════════════════════════════════════════════
//  Tower（防禦塔）
// ══════════════════════════════════════════════════════════════════
struct Tower {
    int   id;
    TType type;
    int   gx, gy;
    int   level{1};

    // 神經元參數（感知器專用）
    float sig{0.f};
    float w1{0.8f};
    float w2{0.6f};
    float bias{-0.5f};

    int   cooldown{0};
    float anim{0.f};
    std::vector<int> conns;     // 連線目標 ID 清單

    float range{7.f};
    float damage{36.f};
    int   fireRateTicks{18};

    int   kills{0};
    float totalDmg{0.f};
    float upgradeFlash{0.f};    // 升級閃光計時（從 1 衰減到 0）
    bool  pulseBurst{false};
    int   burstCount{0};
    float activeCd{0.f};        // 主動技能冷卻倒計（0 = 就緒）

    PerceptronLearner learner;

    // 用於波次結束訓練的取樣緩衝
    float sampledIn1{0.f};
    float sampledIn2{0.f};
    float sampledOut{0.f};
    int   sampleCount{0};

    std::vector<float> lossHistory;  // 最近 14 次 loss，用於迷你折線圖

    // 每幀累積輸入/輸出樣本
    void SampleSignal(float in1, float in2, float out) {
        sampledIn1 += in1;
        sampledIn2 += in2;
        sampledOut += out;
        sampleCount++;
    }

    // 波次結束時將平均樣本送給 learner
    void FlushSample() {
        if (sampleCount > 0 && type == TType::PERCEPTRON) {
            learner.Accumulate(
                sampledIn1 / sampleCount,
                sampledIn2 / sampleCount,
                sampledOut / sampleCount
            );
        }
        sampledIn1 = sampledIn2 = sampledOut = 0.f;
        sampleCount = 0;
    }
};

// ══════════════════════════════════════════════════════════════════
//  Enemy（敵人）
// ══════════════════════════════════════════════════════════════════
struct Enemy {
    int   id;
    EType type;

    float pathPos{0.f};         // 在路徑上的進度（格子索引 + 小數插值）
    float hp;
    float maxHp;
    float armor{0.f};           // 護甲：傷害乘以 (1 - armor)
    float spd;
    int   reward;
    float angle{0.f};           // 六角形自轉角度（視覺用）

    bool  marked{false};        // XOR 標記（受傷加成）
    float markTimer{0.f};
    bool  stealth{false};       // 隱身（需 Lv.2 感測器才能偵測）
    float flashTimer{0.f};      // 受擊白色閃爍計時
    float regenTimer{0.f};      // 再生計時

    float stunTimer{0.f};       // EMP 暈眩倒計
    int   pathIdx{0};           // 0 = 主路徑，1 = 副路徑

    bool  shielded{false};      // 精英護盾（Wave 10+ 出場自帶）
    float shieldHp{0.f};        // 護盾目前剩餘 HP

    bool  hasSplit{false};      // Boss 分裂旗標（50% HP 觸發一次）

    BossState bossState{BossState::CHARGE};
    float     bossStateTimer{0.f};
    float     evadeSpdMult{1.f};
    float     localThreat{0.f};  // 用於 Boss AI 偵測前方威脅

    // Boss 狀態機更新（每幀呼叫）
    void TickBoss(float dt, float threat, float hpRatio) {
        bossStateTimer -= dt;

        // HP < 30%：永久進入 RAMPAGE
        if (hpRatio < 0.30f) {
            if (bossState != BossState::RAMPAGE) {
                bossState      = BossState::RAMPAGE;
                evadeSpdMult   = 2.8f;
                bossStateTimer = 9999.f;
            }
            return;
        }

        if (bossState == BossState::CHARGE) {
            // 前方威脅過高時切換成 EVADE
            if (threat > 2.5f && bossStateTimer <= 0.f) {
                bossState      = BossState::EVADE;
                evadeSpdMult   = 2.0f;
                bossStateTimer = 1.8f;
            } else {
                evadeSpdMult = 1.0f;
            }
        } else if (bossState == BossState::EVADE) {
            // EVADE 時間到，恢復 CHARGE
            if (bossStateTimer <= 0.f) {
                bossState      = BossState::CHARGE;
                evadeSpdMult   = 1.0f;
                bossStateTimer = 2.0f;
            }
        }
    }
};

// ── 其他小型結構 ─────────────────────────────────────────────────
struct Bullet {
    Vector2 pos, vel;
    int     targetId;
    int     sourceId{-1};   // 發射砲塔的 id，用於更新 kills / totalDmg
    float   dmg;
    bool    splash{false};
    float   splashR{0.f};
    Color   col;
};

struct SigPulse  { Vector2 src, dst; float t; Color col; };
struct Particle  { Vector2 pos, vel; float life, maxLife, radius; Color col; };
struct FloatText { Vector2 pos; std::string text; Color col; float life; };
struct Star      { float x, y, r, bright; };

// ══════════════════════════════════════════════════════════════════
//  Game（主遊戲狀態）
// ══════════════════════════════════════════════════════════════════
struct Game {
    // ── 實體容器 ───────────────────────────────────────────────────
    std::vector<Tower>     towers;
    std::vector<Enemy>     enemies;
    std::vector<Bullet>    bullets;
    std::vector<SigPulse>  pulses;
    std::vector<Particle>  particles;
    std::vector<FloatText> floats;
    std::vector<Star>      stars;

    ThreatMap           threatMap;
    std::vector<AIHint> aiHints;

    // ── 波次突發事件 ───────────────────────────────────────────────
    enum class WaveEvent {
        NONE,
        SWARM,       // 蜂群：數量 x2.2，HP 只有 60%
        ARMORED,     // 裝甲洪流：全部為裝甲兵
        STEALTH,     // 霧戰：全部隱身
        ELITE_RUSH,  // 精英突擊：全精英，速度 x1.5
        BOSS_ESCORT, // 護衛 Boss：波次 Boss + 隨扈
        REGEN_ARMY,  // 再生軍：所有敵人帶持續回血
        DOUBLE_SPD,  // 神速衝鋒：速度 x2
        BLACKOUT,    // 通訊中斷：感測器範圍縮減至 30%
        MUTANT,      // 變異體：HP x2、速度 x1.3、無裝甲、帶再生
        SIEGE,       // 圍城艦：第一隻 HP x8、裝甲 0.8 的超重坦
    };

    WaveEvent   currentEvent{WaveEvent::NONE};
    std::string eventName;
    float       eventBannerTimer{0.f};

    // 根據波數抽取本波事件
    static WaveEvent RollEvent(int wave, std::mt19937& rng) {
        if (wave < 3)      return WaveEvent::NONE;
        if (wave % 5 == 0) return WaveEvent::BOSS_ESCORT;

        std::uniform_int_distribution<int> d(0, 10);
        int r = d(rng);
        if (wave < 6) r %= 3;   // 前期只出現溫和事件

        switch (r) {
            case  0: return WaveEvent::NONE;
            case  1: return WaveEvent::SWARM;
            case  2: return WaveEvent::ARMORED;
            case  3: return WaveEvent::STEALTH;
            case  4: return WaveEvent::ELITE_RUSH;
            case  5: return WaveEvent::REGEN_ARMY;
            case  6: return WaveEvent::DOUBLE_SPD;
            case  7: return WaveEvent::BLACKOUT;
            case  8: return WaveEvent::MUTANT;
            case  9: return WaveEvent::SIEGE;
            default: return WaveEvent::NONE;
        }
    }

    static const char* EventName(WaveEvent e) {
        switch (e) {
            case WaveEvent::SWARM:       return "蜂群入侵！";
            case WaveEvent::ARMORED:     return "裝甲洪流！";
            case WaveEvent::STEALTH:     return "霧戰！";
            case WaveEvent::ELITE_RUSH:  return "精英突擊！";
            case WaveEvent::BOSS_ESCORT: return "護衛 Boss！";
            case WaveEvent::REGEN_ARMY:  return "再生軍！";
            case WaveEvent::DOUBLE_SPD:  return "神速衝鋒！";
            case WaveEvent::BLACKOUT:    return "通訊中斷！";
            case WaveEvent::MUTANT:      return "變異體入侵！";
            case WaveEvent::SIEGE:       return "圍城艦來襲！";
            default:                     return "";
        }
    }

    // ── 遊戲狀態旗標 ───────────────────────────────────────────────
    bool  showThreat{false};
    bool  showAIHints{true};
    bool  dualPath{false};       // Wave 8+ 雙路徑開啟
    bool  blackoutActive{false}; // BLACKOUT 事件期間感測器削弱
    int   nextPreviewPath{1};    // 下波路線預覽索引

    // 玩家主動技能 Buff 計時器（倒計到 0 失效）
    float buffOverfreq{0.f};     // OR 技能：射速 x2
    float buffArmorBreak{0.f};   // AND/NAND 技能：護甲歸零
    float buffGlobalMark{0.f};   // XOR 技能：全域標記

    // ── 核心數值 ───────────────────────────────────────────────────
    int   nextId{1};
    int   credits{380};
    int   score{0};
    int   highScore{0};
    int   wave{0};
    int   lives{20};
    float cpuHp{100.f};
    bool  paused{false};
    bool  showHelp{false};

    // ── 遊戲階段 ───────────────────────────────────────────────────
    enum Phase { MENU, BUILD, FIGHT, TRAINING, GAMEOVER } phase{MENU};

    int   waveCount{0};
    int   spawned{0};
    float spawnTimer{0.f};
    float trainingTimer{0.f};
    constexpr static float TRAIN_TIME = 15.f;

    TType placing{TType::NONE};
    int   selectedId{-1};
    int   connectSrc{-1};

    std::string msg;
    float       msgTimer{0.f};

    std::mt19937 rng{std::random_device{}()};

    // 左側面板按鈕位置（由 DrawLeftPanel 計算並儲存）
    int btnY0{0};
    int waveBtnY{0};
    constexpr static int BTN_H   = 66;
    constexpr static int BTN_GAP = 5;

    int   waveKills{0};
    int   waveEscaped{0};
    float waveDmg{0.f};

    int   combo{0};
    float comboTimer{0.f};
    constexpr static float COMBO_WINDOW = 3.f;

    float shakeT{0.f};
    float shakePow{0.f};

    // ── 座標轉換工具 ───────────────────────────────────────────────

    // 地圖左上角世界座標（含震動偏移）
    Vector2 MapOrigin() {
        return { (float)PANEL_L + gShkX, (float)TOPBAR_H + gShkY };
    }

    // 格子中心 → 世界座標
    Vector2 CC(int gx, int gy) {
        auto o = MapOrigin();
        return { o.x + (gx + 0.5f) * CELL, o.y + (gy + 0.5f) * CELL };
    }
    Vector2 CC(float gx, float gy) {
        auto o = MapOrigin();
        return { o.x + (gx + 0.5f) * CELL, o.y + (gy + 0.5f) * CELL };
    }

    // 查找工具（線性搜索，塔不多所以不需要 map）
    Tower* FindTower(int id) {
        for (auto& t : towers) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }
    Tower* TowerAt(int gx, int gy) {
        for (auto& t : towers) {
            if (t.gx == gx && t.gy == gy) return &t;
        }
        return nullptr;
    }
    Enemy* FindEnemy(int id) {
        for (auto& e : enemies) {
            if (e.id == id) return &e;
        }
        return nullptr;
    }

    // 格子是否在任一路徑上
    bool IsPath(int gx, int gy) {
        if (gx < 0 || gx >= COLS || gy < 0 || gy >= ROWS) return false;
        return IS_PATH[gx][gy] || IS_PATH2[gx][gy];
    }

    // 敵人目前的世界座標（路徑插值）
    Vector2 EnemyWorld(const Enemy& e) {
        auto& cells = (e.pathIdx == 1 && dualPath) ? PATH_CELLS2 : PATH_CELLS;
        int   i = (int)e.pathPos;
        float f = e.pathPos - i;

        if (i >= (int)cells.size() - 1) return CC(CPU_GX, CPU_GY);
        if (i < 0)                       return CC(cells[0].gx, cells[0].gy);

        Vector2 a = CC(cells[i].gx,   cells[i].gy);
        Vector2 b = CC(cells[i+1].gx, cells[i+1].gy);
        return { a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f };
    }

    // 敵人目前的格子座標（浮點，路徑插值）
    Vector2 EnemyGrid(const Enemy& e) {
        auto& cells = (e.pathIdx == 1 && dualPath) ? PATH_CELLS2 : PATH_CELLS;
        int   i = (int)e.pathPos;
        float f = e.pathPos - i;

        if (i >= (int)cells.size() - 1) return { (float)CPU_GX, (float)CPU_GY };
        if (i < 0)                       return { (float)cells[0].gx, (float)cells[0].gy };

        float ax = cells[i].gx,   ay = cells[i].gy;
        float bx = cells[i+1].gx, by = cells[i+1].gy;
        return { ax + (bx - ax) * f, ay + (by - ay) * f };
    }

    // ── 塔的升級與屬性 ──────────────────────────────────────────
    void ApplyTowerStats(Tower& t) {
        if (t.type == TType::SENSOR) {
            float ranges[] = { 4.5f, 6.0f, 8.0f };
            t.range = ranges[t.level - 1];
        } else if (t.type == TType::CANNON) {
            float dmgs[]   = { 36.f, 60.f, 100.f };
            float ranges[] = {  7.f,  9.f,  11.f };
            int   frs[]    = { 18,   12,     8   };
            t.damage        = dmgs[t.level - 1];
            t.range         = ranges[t.level - 1];
            t.fireRateTicks = frs[t.level - 1];
        }
    }

    bool UpgradeTower(Tower& t) {
        if (t.type == TType::CPU || t.level >= 3) return false;

        int cost = TDef(t.type).tiers[t.level].cost;
        if (credits < cost) return false;

        credits -= cost;
        t.level++;
        ApplyTowerStats(t);
        t.upgradeFlash = 1.f;

        char buf[64];
        snprintf(buf, 64, "%s 升至 Lv.%d！", TDef(t.type).label, t.level);
        SetMsg(buf);
        SpawnParticles(CC(t.gx, t.gy), TDef(t.type).col, 25, 130.f);
        return true;
    }

    // ── 連線驗證 ────────────────────────────────────────────────
    static constexpr int MAX_CONNS = 8;

    // DFS 檢查加入這條邊是否會形成環
    bool WouldCycle(int src, int dst) {
        std::vector<int> stk = {dst};
        std::vector<int> vis;

        while (!stk.empty()) {
            int c = stk.back();
            stk.pop_back();
            if (c == src) return true;
            if (std::find(vis.begin(), vis.end(), c) != vis.end()) continue;
            vis.push_back(c);
            Tower* t = FindTower(c);
            if (t) {
                for (int x : t->conns) stk.push_back(x);
            }
        }
        return false;
    }

    std::string ValidateConnect(int src, int dst) {
        Tower* s = FindTower(src);
        Tower* d = FindTower(dst);
        if (!s || !d)                        return "元件不存在";
        if (src == dst)                      return "不能連接自己";
        if (s->type == TType::CANNON)        return "砲塔不能輸出";
        if (s->type == TType::CPU)           return "CPU不能連接";
        if (d->type == TType::SENSOR)        return "感測器不能被連入";
        if (d->type == TType::CPU)           return "CPU不能連入";
        if ((int)s->conns.size() >= MAX_CONNS) return "已達上限(8條)";
        if (WouldCycle(src, dst))            return "不能形成迴圈";
        return "";
    }

    // ── 特效工具 ────────────────────────────────────────────────
    void SpawnParticles(Vector2 pos, Color col, int n = 10, float spd = 70.f) {
        std::uniform_real_distribution<float> ang(0.f, 6.28f);
        std::uniform_real_distribution<float> sp(spd * 0.3f, spd);
        for (int i = 0; i < n; i++) {
            float a = ang(rng);
            float s = sp(rng);
            particles.push_back({ pos, {cosf(a) * s, sinf(a) * s}, 0.7f, 0.7f, 5.f, col });
        }
    }

    void AddFloat(Vector2 pos, const std::string& txt, Color col) {
        floats.push_back({ pos, txt, col, 2.f });
    }

    void SetMsg(const std::string& m) {
        msg      = m;
        msgTimer = 5.f;
    }

    void Shake(float p = 8.f, float t = 0.35f) {
        shakeT   = t;
        shakePow = p;
    }

    // ── 背景星星 ────────────────────────────────────────────────
    void InitStars() {
        stars.clear();
        std::uniform_real_distribution<float> rw(0.f, (float)VIRT_W);
        std::uniform_real_distribution<float> rh(0.f, (float)VIRT_H);
        std::uniform_real_distribution<float> rr(0.8f, 2.f);
        std::uniform_real_distribution<float> rb(0.3f, 1.f);
        for (int i = 0; i < 200; i++) {
            stars.push_back({ rw(rng), rh(rng), rr(rng), rb(rng) });
        }
    }

    // ── AI 顧問提示生成 ─────────────────────────────────────────
    void GenerateAIHints() {
        aiHints.clear();

        for (auto& pc : PATH_CELLS) {
            // 找最近的塔距離
            float minDist = 999.f;
            for (auto& t : towers) {
                float d = Dist({(float)t.gx, (float)t.gy}, {(float)pc.gx, (float)pc.gy});
                if (d < minDist) minDist = d;
            }
            if (minDist < 4.5f) continue;  // 已有足夠覆蓋，跳過

            // 根據路徑進度決定建議種類
            int pathIdx = 0;
            for (int pi = 0; pi < (int)PATH_CELLS.size(); pi++) {
                if (PATH_CELLS[pi].gx == pc.gx && PATH_CELLS[pi].gy == pc.gy) {
                    pathIdx = pi;
                    break;
                }
            }
            float progress = (float)pathIdx / PATH_CELLS.size();

            TType       bestSuggest;
            std::string reason;
            float       score = (minDist - 4.5f) / 8.f;

            if (progress < 0.3f) {
                bestSuggest = TType::SENSOR;
                reason      = "前段覆蓋不足，建議感測器";
            } else if (progress < 0.6f) {
                bestSuggest = TType::OR;
                reason      = "中段需要訊號中繼，建議OR閘";
            } else {
                bestSuggest = TType::CANNON;
                reason      = "CPU防線薄弱，建議砲塔";
            }

            // 在路徑旁找一個空格放置提示
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    int hx = pc.gx + dx;
                    int hy = pc.gy + dy;
                    if (hx < 0 || hx >= COLS || hy < 0 || hy >= ROWS) continue;
                    if (IS_PATH[hx][hy]) continue;
                    if (TowerAt(hx, hy))  continue;

                    AIHint hint;
                    hint.gx      = hx;
                    hint.gy      = hy;
                    hint.suggest = bestSuggest;
                    hint.score   = std::min(1.f, score);
                    hint.reason  = reason;
                    hint.flashT  = 0.f;
                    aiHints.push_back(hint);
                    goto next_cell;
                }
            }
            next_cell:;

            if ((int)aiHints.size() >= 3) break;
        }
    }

    // ── 波次結束感知器訓練 ──────────────────────────────────────
    void TrainPerceptrons() {
        if (waveKills + waveEscaped == 0) return;

        float killRate = (float)waveKills / (waveKills + waveEscaped);

        for (auto& t : towers) {
            if (t.type != TType::PERCEPTRON) continue;

            t.FlushSample();
            t.learner.Update(killRate, t.w1, t.w2, t.bias);

            // 記錄 loss 歷史（用於右側面板折線圖）
            t.lossHistory.push_back(t.learner.lastLoss);
            if ((int)t.lossHistory.size() > 14) {
                t.lossHistory.erase(t.lossHistory.begin());
            }

            char buf[80];
            snprintf(buf, 80, "PCT[%d] loss=%.3f", t.id, t.learner.lastLoss);
            AddFloat(CC(t.gx, t.gy), buf, COL_AI);
        }
    }

    // ── 遊戲重置 ─────────────────────────────────────────────────
    void Reset() {
        towers.clear();
        enemies.clear();
        bullets.clear();
        pulses.clear();
        particles.clear();
        floats.clear();

        nextId   = 1;
        credits  = 380;
        score    = 0;
        wave     = 0;
        cpuHp    = 100.f;
        lives    = 20;

        paused   = false;
        showHelp = false;
        phase    = BUILD;

        placing    = TType::NONE;
        selectedId = -1;
        connectSrc = -1;

        spawned      = 0;
        spawnTimer   = 0.f;
        waveCount    = 0;
        trainingTimer= 0.f;
        combo        = 0;
        comboTimer   = 0.f;
        shakeT       = 0.f;

        waveKills   = 0;
        waveEscaped = 0;
        waveDmg     = 0.f;

        currentEvent     = WaveEvent::NONE;
        eventName        = "";
        eventBannerTimer = 0.f;
        aiHints.clear();

        dualPath        = false;
        blackoutActive  = false;
        nextPreviewPath = 1;

        buffOverfreq  = 0.f;
        buffArmorBreak= 0.f;
        buffGlobalMark= 0.f;

        BuildPath(0);
        BuildDualPath(1);

        // 放置 CPU（固定 id=0，不可刪除）
        Tower cpu;
        cpu.id    = 0;
        cpu.type  = TType::CPU;
        cpu.gx    = CPU_GX;
        cpu.gy    = CPU_GY;
        cpu.level = 1;
        cpu.sig   = 1.f;
        towers.push_back(cpu);

        SetMsg("目標：感測器→邏輯閘→砲塔！按[空白鍵]發動第一波。");
    }
};
// ══════════════════════════════════════════════════════════════════
//  座標工具
// ══════════════════════════════════════════════════════════════════

// 把虛擬滑鼠座標轉換成地圖格子索引
// 返回 false 代表滑鼠在地圖外
bool ScreenToGrid(Game& G, Vector2 sp, int& gx, int& gy) {
    auto o = G.MapOrigin();
    int x = (int)((sp.x - o.x) / CELL);
    int y = (int)((sp.y - o.y) / CELL);
    if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return false;
    gx = x;
    gy = y;
    return true;
}

// ── 方便存取 WaveEvent ──────────────────────────────────────────
using WaveEvent = Game::WaveEvent;

// ══════════════════════════════════════════════════════════════════
//  SpawnEnemy  —  生成單隻敵人並加入 G.enemies
// ══════════════════════════════════════════════════════════════════
void SpawnEnemy(Game& G) {
    std::uniform_int_distribution<int> typeR(0, 4);
    EType et;

    // ── 1. 根據本波突發事件決定敵人種類 ─────────────────────────
    switch (G.currentEvent) {
        case WaveEvent::ARMORED:
            et = EType::ARMORED;
            break;

        case WaveEvent::STEALTH:
        case WaveEvent::BLACKOUT:
            // BLACKOUT 也全隱身，強迫玩家靠感知器應對
            et = (typeR(G.rng) % 3 == 0) ? EType::FAST : EType::BASIC;
            break;

        case WaveEvent::ELITE_RUSH:
            et = EType::ELITE;
            break;

        case WaveEvent::BOSS_ESCORT:
            // 第一隻是 Boss，其餘是隨扈
            et = (G.spawned == 0) ? EType::BOSS : EType::BASIC;
            break;

        case WaveEvent::SWARM:
            // 偶爾摻一隻快速，其餘都是基礎
            et = (typeR(G.rng) % 5 == 0) ? EType::FAST : EType::BASIC;
            break;

        case WaveEvent::SIEGE:
            // 圍城艦：第一隻是超重坦，其餘是基礎
            et = (G.spawned == 0) ? EType::ARMORED : EType::BASIC;
            break;

        default: {
            // 一般波次：根據波數解鎖更多種類
            int r = typeR(G.rng);
            if (G.wave < 3) r %= 2;   // 前期只出 BASIC / FAST

            switch (r) {
                case 0: case 1: et = EType::BASIC;   break;
                case 2:         et = EType::FAST;    break;
                case 3:         et = EType::ARMORED; break;
                default:        et = EType::ELITE;   break;
            }
        }
    }

    // ── 2. 初始化敵人基礎屬性 ────────────────────────────────────
    Enemy e;
    e.id           = G.nextId++;
    e.type         = et;
    e.pathPos      = 0.f;
    e.angle        = 0.f;
    e.bossState    = BossState::CHARGE;
    e.evadeSpdMult = 1.f;

    switch (et) {
        case EType::BASIC:
            e.hp = e.maxHp = 55.f + G.wave * 25.f;
            e.spd          = 1.4f + G.wave * 0.07f;
            e.reward       = 10 + G.wave;
            break;

        case EType::FAST:
            e.hp = e.maxHp = 30.f + G.wave * 14.f;
            e.spd          = 2.8f + G.wave * 0.12f;
            e.reward       = 8 + G.wave;
            e.stealth      = (G.wave >= 4);   // Wave 4+ 自動隱身
            break;

        case EType::ARMORED:
            e.hp = e.maxHp = 120.f + G.wave * 40.f;
            e.armor        = 0.60f;
            e.spd          = 0.85f + G.wave * 0.045f;
            e.reward       = 22 + G.wave * 2;
            break;

        case EType::ELITE:
            e.hp = e.maxHp = 90.f + G.wave * 33.f;
            e.spd          = 1.1f + G.wave * 0.06f;
            e.reward       = 28 + G.wave * 2;
            // Wave 10+ 精英自帶護盾（可吸收 25% 最大 HP 的傷害）
            if (G.wave >= 10) {
                e.shielded = true;
                e.shieldHp = e.maxHp * 0.25f;
            }
            break;

        case EType::BOSS:
            e.hp = e.maxHp = 800.f + G.wave * 220.f;
            e.armor        = 0.35f;
            e.spd          = 0.55f;
            e.reward       = 150 + G.wave * 20;
            break;
    }

    // ── 3. 套用事件修飾符 ────────────────────────────────────────
    switch (G.currentEvent) {
        case WaveEvent::SWARM:
            // 蜂群數量多但體力弱
            e.hp    *= 0.6f;
            e.maxHp  = e.hp;
            break;

        case WaveEvent::STEALTH:
        case WaveEvent::BLACKOUT:
            e.stealth = true;
            break;

        case WaveEvent::ELITE_RUSH:
            e.spd *= 1.5f;
            break;

        case WaveEvent::REGEN_ARMY:
            e.regenTimer = -1.f;   // 負值代表「帶再生」
            break;

        case WaveEvent::DOUBLE_SPD:
            e.spd *= 2.f;
            break;

        case WaveEvent::MUTANT:
            // 變異體：大血量 + 快速 + 無裝甲 + 再生
            e.hp    *= 2.0f;
            e.maxHp  = e.hp;
            e.spd   *= 1.3f;
            e.armor  = 0.f;
            e.regenTimer = -1.f;
            break;

        case WaveEvent::SIEGE:
            // 圍城艦：第一隻是 HP x8、裝甲 0.8 的超重坦
            if (G.spawned == 0) {
                e.hp    *= 8.0f;
                e.maxHp  = e.hp;
                e.armor  = 0.80f;
                e.spd   *= 0.45f;
            }
            break;

        default:
            break;
    }

    // ── 4. 推入敵人清單，並分配雙路徑 ───────────────────────────
    G.enemies.push_back(e);

    if (G.dualPath && !PATH_CELLS2.empty()) {
        // 奇數批次走副路，偶數批次走主路
        G.enemies.back().pathIdx = (G.spawned % 2 == 1) ? 1 : 0;
    }
}

// ══════════════════════════════════════════════════════════════════
//  StartWave  —  玩家按下發動時呼叫
// ══════════════════════════════════════════════════════════════════
void StartWave(Game& G) {
    if (G.phase == Game::FIGHT || G.phase == Game::TRAINING) return;

    G.wave++;

    // ── 每 3 波輪換路線 ──────────────────────────────────────────
    if (G.wave > 1 && (G.wave - 1) % 3 == 0) {
        int nextPath = (CURRENT_PATH_IDX + 1) % 5;
        BuildPath(nextPath);

        if (G.dualPath) {
            int nextPath2 = (CURRENT_PATH_IDX2 + 2) % 5;
            if (nextPath2 == nextPath) nextPath2 = (nextPath2 + 1) % 5;
            BuildDualPath(nextPath2);
        }

        // 拆除壓到新路線的塔（全額退款）
        int demolished = 0;
        G.towers.erase(
            std::remove_if(G.towers.begin(), G.towers.end(),
                [&](const Tower& t) {
                    if (t.type == TType::CPU) return false;
                    bool onPath = IS_PATH[t.gx][t.gy] ||
                                  (G.dualPath && IS_PATH2[t.gx][t.gy]);
                    if (onPath) {
                        G.credits += TDef(t.type).baseCost;
                        demolished++;
                    }
                    return onPath;
                }),
            G.towers.end()
        );

        // 清除指向已刪除塔的懸空連線
        for (auto& tw : G.towers) {
            tw.conns.erase(
                std::remove_if(tw.conns.begin(), tw.conns.end(),
                    [&](int id) { return G.FindTower(id) == nullptr; }),
                tw.conns.end()
            );
        }

        G.credits += 200;

        // 組合提示訊息
        char pb[120];
        if (demolished > 0) {
            if (G.dualPath) {
                snprintf(pb, 120,
                    "雙路徑重組！+200CR。%d座塔被占用（全額退款）。主：%s | 副：%s",
                    demolished, CUR_PRESET->name, CUR_PRESET2->name);
            } else {
                snprintf(pb, 120,
                    "路徑重組！+200CR。%d座塔被占用（全額退款）。新路線：%s",
                    demolished, CUR_PRESET->name);
            }
        } else {
            if (G.dualPath) {
                snprintf(pb, 120,
                    "雙路徑重組！+200CR。主：%s | 副：%s",
                    CUR_PRESET->name, CUR_PRESET2->name);
            } else {
                snprintf(pb, 120,
                    "路徑重組！+200CR。新路線：%s",
                    CUR_PRESET->name);
            }
        }
        G.SetMsg(pb);
    }

    // ── Wave 8 開通雙路徑 ────────────────────────────────────────
    if (G.wave >= 8 && !G.dualPath) {
        G.dualPath = true;
        int p2 = (CURRENT_PATH_IDX + 2) % 5;
        if (p2 == CURRENT_PATH_IDX2) p2 = (p2 + 1) % 5;
        BuildDualPath(p2);
        G.SetMsg("警告：第二條路徑開通！雙路進攻！");
    }

    // ── 抽取本波突發事件 ─────────────────────────────────────────
    G.nextPreviewPath    = (CURRENT_PATH_IDX + 1) % 5;
    G.currentEvent       = Game::RollEvent(G.wave, G.rng);
    G.eventName          = Game::EventName(G.currentEvent);
    G.eventBannerTimer   = 4.f;
    G.blackoutActive     = (G.currentEvent == WaveEvent::BLACKOUT);

    // ── 計算本波敵人數量 ─────────────────────────────────────────
    bool boss      = (G.wave % 5 == 0);
    int  baseCount = boss ? 1 + (G.wave / 5) * 2 : 6 + G.wave * 3;

    if (G.currentEvent == WaveEvent::SWARM)       baseCount = (int)(baseCount * 2.2f);
    if (G.currentEvent == WaveEvent::BOSS_ESCORT) baseCount = 1 + G.wave * 2;
    if (G.currentEvent == WaveEvent::ELITE_RUSH)  baseCount = std::max(4, baseCount / 2);

    G.waveCount  = baseCount;
    G.spawned    = 0;
    G.spawnTimer = 0.f;
    G.phase      = Game::FIGHT;
    G.waveKills  = 0;
    G.waveEscaped= 0;
    G.waveDmg    = 0.f;

    // ── 顯示波次提示訊息 ─────────────────────────────────────────
    char buf[96];
    if (G.eventName.empty() || G.currentEvent == WaveEvent::NONE) {
        snprintf(buf, 96, "第%d波 — %d 個病毒！", G.wave, G.waveCount);
    } else {
        snprintf(buf, 96, "第%d波 %s (%d 個)", G.wave, G.eventName.c_str(), G.waveCount);
    }
    G.SetMsg(buf);
}

// ══════════════════════════════════════════════════════════════════
//  PropagateSignals  —  每幀更新所有塔的訊號值
// ══════════════════════════════════════════════════════════════════

// 計算兩塔之間的格子距離（用於訊號衰減）
float ConnDistance(Tower& src, Tower& dst) {
    float dx = (float)(src.gx - dst.gx);
    float dy = (float)(src.gy - dst.gy);
    return sqrtf(dx * dx + dy * dy);
}

void PropagateSignals(Game& G, float dt) {

    // ── Pass 1：感測器根據最近敵人距離輸出訊號 ──────────────────
    for (auto& t : G.towers) {
        if (t.type != TType::SENSOR) continue;

        // BLACKOUT 事件中感測器範圍縮減至 30%
        float effectiveRange = G.blackoutActive ? t.range * 0.30f : t.range;
        float minD = effectiveRange + 1.f;

        for (auto& e : G.enemies) {
            if (e.stealth && t.level < 2) continue;  // 低等感測器偵測不到隱身
            Vector2 eg = G.EnemyGrid(e);
            float d = Dist({(float)t.gx, (float)t.gy}, eg);
            if (d < minD) minD = d;
        }

        float newSig = (minD <= effectiveRange)
            ? std::max(0.f, 1.f - minD / effectiveRange)
            : 0.f;

        if (t.level == 3) newSig = std::min(1.f, newSig * 1.3f);  // Lv3 增幅

        float prev = t.sig;
        t.sig += (newSig - t.sig) * 10.f * dt;

        // 訊號有明顯變化時，向下游發出脈衝視覺效果
        if (fabsf(t.sig - prev) > 0.02f) {
            for (int cid : t.conns) {
                auto* d = G.FindTower(cid);
                if (d) {
                    G.pulses.push_back({ G.CC(t.gx, t.gy), G.CC(d->gx, d->gy), 0.f, TDef(t.type).col });
                }
            }
        }
    }

    // ── Pass 2：邏輯閘與感知器根據輸入訊號計算輸出 ──────────────
    for (auto& t : G.towers) {
        if (t.type == TType::SENSOR || t.type == TType::CPU || t.type == TType::NONE) continue;

        // 收集所有連向這個塔的訊號（含距離衰減）
        std::vector<float> ins;
        for (auto& src : G.towers) {
            for (int cid : src.conns) {
                if (cid != t.id) continue;
                float dist  = ConnDistance(src, t);
                float atten = powf(0.92f, dist);   // 距離越遠衰減越多
                ins.push_back(src.sig * atten);
            }
        }

        if (ins.empty()) {
            // 無輸入：訊號平滑歸零
            t.sig += (0.f - t.sig) * 8.f * dt;
            continue;
        }

        float newSig = t.sig;

        switch (t.type) {
            case TType::PERCEPTRON: {
                float s1    = ins.size() > 0 ? ins[0] : 0.f;
                float s2    = ins.size() > 1 ? ins[1] : 0.f;
                float boost = (t.level == 3) ? 2.0f : (t.level == 2) ? 1.4f : 1.0f;
                newSig = Sigmoid((s1 * t.w1 + s2 * t.w2 + t.bias) * 3.f) * boost;
                newSig = std::min(1.f, newSig);
                t.SampleSignal(s1, s2, newSig);
                break;
            }

            case TType::AND: {
                bool allHigh = true;
                for (float s : ins) {
                    if (s < 0.5f) { allHigh = false; break; }
                }
                float mult = (t.level >= 2) ? 1.5f : 1.0f;
                newSig = allHigh ? std::min(1.f, 1.f * mult) : 0.f;
                break;
            }

            case TType::OR: {
                bool anyHigh = false;
                for (float s : ins) {
                    if (s > 0.5f) { anyHigh = true; break; }
                }
                newSig = anyHigh ? 1.f : 0.f;
                break;
            }

            case TType::XOR: {
                int highCount = 0;
                for (float s : ins) {
                    if (s > 0.5f) highCount++;
                }
                newSig = (highCount == 1) ? 1.f : 0.f;
                break;
            }

            case TType::NAND: {
                bool allHigh = true;
                for (float s : ins) {
                    if (s < 0.5f) { allHigh = false; break; }
                }
                float armorBonus = (G.buffArmorBreak > 0) ? 0.3f
                                 : (t.level >= 2)         ? 0.2f
                                 :                          0.f;
                // NAND：全高時輸出 0，否則輸出 1（可含破甲加成）
                newSig = allHigh ? 0.f : std::min(1.f, 1.0f + armorBonus);
                break;
            }

            case TType::CANNON: {
                // 砲塔取最大輸入訊號
                float mx = 0.f;
                for (float s : ins) mx = std::max(mx, s);
                newSig = mx;
                break;
            }

            default:
                break;
        }

        float prev = t.sig;
        t.sig += (newSig - t.sig) * 12.f * dt;

        if (fabsf(t.sig - prev) > 0.02f) {
            for (int cid : t.conns) {
                auto* d = G.FindTower(cid);
                if (d) {
                    G.pulses.push_back({ G.CC(t.gx, t.gy), G.CC(d->gx, d->gy), 0.f, TDef(t.type).col });
                }
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════
//  CalcDamageMultiplier  —  根據連線邏輯閘計算傷害乘數
// ══════════════════════════════════════════════════════════════════
float CalcDamageMultiplier(Game& G, Tower& cannon, Enemy& target) {
    float mult = 1.f;

    // 遍歷所有連向這個砲塔的上游閘道
    for (auto& src : G.towers) {
        bool feedsCannon = false;
        for (int cid : src.conns) {
            if (cid == cannon.id) { feedsCannon = true; break; }
        }
        if (!feedsCannon) continue;

        if (src.type == TType::AND  && target.type == EType::ARMORED) mult += 0.5f;
        if (src.type == TType::XOR  && target.type == EType::ELITE)   mult += 1.0f;
        if (src.type == TType::NAND && target.type == EType::ARMORED) {
            float bonus = (src.level == 3) ? 1.2f
                        : (src.level == 2) ? 0.7f
                        :                    0.4f;
            mult += bonus;
        }
    }

    if (target.marked)         mult += 0.5f;
    if (G.buffGlobalMark > 0)  mult += 0.3f;

    // ── 護甲處理 ─────────────────────────────────────────────────
    float armorReduction = target.armor;

    // 判斷護甲是否被破（AND/NAND 技能 Buff，或 NAND Lv3 在連線中）
    bool armorBroken = (G.buffArmorBreak > 0);
    if (!armorBroken) {
        for (auto& src : G.towers) {
            for (int cid : src.conns) {
                if (cid == cannon.id && src.type == TType::NAND && src.level >= 3) {
                    armorBroken = true;
                }
            }
        }
    }

    if (armorReduction > 0 && !armorBroken) {
        // 有 AND 閘在連線中則無視護甲
        bool hasAnd = false;
        for (auto& src : G.towers) {
            for (int cid : src.conns) {
                if (cid == cannon.id && src.type == TType::AND) {
                    hasAnd = true;
                }
            }
        }
        if (!hasAnd) mult *= (1.f - armorReduction);
    }

    return mult;
}

// ══════════════════════════════════════════════════════════════════
//  UpdateBossAI  —  更新 Boss 的 AI 狀態與威脅感知
// ══════════════════════════════════════════════════════════════════
void UpdateBossAI(Game& G, Enemy& boss, float dt) {
    Vector2 eg  = G.EnemyGrid(boss);
    int     bgx = (int)(eg.x + 0.5f);
    int     bgy = (int)(eg.y + 0.5f);

    // 取當前格與前方格的威脅值，取較大者
    float threat  = G.threatMap.Get(bgx, bgy);
    int   nextIdx = (int)boss.pathPos + 2;
    if (nextIdx < (int)PATH_CELLS.size()) {
        threat = std::max(threat,
            G.threatMap.Get(PATH_CELLS[nextIdx].gx, PATH_CELLS[nextIdx].gy));
    }

    boss.localThreat = threat;
    float hpRatio    = boss.hp / boss.maxHp;
    boss.TickBoss(dt, threat, hpRatio);

    // RAMPAGE 狀態下每 3 秒顯示提示文字
    static float rampageFloatTimer = 0.f;
    rampageFloatTimer -= dt;
    if (boss.bossState == BossState::RAMPAGE && rampageFloatTimer <= 0.f) {
        G.AddFloat(G.EnemyWorld(boss), "RAMPAGE！", Color{255, 50, 50, 255});
        rampageFloatTimer = 3.f;
    }
}

// ══════════════════════════════════════════════════════════════════
//  主動技能系統
// ══════════════════════════════════════════════════════════════════

// 各塔型別的技能冷卻時間（秒）, 索引對應 TType 整數值
static const float SKILL_CD[] = {
    20.f,  // SENSOR    — 電磁脈衝
    15.f,  // PERCEPTRON— 急速突觸
    22.f,  // AND       — 裝甲穿透
    18.f,  // OR        — 超頻連射
    16.f,  // XOR       — 全域標記
    12.f,  // CANNON    — 超砲擊
     0.f,  // CPU       — （無技能）
    22.f,  // NAND      — 破甲反轉
};

static const char* SKILL_NAME[] = {
    "電磁脈衝", "急速突觸", "裝甲穿透", "超頻連射",
    "全域標記", "超砲擊",   "",         "破甲反轉"
};

static const char* SKILL_DESC[] = {
    "範圍內敵人暫停2秒",
    "加速w1/w2學習",
    "10秒護甲歸零",
    "10秒射速x2",
    "全域標記所有敵人5秒",
    "立即超砲爆炸",
    "",
    "10秒裝甲全破+標記"
};

// 計算感知器的層數（用於顯示 L1/L2/L3 標籤）
int CalcPCTLayer(Game& G, int towerId, int depth = 0) {
    if (depth > 6) return 1;

    Tower* t = G.FindTower(towerId);
    if (!t || t->type != TType::PERCEPTRON) return 1;

    int maxLayer = 1;
    for (auto& src : G.towers) {
        for (int cid : src.conns) {
            if (cid != towerId) continue;
            if (src.type == TType::PERCEPTRON) {
                maxLayer = std::max(maxLayer, 1 + CalcPCTLayer(G, src.id, depth + 1));
            }
        }
    }
    return maxLayer;
}

// 觸發指定塔的主動技能
void ActivateSkill(Game& G, Tower& t) {
    if (t.activeCd > 0.f) return;
    if (t.type == TType::CPU) return;

    int     ti  = (int)t.type;
    Vector2 pos = G.CC(t.gx, t.gy);
    t.activeCd  = SKILL_CD[ti];

    switch (t.type) {
        case TType::SENSOR: {
            // 電磁脈衝：範圍內敵人暈眩 2 秒
            int cnt = 0;
            for (auto& e : G.enemies) {
                float d = Dist({(float)t.gx, (float)t.gy}, G.EnemyGrid(e));
                if (d <= t.range * 1.5f) {
                    e.stunTimer = 2.f;
                    cnt++;
                }
            }
            G.SpawnParticles(pos, COL_SENSOR, 20, 120.f);
            char b[48];
            snprintf(b, 48, "EMP！暫停 %d 個", cnt);
            G.AddFloat(pos, b, COL_SENSOR);
            break;
        }

        case TType::PERCEPTRON: {
            // 急速突觸：強制正向梯度更新，加速學習
            t.w1 = std::min(2.f, t.w1 + 0.15f);
            t.w2 = std::min(2.f, t.w2 + 0.15f);
            t.learner.lrDecay = std::min(1.f, t.learner.lrDecay + 0.2f);
            G.SpawnParticles(pos, COL_AI, 16, 110.f);
            G.AddFloat(pos, "突觸強化！", COL_AI);
            break;
        }

        case TType::AND: {
            // 裝甲穿透：10 秒護甲歸零
            G.buffArmorBreak = 10.f;
            G.SpawnParticles(pos, COL_AND, 20, 140.f);
            G.AddFloat(pos, "裝甲穿透！10秒", COL_AND);
            break;
        }

        case TType::OR: {
            // 超頻連射：10 秒射速 x2
            G.buffOverfreq = 10.f;
            G.SpawnParticles(pos, COL_OR, 18, 130.f);
            G.AddFloat(pos, "超頻連射！10秒", COL_OR);
            break;
        }

        case TType::XOR: {
            // 全域標記：所有敵人被標記 5 秒
            for (auto& e : G.enemies) {
                e.marked     = true;
                e.markTimer  = 5.f;
            }
            G.buffGlobalMark = 5.f;
            G.SpawnParticles(pos, COL_XOR, 24, 130.f);
            G.AddFloat(pos, "全域標記！", COL_XOR);
            break;
        }

        case TType::CANNON: {
            // 超砲：對最近敵人發射 500 傷害濺射砲彈
            Enemy* tgt  = nullptr;
            float  best = 999.f;
            for (auto& e : G.enemies) {
                float d = Dist({(float)t.gx, (float)t.gy}, G.EnemyGrid(e));
                if (d < best) { best = d; tgt = &e; }
            }
            if (tgt) {
                Vector2 tp  = G.EnemyWorld(*tgt);
                Vector2 sp  = G.CC(t.gx, t.gy);
                Vector2 dir = Vector2Normalize(Vector2Subtract(tp, sp));
                G.bullets.push_back({ sp, {dir.x * 600.f, dir.y * 600.f},
                                       tgt->id, t.id, 500.f, true, 80.f, RED });
                G.AddFloat(pos, "超砲！", RED);
            }
            G.SpawnParticles(pos, RED, 16, 160.f);
            break;
        }

        case TType::NAND: {
            // 破甲反轉：10 秒護甲歸零 + 全員標記
            G.buffArmorBreak = 10.f;
            G.buffGlobalMark = 10.f;
            for (auto& e : G.enemies) {
                e.marked    = true;
                e.markTimer = 10.f;
            }
            G.SpawnParticles(pos, COL_NAND, 24, 150.f);
            G.AddFloat(pos, "破甲反轉！全標記10秒", COL_NAND);
            break;
        }

        default:
            break;
    }

    G.Shake(6.f, 0.2f);
}

// ══════════════════════════════════════════════════════════════════
//  Update  —  每幀主更新迴圈
// ══════════════════════════════════════════════════════════════════
void Update(Game& G, float dt) {
    if (G.paused || G.phase == Game::GAMEOVER || G.phase == Game::MENU) return;

    // ── 倒數計時器更新 ───────────────────────────────────────────
    if (G.msgTimer        > 0) G.msgTimer        -= dt;
    if (G.shakeT          > 0) G.shakeT          -= dt;
    if (G.eventBannerTimer> 0) G.eventBannerTimer -= dt;  // Bug fix: 從 DrawTopBar 移到此處
    if (G.comboTimer > 0) {
        G.comboTimer -= dt;
        if (G.comboTimer <= 0) G.combo = 0;
    }

    for (auto& h : G.aiHints) h.flashT += dt * 2.5f;

    if (G.buffOverfreq   > 0) G.buffOverfreq   -= dt;
    if (G.buffArmorBreak > 0) G.buffArmorBreak -= dt;
    if (G.buffGlobalMark > 0) G.buffGlobalMark -= dt;

    for (auto& t : G.towers) {
        if (t.activeCd > 0) t.activeCd = std::max(0.f, t.activeCd - dt);
    }

    // ── 訓練階段：等待倒計結束後進入建造階段 ────────────────────
    if (G.phase == Game::TRAINING) {
        G.trainingTimer -= dt;
        if (G.trainingTimer <= 0) {
            G.phase = Game::BUILD;
            G.SetMsg("訓練完成！準備下一波。");
        }
        return;
    }

    // ── 生成敵人 ─────────────────────────────────────────────────
    if (G.phase == Game::FIGHT && G.spawned < G.waveCount) {
        G.spawnTimer += dt;
        float interval = std::max(0.15f, 0.75f - G.wave * 0.035f);
        if (G.spawnTimer >= interval) {
            G.spawnTimer = 0.f;
            SpawnEnemy(G);
            G.spawned++;
        }
    }

    // ── 訊號傳播 ─────────────────────────────────────────────────
    if (G.phase == Game::FIGHT) {
        PropagateSignals(G, dt);
    }

    // ── 塔的動畫計時器 ───────────────────────────────────────────
    for (auto& t : G.towers) {
        t.anim += dt * 3.f;
        if (t.anim > 6.28f) t.anim -= 6.28f;
        if (t.upgradeFlash > 0) t.upgradeFlash -= dt * 2.f;
    }

    // ── 砲塔開火邏輯 ─────────────────────────────────────────────
    for (auto& cannon : G.towers) {
        if (cannon.type != TType::CANNON) continue;
        if (cannon.cooldown > 0) { cannon.cooldown--; continue; }
        if (cannon.sig < 0.5f)   continue;

        // 計算實際射速（OR Lv2 加速 / buffOverfreq 超頻）
        int effectiveCd = cannon.fireRateTicks;
        bool hasOrL2 = false;
        for (auto& src : G.towers) {
            for (int cid : src.conns) {
                if (cid == cannon.id && src.type == TType::OR && src.level >= 2) {
                    hasOrL2 = true;
                }
            }
        }
        if (hasOrL2)            effectiveCd = (int)(effectiveCd * 0.7f);
        if (G.buffOverfreq > 0) effectiveCd = (int)(effectiveCd * 0.5f);

        // 找範圍內最近的目標
        Enemy* tgt  = nullptr;
        float  best = cannon.range;
        for (auto& e : G.enemies) {
            float d = Dist({(float)cannon.gx, (float)cannon.gy}, G.EnemyGrid(e));
            if (d < best) { best = d; tgt = &e; }
        }
        if (!tgt) continue;

        cannon.cooldown = effectiveCd;
        float dmg = cannon.damage * CalcDamageMultiplier(G, cannon, *tgt);

        // XOR Lv2 有 20% 暴擊
        bool hasXorL2 = false;
        for (auto& src : G.towers) {
            for (int cid : src.conns) {
                if (cid == cannon.id && src.type == TType::XOR && src.level >= 2) {
                    hasXorL2 = true;
                }
            }
        }
        bool crit = hasXorL2 && (G.rng() % 5 == 0);
        if (crit) dmg *= 2.f;

        // XOR Lv3 命中時標記目標
        bool hasXorL3 = false;
        for (auto& src : G.towers) {
            for (int cid : src.conns) {
                if (cid == cannon.id && src.type == TType::XOR && src.level >= 3) {
                    hasXorL3 = true;
                }
            }
        }
        if (hasXorL3) {
            tgt->marked    = true;
            tgt->markTimer = 4.f;
        }

        // 發射主砲彈
        Vector2 tp  = G.EnemyWorld(*tgt);
        Vector2 sp  = G.CC(cannon.gx, cannon.gy);
        Vector2 dir = Vector2Normalize(Vector2Subtract(tp, sp));
        bool hasSplash = (cannon.level == 3);
        Color bcol = crit ? YELLOW : COL_CANNON;
        G.bullets.push_back({ sp, {dir.x * 420.f, dir.y * 420.f},
                               tgt->id, cannon.id, dmg, hasSplash, hasSplash ? 55.f : 0.f, bcol });

        // OR Lv3 同時射擊第二目標
        bool hasOrL3 = false;
        for (auto& src : G.towers) {
            for (int cid : src.conns) {
                if (cid == cannon.id && src.type == TType::OR && src.level >= 3) {
                    hasOrL3 = true;
                }
            }
        }
        if (hasOrL3) {
            Enemy* tgt2 = nullptr;
            float  best2 = cannon.range;
            for (auto& e : G.enemies) {
                if (e.id == tgt->id) continue;
                float d = Dist({(float)cannon.gx, (float)cannon.gy}, G.EnemyGrid(e));
                if (d < best2) { best2 = d; tgt2 = &e; }
            }
            if (tgt2) {
                Vector2 tp2  = G.EnemyWorld(*tgt2);
                Vector2 dir2 = Vector2Normalize(Vector2Subtract(tp2, sp));
                G.bullets.push_back({ sp, {dir2.x * 420.f, dir2.y * 420.f},
                                       tgt2->id, cannon.id, dmg * 0.6f, false, 0.f, COL_OR });
            }
        }
    }

    // ── 更新砲彈位置 ─────────────────────────────────────────────
    for (auto& b : G.bullets) {
        b.pos.x += b.vel.x * dt;
        b.pos.y += b.vel.y * dt;
    }

    // ── 砲彈命中檢測 ─────────────────────────────────────────────
    std::vector<Bullet> remainBullets;
    for (auto& b : G.bullets) {
        bool hit = false;

        for (auto& e : G.enemies) {
            Vector2 ep = G.EnemyWorld(e);
            if (Dist(b.pos, ep) >= 20.f) continue;

            // 護盾優先吸收傷害
            float actualDmg = b.dmg;
            if (e.shielded && e.shieldHp > 0.f) {
                float absorbed = std::min(actualDmg, e.shieldHp);
                e.shieldHp -= absorbed;
                actualDmg  -= absorbed;
                if (e.shieldHp <= 0.f) {
                    e.shielded = false;
                    G.SpawnParticles(ep, Color{150, 220, 255, 255}, 12, 90.f);
                    G.AddFloat(ep, "護盾破！", Color{150, 220, 255, 255});
                }
            }

            e.hp         -= actualDmg;
            e.flashTimer  = 0.15f;
            G.waveDmg    += actualDmg;   // Bug fix: 用 actualDmg 而非 b.dmg（護盾吸收後的實際傷害）
            G.SpawnParticles(b.pos, b.col, 6, 80.f);
            hit = true;

            // 更新發射砲塔的擊傷統計（用 sourceId 反查砲塔）
            Tower* src = G.FindTower(b.sourceId);
            if (src) src->totalDmg += actualDmg;

            // 濺射傷害（同樣走護盾吸收邏輯）
            if (b.splash) {
                for (auto& e2 : G.enemies) {
                    if (e2.id == e.id) continue;
                    if (Dist(G.EnemyWorld(e2), b.pos) >= b.splashR) continue;

                    float splashDmg = b.dmg * 0.6f;
                    // 濺射也需先過護盾 (Bug fix: 原版濺射直接扣 hp)
                    if (e2.shielded && e2.shieldHp > 0.f) {
                        float absorbed2 = std::min(splashDmg, e2.shieldHp);
                        e2.shieldHp -= absorbed2;
                        splashDmg   -= absorbed2;
                        if (e2.shieldHp <= 0.f) {
                            e2.shielded = false;
                            G.SpawnParticles(G.EnemyWorld(e2), Color{150, 220, 255, 255}, 8, 80.f);
                            G.AddFloat(G.EnemyWorld(e2), "護盾破！", Color{150, 220, 255, 255});
                        }
                    }
                    e2.hp -= splashDmg;
                    G.SpawnParticles(G.EnemyWorld(e2), Color{255, 180, 50, 255}, 4, 60.f);
                }
                G.SpawnParticles(b.pos, Color{255, 120, 40, 255}, 16, 100.f);
            }
            break;
        }

        // 未命中且仍在地圖內則保留
        if (!hit) {
            auto o = G.MapOrigin();
            if (b.pos.x > o.x && b.pos.x < o.x + MAP_W &&
                b.pos.y > o.y && b.pos.y < o.y + MAP_H) {
                remainBullets.push_back(b);
            }
        }
    }
    G.bullets = remainBullets;

    // ── 擊殺結算 ─────────────────────────────────────────────────
    for (auto it = G.enemies.begin(); it != G.enemies.end();) {
        if (it->hp > 0) { ++it; continue; }

        G.waveKills++;
        G.combo++;
        G.comboTimer = Game::COMBO_WINDOW;

        // 找最後命中的砲塔（掃描砲彈找 targetId，但砲彈可能已被移除，改用 threatMap 記錄擊殺位置即可）
        // kills 統計：找任一 sourceId 對應砲塔累加（若有子彈正瞄準這個敵人）
        for (auto& b : G.bullets) {
            if (b.targetId == it->id) {
                Tower* src = G.FindTower(b.sourceId);
                if (src) { src->kills++; break; }
            }
        }

        Vector2 eg  = G.EnemyGrid(*it);
        int kgx = (int)(eg.x + 0.5f);
        int kgy = (int)(eg.y + 0.5f);
        float killValue = (it->type == EType::BOSS)  ? 8.f
                        : (it->type == EType::ELITE) ? 3.f
                        :                              1.f;
        G.threatMap.AddKill(kgx, kgy, killValue);

        // 連擊倍率獎勵
        int   baseReward = it->reward;
        float comboMult  = (G.combo >= 10) ? 2.f
                         : (G.combo >=  5) ? 1.5f
                         : (G.combo >=  3) ? 1.2f
                         :                   1.f;
        int reward = (int)(baseReward * comboMult);
        G.credits += reward;
        G.score   += (int)(reward * 1.5f);
        if (G.score > G.highScore) G.highScore = G.score;

        Vector2 p = G.EnemyWorld(*it);
        int particleN = (it->type == EType::BOSS) ? 40 : 12;
        float particleSpd = (it->type == EType::BOSS) ? 180.f : 100.f;
        G.SpawnParticles(p, COL_VIRUS, particleN, particleSpd);

        std::string ftxt = "+" + std::to_string(reward) + " CR";
        if (G.combo >= 3) {
            char cb[16];
            snprintf(cb, 16, " x%.1f", comboMult);
            ftxt += cb;
        }
        G.AddFloat(p, ftxt, G.combo >= 5 ? COL_STAR : COL_AND);

        if (it->type == EType::BOSS) {
            G.AddFloat({p.x, p.y - 50}, "BOSS 擊倒！", COL_BOSS);
            G.Shake(20.f, 0.6f);
        }
        if (G.combo ==  3) G.AddFloat({p.x, p.y - 30}, "COMBO x3！",  Color{255, 220, 100, 255});
        if (G.combo ==  5) G.AddFloat({p.x, p.y - 30}, "COMBO x5！",  COL_STAR);
        if (G.combo == 10) G.AddFloat({p.x, p.y - 30}, "MEGA COMBO！", Color{255, 100, 255, 255});

        it = G.enemies.erase(it);
    }

    // ── Boss 分裂：降至 50% HP 時召喚 3 隻精英 ───────────────────
    // Bug fix: 不能在 range-for 迭代 G.enemies 時直接 push_back
    // （push_back 可能觸發 realloc，使 reference `e` 懸空 → UB）
    // 改成先收集、迭代結束後再一次性插入。
    {
        std::vector<Enemy> splitSpawns;
        for (auto& e : G.enemies) {
            if (e.type != EType::BOSS) continue;
            if (e.hasSplit)            continue;
            if (e.hp / e.maxHp > 0.50f) continue;

            e.hasSplit = true;
            G.AddFloat(G.EnemyWorld(e), "分裂！", Color{255, 80, 200, 255});
            G.Shake(10.f, 0.3f);

            for (int k = 0; k < 3; k++) {
                Enemy sp;
                sp.id      = G.nextId++;
                sp.type    = EType::ELITE;
                sp.pathPos = std::max(0.f, e.pathPos - 1.f - k * 0.5f);
                sp.hp = sp.maxHp = 80.f + G.wave * 20.f;
                sp.spd     = 1.6f + G.wave * 0.05f;
                sp.reward  = 15;
                sp.pathIdx = e.pathIdx;
                splitSpawns.push_back(sp);
            }
        }
        // 安全地插入分裂出的精英（此時 range-for 已結束）
        for (auto& sp : splitSpawns) {
            G.enemies.push_back(sp);
        }
    }

    // ── 敵人移動與再生 ───────────────────────────────────────────
    for (auto& e : G.enemies) {
        e.angle += 180.f * dt * (e.type == EType::BOSS ? 1.f : 2.f);
        if (e.flashTimer > 0) e.flashTimer -= dt;
        if (e.marked) {
            e.markTimer -= dt;
            if (e.markTimer <= 0) e.marked = false;
        }

        // 精英固定每 2 秒回血 5%
        if (e.type == EType::ELITE) {
            e.regenTimer += dt;
            if (e.regenTimer >= 2.f) {
                e.regenTimer = 0.f;
                e.hp = std::min(e.maxHp, e.hp + e.maxHp * 0.05f);
            }
        }

        // REGEN_ARMY 事件：其他種類每 1 秒回血 8%
        if (G.currentEvent == WaveEvent::REGEN_ARMY && e.type != EType::ELITE) {
            e.regenTimer += dt;
            if (e.regenTimer >= 1.f) {
                e.regenTimer = 0.f;
                e.hp = std::min(e.maxHp, e.hp + e.maxHp * 0.08f);
            }
        }

        if (e.type == EType::BOSS) UpdateBossAI(G, e, dt);

        // 暈眩中：跳過移動
        if (e.stunTimer > 0) { e.stunTimer -= dt; continue; }

        // 移動
        float speed = e.spd;
        if (e.type == EType::BOSS) speed *= e.evadeSpdMult;
        e.pathPos += speed * dt;

        // 到達終點：扣血
        auto& pathRef = (e.pathIdx == 1 && G.dualPath) ? PATH_CELLS2 : PATH_CELLS;
        if (e.pathPos >= (float)(pathRef.size() - 1)) {
            int   liveDmg = (e.type == EType::BOSS) ? 5 : 1;
            float cpuDmg  = (e.type == EType::BOSS) ? 25.f : 8.f;

            G.lives  -= liveDmg;
            G.cpuHp   = std::max(0.f, G.cpuHp - cpuDmg);

            G.SpawnParticles(G.CC(CPU_GX, CPU_GY), COL_VIRUS, 16, 120.f);
            G.AddFloat(G.CC(CPU_GX, CPU_GY), "-" + std::to_string(liveDmg) + " 命", RED);
            G.Shake(12.f, 0.4f);

            G.waveEscaped++;
            e.hp = 0.f;
        }
    }

    // 清除 hp <= 0 的殘留敵人（已到達終點或被殺）
    G.enemies.erase(
        std::remove_if(G.enemies.begin(), G.enemies.end(),
            [](const Enemy& e) { return e.hp <= 0; }),
        G.enemies.end()
    );

    // ── 訊號脈衝更新 ─────────────────────────────────────────────
    for (auto& p : G.pulses) p.t += dt * 3.5f;
    G.pulses.erase(
        std::remove_if(G.pulses.begin(), G.pulses.end(),
            [](const SigPulse& p) { return p.t >= 1.f; }),
        G.pulses.end()
    );
    if (G.pulses.size() > 500) {
        G.pulses.erase(G.pulses.begin(), G.pulses.begin() + G.pulses.size() - 500);
    }

    // ── 粒子更新 ─────────────────────────────────────────────────
    for (auto& p : G.particles) {
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.vel.y += 55.f * dt;   // 重力
        p.life  -= dt;
    }
    G.particles.erase(
        std::remove_if(G.particles.begin(), G.particles.end(),
            [](const Particle& p) { return p.life <= 0; }),
        G.particles.end()
    );

    // ── 浮動文字更新 ─────────────────────────────────────────────
    for (auto& f : G.floats) {
        f.pos.y -= 28.f * dt;
        f.life  -= dt;
    }
    G.floats.erase(
        std::remove_if(G.floats.begin(), G.floats.end(),
            [](const FloatText& f) { return f.life <= 0; }),
        G.floats.end()
    );

    // ── 波次結束判定 ─────────────────────────────────────────────
    bool waveFinished = (G.phase    == Game::FIGHT) &&
                        G.enemies.empty()           &&
                        (G.spawned  >= G.waveCount);

    if (waveFinished) {
        int bonus = 50 + G.wave * 10;
        G.blackoutActive = false;   // 波次結束，通訊恢復

        if (G.waveEscaped == 0) {
            bonus = (int)(bonus * 1.5f);
            G.SetMsg("完美防守！+" + std::to_string(bonus) + " CR！進入訓練階段...");
        } else {
            char buf[96];
            snprintf(buf, 96, "第%d波結束！+%d CR。%d個病毒突破。",
                     G.wave, bonus, G.waveEscaped);
            G.SetMsg(buf);
        }

        G.credits       += bonus;
        G.phase          = Game::TRAINING;
        G.trainingTimer  = Game::TRAIN_TIME;

        G.threatMap.Decay(0.85f);
        G.TrainPerceptrons();
        G.GenerateAIHints();
    }

    // ── 遊戲結束判定 ─────────────────────────────────────────────
    if (G.lives <= 0 || G.cpuHp <= 0) {
        G.lives  = 0;
        G.cpuHp  = 0;
        G.phase  = Game::GAMEOVER;
        if (G.score > G.highScore) G.highScore = G.score;
        G.SetMsg("防線崩潰 — 遊戲結束！");
        G.Shake(25.f, 1.f);
    }
}
// ══════════════════════════════════════════════════════════════════
//  繪圖輔助函式
// ══════════════════════════════════════════════════════════════════

// 圓角矩形（帶邊框）
void DrawRoundBox(float x, float y, float w, float h, float r,
                  Color fill, Color border, float bw = 1.5f) {
    float rr = r / std::min(w, h) * 2;
    DrawRectangleRounded({x, y, w, h}, rr, 8, fill);
    DrawRectangleRoundedLinesEx({x, y, w, h}, rr, 8, bw, border);
}

// 正六角形
void DrawHex(Vector2 c, float r, Color fill, Color border) {
    Vector2 pts[6];
    for (int i = 0; i < 6; i++) {
        float a = i * 60.f * DEG2RAD - 30.f * DEG2RAD;
        pts[i] = { c.x + r * cosf(a), c.y + r * sinf(a) };
    }
    for (int i = 0; i < 6; i++) DrawTriangle(c, pts[i], pts[(i + 1) % 6], fill);
    for (int i = 0; i < 6; i++) DrawLineV(pts[i], pts[(i + 1) % 6], border);
}

// ══════════════════════════════════════════════════════════════════
//  DrawStars  —  背景閃爍星點
// ══════════════════════════════════════════════════════════════════
void DrawStars(Game& G) {
    float t = (float)GetTime();
    for (auto& s : G.stars) {
        float b = s.bright * (0.7f + 0.3f * sinf(t * 1.1f + s.x));
        Color c = {
            (unsigned char)(150 * b),
            (unsigned char)(170 * b),
            (unsigned char)(255 * b),
            255
        };
        DrawCircle((int)s.x, (int)s.y, s.r, c);
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawPath  —  路徑格、方向線、箭頭、下波預覽、BLACKOUT警示
// ══════════════════════════════════════════════════════════════════
void DrawPath(Game& G) {
    auto  o = G.MapOrigin();
    float t = (float)GetTime();

    // ── 下一波路線預覽（每 3 波輪換前的警示） ───────────────────
    bool showPreview = (G.phase == Game::TRAINING || G.phase == Game::BUILD)
                    && (G.wave > 0)
                    && (G.wave % 3 == 0);

    if (showPreview) {
        const PathPreset* nextP = &PATH_PRESETS[G.nextPreviewPath];

        // 計算預覽格子清單
        std::vector<PathCell> previewCells;
        for (int wi = 0; wi + 1 < nextP->count; wi++) {
            int x0 = nextP->wx[wi],   y0 = nextP->wy[wi];
            int x1 = nextP->wx[wi+1], y1 = nextP->wy[wi+1];
            int dx = (x1 > x0) ? 1 : (x1 < x0) ? -1 : 0;
            int dy = (y1 > y0) ? 1 : (y1 < y0) ? -1 : 0;
            int cx = x0, cy = y0;
            while (cx != x1 || cy != y1) {
                if (cx >= 0 && cx < COLS && cy >= 0 && cy < ROWS) {
                    previewCells.push_back({cx, cy});
                }
                cx += dx;
                cy += dy;
            }
        }

        float pulse = 0.4f + 0.4f * sinf(t * 3.f);
        for (auto& pc : previewCells) {
            float px = o.x + pc.gx * CELL;
            float py = o.y + pc.gy * CELL;
            Color pvc = {255, 200, 50, (unsigned char)(pulse * 90)};
            DrawRectangle((int)px + 3, (int)py + 3, CELL - 6, CELL - 6, pvc);
            DrawRectangleLinesEx(
                {px + 2, py + 2, (float)CELL - 4, (float)CELL - 4},
                1.5f,
                AlphaOf({255, 200, 50, 255}, (int)(pulse * 160))
            );
        }

        char pn2[64];
        snprintf(pn2, 64, "⚠ 下波路線：%s", nextP->name);
        DTC(pn2,
            (int)(o.x + MAP_W / 2), (int)(o.y + MAP_H - 20),
            FS_SMALL, AlphaOf({255, 200, 50, 255}, (int)(pulse * 255)));
    }

    // ── 主路徑格子 ───────────────────────────────────────────────
    for (auto& pc : PATH_CELLS) {
        float px = o.x + pc.gx * CELL;
        float py = o.y + pc.gy * CELL;
        DrawRectangle((int)px,     (int)py,     CELL,     CELL,     COL_PATH);
        DrawRectangle((int)px + 2, (int)py + 2, CELL - 4, CELL - 4, AlphaOf({15, 28, 45, 255}, 255));
    }

    // ── 副路徑格子（雙路徑模式）─────────────────────────────────
    if (G.dualPath && !PATH_CELLS2.empty()) {
        for (auto& pc : PATH_CELLS2) {
            float px = o.x + pc.gx * CELL;
            float py = o.y + pc.gy * CELL;
            DrawRectangle((int)px,     (int)py,     CELL,     CELL,     Color{20, 15, 40, 255});
            DrawRectangle((int)px + 2, (int)py + 2, CELL - 4, CELL - 4, AlphaOf({30, 15, 55, 255}, 255));
        }
    }

    // ── 路段方向底線（主路）──────────────────────────────────────
    for (int wi = 0; wi + 1 < CUR_PRESET->count; wi++) {
        int x0 = CUR_PRESET->wx[wi],   y0 = CUR_PRESET->wy[wi];
        int x1 = CUR_PRESET->wx[wi+1], y1 = CUR_PRESET->wy[wi+1];
        if (x0 < 0 || x0 >= COLS) continue;
        Vector2 p0 = { o.x + (x0 + 0.5f) * CELL, o.y + (y0 + 0.5f) * CELL };
        Vector2 p1 = { o.x + (x1 + 0.5f) * CELL, o.y + (y1 + 0.5f) * CELL };
        DrawLineEx(p0, p1, CELL - 4.f, Color{0, 100, 60, 35});
    }

    // ── 路段方向底線（副路）──────────────────────────────────────
    if (G.dualPath) {
        for (int wi = 0; wi + 1 < CUR_PRESET2->count; wi++) {
            int x0 = CUR_PRESET2->wx[wi],   y0 = CUR_PRESET2->wy[wi];
            int x1 = CUR_PRESET2->wx[wi+1], y1 = CUR_PRESET2->wy[wi+1];
            if (x0 < 0 || x0 >= COLS) continue;
            Vector2 p0 = { o.x + (x0 + 0.5f) * CELL, o.y + (y0 + 0.5f) * CELL };
            Vector2 p1 = { o.x + (x1 + 0.5f) * CELL, o.y + (y1 + 0.5f) * CELL };
            DrawLineEx(p0, p1, CELL - 4.f, Color{80, 0, 100, 35});
        }
    }

    // ── 動態箭頭（主路）──────────────────────────────────────────
    for (int wi = 0; wi + 1 < CUR_PRESET->count; wi++) {
        int x0 = CUR_PRESET->wx[wi];
        if (x0 < 0) continue;
        int x1 = CUR_PRESET->wx[wi+1];
        int y0 = CUR_PRESET->wy[wi],   y1 = CUR_PRESET->wy[wi+1];
        Vector2 p0  = { o.x + (x0 + 0.5f) * CELL, o.y + (y0 + 0.5f) * CELL };
        Vector2 p1  = { o.x + (x1 + 0.5f) * CELL, o.y + (y1 + 0.5f) * CELL };
        Vector2 mid = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };
        Vector2 dir = Vector2Normalize(Vector2Subtract(p1, p0));
        float   ang  = atan2f(dir.y, dir.x);
        float   al   = 14.f, aw = 0.5f;
        float   anim = 0.7f + 0.3f * sinf(t * 2.5f + wi * 0.8f);
        Color   ac   = {0, 180, 100, (unsigned char)(80 * anim)};
        DrawTriangle(
            mid,
            {mid.x + al * cosf(ang + aw + 3.14f), mid.y + al * sinf(ang + aw + 3.14f)},
            {mid.x + al * cosf(ang - aw + 3.14f), mid.y + al * sinf(ang - aw + 3.14f)},
            ac
        );
    }

    // ── 動態箭頭（副路）──────────────────────────────────────────
    if (G.dualPath) {
        for (int wi = 0; wi + 1 < CUR_PRESET2->count; wi++) {
            int x0 = CUR_PRESET2->wx[wi];
            if (x0 < 0) continue;
            int x1 = CUR_PRESET2->wx[wi+1];
            int y0 = CUR_PRESET2->wy[wi], y1 = CUR_PRESET2->wy[wi+1];
            Vector2 p0  = { o.x + (x0 + 0.5f) * CELL, o.y + (y0 + 0.5f) * CELL };
            Vector2 p1  = { o.x + (x1 + 0.5f) * CELL, o.y + (y1 + 0.5f) * CELL };
            Vector2 mid = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };
            Vector2 dir = Vector2Normalize(Vector2Subtract(p1, p0));
            float   ang  = atan2f(dir.y, dir.x);
            float   al   = 14.f, aw = 0.5f;
            float   anim = 0.7f + 0.3f * sinf(t * 2.5f + wi * 0.8f + 1.5f);
            Color   ac   = {200, 80, 255, (unsigned char)(80 * anim)};
            DrawTriangle(
                mid,
                {mid.x + al * cosf(ang + aw + 3.14f), mid.y + al * sinf(ang + aw + 3.14f)},
                {mid.x + al * cosf(ang - aw + 3.14f), mid.y + al * sinf(ang - aw + 3.14f)},
                ac
            );
        }
    }

    // ── BLACKOUT 感測器干擾警示邊框 ──────────────────────────────
    if (G.blackoutActive) {
        float flicker = 0.5f + 0.5f * sinf(t * 9.f);
        Color warnC   = {255, 60, 60, (unsigned char)(120 * flicker)};
        DrawRectangleLinesEx(
            {(float)PANEL_L, (float)TOPBAR_H, (float)MAP_W, (float)MAP_H},
            4.f, warnC
        );
        DTC("通訊中斷！感測器失效 30%",
            PANEL_L + MAP_W / 2, TOPBAR_H + 24,
            FS_SMALL, AlphaOf(Color{255, 100, 100, 255}, (int)(220 * flicker)));
    }

    // ── 格線 ─────────────────────────────────────────────────────
    for (int x = 0; x <= COLS; x++) {
        Color c = AlphaOf({0, 200, 100, 255}, (x % 5 == 0) ? 18 : 8);
        DrawLine((int)(o.x + x * CELL), (int)o.y,
                 (int)(o.x + x * CELL), (int)(o.y + ROWS * CELL), c);
    }
    for (int y = 0; y <= ROWS; y++) {
        Color c = AlphaOf({0, 200, 100, 255}, (y % 4 == 0) ? 18 : 8);
        DrawLine((int)o.x,              (int)(o.y + y * CELL),
                 (int)(o.x + COLS * CELL), (int)(o.y + y * CELL), c);
    }

    // ── 路線名稱標籤 ─────────────────────────────────────────────
    char pn[48];
    if (G.dualPath) {
        snprintf(pn, 48, "主：%s | 副：%s", CUR_PRESET->name, CUR_PRESET2->name);
    } else {
        snprintf(pn, 32, "路線：%s", CUR_PRESET->name);
    }
    DTX(pn, o.x + 4, o.y + 4, FS_TINY, AlphaOf(COL_SENSOR, 120));
}

// ══════════════════════════════════════════════════════════════════
//  DrawThreatMap  —  威脅熱圖疊加
// ══════════════════════════════════════════════════════════════════
void DrawThreatMap(Game& G) {
    if (!G.showThreat) return;

    float mx = G.threatMap.GetMax();
    if (mx < 0.01f) return;

    auto o = G.MapOrigin();

    for (auto& pc : PATH_CELLS) {
        float v = G.threatMap.Get(pc.gx, pc.gy);
        if (v < 0.01f) continue;

        float norm = std::min(1.f, v / mx);
        Color heatCol = {
            (unsigned char)(norm * 255),
            (unsigned char)((1.f - norm) * 120),
            (unsigned char)((1.f - norm) * 200),
            (unsigned char)(40 + norm * 120)
        };

        float px = o.x + pc.gx * CELL;
        float py = o.y + pc.gy * CELL;
        DrawRectangle((int)px + 2, (int)py + 2, CELL - 4, CELL - 4, heatCol);

        if (norm > 0.6f) {
            char buf[8];
            snprintf(buf, 8, "%.0f", v);
            DTC(buf,
                (int)(px + CELL * 0.5f), (int)(py + CELL * 0.5f),
                FS_TINY, AlphaOf(WHITE, (int)(160 * norm)));
        }
    }

    // 圖例漸層條
    auto mo = G.MapOrigin();
    int lx = (int)(mo.x + MAP_W - 130);
    int ly = (int)(mo.y + MAP_H - 30);
    for (int i = 0; i < 100; i++) {
        float n = i / 100.f;
        DrawRectangle(lx + i, ly, 1, 14, {
            (unsigned char)(n * 255),
            (unsigned char)((1.f - n) * 120),
            (unsigned char)((1.f - n) * 200),
            200
        });
    }
    DTX("低", (float)(lx - 24), (float)ly, FS_TINY, AlphaOf(WHITE, 160));
    DTX("高", (float)(lx + 102), (float)ly, FS_TINY, AlphaOf(WHITE, 160));
}

// ══════════════════════════════════════════════════════════════════
//  DrawAIHints  —  AI 顧問建議格（建造/訓練階段顯示）
// ══════════════════════════════════════════════════════════════════
void DrawAIHints(Game& G) {
    if (!G.showAIHints) return;
    if (G.phase != Game::BUILD && G.phase != Game::TRAINING) return;

    float t = (float)GetTime();

    for (auto& h : G.aiHints) {
        float px = (float)PANEL_L  + h.gx * CELL;
        float py = (float)TOPBAR_H + h.gy * CELL;

        float pulse = 0.55f + 0.45f * sinf(t * 3.5f + h.gx * 0.7f);
        unsigned char alpha = (unsigned char)(pulse * 200);

        // 背景填色
        Color bgCol = TDef(h.suggest).col;
        bgCol.a = (unsigned char)(pulse * 35);
        DrawRectangle((int)px + 3, (int)py + 3, CELL - 6, CELL - 6, bgCol);

        // 邊框
        Color bdCol = COL_AI;
        bdCol.a = alpha;
        DrawRectangleLinesEx({px + 2, py + 2, (float)CELL - 4, (float)CELL - 4}, 2.f, bdCol);

        // 標籤
        DTX("AI", px + 4, py + 3, FS_TINY, AlphaOf(COL_AI, (int)(pulse * 220)));
        DTC(TDef(h.suggest).sym,
            (int)(px + CELL / 2), (int)(py + CELL / 2 + 4),
            FS_SMALL, AlphaOf(TDef(h.suggest).col, (int)(pulse * 200)));

        // 滑鼠懸停時顯示提示文字
        Vector2 mp = VirtualMouse();
        if (mp.x >= px && mp.x < px + CELL && mp.y >= py && mp.y < py + CELL) {
            float tw = MCN(h.reason.c_str(), FS_TINY) + 12;
            float tx = px + CELL / 2 - tw / 2;
            float ty = py - 26;
            DrawRectangle((int)tx - 2, (int)ty - 2, (int)tw + 4, 22, AlphaOf({4, 9, 18, 255}, 220));
            DrawRectangleLinesEx({tx - 2, ty - 2, tw + 4, 22}, 1.f, AlphaOf(COL_AI, 180));
            DTX(h.reason.c_str(), tx + 4, ty + 2, FS_TINY, AlphaOf(COL_AI, 230));
        }
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawConnections  —  塔之間的連線與流動箭頭
// ══════════════════════════════════════════════════════════════════
void DrawConnections(Game& G) {
    float t = (float)GetTime();

    for (auto& tw : G.towers) {
        for (int cid : tw.conns) {
            auto* dst = G.FindTower(cid);
            if (!dst) continue;

            Vector2 src = G.CC(tw.gx, tw.gy);
            Vector2 d   = G.CC(dst->gx, dst->gy);

            // 連線顏色隨訊號強弱變化
            Color sc = TDef(tw.type).col;
            sc.a = (unsigned char)(60 + tw.sig * 110);
            DrawLineEx(src, d, 2.f + tw.sig * 2.5f, sc);

            // 流動箭頭
            Vector2 dir = Vector2Normalize(Vector2Subtract(d, src));
            float   aT  = fmodf(t * 0.7f + tw.id * 0.25f, 1.f);
            Vector2 mid = { src.x + (d.x - src.x) * aT, src.y + (d.y - src.y) * aT };
            float   ang = atan2f(dir.y, dir.x);
            float   al  = 11.f, aw = 0.45f;
            Color   ac  = sc;
            ac.a = (unsigned char)(120 + tw.sig * 90);
            DrawTriangle(
                mid,
                {mid.x + al * cosf(ang + aw + 3.14f), mid.y + al * sinf(ang + aw + 3.14f)},
                {mid.x + al * cosf(ang - aw + 3.14f), mid.y + al * sinf(ang - aw + 3.14f)},
                ac
            );
        }
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawPulses  —  訊號脈衝粒子（連線上的移動光點）
// ══════════════════════════════════════════════════════════════════
void DrawPulses(Game& G) {
    for (auto& p : G.pulses) {
        float   fade = 1.f - p.t;
        Vector2 pos  = {
            p.src.x + (p.dst.x - p.src.x) * p.t,
            p.src.y + (p.dst.y - p.src.y) * p.t
        };

        Color c = p.col;
        c.a = (unsigned char)(240 * fade);
        DrawCircleV(pos, 7.f, c);

        c.a = (unsigned char)(70 * fade);
        DrawCircleV(pos, 13.f, c);

        Color cc = WHITE;
        cc.a = (unsigned char)(180 * fade);
        DrawCircleV(pos, 3.f, cc);
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawTower  —  繪製單一防禦塔
// ══════════════════════════════════════════════════════════════════
void DrawTower(Game& G, Tower& t, bool sel) {
    Vector2 ctr = G.CC(t.gx, t.gy);
    float   px  = (float)PANEL_L  + t.gx * CELL;
    float   py  = (float)TOPBAR_H + t.gy * CELL;

    // ── CPU 特殊繪製 ─────────────────────────────────────────────
    if (t.type == TType::CPU) {
        float glow  = 10.f + 8.f * sinf((float)GetTime() * 2.5f);
        float cpuR  = G.cpuHp / 100.f;
        Color cpuCol = (cpuR > 0.5f) ? COL_CPU
                     : (cpuR > 0.25f) ? ORANGE
                     :                   RED;

        for (int i = 3; i >= 1; i--) {
            Color gc  = cpuCol;
            gc.a      = (unsigned char)(12 * i);
            float ext = glow * i * 0.5f;
            DrawRectangleRounded(
                {px + 3.f - ext, py + 3.f - ext, (float)CELL - 6 + ext * 2, (float)CELL - 6 + ext * 2},
                0.25f, 8, gc
            );
        }
        DrawRectangleRounded({px + 4, py + 4, (float)CELL - 8, (float)CELL - 8}, 0.2f, 8, Color{6, 20, 35, 255});
        DrawRectangleRoundedLines({px + 4, py + 4, (float)CELL - 8, (float)CELL - 8}, 0.2f, 8, cpuCol);

        DTC("CPU", (int)ctr.x, (int)ctr.y - 8,  FS_MED,   cpuCol);
        char buf[8];
        snprintf(buf, 8, "%.0f%%", G.cpuHp);
        DTC(buf,   (int)ctr.x, (int)ctr.y + 14, FS_SMALL, cpuCol);
        return;
    }

    TowerDef& def = TDef(t.type);

    // 背景填色（訊號強度影響透明度）
    Color fill   = def.col;
    fill.a       = (unsigned char)((0.15f + t.sig * 0.85f) * 65);
    Color border = def.col;
    border.a     = (unsigned char)(150 + t.sig * 100);

    // 升級閃光效果
    if (t.upgradeFlash > 0) {
        Color ufc = WHITE;
        ufc.a = (unsigned char)(200 * t.upgradeFlash);
        DrawRectangleRounded({px + 2, py + 2, (float)CELL - 4, (float)CELL - 4}, 0.2f, 8, ufc);
    }

    // 訊號光暈
    if (t.sig > 0.05f) {
        for (int i = 2; i >= 1; i--) {
            float gr = 12.f + t.sig * 18.f;
            Color gc = def.col;
            gc.a = (unsigned char)(t.sig * 22 / i);
            if (t.type == TType::PERCEPTRON) {
                DrawCircle((int)ctr.x, (int)ctr.y, (int)((float)CELL * 0.43f + gr * i / 2), gc);
            } else {
                DrawRectangle(
                    (int)(px + 2 - gr * i / 3),
                    (int)(py + 2 - gr * i / 3),
                    (int)(CELL - 4 + gr * i * 2 / 3),
                    (int)(CELL - 4 + gr * i * 2 / 3),
                    gc
                );
            }
        }
    }

    // 主體形狀（感知器用六角，其餘用圓角矩形）
    if (t.type == TType::PERCEPTRON) {
        DrawHex(ctr, (float)CELL * 0.42f, fill, border);
    } else {
        DrawRoundBox(px + 5, py + 5, (float)CELL - 10, (float)CELL - 10, 6, fill, border, 2.f);
    }

    // 選取圓圈 + 範圍預覽
    if (sel) {
        float pulse = 0.85f + 0.15f * sinf((float)GetTime() * 5.f);
        Color wc    = WHITE;
        wc.a        = 200;
        DrawCircleLinesV(ctr, (float)CELL * 0.54f * pulse, wc);

        if (t.type == TType::SENSOR) {
            DrawCircleV(ctr, t.range * CELL, AlphaOf(COL_SENSOR, 30));
            DrawCircleLinesV(ctr, t.range * CELL, AlphaOf(COL_SENSOR, 100));
        }
        if (t.type == TType::CANNON) {
            DrawCircleV(ctr, t.range * CELL, AlphaOf(COL_CANNON, 25));
            DrawCircleLinesV(ctr, t.range * CELL, AlphaOf(COL_CANNON, 80));
        }
    }

    // 感知器層數標籤（L1/L2/L3）
    if (t.type == TType::PERCEPTRON) {
        int   layer = CalcPCTLayer(G, t.id);
        char  lb[8];
        snprintf(lb, 8, "L%d", layer);
        Color lc = (layer == 1) ? COL_PERC
                 : (layer == 2) ? COL_AI
                 :                COL_STAR;
        DTX(lb, px + CELL - 22, py + 4, FS_TINY, AlphaOf(lc, 220));
    }

    // 符號文字
    DTC(def.sym, (int)ctr.x, (int)ctr.y - 8, FS_SMALL, border);

    // 等級星星
    for (int lv = 0; lv < t.level; lv++) {
        DrawPoly({px + 8 + lv * 11.f, py + (float)CELL - 16.f}, 5, 5.f, 0.f, COL_STAR);
    }

    // 訊號強度條
    int bx = (int)px + 6, by = (int)py + CELL - 10;
    int bw = CELL - 12,   bh = 7;
    DrawRectangle(bx, by, bw, bh, Color{0, 0, 0, 140});
    int fw = (int)(bw * t.sig);
    if (fw > 0) {
        Color bc = def.col;
        bc.a = 220;
        DrawRectangle(bx, by, fw, bh, bc);
    }

    // 主動技能冷卻條（訊號條上方的細條）
    if (t.type != TType::CPU) {
        float maxCd = SKILL_CD[(int)t.type];
        if (maxCd > 0.f) {
            int abx = bx, aby = by - 8, abw = bw, abh = 4;
            DrawRectangle(abx, aby, abw, abh, Color{0, 0, 0, 100});

            float ready = 1.f - t.activeCd / maxCd;
            int   afw   = (int)(abw * ready);
            Color adc   = (t.activeCd <= 0) ? Color{100, 255, 180, 255}
                                             : Color{ 80, 140, 255, 200};
            if (afw > 0) DrawRectangle(abx, aby, afw, abh, adc);

            // 就緒時邊框脈衝
            if (t.activeCd <= 0) {
                float p2 = 0.6f + 0.4f * sinf((float)GetTime() * 5.f);
                Color rc = {100, 255, 180, (unsigned char)(180 * p2)};
                DrawRectangleLinesEx({(float)abx, (float)aby, (float)abw, (float)abh}, 1.f, rc);
            }
        }
    }

    // 感知器 loss 指示燈（右上角小圓）
    if (t.type == TType::PERCEPTRON && t.learner.lastLoss > 0.f) {
        Color lossCol = (t.learner.lastLoss < 0.05f) ? GREEN
                      : (t.learner.lastLoss < 0.15f) ? YELLOW
                      :                                 RED;
        DrawCircle((int)(px + CELL - 9), (int)(py + 9), 5.f, lossCol);
        DrawCircleLines((int)(px + CELL - 9), (int)(py + 9), 5.f, AlphaOf(WHITE, 120));
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawGhostTower  —  放置時的透明預覽
// ══════════════════════════════════════════════════════════════════
void DrawGhostTower(Game& G, int gx, int gy) {
    if (G.placing == TType::NONE) return;
    if (gx < 0 || gx >= COLS || gy < 0 || gy >= ROWS) return;
    if (G.TowerAt(gx, gy) || G.IsPath(gx, gy)) return;

    TowerDef& def = TDef(G.placing);
    float px = (float)PANEL_L  + gx * CELL;
    float py = (float)TOPBAR_H + gy * CELL;
    bool  can = (G.credits >= def.baseCost);

    Color fill = def.col;   fill.a = can ? 45  : 18;
    Color bd   = def.col;   bd.a   = can ? 160 : 50;
    DrawRoundBox(px + 5, py + 5, (float)CELL - 10, (float)CELL - 10, 6, fill, bd);
    DTC(def.sym, (int)(px + CELL / 2), (int)(py + CELL / 2 - 5), FS_SMALL, bd);
    if (!can) DTC("不足", (int)(px + CELL / 2), (int)(py + CELL / 2 + 14), FS_TINY, RED);

    // 感測器 / 砲塔顯示射程預覽圓
    Vector2 c2 = { px + CELL * 0.5f, py + CELL * 0.5f };
    if (G.placing == TType::SENSOR) {
        DrawCircleV(c2, 4.5f * CELL, AlphaOf(COL_SENSOR, 15));
        DrawCircleLinesV(c2, 4.5f * CELL, AlphaOf(COL_SENSOR, 70));
    }
    if (G.placing == TType::CANNON) {
        DrawCircleV(c2, 7.f * CELL, AlphaOf(COL_CANNON, 12));
        DrawCircleLinesV(c2, 7.f * CELL, AlphaOf(COL_CANNON, 60));
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawEnemies  —  敵人六角形、HP條、護盾環、Boss標籤
// ══════════════════════════════════════════════════════════════════
void DrawEnemies(Game& G) {
    float t = (float)GetTime();

    for (auto& e : G.enemies) {
        Vector2 p  = G.EnemyWorld(e);
        bool    fl = (e.flashTimer > 0);

        // 決定顏色與大小
        Color base, ring;
        float sz = 14.f;
        switch (e.type) {
            case EType::BASIC:   base = COL_VIRUS;   ring = {255,  80, 120, 200}; sz = 14.f; break;
            case EType::FAST:    base = COL_FAST;    ring = {255, 220,  80, 200}; sz = 11.f; break;
            case EType::ARMORED: base = COL_ARMORED; ring = {160, 200, 255, 200}; sz = 17.f; break;
            case EType::ELITE:   base = COL_ELITE;   ring = {255, 120, 210, 200}; sz = 15.f; break;
            case EType::BOSS:    base = COL_BOSS;    ring = {200,  80, 255, 220}; sz = 26.f; break;
        }
        if (fl) { base = WHITE; ring = WHITE; }

        // 六角形本體
        Vector2 pts[6];
        for (int i = 0; i < 6; i++) {
            float a = i * 60.f * DEG2RAD + e.angle * DEG2RAD;
            pts[i] = { p.x + sz * cosf(a), p.y + sz * sinf(a) };
        }
        for (int i = 0; i < 6; i++) DrawTriangle(p, pts[i], pts[(i + 1) % 6], AlphaOf(base, fl ? 220 : 160));
        for (int i = 0; i < 6; i++) DrawLineV(pts[i], pts[(i + 1) % 6], ring);

        // 隱身脈衝環
        if (e.stealth) {
            float st = 0.5f + 0.5f * sinf(t * 4.f + e.id);
            DrawCircleLinesV(p, sz + 4.f, AlphaOf(COL_FAST, (int)(80 * st)));
        }

        // XOR 標記光環
        if (e.marked) {
            float mp2 = 0.7f + 0.3f * sinf(t * 6.f);
            DrawCircleLinesV(p, sz + 6.f, AlphaOf(COL_XOR, (int)(180 * mp2)));
        }

        // 護盾光環（藍色雙環）
        if (e.shielded && e.shieldHp > 0.f) {
            float sp2 = 0.6f + 0.4f * sinf(t * 5.f + e.id);
            DrawCircleLinesV(p, sz +  9.f, AlphaOf(Color{150, 220, 255, 255}, (int)(200 * sp2)));
            DrawCircleLinesV(p, sz + 11.f, AlphaOf(Color{100, 180, 255, 255}, (int)( 80 * sp2)));
        }

        // HP 條
        int bw = (int)(sz * 2) + 4, bh = 5;
        int bx = (int)(p.x - bw / 2), by = (int)(p.y - sz - 10);
        float hpR = e.hp / e.maxHp;
        DrawRectangle(bx, by, bw, bh, Color{0, 0, 0, 160});
        Color hpC = (hpR > 0.6f) ? GREEN : (hpR > 0.3f) ? ORANGE : RED;
        DrawRectangle(bx, by, (int)(bw * hpR), bh, hpC);

        // Boss 狀態標籤
        if (e.type == EType::BOSS) {
            const char* stateLabel =
                (e.bossState == BossState::RAMPAGE) ? "RAMPAGE！" :
                (e.bossState == BossState::EVADE)   ? "迴避中"   :
                                                       "BOSS";
            Color stateCol =
                (e.bossState == BossState::RAMPAGE) ? RED    :
                (e.bossState == BossState::EVADE)   ? YELLOW :
                                                       COL_BOSS;
            float pulse2 = 0.8f + 0.2f * sinf(t * 5.f);
            stateCol.a = (unsigned char)(220 * pulse2);
            DTC(stateLabel, (int)p.x, (int)(p.y - sz - 22), FS_SMALL, stateCol);

            if (G.showThreat) {
                char tb[24];
                snprintf(tb, 24, "thr:%.1f", e.localThreat);
                DTC(tb, (int)p.x, (int)(p.y + sz + 14), FS_TINY, AlphaOf(COL_THREAT, 180));
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawBullets / DrawParticles / DrawFloats  —  特效層
// ══════════════════════════════════════════════════════════════════
void DrawBullets(Game& G) {
    for (auto& b : G.bullets) {
        DrawCircleV(b.pos, b.splash ? 8.f : 5.f, b.col);
        Color gc = b.col;
        gc.a = 80;
        DrawCircleV(b.pos, b.splash ? 14.f : 9.f, gc);
    }
}

void DrawParticles(Game& G) {
    for (auto& p : G.particles) {
        float alpha = p.life / p.maxLife;
        Color c = p.col;
        c.a = (unsigned char)(200 * alpha);
        DrawCircleV(p.pos, p.radius * alpha, c);
    }
}

void DrawFloats(Game& G) {
    for (auto& f : G.floats) {
        float alpha = std::min(1.f, f.life);
        Color c = f.col;
        c.a = (unsigned char)(230 * alpha);
        DTC(f.text.c_str(), (int)f.pos.x, (int)f.pos.y, FS_SMALL, c);
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawLeftPanel  —  左側元件選擇面板
// ══════════════════════════════════════════════════════════════════
void DrawLeftPanel(Game& G) {
    DrawRectangle(0, 0, PANEL_L, VIRT_H, PANEL_BG);
    DrawRectangleLines(0, 0, PANEL_L, VIRT_H, PANEL_BD);

    float t = (float)GetTime();
    DTC("元件選擇", PANEL_L / 2, 38, FS_MED, AlphaOf(COL_CPU, 220));

    // ── 塔型別按鈕清單 ───────────────────────────────────────────
    static TType ORDER[] = {
        TType::SENSOR, TType::PERCEPTRON, TType::AND, TType::OR,
        TType::XOR,    TType::NAND,       TType::CANNON
    };
    G.btnY0 = 68;
    constexpr int BTN_H_SM  = 56;
    constexpr int BTN_GAP_SM = 3;

    for (int i = 0; i < 7; i++) {
        TType     tt  = ORDER[i];
        TowerDef& def = TDef(tt);
        int       by  = G.btnY0 + i * (BTN_H_SM + BTN_GAP_SM);

        bool sel    = (G.placing == tt);
        bool canBuy = (G.credits >= def.baseCost);

        // 按鈕背景
        Color bg = PANEL_BG;
        if (sel)        { bg = def.col; bg.a = 55; }
        else if (!canBuy) bg = {8, 8, 8, 255};

        // 按鈕邊框
        Color bd = sel ? def.col : AlphaOf(def.col, canBuy ? 80 : 30);
        if (sel) {
            float p2 = 0.7f + 0.3f * sinf(t * 4.f);
            bd.a = (unsigned char)(200 * p2);
        }

        DrawRoundBox(10, (float)by, PANEL_L - 20, (float)BTN_H_SM, 8, bg, bd, sel ? 2.5f : 1.5f);

        // 符號、名稱、費用、說明
        Color sc = sel ? def.col : AlphaOf(def.col, canBuy ? 200 : 80);
        DTC(def.sym, 48, by + BTN_H_SM / 2, FS_MED, sc);

        Color tc = canBuy ? WHITE : AlphaOf(WHITE, 80);
        DTX(def.label, 70, (float)by + 5, FS_TINY, tc);

        char cs[16];
        snprintf(cs, 16, "%d CR", def.baseCost);
        DTX(cs, 70, (float)by + 22, FS_TINY, AlphaOf(COL_AND, canBuy ? 200 : 80));
        DTX(def.desc, 12, (float)by + BTN_H_SM - 15, FS_TINY, AlphaOf(WHITE, canBuy ? 100 : 40));
    }

    // ── 發動波次按鈕 ─────────────────────────────────────────────
    G.waveBtnY = G.btnY0 + 7 * (BTN_H_SM + BTN_GAP_SM) + 12;
    if (G.phase == Game::BUILD || G.phase == Game::TRAINING) {
        float p2 = 0.7f + 0.3f * sinf(t * 3.f);
        Color wbc = (G.phase == Game::BUILD) ? COL_PERC : AlphaOf(COL_PERC, 120);
        DrawRoundBox(10, (float)G.waveBtnY, PANEL_L - 20, 54, 10, AlphaOf(wbc, 25), wbc, 2.5f);

        const char* wbTxt = (G.phase == Game::BUILD) ? "▶ 發動下一波" : "訓練中...";
        DTC(wbTxt, PANEL_L / 2, G.waveBtnY + 27, FS_MED, AlphaOf(wbc, (int)(220 * p2)));

        if (G.phase == Game::TRAINING) {
            char tb[32];
            snprintf(tb, 32, "%.1f 秒", G.trainingTimer);
            DTC(tb, PANEL_L / 2, G.waveBtnY + 46, FS_TINY, AlphaOf(COL_AND, 180));
        }
    }

    // ── 金幣顯示 + 訊息 ──────────────────────────────────────────
    int infoY = G.waveBtnY + 70;
    char cr[32];
    snprintf(cr, 32, "金幣 %d CR", G.credits);
    DTC(cr, PANEL_L / 2, infoY, FS_MED, COL_AND);

    if (G.msgTimer > 0 && !G.msg.empty()) {
        float alpha = std::min(1.f, G.msgTimer);
        int   wrap  = 28;

        if ((int)G.msg.size() > wrap) {
            DTX(G.msg.substr(0, wrap).c_str(),  8, (float)(infoY + 28), FS_TINY, AlphaOf(COL_SENSOR, (int)(200 * alpha)));
            DTX(G.msg.substr(wrap).c_str(),     8, (float)(infoY + 48), FS_TINY, AlphaOf(COL_SENSOR, (int)(200 * alpha)));
        } else {
            DTX(G.msg.c_str(), 8, (float)(infoY + 28), FS_TINY, AlphaOf(COL_SENSOR, (int)(200 * alpha)));
        }
    }

    // ── AI 工具切換列 ────────────────────────────────────────────
    int akY = infoY + 80;
    DrawRoundBox(10, (float)akY, PANEL_L - 20, 52, 6, AlphaOf(COL_AI, 12), AlphaOf(COL_AI, 60), 1.f);
    DTC("AI 工具", PANEL_L / 2, akY + 12, FS_TINY, AlphaOf(COL_AI, 180));
    DTX("[T] 熱圖",  18,          (float)(akY + 28), FS_TINY, G.showThreat   ? COL_AI : AlphaOf(COL_AI, 100));
    DTX("[A] 提示", PANEL_L / 2 + 4, (float)(akY + 28), FS_TINY, G.showAIHints ? COL_AI : AlphaOf(COL_AI, 100));
}

// ══════════════════════════════════════════════════════════════════
//  DrawRightPanel  —  右側元件資訊面板
// ══════════════════════════════════════════════════════════════════
void DrawRightPanel(Game& G) {
    int rx = VIRT_W - PANEL_R;
    int cx = rx + PANEL_R / 2;

    DrawRectangle(rx, 0, PANEL_R, VIRT_H, PANEL_BG);
    DrawRectangleLines(rx, 0, PANEL_R, VIRT_H, PANEL_BD);
    DTC("元件資訊", cx, 38, FS_MED, AlphaOf(COL_CPU, 220));

    Tower* sel = G.FindTower(G.selectedId);
    if (!sel) {
        DTC("點擊元件查看", cx, VIRT_H / 2, FS_SMALL, AlphaOf(WHITE, 80));
        return;
    }

    TowerDef& def = TDef(sel->type);
    int y = 70;

    // 名稱 + 星級
    DTC(def.label, cx, y, FS_LARGE, def.col);
    y += 44;
    for (int lv = 0; lv < sel->level; lv++) {
        DrawPoly({(float)(rx + 30 + lv * 22), (float)y}, 5, 9.f, 0.f, COL_STAR);
    }
    y += 26;

    // 訊號強度條
    DrawRoundBox((float)rx + 12, (float)y, (float)PANEL_R - 24, 18, 4, AlphaOf(BLACK, 180), AlphaOf(def.col, 80));
    int sw = (int)((PANEL_R - 28) * sel->sig);
    if (sw > 0) DrawRectangle(rx + 14, y + 2, sw, 14, AlphaOf(def.col, 200));
    char sb[20];
    snprintf(sb, 20, "訊號 %.2f", sel->sig);
    DTC(sb, cx, y + 9, FS_TINY, WHITE);
    y += 32;

    // 描述 + 技能說明
    DTX(def.desc,                          (float)rx + 10, (float)y, FS_TINY, AlphaOf(WHITE,    160)); y += 22;
    DTX(def.tiers[sel->level - 1].ability, (float)rx + 10, (float)y, FS_TINY, AlphaOf(def.col, 200)); y += 22;

    // 升級按鈕
    if (sel->type != TType::CPU && sel->level < 3) {
        int  ucost = TDef(sel->type).tiers[sel->level].cost;
        bool canUp = (G.credits >= ucost);
        Color uc   = canUp ? COL_PERC : AlphaOf(COL_PERC, 60);
        DrawRoundBox((float)rx + 12, (float)y, (float)PANEL_R - 24, 36, 6, AlphaOf(uc, canUp ? 25 : 10), uc, 1.5f);
        char ub[40];
        snprintf(ub, 40, "[U] 升級 Lv.%d  %d CR", sel->level + 1, ucost);
        DTC(ub, cx, y + 18, FS_TINY, uc);
        y += 46;
    }

    // 統計數據
    char kb[32], db[40];
    snprintf(kb, 32, "擊殺：%d",     sel->kills);
    snprintf(db, 40, "總傷：%.0f",   sel->totalDmg);
    DTX(kb, (float)rx + 12, (float)y, FS_TINY, AlphaOf(WHITE, 160)); y += 20;
    DTX(db, (float)rx + 12, (float)y, FS_TINY, AlphaOf(WHITE, 140)); y += 28;

    // ── 感知器學習狀態 ───────────────────────────────────────────
    if (sel->type == TType::PERCEPTRON) {
        DrawRoundBox((float)rx + 8, (float)y, (float)PANEL_R - 16, 110, 6,
                     AlphaOf(COL_AI, 10), AlphaOf(COL_AI, 80), 1.5f);
        DTC("神經元學習狀態", cx, y + 14, FS_TINY, AlphaOf(COL_AI, 220));
        y += 28;

        char w1b[32], w2b[32], bib[32];
        snprintf(w1b, 32, "w1  = %+.3f", sel->w1);
        snprintf(w2b, 32, "w2  = %+.3f", sel->w2);
        snprintf(bib, 32, "bias= %+.3f", sel->bias);
        DTX(w1b, (float)rx + 14, (float)y, FS_TINY, AlphaOf(COL_PERC, 200)); y += 18;
        DTX(w2b, (float)rx + 14, (float)y, FS_TINY, AlphaOf(COL_PERC, 200)); y += 18;
        DTX(bib, (float)rx + 14, (float)y, FS_TINY, AlphaOf(COL_OR,   200)); y += 18;

        float loss = sel->learner.lastLoss;
        Color lc   = (loss < 0.05f) ? GREEN : (loss < 0.15f) ? YELLOW : RED;
        char  lb2[32], lrd[36];
        snprintf(lb2, 32, "loss= %.4f", loss);
        snprintf(lrd, 36, "lr衰減= %.3f", sel->learner.lrDecay);
        DTX(lb2, (float)rx + 14, (float)y, FS_TINY, lc);                    y += 18;
        DTX(lrd, (float)rx + 14, (float)y, FS_TINY, AlphaOf(WHITE, 140));   y += 20;

        // Loss 折線圖（迷你 sparkline）
        if (!sel->lossHistory.empty()) {
            y += 4;
            int sparW = PANEL_R - 24, sparH = 36;
            DrawRectangle(rx + 12, y, sparW, sparH, AlphaOf(BLACK, 120));
            DrawRectangleLinesEx({(float)rx + 12, (float)y, (float)sparW, (float)sparH}, 1.f, AlphaOf(COL_AI, 60));
            DTX("loss趨勢", (float)rx + 14, (float)y + 2, FS_TINY, AlphaOf(COL_AI, 160));

            int   n    = (int)sel->lossHistory.size();
            float maxL = *std::max_element(sel->lossHistory.begin(), sel->lossHistory.end());
            maxL = std::max(maxL, 0.01f);

            for (int i = 0; i < n; i++) {
                float lv  = sel->lossHistory[i] / maxL;
                int   bwi = std::max(2, (sparW - 4) / std::max(n, 1));
                int   hh  = (int)(lv * (sparH - 10));
                int   bxi = rx + 14 + (sparW - 4) * i / std::max(n, 1);
                int   byi = y + sparH - 2 - hh;
                Color bc  = (sel->lossHistory[i] < 0.05f) ? GREEN
                           : (sel->lossHistory[i] < 0.15f) ? YELLOW : RED;
                DrawRectangle(bxi, byi, std::min(bwi - 1, 8), hh, AlphaOf(bc, 180));
            }
            y += sparH + 6;
        }
    }

    // ── 主動技能資訊 ─────────────────────────────────────────────
    if (sel->type != TType::CPU) {
        int   ti    = (int)sel->type;
        float maxCd = SKILL_CD[ti];
        if (maxCd > 0.f) {
            DrawRoundBox((float)rx + 8, (float)y, (float)PANEL_R - 16, 40, 4,
                         AlphaOf(COL_AI, 8), AlphaOf(COL_AI, 50), 1.f);
            char sn[48];
            snprintf(sn, 48, "[Q] %s", SKILL_NAME[ti]);
            DTX(sn, (float)rx + 12, (float)y + 4, FS_TINY, AlphaOf(COL_AI, 200));

            if (sel->activeCd <= 0) {
                DTX("✓ 就緒！", (float)rx + 12, (float)y + 20, FS_TINY, AlphaOf({100, 255, 180, 255}, 230));
            } else {
                char cdb[32];
                snprintf(cdb, 32, "冷卻 %.0f秒", sel->activeCd);
                DTX(cdb, (float)rx + 12, (float)y + 20, FS_TINY, AlphaOf(ORANGE, 200));
            }
            y += 48;
        }
    }

    // 連線數 + 快捷鍵提示
    if (!sel->conns.empty()) {
        char cc[24];
        snprintf(cc, 24, "連線：%d 條", (int)sel->conns.size());
        DTX(cc, (float)rx + 12, (float)y, FS_TINY, AlphaOf(WHITE, 120));
        y += 20;
    }
    DTX("[C] 連線  [U] 升級", (float)rx + 12, (float)y, FS_TINY, AlphaOf(WHITE, 90)); y += 18;
    DTX("[DEL] 移除",         (float)rx + 12, (float)y, FS_TINY, AlphaOf(WHITE, 90));
}

// ══════════════════════════════════════════════════════════════════
//  DrawTopBar  —  頂部狀態列
// ══════════════════════════════════════════════════════════════════
void DrawTopBar(Game& G) {
    DrawRectangle(0, 0, VIRT_W, TOPBAR_H, PANEL_BG);
    DrawLine(0, TOPBAR_H, VIRT_W, TOPBAR_H, PANEL_BD);

    float t  = (float)GetTime();
    float p2 = 0.8f + 0.2f * sinf(t * 1.5f);

    // 中央標題
    DTC("邏輯閘防禦戰", VIRT_W / 2, TOPBAR_H / 2, FS_TITLE, AlphaOf(COL_CPU, (int)(220 * p2)));

    // 左側：波數 + 命數
    char wb[32], lb[20];
    snprintf(wb, 32, "第 %d 波", G.wave);
    snprintf(lb, 20, "命 %d",    G.lives);
    DTX(wb, PANEL_L + 20, 12, FS_MED, COL_SENSOR);
    DTX(lb, PANEL_L + 20, 40, FS_MED,
        G.lives > 10 ? COL_PERC : G.lives > 5 ? ORANGE : RED);

    // 左中：分數
    char sc[32], hs[32];
    snprintf(sc, 32, "分數：%d",   G.score);
    snprintf(hs, 32, "最高：%d",   G.highScore);
    DTX(sc, PANEL_L + 200, 12, FS_MED, COL_AND);
    DTX(hs, PANEL_L + 200, 38, FS_TINY, AlphaOf(COL_AND, 160));

    // 雙路進攻指示
    if (G.dualPath) {
        float dp = 0.7f + 0.3f * sinf(t * 4.f);
        DTC("⚡雙路進攻", PANEL_L + 430, TOPBAR_H / 2, FS_SMALL, AlphaOf(COL_BOSS, (int)(220 * dp)));
    }

    // 啟動中的 Buff 標籤
    int bx2 = PANEL_L + 560, by2 = 8;
    if (G.buffArmorBreak > 0) {
        char ab[24];
        snprintf(ab, 24, "破甲 %.0fs", G.buffArmorBreak);
        DrawRoundBox((float)bx2, (float)by2, 90, 22, 4, AlphaOf(COL_AND, 30), AlphaOf(COL_AND, 180));
        DTC(ab, bx2 + 45, by2 + 11, FS_TINY, COL_AND);
        bx2 += 96;
    }
    if (G.buffOverfreq > 0) {
        char ob[24];
        snprintf(ob, 24, "超頻 %.0fs", G.buffOverfreq);
        DrawRoundBox((float)bx2, (float)by2, 90, 22, 4, AlphaOf(COL_OR, 30), AlphaOf(COL_OR, 180));
        DTC(ob, bx2 + 45, by2 + 11, FS_TINY, COL_OR);
        bx2 += 96;
    }
    if (G.buffGlobalMark > 0) {
        char ob2[24];
        snprintf(ob2, 24, "標記 %.0fs", G.buffGlobalMark);
        DrawRoundBox((float)bx2, (float)by2, 90, 22, 4, AlphaOf(COL_XOR, 30), AlphaOf(COL_XOR, 180));
        DTC(ob2, bx2 + 45, by2 + 11, FS_TINY, COL_XOR);
    }

    // 連擊倍率
    if (G.combo >= 3) {
        char  cb[24];
        snprintf(cb, 24, "COMBO x%d", G.combo);
        float cp  = 0.8f + 0.2f * sinf(t * 8.f);
        Color cc2 = (G.combo >= 10) ? Color{255, 100, 255, 255}
                  : (G.combo >=  5) ? COL_STAR
                  :                   Color{255, 220, 100, 255};
        cc2.a = (unsigned char)(230 * cp);
        DTC(cb, VIRT_W - PANEL_R - 200, TOPBAR_H / 2, FS_LARGE, cc2);
    }

    // 階段標籤
    const char* phaseStr =
        (G.phase == Game::FIGHT)    ? "戰鬥中" :
        (G.phase == Game::TRAINING) ? "訓練中" :
        (G.phase == Game::BUILD)    ? "建置"   : "";
    Color phaseCol =
        (G.phase == Game::FIGHT)    ? COL_VIRUS :
        (G.phase == Game::TRAINING) ? COL_AI    : COL_PERC;
    DTC(phaseStr, VIRT_W - PANEL_R - 60, TOPBAR_H / 2, FS_MED, phaseCol);

    // 突發事件橫幅
    if (G.eventBannerTimer > 0 && G.currentEvent != WaveEvent::NONE) {
        float alpha  = std::min(1.f, G.eventBannerTimer);
        float pulse2 = 0.8f + 0.2f * sinf((float)GetTime() * 6.f);
        Color ec     = {255, 80, 80, (unsigned char)(230 * alpha * pulse2)};
        DTC(G.eventName.c_str(), VIRT_W / 2, TOPBAR_H + 36, FS_LARGE, ec);
    }
}

// ── 底部快捷鍵提示列 ─────────────────────────────────────────────
void DrawBotBar(Game& G) {
    int by = VIRT_H - BOTBAR_H;
    DrawRectangle(0, by, VIRT_W, BOTBAR_H, PANEL_BG);
    DrawLine(0, by, VIRT_W, by, PANEL_BD);
    const char* hint =
        "[空白]發動  [C]連線  [U]升級  [Q]主動技能  [DEL]移除  "
        "[T]熱圖  [A]AI提示  [P]暫停  [H]說明  [F11]全螢幕";
    DTC(hint, VIRT_W / 2, by + BOTBAR_H / 2, FS_TINY, AlphaOf(WHITE, 120));
}

// ══════════════════════════════════════════════════════════════════
//  DrawMenu  —  主選單畫面
// ══════════════════════════════════════════════════════════════════
void DrawMenu(Game& G) {
    float t  = (float)GetTime();
    int   cx = VIRT_W / 2, cy = VIRT_H / 2;

    DrawRectangleGradientV(0, 0, VIRT_W, VIRT_H, Color{3, 7, 14, 255}, Color{8, 18, 35, 255});
    DrawStars(G);

    // 光暈
    for (int i = 4; i >= 1; i--) {
        float glow = 30.f * i * (0.8f + 0.2f * sinf(t * 1.5f));
        DrawCircleV({(float)cx, (float)(cy - 120)}, glow, AlphaOf(COL_CPU, 6 / i));
    }

    // 標題
    float tp = 0.85f + 0.15f * sinf(t * 2.f);
    DTC("邏輯閘防禦戰",                          cx, cy - 140, FS_BIG, AlphaOf(COL_CPU,    (int)(240 * tp)));
    DTC("Logic Gate Defense  v3.0 + AI Edition", cx, cy -  82, FS_MED, AlphaOf(COL_SENSOR, 180));

    // 功能特色清單
    const char* feats[] = {
        "★ 神經網路訊號傳播", "★ 感知器多層學習+loss圖", "★ Boss 狀態機 AI",
        "★ 威脅熱圖視覺化",   "★ 雙路徑+預告系統",       "★ 主動技能+NAND閘"
    };
    for (int i = 0; i < 6; i++) {
        int   col = i % 3, row = i / 3;
        int   fx  = cx - 460 + col * 320;
        int   fy  = cy -  30 + row *  28;
        float fp  = 0.6f + 0.4f * sinf(t * 1.8f + i * 0.6f);
        DTC(feats[i], fx, fy, FS_SMALL, AlphaOf(COL_AI, (int)(180 * fp)));
    }

    // 開始提示
    float sp = 0.7f + 0.3f * sinf(t * 3.5f);
    DTC("按 ENTER 或 空白鍵 開始", cx, cy + 60, FS_LARGE, AlphaOf(COL_PERC, (int)(230 * sp)));
    DTC("[H] 查看說明",             cx, cy + 96, FS_MED,   AlphaOf(WHITE, 120));

    if (G.highScore > 0) {
        char hs[40];
        snprintf(hs, 40, "歷史最高分：%d", G.highScore);
        DTC(hs, cx, cy + 138, FS_MED, AlphaOf(COL_STAR, 200));
    }

    DTX("v3.0+AI  Powered by Raylib", 16, (float)(VIRT_H - 28), FS_TINY, AlphaOf(WHITE, 50));

    // 底部裝飾神經網路動畫
    static float nodeT = 0.f;
    nodeT += GetFrameTime();   // Bug fix: 原為 hardcoded 0.016f，與實際 FPS 脫鉤
    const int NN = 6;
    float     nx[NN], ny[NN];
    for (int i = 0; i < NN; i++) {
        nx[i] = (float)cx - 250 + i * 100.f;
        ny[i] = (float)(cy + 195) + sinf(nodeT * 1.4f + i) * 18.f;
    }
    for (int i = 0; i < NN - 1; i++) {
        float   sig = 0.5f + 0.5f * sinf(nodeT * 2.f + i * 0.9f);
        Color   lc  = COL_PERC;
        lc.a = (unsigned char)(60 + sig * 100);
        DrawLineEx({nx[i], ny[i]}, {nx[i+1], ny[i+1]}, 1.5f + sig * 2.f, lc);

        float   dot = fmodf(nodeT * 0.8f + i * 0.18f, 1.f);
        Vector2 dp  = { nx[i] + (nx[i+1] - nx[i]) * dot, ny[i] + (ny[i+1] - ny[i]) * dot };
        DrawCircleV(dp, 5.f, AlphaOf(COL_PERC, (int)(180 * sig)));
    }
    for (int i = 0; i < NN; i++) {
        float sig = 0.5f + 0.5f * sinf(nodeT * 1.6f + i);
        DrawHex({nx[i], ny[i]}, 14.f, AlphaOf(COL_PERC, (int)(40 * sig)), AlphaOf(COL_PERC, (int)(160 * sig)));
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawHelp  —  說明頁面
// ══════════════════════════════════════════════════════════════════
void DrawHelp() {
    DrawRectangle(PANEL_L + 80, TOPBAR_H + 40, MAP_W - 160, MAP_H - 80, Color{4, 9, 18, 240});
    DrawRectangleLinesEx(
        {(float)(PANEL_L + 80), (float)(TOPBAR_H + 40), (float)(MAP_W - 160), (float)(MAP_H - 80)},
        2.f, COL_SENSOR
    );

    int cx = PANEL_L + MAP_W / 2;
    int y  = TOPBAR_H + 70;
    DTC("── 操作說明 ──", cx, y, FS_LARGE, COL_CPU);
    y += 46;

    struct HelpRow { const char* key; const char* desc; Color col; };
    HelpRow rows[] = {
        {"點擊左側按鈕", "選擇元件（再按取消）",          WHITE     },
        {"點擊地圖空格", "放置元件",                       WHITE     },
        {"[C]",          "進入連線模式",                   COL_SENSOR},
        {"[U]",          "升級選取元件",                   COL_PERC  },
        {"[Q]",          "發動主動技能（選取元件後）",     COL_AI    },
        {"[DEL]",        "移除選取元件（退款50%）",        COL_CANNON},
        {"[空白/ENTER]", "發動下一波次",                   COL_PERC  },
        {"[T]",          "切換威脅熱圖",                   COL_AI    },
        {"[A]",          "切換 AI 顧問提示",               COL_AI    },
        {"[P]",          "暫停/繼續",                      WHITE     },
        {"[H]",          "說明",                           WHITE     },
        {"[F11]",        "全螢幕切換",                     WHITE     },
    };
    for (auto& r : rows) {
        float kw = MCN(r.key, FS_SMALL);
        int   kx = PANEL_L + 200;
        int   dx = kx + 80;
        DTX(r.key,  (float)(kx - (int)kw / 2 - 30), (float)y, FS_SMALL, AlphaOf(COL_AND, 220));
        DTX(r.desc, (float)dx,                        (float)y, FS_SMALL, AlphaOf(r.col,  190));
        y += 28;
    }

    y += 10;
    DTC("── AI 與關卡說明 ──", cx, y, FS_MED, COL_AI);
    y += 34;

    struct AIRow { const char* title; const char* desc; };
    AIRow aiRows[] = {
        {"感知器學習",  "每波結束以擊殺率為目標，對 w1/w2/bias 執行 Delta Rule 梯度更新"},
        {"感知器層數",  "感知器串接形成多層網路（L1/L2/L3），右側面板顯示 loss 折線圖"},
        {"威脅熱圖",    "擊殺位置累積熱度，跨波衰減 15%，Boss 貢獻 8x"},
        {"Boss 狀態機", "偵測前方高威脅→EVADE加速x2.0；血量<30%永久RAMPAGE x2.8"},
        {"Boss 分裂",   "Boss 降至 50% HP 時立即召喚 3 隻精英，務必保留火力！"},
        {"精英護盾",    "Wave 10+ 精英帶護盾（25% HP），護盾破碎前傷害全吸收"},
        {"波次事件",    "蜂群/裝甲洪流/霧戰/精英突擊/護衛Boss/再生軍/神速"},
        {"通訊中斷",    "感測器範圍縮減至 30%，必須依賴感知器網路辨識目標"},
        {"變異體",      "HP x2、速度 x1.3、無裝甲、帶再生，爆發傷害才能壓制"},
        {"圍城艦",      "單隻 HP x8、裝甲 0.8 的超重坦，需集中全火力穿甲"},
        {"路線輪換",    "每 3 波自動換路線（共 5 條），訓練期間顯示下波路線預告"},
        {"雙路進攻",    "Wave 8 起開通第二條路徑，敵人交替從兩路進攻"},
        {"主動技能",    "選取元件後按 [Q]：SENSOR=EMP暫停，OR=超頻，CANNON=超砲等"},
        {"NAND閘",      "非AND閘：後期反制裝甲利器，配合主動技能可破甲歸零"},
    };
    for (auto& r : aiRows) {
        DTX(r.title, (float)(PANEL_L + 100), (float)y, FS_SMALL, AlphaOf(COL_AI, 230)); y += 22;
        DTX(r.desc,  (float)(PANEL_L + 120), (float)y, FS_TINY,  AlphaOf(WHITE,  160)); y += 26;
    }

    DTC("點擊任意處或按 [H]/[ESC] 關閉", cx, TOPBAR_H + MAP_H - 60, FS_MED, AlphaOf(WHITE, 140));
}

// ══════════════════════════════════════════════════════════════════
//  DrawGameOver  —  遊戲結束畫面
// ══════════════════════════════════════════════════════════════════
void DrawGameOver(Game& G) {
    DrawRectangle(PANEL_L, TOPBAR_H, MAP_W, MAP_H, Color{5, 0, 0, 200});

    int   cx = PANEL_L + MAP_W / 2, cy = VIRT_H / 2;
    float t  = (float)GetTime();

    float rp = 0.75f + 0.25f * sinf(t * 4.f);
    DTC("防線崩潰", cx, cy - 110, FS_BIG, AlphaOf(RED, (int)(240 * rp)));

    char sbuf[64];
    snprintf(sbuf, 64, "最終分數：%d", G.score);
    DTC(sbuf, cx, cy - 38, FS_TITLE, COL_AND);

    if (G.score >= G.highScore && G.highScore > 0) {
        DTC("新紀錄！", cx, cy + 10, FS_LARGE, COL_STAR);
    } else {
        char hs[48];
        snprintf(hs, 48, "最高分：%d", G.highScore);
        DTC(hs, cx, cy + 10, FS_LARGE, AlphaOf(COL_STAR, 180));
    }

    char wb[48];
    snprintf(wb, 48, "撐過 %d 波次", G.wave);
    DTC(wb, cx, cy + 50, FS_LARGE, AlphaOf(WHITE, 180));

    // 感知器學習成果統計
    int   pctCount = 0;
    float avgLoss  = 0.f;
    for (auto& tw : G.towers) {
        if (tw.type == TType::PERCEPTRON) {
            pctCount++;
            avgLoss += tw.learner.lastLoss;
        }
    }
    if (pctCount > 0) {
        avgLoss /= pctCount;
        char aib[64];
        snprintf(aib, 64, "感知器 %d 個 | 平均 loss %.4f", pctCount, avgLoss);
        DTC(aib, cx, cy + 86, FS_MED, AlphaOf(COL_AI, 200));

        const char* grade =
            (avgLoss < 0.05f) ? "神經網路已收斂"     :
            (avgLoss < 0.15f) ? "學習中，再多幾波"   :
                                "需要更多感知器或連線";
        DTC(grade, cx, cy + 114, FS_SMALL, AlphaOf(COL_AI, 170));
    }

    float pp = 0.8f + 0.2f * sinf(t * 4.f);
    DTC("[R] 重新開始    [ENTER] 主選單", cx, cy + 152, FS_LARGE,
        AlphaOf(Color{210, 210, 210, 255}, (int)(210 * pp)));
}

// ══════════════════════════════════════════════════════════════════
//  HandleInput  —  鍵盤 + 滑鼠輸入處理
// ══════════════════════════════════════════════════════════════════
void HandleInput(Game& G) {
    Vector2 mp  = VirtualMouse();
    int     hgx = -1, hgy = -1;
    bool    onMap = ScreenToGrid(G, mp, hgx, hgy);

    // ── 主選單輸入 ───────────────────────────────────────────────
    if (G.phase == Game::MENU) {
        if (G.showHelp) {
            if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_ESCAPE) ||
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                G.showHelp = false;
            }
            return;
        }
        if (IsKeyPressed(KEY_H))                                 G.showHelp = true;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) G.Reset();
        return;
    }

    // ── 說明頁面開關 ─────────────────────────────────────────────
    if (IsKeyPressed(KEY_H)) {
        G.showHelp = !G.showHelp;
        if (G.showHelp && G.phase == Game::FIGHT) G.paused = true;
        else if (!G.showHelp)                     G.paused = false;
        return;
    }
    if (G.showHelp) {
        if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            G.showHelp = false;
            G.paused   = false;
        }
        return;
    }

    // ── 全域快捷鍵 ───────────────────────────────────────────────
    if (IsKeyPressed(KEY_P) && G.phase != Game::GAMEOVER) { G.paused = !G.paused; return; }
    if (IsKeyPressed(KEY_R))                               { G.Reset(); return; }
    if (IsKeyPressed(KEY_T)) {
        G.showThreat = !G.showThreat;
        G.SetMsg(G.showThreat ? "熱圖：開啟" : "熱圖：關閉");
        return;
    }
    if (IsKeyPressed(KEY_A)) {
        G.showAIHints = !G.showAIHints;
        G.SetMsg(G.showAIHints ? "AI 提示：開啟" : "AI 提示：關閉");
        return;
    }
    if (G.phase == Game::GAMEOVER) {
        if (IsKeyPressed(KEY_ENTER)) G.phase = Game::MENU;
        return;
    }
    if (G.paused) return;

    // ── 技能觸發 [Q] ─────────────────────────────────────────────
    if (IsKeyPressed(KEY_Q) && G.selectedId >= 0) {
        Tower* st = G.FindTower(G.selectedId);
        if (st && st->type != TType::CPU) {
            if (st->activeCd > 0.f) {
                char b2[48];
                snprintf(b2, 48, "技能冷卻中...%.0f 秒", st->activeCd);
                G.SetMsg(b2);
            } else {
                ActivateSkill(G, *st);
                char b2[48];
                snprintf(b2, 48, "[%s] %s！", TDef(st->type).sym, SKILL_NAME[(int)st->type]);
                G.SetMsg(b2);
            }
        }
    }

    // ── 升級 [U] ─────────────────────────────────────────────────
    if (IsKeyPressed(KEY_U) && G.selectedId >= 0) {
        Tower* st = G.FindTower(G.selectedId);
        if (st) G.UpgradeTower(*st);
    }

    // ── 左側面板按鈕（滑鼠點擊）────────────────────────────────
    static TType ORDER[] = {
        TType::SENSOR, TType::PERCEPTRON, TType::AND, TType::OR,
        TType::XOR,    TType::NAND,       TType::CANNON
    };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && mp.x < PANEL_L) {
        constexpr int BTN_H_SM  = 56;
        constexpr int BTN_GAP_SM = 3;

        for (int i = 0; i < 7; i++) {
            int by = G.btnY0 + i * (BTN_H_SM + BTN_GAP_SM);
            if (mp.y >= by && mp.y < by + BTN_H_SM) {
                TType tt  = ORDER[i];
                G.placing = (G.placing == tt) ? TType::NONE : tt;
                G.selectedId  = -1;
                G.connectSrc  = -1;
                return;
            }
        }
        if (mp.y >= G.waveBtnY && mp.y < G.waveBtnY + 58 && G.phase == Game::BUILD) {
            StartWave(G);
            return;
        }
    }

    // ── 地圖點擊（放塔 / 連線 / 選取）──────────────────────────
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && onMap) {

        // 連線模式：點擊目標
        if (G.connectSrc >= 0) {
            Tower* dst = G.TowerAt(hgx, hgy);
            if (dst && dst->id != G.connectSrc) {
                Tower* src = G.FindTower(G.connectSrc);
                if (src) {
                    auto it = std::find(src->conns.begin(), src->conns.end(), dst->id);
                    if (it != src->conns.end()) {
                        src->conns.erase(it);
                        G.SetMsg("連線已取消！");
                    } else {
                        std::string err = G.ValidateConnect(G.connectSrc, dst->id);
                        if (err.empty()) {
                            src->conns.push_back(dst->id);
                            G.SetMsg("連接成功！");
                        } else {
                            G.SetMsg("[錯誤] " + err);
                        }
                    }
                }
            }
            G.connectSrc = -1;
            return;
        }

        // 點到已有塔 → 選取
        Tower* ex = G.TowerAt(hgx, hgy);
        if (ex) {
            G.selectedId = ex->id;
            G.placing    = TType::NONE;
            return;
        }

        // 放置新塔
        if (G.placing != TType::NONE) {
            if (G.IsPath(hgx, hgy)) {
                G.SetMsg("[錯誤] 不能放在路徑上！");
            } else if (G.credits >= TDef(G.placing).baseCost) {
                Tower nt;
                nt.id       = G.nextId++;
                nt.type     = G.placing;
                nt.gx       = hgx;
                nt.gy       = hgy;
                nt.level    = 1;
                nt.sig      = 0.f;
                nt.w1       = 0.8f;
                nt.w2       = 0.6f;
                nt.bias     = -0.5f;
                nt.cooldown = 0;
                nt.anim     = 0.f;
                G.ApplyTowerStats(nt);
                G.towers.push_back(nt);
                G.credits -= TDef(G.placing).baseCost;

                if (G.placing == TType::PERCEPTRON) {
                    G.SetMsg("感知器已放置，每波結束會自動學習！");
                } else {
                    char b2[48];
                    snprintf(b2, 48, "已放置 %s", TDef(G.placing).label);
                    G.SetMsg(b2);
                }
            } else {
                G.SetMsg("金幣不足！");
            }
            return;
        }

        // 點到空地（且無選取模式）→ 取消選取
        G.selectedId = -1;
    }

    // ── 其他快捷鍵 ───────────────────────────────────────────────
    if (IsKeyPressed(KEY_SPACE) && G.phase == Game::BUILD)  StartWave(G);
    if (IsKeyPressed(KEY_ESCAPE)) { G.placing = TType::NONE; G.connectSrc = -1; }

    // ── 連線模式 [C] ─────────────────────────────────────────────
    if (IsKeyPressed(KEY_C) && G.selectedId >= 0) {
        Tower* st = G.FindTower(G.selectedId);
        if (!st) {
            // 找不到塔，忽略
        } else if (st->type == TType::CANNON) {
            G.SetMsg("[錯誤] 砲塔是終端節點，不能輸出");
        } else if (st->type == TType::CPU) {
            G.SetMsg("[錯誤] CPU不能連接");
        } else if ((int)st->conns.size() >= Game::MAX_CONNS) {
            G.SetMsg("[錯誤] 已達8條上限");
        } else {
            G.connectSrc = G.selectedId;
            G.SetMsg("點擊目標建立連線 | 點已連接目標取消 | [ESC]取消");
        }
    }

    // ── 移除塔 [DEL] ─────────────────────────────────────────────
    if (IsKeyPressed(KEY_DELETE) && G.selectedId >= 0) {
        Tower* st = G.FindTower(G.selectedId);
        if (st && st->type != TType::CPU) {
            int rid    = G.selectedId;
            int refund = TDef(st->type).baseCost / 2;
            G.credits += refund;

            G.towers.erase(
                std::remove_if(G.towers.begin(), G.towers.end(),
                    [rid](const Tower& t) { return t.id == rid; }),
                G.towers.end()
            );
            for (auto& t : G.towers) {
                t.conns.erase(
                    std::remove(t.conns.begin(), t.conns.end(), rid),
                    t.conns.end()
                );
            }

            G.selectedId = -1;
            char b3[48];
            snprintf(b3, 48, "已移除，退款+%d CR", refund);
            G.SetMsg(b3);
        }
    }
}

// ══════════════════════════════════════════════════════════════════
//  main  —  程式入口
// ══════════════════════════════════════════════════════════════════
int main() {
    const int WIN_W = 1280, WIN_H = 720;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(WIN_W, WIN_H, "邏輯閘防禦戰 v3.0 + AI Edition");
    SetExitKey(KEY_NULL);

    int mon = GetCurrentMonitor();
    int mw  = GetMonitorWidth(mon);
    int mh  = GetMonitorHeight(mon);
    SetWindowPosition((mw - WIN_W) / 2, (mh - WIN_H) / 2);

    LoadChineseFont();
    BuildPath(0);

    // 無邊框全螢幕切換狀態
    bool borderlessFS = false;
    int  savedW = WIN_W, savedH = WIN_H;
    int  savedX = (mw - WIN_W) / 2, savedY = (mh - WIN_H) / 2;

    RenderTexture2D target = LoadRenderTexture(VIRT_W, VIRT_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    SetTargetFPS(60);

    Game G;
    G.InitStars();
    G.phase = Game::MENU;

    while (!WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.05f);

        // ── F11 全螢幕切換 ────────────────────────────────────────
        if (IsKeyPressed(KEY_F11)) {
            if (!borderlessFS) {
                savedW = GetScreenWidth();
                savedH = GetScreenHeight();
                savedX = (int)GetWindowPosition().x;
                savedY = (int)GetWindowPosition().y;
                mon    = GetCurrentMonitor();
                mw     = GetMonitorWidth(mon);
                mh     = GetMonitorHeight(mon);
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

        // ── 計算縮放比例（保持長寬比置中）───────────────────────
        int   wW = GetScreenWidth(), wH = GetScreenHeight();
        float sx = (float)wW / VIRT_W;
        float sy = (float)wH / VIRT_H;
        gScale = (sx < sy) ? sx : sy;
        gOffX  = (wW - VIRT_W * gScale) * 0.5f;
        gOffY  = (wH - VIRT_H * gScale) * 0.5f;

        HandleInput(G);
        Update(G, dt);

        // ── 螢幕震動計算 ─────────────────────────────────────────
        Vector2 vmp = VirtualMouse();
        int     hgx = -1, hgy = -1;
        ScreenToGrid(G, vmp, hgx, hgy);

        float shakeX = 0.f, shakeY = 0.f;
        if (G.shakeT > 0) {
            float sp = G.shakePow * (G.shakeT * 3.f);
            std::uniform_real_distribution<float> sd(-sp, sp);
            shakeX = sd(G.rng);
            shakeY = sd(G.rng);
        }

        // ── 渲染到虛擬材質 ───────────────────────────────────────
        BeginTextureMode(target);
        ClearBackground(BG);

        if (G.phase == Game::MENU) {
            DrawMenu(G);
            if (G.showHelp) DrawHelp();
        } else {
            DrawStars(G);
            gShkX = shakeX;
            gShkY = shakeY;

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

            gShkX = 0.f;
            gShkY = 0.f;

            DrawLeftPanel(G);
            DrawRightPanel(G);
            DrawTopBar(G);
            DrawBotBar(G);

            if (G.phase == Game::GAMEOVER) DrawGameOver(G);
            if (G.showHelp)                DrawHelp();
        }
        EndTextureMode();

        // ── 縮放輸出到實體視窗 ───────────────────────────────────
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(
            target.texture,
            {0.f, 0.f, (float)VIRT_W, -(float)VIRT_H},   // 垂直翻轉（OpenGL 紋理座標）
            {gOffX, gOffY, VIRT_W * gScale, VIRT_H * gScale},
            {0.f, 0.f}, 0.f, WHITE
        );
        EndDrawing();
    }

    UnloadRenderTexture(target);
    if (gFontLoaded) UnloadFont(gFont);
    CloseWindow();
    return 0;
}