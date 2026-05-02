// ================================================================
//  logic.cpp — 訊號傳播、戰鬥、AI、技能、波次、主更新迴圈
// ================================================================
#include "logic.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

static const char* EnemyTagName(EnemyTag tag) {
    switch (tag) {
        case EnemyTag::BRUTE:  return "BRUTE";
        case EnemyTag::SWIFT:  return "SWIFT";
        case EnemyTag::BOUNTY: return "BOUNTY";
        default:               return "";
    }
}

static Color EnemyTagColor(EnemyTag tag) {
    switch (tag) {
        case EnemyTag::BRUTE:  return Color{255, 120, 120, 255};
        case EnemyTag::SWIFT:  return Color{120, 255, 200, 255};
        case EnemyTag::BOUNTY: return Color{255, 220, 90, 255};
        default:               return WHITE;
    }
}

static void AssignEnemyTag(Game& G, Enemy& e) {
    if (e.type == EType::BOSS || G.wave < 4) return;

    std::uniform_int_distribution<int> roll(0, 99);
    int r = roll(G.rng);
    if (r < 12) {
        e.tag = EnemyTag::BRUTE;
        e.hp *= 1.35f;
        e.maxHp = e.hp;
        e.spd *= 0.92f;
        e.reward += 6 + G.wave / 2;
    } else if (r < 22) {
        e.tag = EnemyTag::SWIFT;
        e.hp *= 0.82f;
        e.maxHp = e.hp;
        e.spd *= 1.25f;
        e.reward += 4 + G.wave / 3;
    } else if (r < 30) {
        e.tag = EnemyTag::BOUNTY;
        e.hp *= 1.10f;
        e.maxHp = e.hp;
        e.reward = (int)std::round(e.reward * 1.55f);
    }
}

static Tower* FindTrainingUpgradeTarget(Game& G) {
    Tower* best = nullptr;
    int bestScore = -100000;
    for (auto& t : G.towers) {
        if (t.type == TType::CPU || t.level >= 3) continue;
        int score = t.kills * 10 + (int)t.totalDmg;
        if (t.type == TType::CANNON)     score += 220;
        if (t.type == TType::PERCEPTRON) score += 120;
        if (score > bestScore) {
            bestScore = score;
            best = &t;
        }
    }
    return best;
}

static void BuildTrainingChoices(Game& G) {
    G.trainingChoiceCount = 3;

    int creditReward = 90 + G.wave * 15;
    G.trainingChoices[0] = {
        Game::TrainingRewardKind::CREDITS,
        "資金快取",
        "+" + std::to_string(creditReward) + " CR，立刻補強防線",
        COL_AND,
        creditReward,
        0
    };

    int livePatch = std::min(4, 2 + G.wave / 5);
    int cpuPatch  = std::min(30, 12 + G.wave * 2);
    G.trainingChoices[1] = {
        Game::TrainingRewardKind::PATCH,
        "系統修補",
        "+" + std::to_string(livePatch) + " 命 / CPU +" + std::to_string(cpuPatch) + "%",
        COL_SENSOR,
        livePatch,
        cpuPatch
    };

    Tower* target = FindTrainingUpgradeTarget(G);
    G.trainingChoices[2] = {
        Game::TrainingRewardKind::UPGRADE,
        "實戰調校",
        target
            ? ("免費升級一座「" + std::string(TDef(target->type).label) + "」")
            : "若無可升級塔，改領 +80 CR",
        COL_PERC,
        1,
        80
    };
}

static bool WaveRotatesRoutes(int wave) {
    return wave > 1 && (wave - 1) % 3 == 0;
}

static bool IsOppositeSide(PathEntrySide a, PathEntrySide b) {
    return (a == PathEntrySide::LEFT   && b == PathEntrySide::RIGHT) ||
           (a == PathEntrySide::RIGHT  && b == PathEntrySide::LEFT)  ||
           (a == PathEntrySide::TOP    && b == PathEntrySide::BOTTOM) ||
           (a == PathEntrySide::BOTTOM && b == PathEntrySide::TOP);
}

static int ScorePrimaryRouteCandidate(Game& G, int presetIdx) {
    const auto& cand = GetPathPreset(presetIdx);
    const auto& cur0 = G.LanePreset(0);

    int score = cand.dualWeight * 8;
    if (presetIdx != G.LanePresetIdx(0)) score += 30;
    else                                 score -= 120;
    if (cand.entrySide != cur0.entrySide) score += 18;
    if (cand.family    != cur0.family)    score += 12;
    if (G.dualPath && presetIdx != G.LanePresetIdx(1)) score += 6;

    std::uniform_int_distribution<int> jitter(0, 7);
    return score + jitter(G.rng);
}

static int ScoreDualRoutePair(Game& G, int primaryPresetIdx, int secondaryPresetIdx) {
    if (primaryPresetIdx == secondaryPresetIdx) return -100000;

    const auto& a = GetPathPreset(primaryPresetIdx);
    const auto& b = GetPathPreset(secondaryPresetIdx);

    int score = ScorePrimaryRouteCandidate(G, primaryPresetIdx) + b.dualWeight * 8;
    if (secondaryPresetIdx != G.LanePresetIdx(1)) score += 14;
    else                                          score -= 28;
    if (a.entrySide != b.entrySide) score += 42;
    else                            score -= 45;
    if (IsOppositeSide(a.entrySide, b.entrySide)) score += 20;
    if (a.family != b.family) score += 18;
    else                      score -= 22;

    std::uniform_int_distribution<int> jitter(0, 9);
    return score + jitter(G.rng);
}

static int PickSecondaryRouteForPrimary(Game& G, int primaryPresetIdx) {
    int bestIdx = (primaryPresetIdx == 0) ? 1 : 0;
    int bestScore = -100000;
    for (int idx = 0; idx < PATH_PRESET_COUNT; idx++) {
        int score = ScoreDualRoutePair(G, primaryPresetIdx, idx);
        if (score > bestScore) {
            bestScore = score;
            bestIdx = idx;
        }
    }
    return bestIdx;
}

static std::array<int, 2> PlanNextRoutePair(Game& G, int laneCount) {
    std::array<int, 2> plan = { G.LanePresetIdx(0), G.dualPath ? G.LanePresetIdx(1) : -1 };

    if (laneCount <= 1) {
        int bestIdx = G.LanePresetIdx(0);
        int bestScore = -100000;
        for (int idx = 0; idx < PATH_PRESET_COUNT; idx++) {
            int score = ScorePrimaryRouteCandidate(G, idx);
            if (score > bestScore) {
                bestScore = score;
                bestIdx = idx;
            }
        }
        plan[0] = bestIdx;
        plan[1] = -1;
        return plan;
    }

    int bestScore = -100000;
    for (int a = 0; a < PATH_PRESET_COUNT; a++) {
        for (int b = 0; b < PATH_PRESET_COUNT; b++) {
            int score = ScoreDualRoutePair(G, a, b);
            if (score > bestScore) {
                bestScore = score;
                plan[0] = a;
                plan[1] = b;
            }
        }
    }
    return plan;
}

static void UpdateUpcomingRoutePlan(Game& G) {
    int nextWave = G.wave + 1;
    G.nextPreviewLaneCount = (nextWave >= 8) ? 2 : 1;

    if (!WaveRotatesRoutes(nextWave)) {
        G.hasPlannedRouteChange = false;
        G.nextPreviewPaths = { G.LanePresetIdx(0), G.dualPath ? G.LanePresetIdx(1) : -1 };
        return;
    }

    G.hasPlannedRouteChange = true;
    G.nextPreviewPaths = PlanNextRoutePair(G, G.nextPreviewLaneCount);
}

static void ApplyRoutePlan(Game& G, const std::array<int, 2>& plan, int laneCount) {
    BuildPath(plan[0]);
    if (laneCount > 1) BuildDualPath(plan[1]);
}

void ApplyTrainingChoice(Game& G, int choiceIdx) {
    if (G.phase != Game::TRAINING) return;
    if (choiceIdx < 0 || choiceIdx >= G.trainingChoiceCount) return;

    auto reward = G.trainingChoices[choiceIdx];
    switch (reward.kind) {
        case Game::TrainingRewardKind::CREDITS: {
            G.credits += reward.valueA;
            G.AddFloat(G.CC(CPU_GX, CPU_GY), "+" + std::to_string(reward.valueA) + " CR", reward.col);
            G.SetMsg("TRAINING：取得資金快取，建置資源已補充。");
            break;
        }
        case Game::TrainingRewardKind::PATCH: {
            G.lives = std::min(30, G.lives + reward.valueA);
            G.cpuHp = std::min(100.f, G.cpuHp + (float)reward.valueB);
            G.SpawnParticles(G.CC(CPU_GX, CPU_GY), reward.col, 18, 100.f);
            G.AddFloat(G.CC(CPU_GX, CPU_GY), "+PATCH", reward.col);
            G.SetMsg("TRAINING：核心修補完成，防線穩定度提升。");
            break;
        }
        case Game::TrainingRewardKind::UPGRADE: {
            Tower* target = FindTrainingUpgradeTarget(G);
            if (target) {
                target->level++;
                G.ApplyTowerStats(*target);
                target->upgradeFlash = 1.f;
                G.SpawnParticles(G.CC(target->gx, target->gy), reward.col, 24, 130.f);
                G.AddFloat(G.CC(target->gx, target->gy), "TRAIN UP!", reward.col);
                G.SetMsg("TRAINING：實戰調校完成，一座主力塔已免費升級。");
            } else {
                G.credits += reward.valueB;
                G.AddFloat(G.CC(CPU_GX, CPU_GY), "+" + std::to_string(reward.valueB) + " CR", COL_AND);
                G.SetMsg("TRAINING：目前無可升級塔，已改發資金補給。");
            }
            break;
        }
    }

    G.trainingTimer = 0.f;
    G.trainingChoiceCount = 0;
    G.phase = Game::BUILD;
}

// ══════════════════════════════════════════════════════════════════
//  主動技能常數表
// ══════════════════════════════════════════════════════════════════
const float SKILL_CD[] = {
    20.f,   // SENSOR     — 電磁脈衝
    15.f,   // PERCEPTRON — 急速突觸
    22.f,   // AND        — 裝甲穿透
    18.f,   // OR         — 超頻連射
    16.f,   // XOR        — 全域標記
    12.f,   // CANNON     — 超砲擊
     0.f,   // CPU        — （無技能）
    22.f,   // NAND       — 破甲反轉
};

const char* const SKILL_NAME[] = {
    "電磁脈衝", "急速突觸", "裝甲穿透", "超頻連射",
    "全域標記", "超砲擊",   "",         "破甲反轉"
};

const char* const SKILL_DESC[] = {
    "範圍內敵人暫停2秒",
    "加速w1/w2學習",
    "10秒護甲歸零",
    "10秒射速x2",
    "全域標記所有敵人5秒",
    "立即超砲爆炸",
    "",
    "10秒裝甲全破+標記"
};

// ══════════════════════════════════════════════════════════════════
//  CalcPCTLayer  —  計算感知器層數（DFS）
// ══════════════════════════════════════════════════════════════════
int CalcPCTLayer(Game& G, int towerId, int depth) {
    if (depth > 6) return 1;
    Tower* t = G.FindTower(towerId);
    if (!t || t->type != TType::PERCEPTRON) return 1;

    int maxLayer = 1;
    for (auto& src : G.towers) {
        for (int cid : src.conns) {
            if (cid != towerId) continue;
            if (src.type == TType::PERCEPTRON)
                maxLayer = std::max(maxLayer, 1 + CalcPCTLayer(G, src.id, depth + 1));
        }
    }
    return maxLayer;
}

// ══════════════════════════════════════════════════════════════════
//  ActivateSkill  —  觸發主動技能
// ══════════════════════════════════════════════════════════════════
void ActivateSkill(Game& G, Tower& t) {
    if (t.activeCd > 0.f) return;
    if (t.type == TType::CPU) return;

    int     ti  = (int)t.type;
    Vector2 pos = G.CC(t.gx, t.gy);
    t.activeCd  = SKILL_CD[ti];

    switch (t.type) {
        case TType::SENSOR: {
            int cnt = 0;
            for (auto& e : G.enemies) {
                float d = Dist({(float)t.gx, (float)t.gy}, G.EnemyGrid(e));
                if (d <= t.range * 1.5f) { e.stunTimer = 2.f; cnt++; }
            }
            G.SpawnParticles(pos, COL_SENSOR, 20, 120.f);
            char b[48];
            snprintf(b, 48, "EMP！暫停 %d 個", cnt);
            G.AddFloat(pos, b, COL_SENSOR);
            break;
        }

        case TType::PERCEPTRON: {
            t.w1 = std::min(2.f, t.w1 + 0.15f);
            t.w2 = std::min(2.f, t.w2 + 0.15f);
            t.learner.lrDecay = std::min(1.f, t.learner.lrDecay + 0.2f);
            G.SpawnParticles(pos, COL_AI, 16, 110.f);
            G.AddFloat(pos, "突觸強化！", COL_AI);
            break;
        }

        case TType::AND: {
            G.buffArmorBreak = 10.f;
            G.SpawnParticles(pos, COL_AND, 20, 140.f);
            G.AddFloat(pos, "裝甲穿透！10秒", COL_AND);
            break;
        }

        case TType::OR: {
            G.buffOverfreq = 10.f;
            G.SpawnParticles(pos, COL_OR, 18, 130.f);
            G.AddFloat(pos, "超頻連射！10秒", COL_OR);
            break;
        }

        case TType::XOR: {
            for (auto& e : G.enemies) { e.marked = true; e.markTimer = 5.f; }
            G.buffGlobalMark = 5.f;
            G.SpawnParticles(pos, COL_XOR, 24, 130.f);
            G.AddFloat(pos, "全域標記！", COL_XOR);
            break;
        }

        case TType::CANNON: {
            Enemy* tgt = nullptr;
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
                                       tgt->id, t.id, 500.f, false, true, 80.f, RED });
                G.AddFloat(pos, "超砲！", RED);
            }
            G.SpawnParticles(pos, RED, 16, 160.f);
            break;
        }

        case TType::NAND: {
            G.buffArmorBreak = 10.f;
            G.buffGlobalMark = 10.f;
            for (auto& e : G.enemies) { e.marked = true; e.markTimer = 10.f; }
            G.SpawnParticles(pos, COL_NAND, 24, 150.f);
            G.AddFloat(pos, "破甲反轉！全標記10秒", COL_NAND);
            break;
        }

        default: break;
    }

    G.Shake(6.f, 0.2f);
}

// ══════════════════════════════════════════════════════════════════
//  PropagateSignals
// ══════════════════════════════════════════════════════════════════
static float ConnDistance(Tower& src, Tower& dst) {
    float dx = (float)(src.gx - dst.gx);
    float dy = (float)(src.gy - dst.gy);
    return sqrtf(dx * dx + dy * dy);
}

void PropagateSignals(Game& G, float dt) {

    // ── Pass 1：感測器 ───────────────────────────────────────────
    for (auto& t : G.towers) {
        if (t.type != TType::SENSOR) continue;

        float effectiveRange = G.blackoutActive ? t.range * 0.30f : t.range;
        float minD = effectiveRange + 1.f;

        for (auto& e : G.enemies) {
            if (e.stealth && t.level < 2) continue;
            Vector2 eg = G.EnemyGrid(e);
            float d = Dist({(float)t.gx, (float)t.gy}, eg);
            if (d < minD) minD = d;
        }

        float newSig = (minD <= effectiveRange)
            ? std::max(0.f, 1.f - minD / effectiveRange)
            : 0.f;
        if (t.level == 3) newSig = std::min(1.f, newSig * 1.3f);

        float prev = t.sig;
        t.sig += (newSig - t.sig) * 10.f * dt;

        if (fabsf(t.sig - prev) > 0.02f) {
            for (int cid : t.conns) {
                auto* d = G.FindTower(cid);
                if (d) G.pulses.push_back({ G.CC(t.gx, t.gy), G.CC(d->gx, d->gy), 0.f, TDef(t.type).col });
            }
        }
    }

    // ── Pass 2：邏輯閘與感知器 ───────────────────────────────────
    for (auto& t : G.towers) {
        if (t.type == TType::SENSOR || t.type == TType::CPU || t.type == TType::NONE) continue;

        std::vector<float> ins;
        for (auto& src : G.towers) {
            for (int cid : src.conns) {
                if (cid != t.id) continue;
                float dist  = ConnDistance(src, t);
                float atten = powf(0.92f, dist);
                ins.push_back(src.sig * atten);
            }
        }

        if (ins.empty()) { t.sig += (0.f - t.sig) * 8.f * dt; continue; }

        float newSig = t.sig;
        switch (t.type) {
            case TType::PERCEPTRON: {
                float s1    = ins.size() > 0 ? ins[0] : 0.f;
                float s2    = ins.size() > 1 ? ins[1] : 0.f;
                float boost = (t.level == 3) ? 2.0f : (t.level == 2) ? 1.4f : 1.0f;
                newSig = Sigmoid((s1 * t.w1 + s2 * t.w2 + t.bias) * 3.f) * boost;
                newSig = std::min(1.f, newSig);
                t.SampleSignal(s1, s2, newSig);

                // ── 採樣訓練目標：下游砲塔射程內有敵人 → 目標=1 ─
                float targetVal = 0.f;
                for (int cid : t.conns) {
                    Tower* dn = G.FindTower(cid);
                    if (!dn || dn->type != TType::CANNON) continue;
                    for (auto& e : G.enemies) {
                        if (Dist({(float)dn->gx,(float)dn->gy}, G.EnemyGrid(e)) <= dn->range) {
                            targetVal = 1.f; break;
                        }
                    }
                    if (targetVal > 0.f) break;
                }
                t.pctTargetAcc     += targetVal;
                t.pctTargetSamples += 1;
                break;
            }
            case TType::AND: {
                bool allHigh = true;
                for (float s : ins) if (s < 0.5f) { allHigh = false; break; }
                float mult = (t.level >= 2) ? 1.5f : 1.0f;
                newSig = allHigh ? std::min(1.f, 1.f * mult) : 0.f;
                break;
            }
            case TType::OR: {
                bool anyHigh = false;
                for (float s : ins) if (s > 0.5f) { anyHigh = true; break; }
                newSig = anyHigh ? 1.f : 0.f;
                break;
            }
            case TType::XOR: {
                int highCount = 0;
                for (float s : ins) if (s > 0.5f) highCount++;
                newSig = (highCount == 1) ? 1.f : 0.f;
                break;
            }
            case TType::NAND: {
                bool allHigh = true;
                for (float s : ins) if (s < 0.5f) { allHigh = false; break; }
                float armorBonus = (G.buffArmorBreak > 0) ? 0.3f
                                 : (t.level >= 2)         ? 0.2f : 0.f;
                newSig = allHigh ? 0.f : std::min(1.f, 1.0f + armorBonus);
                break;
            }
            case TType::CANNON: {
                float mx = 0.f;
                for (float s : ins) mx = std::max(mx, s);
                newSig = mx;
                break;
            }
            default: break;
        }

        float prev = t.sig;
        t.sig += (newSig - t.sig) * 12.f * dt;

        if (fabsf(t.sig - prev) > 0.02f) {
            for (int cid : t.conns) {
                auto* d = G.FindTower(cid);
                if (d) G.pulses.push_back({ G.CC(t.gx, t.gy), G.CC(d->gx, d->gy), 0.f, TDef(t.type).col });
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════
//  CalcDamageMultiplier
// ══════════════════════════════════════════════════════════════════
float CalcDamageMultiplier(Game& G, Tower& cannon, Enemy& target) {
    float mult = 1.f;

    for (auto& src : G.towers) {
        bool feedsCannon = false;
        for (int cid : src.conns) if (cid == cannon.id) { feedsCannon = true; break; }
        if (!feedsCannon) continue;

        if (src.type == TType::AND  && target.type == EType::ARMORED) mult += 0.5f;
        if (src.type == TType::XOR  && target.type == EType::ELITE)   mult += 1.0f;
        if (src.type == TType::NAND && target.type == EType::ARMORED) {
            float bonus = (src.level == 3) ? 1.2f : (src.level == 2) ? 0.7f : 0.4f;
            mult += bonus;
        }
    }

    if (target.marked)         mult += 0.5f;
    if (G.buffGlobalMark > 0)  mult += 0.3f;

    // ── 護甲處理 ─────────────────────────────────────────────────
    float armorReduction = target.armor;
    bool  armorBroken    = (G.buffArmorBreak > 0);

    if (!armorBroken) {
        for (auto& src : G.towers)
            for (int cid : src.conns)
                if (cid == cannon.id && src.type == TType::NAND && src.level >= 3)
                    armorBroken = true;
    }

    if (armorReduction > 0 && !armorBroken) {
        bool hasAnd = false;
        for (auto& src : G.towers)
            for (int cid : src.conns)
                if (cid == cannon.id && src.type == TType::AND) hasAnd = true;
        if (!hasAnd) mult *= (1.f - armorReduction);
    }

    return mult;
}

// ══════════════════════════════════════════════════════════════════
//  UpdateBossAI
// ══════════════════════════════════════════════════════════════════
void UpdateBossAI(Game& G, Enemy& boss, float dt) {
    Vector2 eg  = G.EnemyGrid(boss);
    int     bgx = (int)(eg.x + 0.5f);
    int     bgy = (int)(eg.y + 0.5f);

    float threat = G.threatMap.Get(bgx, bgy);
    const auto& laneCells = G.EnemyLaneCells(boss);
    int   nextIdx = (int)boss.pathPos + 2;
    if (nextIdx < (int)laneCells.size()) {
        threat = std::max(threat,
            G.threatMap.Get(laneCells[nextIdx].gx, laneCells[nextIdx].gy));
    }

    boss.localThreat = threat;
    float hpRatio    = boss.hp / boss.maxHp;
    boss.TickBoss(dt, threat, hpRatio);

    static float rampageFloatTimer = 0.f;
    rampageFloatTimer -= dt;
    if (boss.bossState == BossState::RAMPAGE && rampageFloatTimer <= 0.f) {
        G.AddFloat(G.EnemyWorld(boss), "RAMPAGE！", Color{255, 50, 50, 255});
        rampageFloatTimer = 3.f;
    }
}

// ══════════════════════════════════════════════════════════════════
//  SpawnEnemy
// ══════════════════════════════════════════════════════════════════
void SpawnEnemy(Game& G) {
    std::uniform_int_distribution<int> typeR(0, 4);
    EType et;

    switch (G.currentEvent) {
        case WaveEvent::ARMORED:
            et = EType::ARMORED; break;
        case WaveEvent::STEALTH:
        case WaveEvent::BLACKOUT:
            et = (typeR(G.rng) % 3 == 0) ? EType::FAST : EType::BASIC; break;
        case WaveEvent::ELITE_RUSH:
            et = EType::ELITE; break;
        case WaveEvent::BOSS_ESCORT:
            et = (G.spawned == 0) ? EType::BOSS : EType::BASIC; break;
        case WaveEvent::SWARM:
            et = (typeR(G.rng) % 5 == 0) ? EType::FAST : EType::BASIC; break;
        case WaveEvent::SIEGE:
            et = (G.spawned == 0) ? EType::ARMORED : EType::BASIC; break;
        default: {
            // ── 敵情學習：依存活率加權選兵種 ───────────────────
            if (G.intel.adapted) {
                std::uniform_real_distribution<float> rd(0.f, 1.f);
                et = G.intel.WeightedType(rd(G.rng), G.wave);
            } else {
                int r = typeR(G.rng);
                if (G.wave < 3) r %= 2;
                switch (r) {
                    case 0: case 1: et = EType::BASIC;   break;
                    case 2:         et = EType::FAST;    break;
                    case 3:         et = EType::ARMORED; break;
                    default:        et = EType::ELITE;   break;
                }
            }
        }
    }

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
            e.spd    = 1.4f + G.wave * 0.07f;
            e.reward = 10 + G.wave;
            break;
        case EType::FAST:
            e.hp = e.maxHp = 30.f + G.wave * 14.f;
            e.spd    = 2.8f + G.wave * 0.12f;
            e.reward = 8 + G.wave;
            e.stealth = (G.wave >= 4);
            break;
        case EType::ARMORED:
            e.hp = e.maxHp = 120.f + G.wave * 40.f;
            e.armor  = 0.60f;
            e.spd    = 0.85f + G.wave * 0.045f;
            e.reward = 22 + G.wave * 2;
            break;
        case EType::ELITE:
            e.hp = e.maxHp = 90.f + G.wave * 33.f;
            e.spd    = 1.1f + G.wave * 0.06f;
            e.reward = 28 + G.wave * 2;
            if (G.wave >= 10) { e.shielded = true; e.shieldHp = e.maxHp * 0.25f; }
            break;
        case EType::BOSS:
            e.hp = e.maxHp = 800.f + G.wave * 220.f;
            e.armor  = 0.35f;
            e.spd    = 0.55f;
            e.reward = 150 + G.wave * 20;
            break;
    }

    switch (G.currentEvent) {
        case WaveEvent::SWARM:
            e.hp *= 0.6f; e.maxHp = e.hp; break;
        case WaveEvent::STEALTH:
        case WaveEvent::BLACKOUT:
            e.stealth = true; break;
        case WaveEvent::ELITE_RUSH:
            e.spd *= 1.5f; break;
        case WaveEvent::REGEN_ARMY:
            e.regenTimer = -1.f; break;
        case WaveEvent::DOUBLE_SPD:
            e.spd *= 2.f; break;
        case WaveEvent::MUTANT:
            e.hp *= 2.0f; e.maxHp = e.hp;
            e.spd *= 1.3f; e.armor = 0.f;
            e.regenTimer = -1.f; break;
        case WaveEvent::SIEGE:
            if (G.spawned == 0) {
                e.hp *= 8.0f; e.maxHp = e.hp;
                e.armor = 0.80f; e.spd *= 0.45f;
            }
            break;
        default: break;
    }

    AssignEnemyTag(G, e);
    e.spawnFx = 0.65f;

    G.enemies.push_back(e);
    if (G.dualPath && !G.LaneCells(1).empty()) {
        // ── 敵方神經網路評估兩條路線的威脅 ─────────────────────
        auto pathThreat = [&](const std::vector<PathCell>& cells) -> float {
            float sum = 0.f;
            int   n   = std::min((int)cells.size(), 10);
            if (n <= 0) return 0.f;
            for (int k = 0; k < n; k++)
                sum += G.threatMap.Get(cells[k].gx, cells[k].gy);
            float maxT = G.threatMap.GetMax();
            return (maxT > 0.f) ? (sum / n) / maxT : 0.f;
        };

        float t0 = pathThreat(G.LaneCells(0));
        float t1 = pathThreat(G.LaneCells(1));

        // 存入用於波末訓練
        G.intel.pathAvgThreat[0] = t0;
        G.intel.pathAvgThreat[1] = t1;

        // 腦選路或隨機（學習前）
        if (G.intel.adapted) {
            G.enemies.back().pathIdx = G.intel.BrainPickPath(t0, t1);
        } else {
            std::uniform_real_distribution<float> rd(0.f, 1.f);
            G.enemies.back().pathIdx = G.intel.PickPath(rd(G.rng), true);
        }
    }

    G.spawnPulseTimer = 0.28f;
    G.spawnPulsePath  = G.enemies.back().pathIdx;

    if (G.enemies.back().tag != EnemyTag::NONE) {
        Vector2 ep = G.EnemyWorld(G.enemies.back());
        G.AddFloat({ep.x, ep.y - 18.f}, EnemyTagName(G.enemies.back().tag), EnemyTagColor(G.enemies.back().tag));
    }

    // ── 追蹤本波生成統計 ─────────────────────────────────────────
    int ti = (int)G.enemies.back().type;
    if (ti >= 0 && ti < 5) G.intel.typeSpawned[ti]++;
    int pi = G.enemies.back().pathIdx;
    if (pi >= 0 && pi < 2) G.intel.pathSpawned[pi]++;
}

// ══════════════════════════════════════════════════════════════════
//  StartWave
// ══════════════════════════════════════════════════════════════════
void StartWave(Game& G) {
    if (G.phase != Game::BUILD) return;

    G.wave++;

    // ── 每 3 波輪換路線 ──────────────────────────────────────────
    if (WaveRotatesRoutes(G.wave)) {
        int laneCount = (G.wave >= 8) ? 2 : 1;
        std::array<int, 2> routePlan = G.hasPlannedRouteChange
            ? G.nextPreviewPaths
            : PlanNextRoutePair(G, laneCount);
        ApplyRoutePlan(G, routePlan, laneCount);

        int demolished = 0;
        G.towers.erase(
            std::remove_if(G.towers.begin(), G.towers.end(),
                [&](const Tower& t) {
                    if (t.type == TType::CPU) return false;
                    bool onPath = IsAnyActivePathCell(t.gx, t.gy, laneCount);
                    if (onPath) { G.credits += TDef(t.type).baseCost; demolished++; }
                    return onPath;
                }),
            G.towers.end()
        );

        for (auto& tw : G.towers) {
            tw.conns.erase(
                std::remove_if(tw.conns.begin(), tw.conns.end(),
                    [&](int id) { return G.FindTower(id) == nullptr; }),
                tw.conns.end()
            );
        }

        G.credits += 200;

        char pb[120];
        if (demolished > 0) {
            if (laneCount > 1)
                snprintf(pb, 120, "雙路徑重組！+200CR。%d座塔被占用（全額退款）。主：%s | 副：%s",
                    demolished, G.LanePreset(0).name, G.LanePreset(1).name);
            else
                snprintf(pb, 120, "路徑重組！+200CR。%d座塔被占用（全額退款）。新路線：%s",
                    demolished, G.LanePreset(0).name);
        } else {
            if (laneCount > 1)
                snprintf(pb, 120, "雙路徑重組！+200CR。主：%s | 副：%s",
                    G.LanePreset(0).name, G.LanePreset(1).name);
            else
                snprintf(pb, 120, "路徑重組！+200CR。新路線：%s", G.LanePreset(0).name);
        }
        G.SetMsg(pb);
    }

    // ── Wave 8 開通雙路徑 ────────────────────────────────────────
    if (G.wave >= 8 && !G.dualPath) {
        G.dualPath = true;
        int p2 = PickSecondaryRouteForPrimary(G, G.LanePresetIdx(0));
        BuildDualPath(p2);
        char db[96];
        snprintf(db, 96, "警告：第二條路徑開通！雙路進攻！副路：%s", G.LanePreset(1).name);
        G.SetMsg(db);
    }

    UpdateUpcomingRoutePlan(G);
    G.currentEvent     = Game::RollEvent(G.wave, G.rng);
    G.eventName        = Game::EventName(G.currentEvent);
    G.eventBannerTimer = 4.f;
    G.blackoutActive   = (G.currentEvent == WaveEvent::BLACKOUT);

    bool boss      = (G.wave % 5 == 0);
    int  baseCount = boss ? 1 + (G.wave / 5) * 2 : 6 + G.wave * 3;

    if (G.currentEvent == WaveEvent::SWARM)       baseCount = (int)(baseCount * 2.2f);
    if (G.currentEvent == WaveEvent::BOSS_ESCORT) baseCount = 1 + G.wave * 2;
    if (G.currentEvent == WaveEvent::ELITE_RUSH)  baseCount = std::max(4, baseCount / 2);

    G.waveCount   = baseCount;
    G.spawned     = 0;
    G.spawnTimer  = 0.f;
    G.phase       = Game::FIGHT;
    G.trainingTimer = 0.f;
    G.trainingChoiceCount = 0;
    G.waveKills   = 0;
    G.waveEscaped = 0;
    G.waveDmg     = 0.f;
    G.waveTelegraphTimer = 1.8f;
    G.spawnPulseTimer = 0.6f;
    G.spawnPulsePath = 0;
    G.placing = TType::NONE;
    G.connectSrc = -1;

    char buf[96];
    if (G.eventName.empty() || G.currentEvent == WaveEvent::NONE)
        snprintf(buf, 96, "第%d波 — %d 個病毒！", G.wave, G.waveCount);
    else
        snprintf(buf, 96, "第%d波 %s (%d 個)", G.wave, G.eventName.c_str(), G.waveCount);
    G.SetMsg(buf);
}

// ══════════════════════════════════════════════════════════════════
//  Update  —  主更新迴圈
// ══════════════════════════════════════════════════════════════════
void Update(Game& G, float dt) {
    if (G.paused || G.phase == Game::GAMEOVER || G.phase == Game::MENU) return;

    // ── 倒數計時器 ───────────────────────────────────────────────
    if (G.msgTimer         > 0) G.msgTimer         -= dt;
    if (G.shakeT           > 0) G.shakeT           -= dt;
    if (G.eventBannerTimer > 0) G.eventBannerTimer  -= dt;
    if (G.waveTelegraphTimer > 0) G.waveTelegraphTimer -= dt;
    if (G.spawnPulseTimer > 0)    G.spawnPulseTimer    -= dt;

    bool surgeWasActive = (G.comboSurgeTimer > 0.f);
    if (G.comboSurgeTimer > 0.f) {
        G.comboSurgeTimer = std::max(0.f, G.comboSurgeTimer - dt);
    }
    if (G.comboTimer > 0) {
        G.comboTimer -= dt;
        if (G.comboTimer <= 0) {
            G.combo = 0;
            G.comboSurgeTimer = 0.f;
        }
    }
    if (surgeWasActive && G.comboSurgeTimer <= 0.f) {
        G.AddFloat(G.CC(CPU_GX, CPU_GY), "SURGE 結束", Color{255, 160, 120, 255});
    }

    if (G.phase == Game::TRAINING && G.trainingTimer > 0.f) {
        G.trainingTimer -= dt;
        if (G.trainingTimer <= 0.f && G.trainingChoiceCount > 0) {
            ApplyTrainingChoice(G, 0);
        }
    }

    for (auto& h : G.aiHints) h.flashT += dt * 2.5f;

    if (G.buffOverfreq   > 0) G.buffOverfreq   -= dt;
    if (G.buffArmorBreak > 0) G.buffArmorBreak -= dt;
    if (G.buffGlobalMark > 0) G.buffGlobalMark -= dt;
    for (auto& t : G.towers) if (t.activeCd > 0) t.activeCd = std::max(0.f, t.activeCd - dt);

    // ── 生成敵人 ─────────────────────────────────────────────────
    if (G.phase == Game::FIGHT && G.waveTelegraphTimer <= 0.f && G.spawned < G.waveCount) {
        G.spawnTimer += dt;
        float interval = std::max(0.15f, 0.75f - G.wave * 0.035f);
        if (G.spawnTimer >= interval) {
            G.spawnTimer = 0.f;
            SpawnEnemy(G);
            G.spawned++;
        }
    }

    if (G.phase == Game::FIGHT) PropagateSignals(G, dt);

    // ── 塔動畫計時 ───────────────────────────────────────────────
    for (auto& t : G.towers) {
        t.anim += dt * 3.f;
        if (t.anim > 6.28f) t.anim -= 6.28f;
        if (t.upgradeFlash > 0) t.upgradeFlash -= dt * 2.f;
    }

    // ── 砲塔開火 ─────────────────────────────────────────────────
    for (auto& cannon : G.towers) {
        if (cannon.type != TType::CANNON) continue;
        if (cannon.cooldown > 0) { cannon.cooldown--; continue; }
        if (cannon.sig < 0.5f)   continue;

        int effectiveCd = cannon.fireRateTicks;
        bool hasOrL2 = false;
        for (auto& src : G.towers)
            for (int cid : src.conns)
                if (cid == cannon.id && src.type == TType::OR && src.level >= 2) hasOrL2 = true;
        if (hasOrL2)            effectiveCd = (int)(effectiveCd * 0.7f);
        if (G.buffOverfreq > 0) effectiveCd = (int)(effectiveCd * 0.5f);

        Enemy* tgt  = nullptr;
        float  best = cannon.range;
        for (auto& e : G.enemies) {
            float d = Dist({(float)cannon.gx, (float)cannon.gy}, G.EnemyGrid(e));
            if (d < best) { best = d; tgt = &e; }
        }
        if (!tgt) continue;

        cannon.cooldown = effectiveCd;
        float dmg = cannon.damage * CalcDamageMultiplier(G, cannon, *tgt);
        bool surgeShot = (G.comboSurgeTimer > 0.f);
        if (surgeShot) dmg *= 1.35f;

        bool hasXorL2 = false;
        for (auto& src : G.towers)
            for (int cid : src.conns)
                if (cid == cannon.id && src.type == TType::XOR && src.level >= 2) hasXorL2 = true;
        bool crit = hasXorL2 && (G.rng() % 5 == 0);
        if (crit) dmg *= 2.f;

        bool hasXorL3 = false;
        for (auto& src : G.towers)
            for (int cid : src.conns)
                if (cid == cannon.id && src.type == TType::XOR && src.level >= 3) hasXorL3 = true;
        if (hasXorL3) { tgt->marked = true; tgt->markTimer = 4.f; }

        Vector2 tp  = G.EnemyWorld(*tgt);
        Vector2 sp  = G.CC(cannon.gx, cannon.gy);
        Vector2 dir = Vector2Normalize(Vector2Subtract(tp, sp));
        bool hasSplash = (cannon.level == 3);
        Color bcol = crit ? YELLOW : (surgeShot ? Color{255, 140, 90, 255} : COL_CANNON);
        G.bullets.push_back({ sp, {dir.x * 420.f, dir.y * 420.f},
                               tgt->id, cannon.id, dmg, crit, hasSplash, hasSplash ? 55.f : 0.f, bcol });

        bool hasOrL3 = false;
        for (auto& src : G.towers)
            for (int cid : src.conns)
                if (cid == cannon.id && src.type == TType::OR && src.level >= 3) hasOrL3 = true;
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
                                       tgt2->id, cannon.id, dmg * 0.6f, false, false, 0.f, COL_OR });
            }
        }
    }

    // ── 砲彈移動 ─────────────────────────────────────────────────
    for (auto& b : G.bullets) { b.pos.x += b.vel.x * dt; b.pos.y += b.vel.y * dt; }

    // ── 砲彈命中 ─────────────────────────────────────────────────
    std::vector<Bullet> remainBullets;
    for (auto& b : G.bullets) {
        bool hit = false;
        for (auto& e : G.enemies) {
            Vector2 ep = G.EnemyWorld(e);
            if (Dist(b.pos, ep) >= 20.f) continue;

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
            e.hp -= actualDmg;
            e.flashTimer = 0.15f;
            G.waveDmg   += actualDmg;
            G.SpawnParticles(b.pos, b.col, b.crit ? 12 : 6, b.crit ? 115.f : 80.f);
            hit = true;

            if (actualDmg >= 45.f || b.crit || e.type == EType::BOSS) {
                char db[24];
                snprintf(db, 24, "-%.0f", actualDmg);
                G.AddFloat({ep.x, ep.y - 16.f}, db, b.crit ? YELLOW : b.col);
            }

            if (b.crit) {
                G.AddFloat({ep.x, ep.y - 34.f}, "CRIT!", YELLOW);
                G.Shake(4.f, 0.12f);
            }

            Tower* src = G.FindTower(b.sourceId);
            if (src) src->totalDmg += actualDmg;

            if (b.splash) {
                for (auto& e2 : G.enemies) {
                    if (e2.id == e.id) continue;
                    if (Dist(G.EnemyWorld(e2), b.pos) >= b.splashR) continue;
                    float splashDmg = b.dmg * 0.6f;
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
        if (!hit) {
            auto o = G.MapOrigin();
            if (b.pos.x > o.x && b.pos.x < o.x + MAP_W &&
                b.pos.y > o.y && b.pos.y < o.y + MAP_H)
                remainBullets.push_back(b);
        }
    }
    G.bullets = remainBullets;

    // ── 擊殺結算 ─────────────────────────────────────────────────
    for (auto it = G.enemies.begin(); it != G.enemies.end();) {
        if (it->hp > 0) { ++it; continue; }

        G.waveKills++;
        G.combo++;
        G.comboTimer = Game::COMBO_WINDOW;

        bool surgeJustStarted = false;
        if (G.combo >= Game::COMBO_SURGE_THRESHOLD) {
            surgeJustStarted = (G.comboSurgeTimer <= 0.f);
            G.comboSurgeTimer = surgeJustStarted
                ? Game::COMBO_SURGE_TIME
                : std::min(Game::COMBO_SURGE_TIME + 1.5f, G.comboSurgeTimer + 0.7f);
        }

        for (auto& b : G.bullets) {
            if (b.targetId == it->id) {
                Tower* src = G.FindTower(b.sourceId);
                if (src) { src->kills++; break; }
            }
        }

        Vector2 eg = G.EnemyGrid(*it);
        int kgx = (int)(eg.x + 0.5f), kgy = (int)(eg.y + 0.5f);
        float killValue = (it->type == EType::BOSS)  ? 8.f
                        : (it->type == EType::ELITE) ? 3.f : 1.f;
        G.threatMap.AddKill(kgx, kgy, killValue);

        float comboMult = (G.combo >= 10) ? 2.f
                        : (G.combo >=  5) ? 1.5f
                        : (G.combo >=  3) ? 1.2f : 1.f;
        int reward = (int)(it->reward * comboMult);
        G.credits += reward;
        G.score   += (int)(reward * 1.5f);
        if (G.score > G.highScore) G.highScore = G.score;

        Vector2 p = G.EnemyWorld(*it);
        G.SpawnParticles(p, COL_VIRUS,
            (it->type == EType::BOSS) ? 40 : 12,
            (it->type == EType::BOSS) ? 180.f : 100.f);
        if (it->tag == EnemyTag::BOUNTY) {
            G.SpawnParticles(p, EnemyTagColor(it->tag), 10, 95.f);
        }

        std::string ftxt = "+" + std::to_string(reward) + " CR";
        if (G.combo >= 3) { char cb[16]; snprintf(cb, 16, " x%.1f", comboMult); ftxt += cb; }
        G.AddFloat(p, ftxt, G.combo >= 5 ? COL_STAR : COL_AND);

        if (it->type == EType::BOSS)  { G.AddFloat({p.x, p.y - 50}, "BOSS 擊倒！", COL_BOSS); G.Shake(20.f, 0.6f); }
        if (G.combo ==  3) G.AddFloat({p.x, p.y - 30}, "COMBO x3！",  Color{255, 220, 100, 255});
        if (G.combo ==  5) G.AddFloat({p.x, p.y - 30}, "COMBO x5！",  COL_STAR);
        if (surgeJustStarted) {
            G.AddFloat({p.x, p.y - 48}, "SURGE ONLINE!", Color{255, 140, 90, 255});
            G.SpawnParticles(p, Color{255, 140, 90, 255}, 20, 140.f);
            G.SetMsg("連殺突破！進入 COMBO SURGE，砲塔火力暫時提升。");
        }
        if (G.combo == 10) G.AddFloat({p.x, p.y - 30}, "MEGA COMBO！", Color{255, 100, 255, 255});

        it = G.enemies.erase(it);
    }

    // ── Boss 分裂 ────────────────────────────────────────────────
    {
        std::vector<Enemy> splitSpawns;
        for (auto& e : G.enemies) {
            if (e.type != EType::BOSS || e.hasSplit || e.hp / e.maxHp > 0.50f) continue;
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
        for (auto& sp : splitSpawns) G.enemies.push_back(sp);
    }

    // ── 敵人移動、再生 ───────────────────────────────────────────
    for (auto& e : G.enemies) {
        e.angle += 180.f * dt * (e.type == EType::BOSS ? 1.f : 2.f);
        if (e.flashTimer > 0) e.flashTimer -= dt;
        if (e.spawnFx > 0)    e.spawnFx    -= dt;
        if (e.marked) { e.markTimer -= dt; if (e.markTimer <= 0) e.marked = false; }

        if (e.type == EType::ELITE) {
            e.regenTimer += dt;
            if (e.regenTimer >= 2.f) { e.regenTimer = 0.f; e.hp = std::min(e.maxHp, e.hp + e.maxHp * 0.05f); }
        }
        if (G.currentEvent == WaveEvent::REGEN_ARMY && e.type != EType::ELITE) {
            e.regenTimer += dt;
            if (e.regenTimer >= 1.f) { e.regenTimer = 0.f; e.hp = std::min(e.maxHp, e.hp + e.maxHp * 0.08f); }
        }

        if (e.type == EType::BOSS) UpdateBossAI(G, e, dt);
        if (e.stunTimer > 0) { e.stunTimer -= dt; continue; }

        float speed = e.spd;
        if (e.type == EType::BOSS) speed *= e.evadeSpdMult;
        e.pathPos += speed * dt;

        const auto& pathRef = G.EnemyLaneCells(e);
        if (e.pathPos >= (float)(pathRef.size() - 1)) {
            int   liveDmg = (e.type == EType::BOSS) ? 5 : 1;
            float cpuDmg  = (e.type == EType::BOSS) ? 25.f : 8.f;
            G.lives  -= liveDmg;
            G.cpuHp   = std::max(0.f, G.cpuHp - cpuDmg);
            G.SpawnParticles(G.CC(CPU_GX, CPU_GY), COL_VIRUS, 16, 120.f);
            G.AddFloat(G.CC(CPU_GX, CPU_GY), "-" + std::to_string(liveDmg) + " 命", RED);
            G.Shake(12.f, 0.4f);
            G.waveEscaped++;

            // ── 敵情學習：記錄突破（存活）的兵種和路徑 ──────────
            int ti = (int)e.type;
            if (ti >= 0 && ti < 5) G.intel.typeSurvived[ti]++;
            int pi = e.pathIdx;
            if (pi >= 0 && pi < 2) G.intel.pathSurvived[pi]++;

            e.hp = 0.f;
        }
    }

    G.enemies.erase(
        std::remove_if(G.enemies.begin(), G.enemies.end(), [](const Enemy& e) { return e.hp <= 0; }),
        G.enemies.end()
    );

    // ── 脈衝更新 ─────────────────────────────────────────────────
    for (auto& p : G.pulses) p.t += dt * 3.5f;
    G.pulses.erase(std::remove_if(G.pulses.begin(), G.pulses.end(), [](const SigPulse& p) { return p.t >= 1.f; }), G.pulses.end());
    if (G.pulses.size() > 500) G.pulses.erase(G.pulses.begin(), G.pulses.begin() + G.pulses.size() - 500);

    // ── 粒子更新 ─────────────────────────────────────────────────
    for (auto& p : G.particles) {
        p.pos.x += p.vel.x * dt; p.pos.y += p.vel.y * dt;
        p.vel.y += 55.f * dt; p.life -= dt;
    }
    G.particles.erase(std::remove_if(G.particles.begin(), G.particles.end(), [](const Particle& p) { return p.life <= 0; }), G.particles.end());

    // ── 浮動文字更新 ─────────────────────────────────────────────
    for (auto& f : G.floats) { f.pos.y -= 28.f * dt; f.life -= dt; }
    G.floats.erase(std::remove_if(G.floats.begin(), G.floats.end(), [](const FloatText& f) { return f.life <= 0; }), G.floats.end());

    // ── 波次結束 ─────────────────────────────────────────────────
    bool waveFinished = (G.phase == Game::FIGHT) && G.enemies.empty() && (G.spawned >= G.waveCount);
    if (waveFinished) {
        int bonus = 50 + G.wave * 10;
        G.blackoutActive = false;
        G.waveTelegraphTimer = 0.f;
        G.spawnPulseTimer = 0.f;
        G.combo = 0;
        G.comboTimer = 0.f;
        G.comboSurgeTimer = 0.f;
        if (G.waveEscaped == 0) {
            bonus = (int)(bonus * 1.5f);
        }
        G.credits      += bonus;
        G.threatMap.Decay(0.85f);
        G.intel.LearnFromWave();
        G.TrainDefenseNN();
        G.TrainPerceptrons();
        G.GenerateAIHints();
        G.phase = Game::TRAINING;
        G.trainingTimer = 8.5f;
        G.placing = TType::NONE;
        G.connectSrc = -1;
        BuildTrainingChoices(G);

        char buf[96];
        if (G.waveEscaped == 0)
            snprintf(buf, 96, "完美防守！+%d CR！進入 TRAINING，選一項強化。", bonus);
        else
            snprintf(buf, 96, "第%d波結束！+%d CR。%d個病毒突破。進入 TRAINING。", G.wave, bonus, G.waveEscaped);
        G.SetMsg(buf);
    }

    // ── 遊戲結束 ─────────────────────────────────────────────────
    if (G.lives <= 0 || G.cpuHp <= 0) {
        G.lives = 0; G.cpuHp = 0; G.phase = Game::GAMEOVER;
        if (G.score > G.highScore) G.highScore = G.score;
        G.SetMsg("防線崩潰 — 遊戲結束！");
        G.Shake(25.f, 1.f);
    }
}
