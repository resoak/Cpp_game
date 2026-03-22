// ================================================================
//  game.cpp — Game 方法實作
// ================================================================
#include "game.h"
#include <algorithm>
#include <random>
#include <cstdio>
#include <cstring>

// ══════════════════════════════════════════════════════════════════
//  WaveEvent 輔助
// ══════════════════════════════════════════════════════════════════
Game::WaveEvent Game::RollEvent(int wave, std::mt19937& rng) {
    if (wave < 3)      return WaveEvent::NONE;
    if (wave % 5 == 0) return WaveEvent::BOSS_ESCORT;

    std::uniform_int_distribution<int> d(0, 10);
    int r = d(rng);
    if (wave < 6) r %= 3;

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

const char* Game::EventName(WaveEvent e) {
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

// ══════════════════════════════════════════════════════════════════
//  ScreenToGrid
// ══════════════════════════════════════════════════════════════════
bool ScreenToGrid(Game& G, Vector2 sp, int& gx, int& gy) {
    auto o = G.MapOrigin();
    int x = (int)((sp.x - o.x) / CELL);
    int y = (int)((sp.y - o.y) / CELL);
    if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return false;
    gx = x;
    gy = y;
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  連線驗證
// ══════════════════════════════════════════════════════════════════
bool Game::WouldCycle(int src, int dst) {
    std::vector<int> stk = {dst};
    std::vector<int> vis;
    while (!stk.empty()) {
        int c = stk.back(); stk.pop_back();
        if (c == src) return true;
        if (std::find(vis.begin(), vis.end(), c) != vis.end()) continue;
        vis.push_back(c);
        Tower* t = FindTower(c);
        if (t) for (int x : t->conns) stk.push_back(x);
    }
    return false;
}

std::string Game::ValidateConnect(int src, int dst) {
    Tower* s = FindTower(src);
    Tower* d = FindTower(dst);
    if (!s || !d)                            return "元件不存在";
    if (src == dst)                          return "不能連接自己";
    if (s->type == TType::CANNON)            return "砲塔不能輸出";
    if (s->type == TType::CPU)               return "CPU不能連接";
    if (d->type == TType::SENSOR)            return "感測器不能被連入";
    if (d->type == TType::CPU)               return "CPU不能連入";
    if ((int)s->conns.size() >= MAX_CONNS)   return "已達上限(8條)";
    if (WouldCycle(src, dst))                return "不能形成迴圈";
    return "";
}

// ══════════════════════════════════════════════════════════════════
//  塔屬性 & 升級
// ══════════════════════════════════════════════════════════════════
void Game::ApplyTowerStats(Tower& t) {
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

bool Game::UpgradeTower(Tower& t) {
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

// ══════════════════════════════════════════════════════════════════
//  特效工具
// ══════════════════════════════════════════════════════════════════
void Game::SpawnParticles(Vector2 pos, Color col, int n, float spd) {
    std::uniform_real_distribution<float> ang(0.f, 6.28f);
    std::uniform_real_distribution<float> sp(spd * 0.3f, spd);
    for (int i = 0; i < n; i++) {
        float a = ang(rng);
        float s = sp(rng);
        particles.push_back({ pos, {cosf(a) * s, sinf(a) * s}, 0.7f, 0.7f, 5.f, col });
    }
}

void Game::AddFloat(Vector2 pos, const std::string& txt, Color col) {
    floats.push_back({ pos, txt, col, 2.f });
}

void Game::SetMsg(const std::string& m) {
    msg      = m;
    msgTimer = 5.f;
}

void Game::Shake(float p, float t) {
    shakeT   = t;
    shakePow = p;
}

// ══════════════════════════════════════════════════════════════════
//  InitStars
// ══════════════════════════════════════════════════════════════════
void Game::InitStars() {
    stars.clear();
    std::uniform_real_distribution<float> rw(0.f, (float)VIRT_W);
    std::uniform_real_distribution<float> rh(0.f, (float)VIRT_H);
    std::uniform_real_distribution<float> rr(0.8f, 2.f);
    std::uniform_real_distribution<float> rb(0.3f, 1.f);
    for (int i = 0; i < 200; i++) {
        stars.push_back({ rw(rng), rh(rng), rr(rng), rb(rng) });
    }
}

// ══════════════════════════════════════════════════════════════════
//  GenerateAIHints
// ══════════════════════════════════════════════════════════════════
void Game::GenerateAIHints() {
    aiHints.clear();

    for (auto& pc : PATH_CELLS) {
        float minDist = 999.f;
        for (auto& t : towers) {
            float d = Dist({(float)t.gx, (float)t.gy}, {(float)pc.gx, (float)pc.gy});
            if (d < minDist) minDist = d;
        }
        if (minDist < 4.5f) continue;

        int pathIdx = 0;
        for (int pi = 0; pi < (int)PATH_CELLS.size(); pi++) {
            if (PATH_CELLS[pi].gx == pc.gx && PATH_CELLS[pi].gy == pc.gy) {
                pathIdx = pi;
                break;
            }
        }
        float progress = (float)pathIdx / PATH_CELLS.size();
        float score    = (minDist - 4.5f) / 8.f;

        TType       bestSuggest;
        std::string reason;
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

        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                int hx = pc.gx + dx, hy = pc.gy + dy;
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

// ══════════════════════════════════════════════════════════════════
//  TrainPerceptrons
// ══════════════════════════════════════════════════════════════════
void Game::TrainPerceptrons() {
    if (waveKills + waveEscaped == 0) return;

    float killRate = (float)waveKills / (waveKills + waveEscaped);
    for (auto& t : towers) {
        if (t.type != TType::PERCEPTRON) continue;

        t.FlushSample();
        t.learner.Update(killRate, t.w1, t.w2, t.bias);

        t.lossHistory.push_back(t.learner.lastLoss);
        if ((int)t.lossHistory.size() > 14)
            t.lossHistory.erase(t.lossHistory.begin());

        char buf[80];
        snprintf(buf, 80, "PCT[%d] loss=%.3f", t.id, t.learner.lastLoss);
        AddFloat(CC(t.gx, t.gy), buf, COL_AI);
    }
}

// ══════════════════════════════════════════════════════════════════
//  Reset
// ══════════════════════════════════════════════════════════════════
void Game::Reset() {
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

    spawned       = 0;
    spawnTimer    = 0.f;
    waveCount     = 0;
    trainingTimer = 0.f;
    combo         = 0;
    comboTimer    = 0.f;
    shakeT        = 0.f;

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
