#pragma once
// ================================================================
//  game.h — Game 結構體完整宣告
// ================================================================
#include "structs.h"
#include "path.h"
#include "font.h"
#include "globals.h"
#include <random>
#include <string>
#include <functional>
#include <algorithm>
#include <array>
#include <sstream>

// ══════════════════════════════════════════════════════════════════
//  Game
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
    EnemyIntel          intel;
    DefenseAdvisorNN    defNN;

    // ── 波次突發事件 ───────────────────────────────────────────────
    enum class WaveEvent {
        NONE, SWARM, ARMORED, STEALTH, ELITE_RUSH,
        BOSS_ESCORT, REGEN_ARMY, DOUBLE_SPD, BLACKOUT, MUTANT, SIEGE
    };

    WaveEvent   currentEvent{WaveEvent::NONE};
    std::string eventName;
    float       eventBannerTimer{0.f};

    static WaveEvent  RollEvent(int wave, std::mt19937& rng);
    static const char* EventName(WaveEvent e);

    // ── 遊戲狀態旗標 ───────────────────────────────────────────────
    bool  showThreat{false};
    bool  showAIHints{true};
    bool  dualPath{false};
    bool  blackoutActive{false};
    std::array<int, 2> nextPreviewPaths{{1, -1}};
    int   nextPreviewLaneCount{1};
    bool  hasPlannedRouteChange{false};

    float buffOverfreq{0.f};
    float buffArmorBreak{0.f};
    float buffGlobalMark{0.f};

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
    float comboSurgeTimer{0.f};
    constexpr static float COMBO_SURGE_TIME = 6.f;
    constexpr static int   COMBO_SURGE_THRESHOLD = 6;

    float waveTelegraphTimer{0.f};
    float spawnPulseTimer{0.f};
    int   spawnPulsePath{0};

    enum class TrainingRewardKind { CREDITS, PATCH, UPGRADE };
    struct TrainingReward {
        TrainingRewardKind kind{TrainingRewardKind::CREDITS};
        std::string        title;
        std::string        desc;
        Color              col{WHITE};
        int                valueA{0};
        int                valueB{0};
    };

    std::array<TrainingReward, 3> trainingChoices{};
    int                           trainingChoiceCount{0};

    float shakeT{0.f};
    float shakePow{0.f};

    // ── 座標轉換（inline，高頻呼叫）─────────────────────────────
    Vector2 MapOrigin() {
        return { (float)PANEL_L + gShkX, (float)TOPBAR_H + gShkY };
    }

    Vector2 CC(int gx, int gy) {
        auto o = MapOrigin();
        return { o.x + (gx + 0.5f) * CELL, o.y + (gy + 0.5f) * CELL };
    }
    Vector2 CC(float gx, float gy) {
        auto o = MapOrigin();
        return { o.x + (gx + 0.5f) * CELL, o.y + (gy + 0.5f) * CELL };
    }

    // ── 查找工具 ─────────────────────────────────────────────────
    Tower* FindTower(int id) {
        for (auto& t : towers) if (t.id == id) return &t;
        return nullptr;
    }
    Tower* TowerAt(int gx, int gy) {
        for (auto& t : towers) if (t.gx == gx && t.gy == gy) return &t;
        return nullptr;
    }
    Enemy* FindEnemy(int id) {
        for (auto& e : enemies) if (e.id == id) return &e;
        return nullptr;
    }

    bool IsPath(int gx, int gy) {
        return IsAnyActivePathCell(gx, gy, dualPath ? 2 : 1);
    }

    bool IsLaneActive(int laneSlot) const {
        return laneSlot == 0 || (laneSlot == 1 && dualPath);
    }

    int ActiveLaneCount() const {
        return dualPath ? 2 : 1;
    }

    int ResolveLaneSlot(int laneSlot) const {
        return (laneSlot == 1 && dualPath) ? 1 : 0;
    }

    const PathPreset& LanePreset(int laneSlot) const {
        return GetActiveLanePreset(ResolveLaneSlot(laneSlot));
    }

    int LanePresetIdx(int laneSlot) const {
        return GetActiveLanePresetIdx(ResolveLaneSlot(laneSlot));
    }

    const std::vector<PathCell>& LaneCells(int laneSlot) const {
        return GetActiveLaneCells(ResolveLaneSlot(laneSlot));
    }

    bool IsLanePathCell(int laneSlot, int gx, int gy) const {
        return IsLaneActive(laneSlot) && IsActiveLaneCell(ResolveLaneSlot(laneSlot), gx, gy);
    }

    const std::vector<PathCell>& EnemyLaneCells(const Enemy& e) const {
        return LaneCells(e.pathIdx);
    }

    Vector2 EnemyWorld(const Enemy& e) {
        const auto& cells = EnemyLaneCells(e);
        int   i = (int)e.pathPos;
        float f = e.pathPos - i;
        if (i >= (int)cells.size() - 1) return CC(CPU_GX, CPU_GY);
        if (i < 0) return CC(cells[0].gx, cells[0].gy);
        Vector2 a = CC(cells[i].gx,   cells[i].gy);
        Vector2 b = CC(cells[i+1].gx, cells[i+1].gy);
        return { a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f };
    }

    Vector2 EnemyGrid(const Enemy& e) {
        const auto& cells = EnemyLaneCells(e);
        int   i = (int)e.pathPos;
        float f = e.pathPos - i;
        if (i >= (int)cells.size() - 1) return { (float)CPU_GX, (float)CPU_GY };
        if (i < 0) return { (float)cells[0].gx, (float)cells[0].gy };
        float ax = cells[i].gx,   ay = cells[i].gy;
        float bx = cells[i+1].gx, by = cells[i+1].gy;
        return { ax + (bx - ax) * f, ay + (by - ay) * f };
    }

    // ── 連線驗證 ─────────────────────────────────────────────────
    static constexpr int MAX_CONNS = 8;
    bool WouldCycle(int src, int dst);
    std::string ValidateConnect(int src, int dst);

    // ── 塔的升級與屬性 ───────────────────────────────────────────
    void ApplyTowerStats(Tower& t);
    bool UpgradeTower(Tower& t);

    // ── 特效工具 ─────────────────────────────────────────────────
    void SpawnParticles(Vector2 pos, Color col, int n = 10, float spd = 70.f);
    void AddFloat(Vector2 pos, const std::string& txt, Color col);
    void SetMsg(const std::string& m);
    void Shake(float p = 8.f, float t = 0.35f);

    // ── 初始化 ───────────────────────────────────────────────────
    void InitStars();
    void GenerateAIHints();
    void TrainDefenseNN();
    void TrainPerceptrons();
    void Reset();
};

// 全域別名（方便 logic.cpp 等使用）
using WaveEvent = Game::WaveEvent;

// 格子座標工具（非 Game 成員）
bool ScreenToGrid(Game& G, Vector2 sp, int& gx, int& gy);
