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

Game::Incident Game::RollIncident(int wave, std::mt19937& rng) {
    if (wave < 3) return Incident::NONE;
    std::uniform_int_distribution<int> d(0, 99);
    int r = d(rng);
    if (r < 34) return Incident::SIGNAL_STORM;
    if (r < 68) return Incident::ROUTE_SURGE;
    return Incident::BOUNTY_WINDOW;
}

const char* Game::IncidentName(Incident i) {
    switch (i) {
        case Incident::SIGNAL_STORM:  return "突發：信號風暴！";
        case Incident::ROUTE_SURGE:   return "突發：路徑暴走！";
        case Incident::BOUNTY_WINDOW: return "突發：漏洞賞金！";
        default:                      return "";
    }
}

static float SegmentCoverage(const Game& G, const std::vector<PathCell>& cells, int seg) {
    int n = (int)cells.size();
    if (n <= 0) return 0.f;

    int from = n * seg / 3;
    int to   = n * (seg + 1) / 3;
    if (from >= to) return 0.f;

    int covered = 0;
    for (int pi = from; pi < to; pi++) {
        for (auto& t : G.towers) {
            if (Dist({(float)t.gx, (float)t.gy},
                     {(float)cells[pi].gx, (float)cells[pi].gy}) <= t.range) {
                covered++;
                break;
            }
        }
    }
    return (float)covered / (to - from);
}

static void ComputeCoverageSegments(const Game& G, float cov[3]) {
    for (int seg = 0; seg < 3; seg++) cov[seg] = 0.f;

    int laneCount = G.ActiveLaneCount();
    for (int lane = 0; lane < laneCount; lane++) {
        const auto& cells = G.LaneCells(lane);
        for (int seg = 0; seg < 3; seg++) {
            cov[seg] += SegmentCoverage(G, cells, seg);
        }
    }

    for (int seg = 0; seg < 3; seg++) cov[seg] /= (float)laneCount;
}

static bool FindNearbyBuildSlot(Game& G, int px, int py, int& outX, int& outY) {
    for (int r = 1; r <= 3; r++) {
        for (int dx = -r; dx <= r; dx++) {
            for (int dy = -r; dy <= r; dy++) {
                if (abs(dx) != r && abs(dy) != r) continue;
                int hx = px + dx, hy = py + dy;
                if (hx < 0 || hx >= COLS || hy < 0 || hy >= ROWS) continue;
                if (G.IsPath(hx, hy)) continue;
                if (G.TowerAt(hx, hy)) continue;
                outX = hx;
                outY = hy;
                return true;
            }
        }
    }
    return false;
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
    if (!IsBuildableTowerType(t.type)) return false;
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
    if (particles.size() > MAX_PARTICLES) {
        particles.erase(particles.begin(), particles.begin() + (particles.size() - MAX_PARTICLES));
    }
}

void Game::AddFloat(Vector2 pos, const std::string& txt, Color col) {
    floats.push_back({ pos, txt, col, 2.f });
    if (floats.size() > MAX_FLOATS) {
        floats.erase(floats.begin(), floats.begin() + (floats.size() - MAX_FLOATS));
    }
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
    ComputeCoverageSegments(*this, cov);

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
    int   laneCount = ActiveLaneCount();

    // ── 輔助：找最薄弱路段中心的可放格 ─────────────────────────
    // ── 提示 1：NN 推薦放置（覆蓋最弱的路段）───────────────────
    {
        int worstSeg = 0;
        for (int i = 1; i < 3; i++) if (cov[i] < cov[worstSeg]) worstSeg = i;
        int bestLane = -1;
        float lowestCoverage = 2.f;
        for (int lane = 0; lane < laneCount; lane++) {
            const auto& cells = LaneCells(lane);
            if (cells.empty()) continue;
            float laneCov = SegmentCoverage(*this, cells, worstSeg);
            if (laneCov < lowestCoverage) {
                lowestCoverage = laneCov;
                bestLane = lane;
            }
        }

        if (bestLane >= 0) {
            const auto& cells = LaneCells(bestLane);
            int n = (int)cells.size();
            int midIdx = n * (worstSeg * 2 + 1) / 6;
            midIdx = std::max(0, std::min(midIdx, n - 1));
            int sx = -1, sy = -1;
            if (FindNearbyBuildSlot(*this, cells[midIdx].gx, cells[midIdx].gy, sx, sy)) {
                char buf[80];
                snprintf(buf, 80, "%s（信心%.0f%%）",
                    NN_REASON[nnClass], defNN.lastProb[nnClass] * 100.f);
                AIHint h;
                h.gx = sx; h.gy = sy; h.suggest = nnSuggest;
                h.score = defNN.lastProb[nnClass]; h.reason = buf;
                aiHints.push_back(h);
            }
        }
    }

    // ── 提示 2：威脅熱點補強 ────────────────────────────────────
    if ((int)aiHints.size() < 3) {
        float maxT = threatMap.GetMax();
        if (maxT > 0.5f) {
            int bx = -1, by = -1; float best = 0.f;
            for (int x = 0; x < COLS; x++) {
                for (int y = 0; y < ROWS; y++) {
                    if (!IsPath(x, y)) continue;
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
                if (FindNearbyBuildSlot(*this, bx, by, sx, sy)) {
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
            int lane = intel.PreferredPath(laneCount);
            const auto& cells = LaneCells(lane);
            if (!cells.empty()) {
                int mid = (int)cells.size() / 4;
                tgx = cells[mid].gx;
                tgy = cells[mid].gy;
            }
        } else if (nCannon >= 1 && nPct == 0 && wave >= 2) {
            reason = "加入感知器讓AI自動學習射擊時機";
            suggest = TType::PERCEPTRON;
            for (auto& t : towers)
                if (t.type==TType::CANNON) { tgx=t.gx; tgy=t.gy; break; }
        } else if (laneCount > 1 && intel.pathSurvRate[intel.PreferredPath(laneCount)] > 0.5f) {
            int lane = intel.PreferredPath(laneCount);
            reason = "高壓路線需要砲塔";
            suggest = TType::CANNON;
            const auto& cells = LaneCells(lane);
            if (!cells.empty()) {
                int mid2 = (int)cells.size() / 2;
                tgx = cells[mid2].gx;
                tgy = cells[mid2].gy;
            }
        }

        if (reason && tgx >= 0) {
            int sx = -1, sy = -1;
            if (FindNearbyBuildSlot(*this, tgx, tgy, sx, sy)) {
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
    ComputeCoverageSegments(*this, cov);

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
    tutorial.ResetTransient();
    phase    = BUILD;

    placing    = TType::NONE;
    selectedId = -1;
    connectSrc = -1;

    spawned       = 0;
    spawnLaneCursor = 0;
    spawnTimer    = 0.f;
    waveCount     = 0;
    trainingTimer = 0.f;
    trainingChoiceCount = 0;
    combo         = 0;
    comboTimer    = 0.f;
    comboSurgeTimer = 0.f;
    waveTelegraphTimer = 0.f;
    spawnPulseTimer = 0.f;
    spawnPulsePath = 0;
    shakeT        = 0.f;

    waveKills   = 0;
    waveEscaped = 0;
    waveDmg     = 0.f;

    currentEvent     = WaveEvent::NONE;
    eventName        = "";
    eventBannerTimer = 0.f;
    currentIncident     = Incident::NONE;
    incidentName        = "";
    incidentTimer       = 0.f;
    incidentBannerTimer = 0.f;
    incidentRollTimer   = 0.f;
    incidentTriggered   = false;
    aiHints.clear();
    intel = EnemyIntel{};
    defNN = DefenseAdvisorNN{};

    dualPath        = false;
    activeLaneCount = 1;
    blackoutActive  = false;
    nextPreviewPaths = {{1, -1, -1, -1, -1, -1}};
    for (auto& cells : nextPreviewCells) cells.clear();
    nextPreviewLaneCount = 1;
    hasPlannedRouteChange = false;
    lastEntrySideWave.fill(-100000);
    lastPresetWave.assign(PATH_PRESET_COUNT, -100000);

    buffOverfreq  = 0.f;
    buffArmorBreak= 0.f;
    buffGlobalMark= 0.f;

    for (int lane = 0; lane < MAX_LANES; lane++) {
        SetActiveLanePreset(lane, lane);
    }

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
