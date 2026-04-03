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
//  GenerateAIHints  —  防禦建議 AI（讀 EnemyIntel + ThreatMap）
// ══════════════════════════════════════════════════════════════════
void Game::GenerateAIHints() {
    aiHints.clear();

    // ── 計算各路段覆蓋率（供 defNN 輸入）────────────────────────
    float cov[3] = { 0.f, 0.f, 0.f };
    int   n      = (int)PATH_CELLS.size();
    for (int seg = 0; seg < 3; seg++) {
        int from = n * seg / 3, to = n * (seg + 1) / 3;
        if (from >= to) continue;
        int covered = 0;
        for (int pi = from; pi < to; pi++) {
            for (auto& t : towers) {
                if (Dist({(float)t.gx,(float)t.gy},
                         {(float)PATH_CELLS[pi].gx,(float)PATH_CELLS[pi].gy}) <= t.range) {
                    covered++; break;
                }
            }
        }
        cov[seg] = (float)covered / (to - from);
    }

    // ── 讓 DefenseAdvisorNN 推薦塔種 ────────────────────────────
    float in[6] = {
        intel.typeSurvRate[2],   // 裝甲兵存活率
        intel.typeSurvRate[1],   // 快速兵存活率
        intel.typeSurvRate[3],   // 精英兵存活率
        cov[0], cov[1], cov[2]
    };
    static const TType NN_TO_TTYPE[] = {
        TType::SENSOR, TType::AND, TType::XOR, TType::CANNON
    };
    static const char* NN_REASON[] = {
        "NN建議：覆蓋率不足，補感測器",
        "NN建議：裝甲兵突破率高，補AND閘",
        "NN建議：快速兵突破率高，補XOR閘",
        "NN建議：後段火力不足，補砲塔"
    };
    int   nnClass   = defNN.Recommend(in);
    TType nnSuggest = NN_TO_TTYPE[nnClass];

    // ── 輔助：找最薄弱路段中心的可放格 ─────────────────────────
    auto findSlot = [&](int px, int py, int& outX, int& outY) -> bool {
        for (int r = 1; r <= 3; r++) {
            for (int dx = -r; dx <= r; dx++) {
                for (int dy = -r; dy <= r; dy++) {
                    if (abs(dx) != r && abs(dy) != r) continue;
                    int hx = px+dx, hy = py+dy;
                    if (hx<0||hx>=COLS||hy<0||hy>=ROWS) continue;
                    if (IS_PATH[hx][hy]||IS_PATH2[hx][hy]) continue;
                    if (TowerAt(hx, hy)) continue;
                    outX = hx; outY = hy; return true;
                }
            }
        }
        return false;
    };

    // ── 提示 1：NN 推薦放置（覆蓋最弱的路段）───────────────────
    {
        int worstSeg = 0;
        for (int i = 1; i < 3; i++) if (cov[i] < cov[worstSeg]) worstSeg = i;
        int midIdx = n * (worstSeg * 2 + 1) / 6;
        midIdx = std::max(0, std::min(midIdx, n - 1));
        int sx = -1, sy = -1;
        if (findSlot(PATH_CELLS[midIdx].gx, PATH_CELLS[midIdx].gy, sx, sy)) {
            char buf[80];
            snprintf(buf, 80, "%s（信心%.0f%%）",
                NN_REASON[nnClass], defNN.lastProb[nnClass] * 100.f);
            AIHint h;
            h.gx = sx; h.gy = sy; h.suggest = nnSuggest;
            h.score = defNN.lastProb[nnClass]; h.reason = buf;
            aiHints.push_back(h);
        }
    }

    // ── 提示 2：威脅熱點補強 ────────────────────────────────────
    if ((int)aiHints.size() < 3) {
        float maxT = threatMap.GetMax();
        if (maxT > 0.5f) {
            int bx = -1, by = -1; float best = 0.f;
            for (int x = 0; x < COLS; x++) {
                for (int y = 0; y < ROWS; y++) {
                    if (!IS_PATH[x][y] && !IS_PATH2[x][y]) continue;
                    float th = threatMap.Get(x,y) / maxT;
                    if (th < 0.4f) continue;
                    float nearCov = 0.f;
                    for (auto& t : towers)
                        if (Dist({(float)t.gx,(float)t.gy},{(float)x,(float)y}) <= 5.f)
                            nearCov += 1.f;
                    float score = th - nearCov * 0.15f;
                    if (score > best) { best = score; bx = x; by = y; }
                }
            }
            if (bx >= 0) {
                int sx = -1, sy = -1;
                if (findSlot(bx, by, sx, sy)) {
                    AIHint h;
                    h.gx = sx; h.gy = sy;
                    h.suggest = TType::CANNON;
                    h.score   = best;
                    h.reason  = "熱點防線薄弱，建議砲塔";
                    aiHints.push_back(h);
                }
            }
        }
    }

    // ── 提示 3：網路結構缺口 ────────────────────────────────────
    if ((int)aiHints.size() < 3) {
        int nSensor=0, nCannon=0, nPct=0;
        for (auto& t : towers) {
            if (t.type==TType::SENSOR)     nSensor++;
            if (t.type==TType::CANNON)     nCannon++;
            if (t.type==TType::PERCEPTRON) nPct++;
        }
        const char* reason = nullptr;
        TType suggest = TType::SENSOR;
        int tgx = -1, tgy = -1;

        if (nCannon > 0 && nSensor == 0) {
            reason = "砲塔沒有感測器！訊號無法傳遞";
            suggest = TType::SENSOR;
            int mid = n / 4;
            tgx = PATH_CELLS[mid].gx; tgy = PATH_CELLS[mid].gy;
        } else if (nCannon >= 1 && nPct == 0 && wave >= 2) {
            reason = "加入感知器讓AI自動學習射擊時機";
            suggest = TType::PERCEPTRON;
            for (auto& t : towers)
                if (t.type==TType::CANNON) { tgx=t.gx; tgy=t.gy; break; }
        } else if (dualPath && intel.pathSurvRate[1] > 0.5f) {
            reason = "副路突破率高！副路需要砲塔";
            suggest = TType::CANNON;
            if (!PATH_CELLS2.empty()) {
                int mid2 = (int)PATH_CELLS2.size() / 2;
                tgx = PATH_CELLS2[mid2].gx; tgy = PATH_CELLS2[mid2].gy;
            }
        }

        if (reason && tgx >= 0) {
            int sx = -1, sy = -1;
            if (findSlot(tgx, tgy, sx, sy)) {
                AIHint h;
                h.gx = sx; h.gy = sy; h.suggest = suggest;
                h.score = 0.6f; h.reason = reason;
                aiHints.push_back(h);
            }
        }
    }
}

void Game::TrainPerceptrons() {
    // ── 訓練目標：下游砲塔射程內有敵人的幀佔比 ──────────────────
    //    這讓感知器學會「當敵人逼近砲塔時輸出高訊號」
    //    而不是無意義的波次擊殺率
    for (auto& t : towers) {
        if (t.type != TType::PERCEPTRON) continue;

        float target = (t.pctTargetSamples > 0)
                       ? t.pctTargetAcc / t.pctTargetSamples
                       : 0.5f;

        t.FlushSample();
        t.learner.Update(target, t.w1, t.w2, t.bias);

        t.lossHistory.push_back(t.learner.lastLoss);
        if ((int)t.lossHistory.size() > 14)
            t.lossHistory.erase(t.lossHistory.begin());

        // 重置累積器
        t.pctTargetAcc     = 0.f;
        t.pctTargetSamples = 0;

        char buf[80];
        snprintf(buf, 80, "PCT[%d] target=%.2f loss=%.3f", t.id, target, t.learner.lastLoss);
        AddFloat(CC(t.gx, t.gy), buf, COL_AI);
    }
}

// ══════════════════════════════════════════════════════════════════
//  TrainDefenseNN  —  訓練防禦建議神經網路
//
//  以當前敵情（兵種存活率）和路線覆蓋率為輸入，
//  以「最危險的兵種對應的剋制塔」為訓練目標，
//  執行一步反向傳播更新 DefenseAdvisorNN 的權重。
// ══════════════════════════════════════════════════════════════════
void Game::TrainDefenseNN() {
    // ── 計算各路段覆蓋率 ─────────────────────────────────────────
    float cov[3] = { 0.f, 0.f, 0.f };
    int   n      = (int)PATH_CELLS.size();
    for (int seg = 0; seg < 3; seg++) {
        int from = n * seg / 3, to = n * (seg + 1) / 3;
        if (from >= to) continue;
        int covered = 0;
        for (int pi = from; pi < to; pi++) {
            for (auto& t : towers) {
                if (Dist({(float)t.gx,(float)t.gy},
                         {(float)PATH_CELLS[pi].gx,(float)PATH_CELLS[pi].gy}) <= t.range) {
                    covered++; break;
                }
            }
        }
        cov[seg] = (float)covered / (to - from);
    }

    float in[6] = {
        intel.typeSurvRate[2],   // 裝甲兵存活率
        intel.typeSurvRate[1],   // 快速兵存活率
        intel.typeSurvRate[3],   // 精英兵存活率
        cov[0], cov[1], cov[2]
    };

    // ── 決定訓練目標（最危險的兵種→對應剋制塔）─────────────────
    //    0=SENSOR  1=AND  2=XOR  3=CANNON
    static const int COUNTER[] = { 3, 2, 1, 3 };
    //  basic → CANNON(3), fast → XOR(2), armor → AND(1), elite → CANNON(3)

    int   targetClass = 3;
    float maxSurv     = -1.f;
    for (int i = 0; i < 4; i++) {
        if (intel.typeSurvRate[i] > maxSurv) {
            maxSurv = intel.typeSurvRate[i];
            targetClass = COUNTER[i];
        }
    }

    // 覆蓋率低於 30% 時優先推薦感測器（偵測先於射擊）
    for (int seg = 0; seg < 3; seg++) {
        if (cov[seg] < 0.30f) { targetClass = 0; break; }
    }

    defNN.Learn(in, targetClass);
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
    intel = EnemyIntel{};
    defNN = DefenseAdvisorNN{};

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