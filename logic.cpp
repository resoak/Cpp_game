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

static void EmitVfxRing(Game& G, Vector2 pos, Color col, float radius, float life) {
    G.particles.push_back({ pos, {0.f, 0.f}, life, life, radius, col });
}

static void EmitMuzzleVfx(Game& G, Vector2 pos, Vector2 dir, Color col, bool charged) {
    float ringR = charged ? 28.f : 20.f;
    EmitVfxRing(G, pos, col, ringR, charged ? 0.28f : 0.20f);
    G.SpawnParticles(pos, col, charged ? 9 : 5, charged ? 135.f : 95.f);

    Vector2 nose = { pos.x + dir.x * CELL * 0.26f, pos.y + dir.y * CELL * 0.26f };
    float speed = charged ? 180.f : 125.f;
    G.particles.push_back({ nose, {dir.x * speed, dir.y * speed}, 0.22f, 0.22f, charged ? 7.f : 5.f, WHITE });
}

static void EmitImpactVfx(Game& G, Vector2 pos, Color col, bool heavy) {
    G.SpawnParticles(pos, col, heavy ? 14 : 7, heavy ? 135.f : 85.f);
    EmitVfxRing(G, pos, col, heavy ? 38.f : 24.f, heavy ? 0.36f : 0.25f);
    if (heavy) EmitVfxRing(G, pos, WHITE, 18.f, 0.18f);
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

static float IncidentDuration(Game::Incident incident) {
    switch (incident) {
        case Game::Incident::SIGNAL_STORM:  return 7.0f;
        case Game::Incident::ROUTE_SURGE:   return 6.5f;
        case Game::Incident::BOUNTY_WINDOW: return 8.0f;
        default:                            return 0.f;
    }
}

static Color IncidentColor(Game::Incident incident) {
    switch (incident) {
        case Game::Incident::SIGNAL_STORM:  return COL_SENSOR;
        case Game::Incident::ROUTE_SURGE:   return Color{255, 130, 90, 255};
        case Game::Incident::BOUNTY_WINDOW: return COL_STAR;
        default:                            return WHITE;
    }
}

static void TriggerIncident(Game& G) {
    G.currentIncident = Game::RollIncident(G.wave, G.rng);
    if (G.currentIncident == Game::Incident::NONE) return;

    G.incidentTriggered   = true;
    G.incidentName        = Game::IncidentName(G.currentIncident);
    G.incidentTimer       = IncidentDuration(G.currentIncident);
    G.incidentBannerTimer = 3.2f;

    Color col = IncidentColor(G.currentIncident);
    G.SetMsg(G.incidentName);
    G.AddFloat(G.CC(CPU_GX, CPU_GY), G.incidentName, col);
    G.SpawnParticles(G.CC(CPU_GX, CPU_GY), col, 18, 120.f);
    G.Shake(7.f, 0.22f);
}

static int DynamicLaneCountForWave(int wave) {
    (void)wave;
    return MAX_LANES;
}

static int LaneCountForWave(int wave) {
    if (wave >= 30) return std::max(1, std::min(MAX_LANES, DynamicLaneCountForWave(wave)));
    if (wave >= 16) return 4;
    if (wave >= 10) return 2;
    return 1;
}

static int RouteReplanCreditForLaneCount(int laneCount, int wave) {
    int credit = 200;
    if (laneCount >= 2) credit += 80;
    if (laneCount >= 4) credit += 170;
    if (laneCount >= 6) credit += 250;
    if (wave == 7 || wave == 10 || wave == 16 || wave == 30) credit += 120;
    return credit;
}

static int BalancedWaveCountForLanes(Game::WaveEvent event, int wave, int laneCount) {
    bool boss = (wave % 5 == 0);
    int baseCount = boss ? 1 + (wave / 5) * 2 : 6 + wave * 3;

    if (laneCount >= 6)      baseCount = (int)(baseCount * 0.78f);
    else if (laneCount >= 4) baseCount = (int)(baseCount * 0.90f);

    if (event == WaveEvent::SWARM) {
        float swarmMult = (laneCount >= 6) ? 1.75f : (laneCount >= 4 ? 1.95f : 2.20f);
        baseCount = (int)(baseCount * swarmMult);
    }
    if (event == WaveEvent::BOSS_ESCORT) {
        baseCount = std::max(laneCount, 1 + wave * 2);
    }
    if (event == WaveEvent::ELITE_RUSH) {
        int minElite = std::max(4, laneCount * 2);
        baseCount = std::max(minElite, baseCount / 2);
    }
    if (event == WaveEvent::BLACKOUT && laneCount >= 4) {
        baseCount = (int)(baseCount * 0.88f);
    }

    return std::max(baseCount, laneCount);
}

static float SpawnIntervalForWave(int wave, int laneCount, Game::WaveEvent event) {
    float interval = std::max(0.15f, 0.75f - wave * 0.035f);
    if (laneCount >= 6)      interval *= 1.28f;
    else if (laneCount >= 4) interval *= 1.18f;

    if (event == WaveEvent::SWARM)       interval *= 0.78f;
    if (event == WaveEvent::BOSS_ESCORT) interval *= 1.08f;
    if (event == WaveEvent::BLACKOUT)    interval *= 1.12f;

    return std::max(0.16f, interval);
}

static float BlackoutSensorRangeMultiplier(int laneCount) {
    if (laneCount >= 6) return 0.55f;
    if (laneCount >= 4) return 0.45f;
    return 0.30f;
}

static std::string RouteThresholdNotice(int wave, int laneCount) {
    switch (wave) {
        case 7:  return "四方向入口開放：路線仍為單線，但可能從任意方向進攻。";
        case 10: return "雙線同步進攻開始：兩條路線會同時出兵。";
        case 16: return "四線同步進攻開始：建議補強各入口覆蓋。";
        case 30: return "高壓多線模式啟動：最多六線同步進攻。";
        default: break;
    }
    if (laneCount >= 6) return "高壓多線模式：六線壓力持續，保留補強空間。";
    return "";
}

static bool RoutePoolSupportsAllEntrySides(int wave) {
    return wave >= 7;
}

static bool RoutePoolOpensAtWave(int wave) {
    return RoutePoolSupportsAllEntrySides(wave) && !RoutePoolSupportsAllEntrySides(wave - 1);
}

using RoutePlan = std::array<int, MAX_LANES>;

static int EntrySideIndex(PathEntrySide side) {
    switch (side) {
        case PathEntrySide::LEFT:   return 0;
        case PathEntrySide::TOP:    return 1;
        case PathEntrySide::RIGHT:  return 2;
        case PathEntrySide::BOTTOM: return 3;
        default:                    return 0;
    }
}

static PathEntrySide EntrySideFromIndex(int sideIdx) {
    switch (sideIdx) {
        case 1:  return PathEntrySide::TOP;
        case 2:  return PathEntrySide::RIGHT;
        case 3:  return PathEntrySide::BOTTOM;
        default: return PathEntrySide::LEFT;
    }
}

static RoutePlan CurrentRoutePlan(Game& G, int laneCount) {
    RoutePlan plan{};
    plan.fill(-1);
    int count = std::max(1, std::min(MAX_LANES, laneCount));
    for (int lane = 0; lane < count; lane++) {
        plan[lane] = G.LanePresetIdx(lane);
    }
    return plan;
}

static bool RouteAllowedForWave(int presetIdx, int wave) {
    return RoutePoolSupportsAllEntrySides(wave) ||
           GetPathPreset(presetIdx).entrySide == PathEntrySide::LEFT;
}

static bool PlanContainsPreset(const RoutePlan& plan, int slot, int presetIdx) {
    for (int lane = 0; lane < slot; lane++) {
        if (plan[lane] == presetIdx) return true;
    }
    return false;
}

static bool PlanContainsEntrySide(const std::array<int, MAX_LANES>& sidePlan, int slot, int sideIdx) {
    for (int lane = 0; lane < slot; lane++) {
        if (sidePlan[lane] == sideIdx) return true;
    }
    return false;
}

static int PresetLastUsed(const Game& G, int presetIdx) {
    if (presetIdx >= 0 && presetIdx < (int)G.lastPresetWave.size()) {
        return G.lastPresetWave[presetIdx];
    }
    return -100000;
}

static bool HasPresetForSide(const RoutePlan& plan, int slot,
                             int sideIdx, int targetWave, bool requireUnused) {
    PathEntrySide side = EntrySideFromIndex(sideIdx);
    for (int idx = 0; idx < PATH_PRESET_COUNT; idx++) {
        if (!RouteAllowedForWave(idx, targetWave)) continue;
        if (GetPathPreset(idx).entrySide != side) continue;
        if (requireUnused && PlanContainsPreset(plan, slot, idx)) continue;
        return true;
    }
    return false;
}

static int PickEntrySideForSlot(Game& G, const RoutePlan& plan,
                                const std::array<int, MAX_LANES>& sidePlan,
                                int slot, int laneCount, int targetWave) {
    if (!RoutePoolSupportsAllEntrySides(targetWave)) return EntrySideIndex(PathEntrySide::LEFT);

    bool requireUniqueSide = laneCount <= ENTRY_SIDE_COUNT || slot < ENTRY_SIDE_COUNT;
    std::vector<int> candidates;

    for (int sideIdx = 0; sideIdx < ENTRY_SIDE_COUNT; sideIdx++) {
        if (requireUniqueSide && PlanContainsEntrySide(sidePlan, slot, sideIdx)) continue;
        if (!HasPresetForSide(plan, slot, sideIdx, targetWave, true)) continue;
        candidates.push_back(sideIdx);
    }

    if (candidates.empty()) {
        for (int sideIdx = 0; sideIdx < ENTRY_SIDE_COUNT; sideIdx++) {
            if (!HasPresetForSide(plan, slot, sideIdx, targetWave, true)) continue;
            candidates.push_back(sideIdx);
        }
    }

    if (candidates.empty()) {
        for (int sideIdx = 0; sideIdx < ENTRY_SIDE_COUNT; sideIdx++) {
            if (!HasPresetForSide(plan, slot, sideIdx, targetWave, false)) continue;
            candidates.push_back(sideIdx);
        }
    }

    int oldestWave = 1000000;
    std::vector<int> oldestSides;
    for (int sideIdx : candidates) {
        int last = G.lastEntrySideWave[sideIdx];
        if (last < oldestWave) {
            oldestWave = last;
            oldestSides.clear();
            oldestSides.push_back(sideIdx);
        } else if (last == oldestWave) {
            oldestSides.push_back(sideIdx);
        }
    }

    if (oldestSides.empty()) return EntrySideIndex(PathEntrySide::LEFT);
    std::uniform_int_distribution<int> pick(0, (int)oldestSides.size() - 1);
    return oldestSides[pick(G.rng)];
}

static int PickPresetForSide(Game& G, const RoutePlan& plan, int slot,
                             int sideIdx, int targetWave, bool requireUnused) {
    PathEntrySide side = EntrySideFromIndex(sideIdx);
    std::vector<int> candidates;
    for (int idx = 0; idx < PATH_PRESET_COUNT; idx++) {
        if (!RouteAllowedForWave(idx, targetWave)) continue;
        if (GetPathPreset(idx).entrySide != side) continue;
        if (requireUnused && PlanContainsPreset(plan, slot, idx)) continue;
        candidates.push_back(idx);
    }

    int oldestWave = 1000000;
    std::vector<int> oldestPresets;
    for (int presetIdx : candidates) {
        int last = PresetLastUsed(G, presetIdx);
        if (last < oldestWave) {
            oldestWave = last;
            oldestPresets.clear();
            oldestPresets.push_back(presetIdx);
        } else if (last == oldestWave) {
            oldestPresets.push_back(presetIdx);
        }
    }

    if (oldestPresets.empty()) return -1;
    std::uniform_int_distribution<int> pick(0, (int)oldestPresets.size() - 1);
    return oldestPresets[pick(G.rng)];
}

static RoutePlan PlanNextRouteSet(Game& G, int laneCount, int targetWave) {
    RoutePlan plan = CurrentRoutePlan(G, laneCount);
    int count = std::max(1, std::min(MAX_LANES, laneCount));
    plan.fill(-1);
    std::array<int, MAX_LANES> sidePlan{};
    sidePlan.fill(-1);

    for (int slot = 0; slot < count; slot++) {
        int sideIdx = PickEntrySideForSlot(G, plan, sidePlan, slot, count, targetWave);
        int presetIdx = PickPresetForSide(G, plan, slot, sideIdx, targetWave, true);
        if (presetIdx < 0) {
            presetIdx = PickPresetForSide(G, plan, slot, sideIdx, targetWave, false);
        }
        plan[slot] = (presetIdx >= 0) ? presetIdx : G.LanePresetIdx(slot);
        sidePlan[slot] = EntrySideIndex(GetPathPreset(plan[slot]).entrySide);
    }

    return plan;
}

static void RefreshPreviewCells(Game& G) {
    int count = std::max(1, std::min(MAX_LANES, G.nextPreviewLaneCount));
    for (int lane = 0; lane < MAX_LANES; lane++) {
        if (lane < count && G.nextPreviewPaths[lane] >= 0) {
            G.nextPreviewCells[lane] = BuildPresetPathCells(G.nextPreviewPaths[lane]);
        } else {
            G.nextPreviewCells[lane].clear();
        }
    }
}

static void UpdateUpcomingRoutePlan(Game& G) {
    int nextWave = G.wave + 1;
    G.nextPreviewLaneCount = LaneCountForWave(nextWave);

    bool mustReplan = WaveRotatesRoutes(nextWave) ||
                      RoutePoolOpensAtWave(nextWave) ||
                      G.nextPreviewLaneCount != G.ActiveLaneCount();

    if (!mustReplan) {
        G.hasPlannedRouteChange = false;
        G.nextPreviewPaths = CurrentRoutePlan(G, G.nextPreviewLaneCount);
        for (auto& cells : G.nextPreviewCells) cells.clear();
        return;
    }

    G.hasPlannedRouteChange = true;
    G.nextPreviewPaths = PlanNextRouteSet(G, G.nextPreviewLaneCount, nextWave);
    RefreshPreviewCells(G);
}

static void ApplyRoutePlan(Game& G, const RoutePlan& plan, int laneCount) {
    int count = std::max(1, std::min(MAX_LANES, laneCount));
    for (int lane = 0; lane < count; lane++) {
        int presetIdx = (plan[lane] >= 0) ? plan[lane] : G.LanePresetIdx(lane);
        SetActiveLanePreset(lane, presetIdx);
    }

    G.activeLaneCount = count;
    G.dualPath = count > 1;

    for (int lane = 0; lane < count; lane++) {
        int presetIdx = G.LanePresetIdx(lane);
        int sideIdx = EntrySideIndex(G.LanePreset(lane).entrySide);
        G.lastEntrySideWave[sideIdx] = G.wave;
        if (presetIdx >= 0 && presetIdx < (int)G.lastPresetWave.size()) {
            G.lastPresetWave[presetIdx] = G.wave;
        }
    }
}

static int DemolishTowersOnActivePaths(Game& G, int laneCount) {
    int count = std::max(1, std::min(MAX_LANES, laneCount));
    int demolished = 0;
    G.towers.erase(
        std::remove_if(G.towers.begin(), G.towers.end(),
            [&](const Tower& t) {
                if (t.type == TType::CPU) return false;
                bool onPath = IsAnyActivePathCell(t.gx, t.gy, count);
                if (onPath) {
                    G.credits += TDef(t.type).baseCost;
                    demolished++;
                }
                return onPath;
            }),
        G.towers.end()
    );
    return demolished;
}

static void RemoveDanglingTowerConnections(Game& G) {
    for (auto& tw : G.towers) {
        tw.conns.erase(
            std::remove_if(tw.conns.begin(), tw.conns.end(),
                [&](int id) { return G.FindTower(id) == nullptr; }),
            tw.conns.end()
        );
    }
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
    if (!HasTowerSkillSlot(t.type)) return;

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
                EmitMuzzleVfx(G, sp, dir, RED, true);
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

static float LaneOpeningThreat(Game& G, int laneSlot) {
    const auto& cells = G.LaneCells(laneSlot);
    float sum = 0.f;
    int   n   = std::min((int)cells.size(), 10);
    if (n <= 0) return 0.f;

    for (int k = 0; k < n; k++) {
        sum += G.threatMap.Get(cells[k].gx, cells[k].gy);
    }

    float maxT = G.threatMap.GetMax();
    return (maxT > 0.f) ? (sum / n) / maxT : 0.f;
}

static void UpdateLegacyPathIntelThreat(Game& G) {
    for (float& threat : G.intel.pathAvgThreat) threat = 0.f;

    int tracked = G.ActiveLaneCount();
    for (int lane = 0; lane < tracked; lane++) {
        G.intel.pathAvgThreat[lane] = LaneOpeningThreat(G, lane);
    }
}

static int NextSpawnLane(Game& G) {
    int laneCount = G.ActiveLaneCount();
    if (laneCount <= 1) return G.LaneCells(0).empty() ? -1 : 0;

    if (G.spawnLaneCursor < laneCount) {
        int lane = G.spawnLaneCursor++;
        if (!G.LaneCells(lane).empty()) return lane;
    }

    if (G.intel.adapted) {
        std::uniform_real_distribution<float> rd(0.f, 1.f);
        int lane = G.intel.PickPath(rd(G.rng), laneCount);
        if (!G.LaneCells(lane).empty()) return lane;
    }

    for (int attempt = 0; attempt < laneCount; attempt++) {
        int lane = G.spawnLaneCursor % laneCount;
        G.spawnLaneCursor = (G.spawnLaneCursor + 1) % laneCount;
        if (!G.LaneCells(lane).empty()) return lane;
    }

    return G.LaneCells(0).empty() ? -1 : 0;
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

    int spawnLane = NextSpawnLane(G);
    if (spawnLane < 0) return;

    Enemy e;
    e.id           = G.nextId++;
    e.type         = et;
    e.pathPos      = 0.f;
    e.angle        = 0.f;
    e.bossState    = BossState::CHARGE;
    e.evadeSpdMult = 1.f;
    e.pathIdx      = spawnLane;

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
            e.spd *= (G.ActiveLaneCount() >= 4) ? 1.65f : 2.f; break;
        case WaveEvent::MUTANT:
            e.hp *= 2.0f; e.maxHp = e.hp;
            e.spd *= (G.ActiveLaneCount() >= 4) ? 1.15f : 1.3f; e.armor = 0.f;
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

    UpdateLegacyPathIntelThreat(G);
    G.enemies.push_back(e);

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
    if (pi >= 0 && pi < MAX_LANES) G.intel.pathSpawned[pi]++;
}

// ══════════════════════════════════════════════════════════════════
//  StartWave
// ══════════════════════════════════════════════════════════════════
void StartWave(Game& G) {
    if (G.phase != Game::BUILD) return;
    if (G.tutorial.active) {
        StartTutorialWave(G);
        return;
    }

    G.wave++;
    int laneCount = LaneCountForWave(G.wave);
    std::string routeNotice;

    // ── 路線輪換 / 門檻波次強制重組 ─────────────────────────────
    bool mustReplan = WaveRotatesRoutes(G.wave) ||
                      RoutePoolOpensAtWave(G.wave) ||
                      laneCount != G.ActiveLaneCount();

    if (mustReplan) {
        RoutePlan routePlan = (G.hasPlannedRouteChange && G.nextPreviewLaneCount == laneCount)
            ? G.nextPreviewPaths
            : PlanNextRouteSet(G, laneCount, G.wave);
        ApplyRoutePlan(G, routePlan, laneCount);

        int activeLaneCount = G.ActiveLaneCount();
        int demolished = DemolishTowersOnActivePaths(G, activeLaneCount);
        RemoveDanglingTowerConnections(G);

        int replanCredit = RouteReplanCreditForLaneCount(activeLaneCount, G.wave);
        G.credits += replanCredit;

        char pb[180];
        if (demolished > 0) {
            if (activeLaneCount == 1)
                snprintf(pb, 180, "路徑重組！+%dCR。%d座塔被占用（全額退款）。新路線：%s",
                    replanCredit, demolished, G.LanePreset(0).name);
            else if (activeLaneCount == 2)
                snprintf(pb, 180, "雙路徑重組！+%dCR。%d座塔被占用（全額退款）。主：%s | 副：%s",
                    replanCredit, demolished, G.LanePreset(0).name, G.LanePreset(1).name);
            else
                snprintf(pb, 180, "%d路徑重組！+%dCR。%d座塔被占用（全額退款）。",
                    activeLaneCount, replanCredit, demolished);
        } else {
            if (activeLaneCount == 1)
                snprintf(pb, 180, "路徑重組！+%dCR。新路線：%s", replanCredit, G.LanePreset(0).name);
            else if (activeLaneCount == 2)
                snprintf(pb, 180, "雙路徑重組！+%dCR。主：%s | 副：%s",
                    replanCredit, G.LanePreset(0).name, G.LanePreset(1).name);
            else
                snprintf(pb, 180, "%d路徑重組！+%dCR。", activeLaneCount, replanCredit);
        }
        routeNotice = pb;
    }

    UpdateUpcomingRoutePlan(G);
    G.currentEvent     = Game::RollEvent(G.wave, G.rng);
    G.eventName        = Game::EventName(G.currentEvent);
    G.eventBannerTimer = 4.f;
    G.blackoutActive   = (G.currentEvent == WaveEvent::BLACKOUT);
    G.currentIncident     = Game::Incident::NONE;
    G.incidentName        = "";
    G.incidentTimer       = 0.f;
    G.incidentBannerTimer = 0.f;
    G.incidentTriggered   = false;
    if (G.wave >= 3) {
        std::uniform_real_distribution<float> delay(4.5f, 8.0f);
        G.incidentRollTimer = delay(G.rng);
    } else {
        G.incidentRollTimer = 0.f;
        G.incidentTriggered = true;
    }
    if (G.tutorial.active) {
        G.currentEvent = WaveEvent::NONE;
        G.eventName = "";
        G.eventBannerTimer = 0.f;
        G.blackoutActive = false;
        G.incidentRollTimer = 0.f;
        G.incidentTriggered = true;
    }

    int baseCount = BalancedWaveCountForLanes(G.currentEvent, G.wave, G.ActiveLaneCount());

    G.waveCount   = baseCount;
    G.spawned     = 0;
    G.spawnLaneCursor = 0;
    G.spawnTimer  = 0.f;
    G.phase       = Game::FIGHT;
    RecordTutorialWaveStarted(G);
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

    std::string thresholdNotice = RouteThresholdNotice(G.wave, G.ActiveLaneCount());
    char buf[128];
    if (G.eventName.empty() || G.currentEvent == WaveEvent::NONE)
        snprintf(buf, 128, "第%d波 — %d 個病毒！", G.wave, G.waveCount);
    else
        snprintf(buf, 128, "第%d波 %s (%d 個)", G.wave, G.eventName.c_str(), G.waveCount);

    std::string waveMsg = buf;
    if (!thresholdNotice.empty()) waveMsg = thresholdNotice + " | " + waveMsg;
    if (!routeNotice.empty()) G.SetMsg(routeNotice + " | " + waveMsg);
    else                      G.SetMsg(waveMsg);
}

// ══════════════════════════════════════════════════════════════════
//  Update  —  主更新迴圈
// ══════════════════════════════════════════════════════════════════
void Update(Game& G, float dt) {
    if (G.paused || G.phase == Game::GAMEOVER ||
        G.phase == Game::MENU || G.phase == Game::TUTORIAL_SELECT) return;

    UpdateTutorial(G, dt);

    // ── 倒數計時器 ───────────────────────────────────────────────
    if (G.msgTimer         > 0) G.msgTimer         -= dt;
    if (G.shakeT           > 0) G.shakeT           -= dt;
    if (G.eventBannerTimer > 0) G.eventBannerTimer  -= dt;
    if (G.incidentBannerTimer > 0) G.incidentBannerTimer -= dt;
    if (G.waveTelegraphTimer > 0) G.waveTelegraphTimer -= dt;
    if (G.spawnPulseTimer > 0)    G.spawnPulseTimer    -= dt;
    if (G.incidentRollTimer > 0)  G.incidentRollTimer  -= dt;
    if (G.incidentTimer > 0) {
        G.incidentTimer -= dt;
        if (G.incidentTimer <= 0.f) {
            G.currentIncident = Game::Incident::NONE;
            G.incidentName = "";
        }
    }

    if (G.phase == Game::FIGHT && G.waveTelegraphTimer <= 0.f &&
        !G.incidentTriggered && G.spawned > 0 && G.incidentRollTimer <= 0.f) {
        TriggerIncident(G);
    }

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
        float interval = SpawnIntervalForWave(G.wave, G.ActiveLaneCount(), G.currentEvent);
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
        EmitMuzzleVfx(G, sp, dir, bcol, crit || surgeShot || hasSplash);

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
                EmitMuzzleVfx(G, sp, dir2, COL_OR, false);
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
                    EmitImpactVfx(G, ep, Color{150, 220, 255, 255}, true);
                    G.AddFloat(ep, "護盾破！", Color{150, 220, 255, 255});
                }
            }
            e.hp -= actualDmg;
            e.flashTimer = 0.15f;
            G.waveDmg   += actualDmg;
            EmitImpactVfx(G, ep, b.crit ? YELLOW : b.col,
                          b.crit || b.splash || e.type == EType::BOSS || actualDmg >= 60.f);
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
                            EmitImpactVfx(G, G.EnemyWorld(e2), Color{150, 220, 255, 255}, true);
                            G.AddFloat(G.EnemyWorld(e2), "護盾破！", Color{150, 220, 255, 255});
                        }
                    }
                    e2.hp -= splashDmg;
                    e2.flashTimer = std::max(e2.flashTimer, 0.08f);
                    EmitImpactVfx(G, G.EnemyWorld(e2), Color{255, 180, 50, 255}, false);
                }
                EmitImpactVfx(G, b.pos, Color{255, 120, 40, 255}, true);
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
    if (G.bullets.size() > Game::MAX_BULLETS) {
        G.bullets.erase(G.bullets.begin(), G.bullets.begin() + (G.bullets.size() - Game::MAX_BULLETS));
    }

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
        if (G.currentIncident == Game::Incident::BOUNTY_WINDOW && G.incidentTimer > 0.f) comboMult += 0.35f;
        int reward = (int)(it->reward * comboMult);
        G.credits += reward;
        G.score   += (int)(reward * 1.5f);
        if (!G.tutorial.active && G.score > G.highScore) G.highScore = G.score;
        RecordTutorialEnemyKilled(G, it->type);

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

        const auto& pathRef = G.EnemyLaneCells(e);
        if (pathRef.size() < 2) {
            e.hp = 0.f;
            continue;
        }

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
        if (G.currentIncident == Game::Incident::ROUTE_SURGE && G.incidentTimer > 0.f) speed *= 1.22f;
        e.pathPos += speed * dt;

        if (e.pathPos >= (float)((int)pathRef.size() - 1)) {
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
            if (pi >= 0 && pi < MAX_LANES) G.intel.pathSurvived[pi]++;

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
    if (G.particles.size() > Game::MAX_PARTICLES) {
        G.particles.erase(G.particles.begin(), G.particles.begin() + (G.particles.size() - Game::MAX_PARTICLES));
    }

    // ── 浮動文字更新 ─────────────────────────────────────────────
    for (auto& f : G.floats) { f.pos.y -= 28.f * dt; f.life -= dt; }
    G.floats.erase(std::remove_if(G.floats.begin(), G.floats.end(), [](const FloatText& f) { return f.life <= 0; }), G.floats.end());
    if (G.floats.size() > Game::MAX_FLOATS) {
        G.floats.erase(G.floats.begin(), G.floats.begin() + (G.floats.size() - Game::MAX_FLOATS));
    }

    // ── 波次結束 ─────────────────────────────────────────────────
    bool waveFinished = (G.phase == Game::FIGHT) && G.enemies.empty() && (G.spawned >= G.waveCount);
    if (waveFinished) {
        if (G.tutorial.active) {
            RecordTutorialSurvived(G);
            G.blackoutActive = false;
            G.waveTelegraphTimer = 0.f;
            G.spawnPulseTimer = 0.f;
            G.currentIncident = Game::Incident::NONE;
            G.incidentName = "";
            G.incidentTimer = 0.f;
            G.incidentBannerTimer = 0.f;
            G.incidentRollTimer = 0.f;
            G.combo = 0;
            G.comboTimer = 0.f;
            G.comboSurgeTimer = 0.f;
            G.phase = Game::BUILD;
            G.trainingTimer = 0.f;
            G.trainingChoiceCount = 0;
            G.placing = TType::NONE;
            G.connectSrc = -1;
            G.SetMsg("教學短波次結束。後續 Phase 將接上章節完成條件。");
            return;
        }

        int bonus = 50 + G.wave * 10;
        G.blackoutActive = false;
        G.waveTelegraphTimer = 0.f;
        G.spawnPulseTimer = 0.f;
        G.currentIncident = Game::Incident::NONE;
        G.incidentName = "";
        G.incidentTimer = 0.f;
        G.incidentBannerTimer = 0.f;
        G.incidentRollTimer = 0.f;
        G.combo = 0;
        G.comboTimer = 0.f;
        G.comboSurgeTimer = 0.f;
        if (G.waveEscaped == 0) {
            bonus = (int)(bonus * 1.5f);
        }
        G.credits      += bonus;
        G.threatMap.Decay(0.85f);
        G.intel.LearnFromWave(G.ActiveLaneCount());
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
        if (G.tutorial.active) {
            G.lives = 0;
            G.cpuHp = 0.f;
            G.enemies.clear();
            G.bullets.clear();
            G.phase = Game::BUILD;
            G.paused = true;
            G.tutorial.exitPromptOpen = true;
            G.tutorial.exitPromptChoice = 0;
            G.SetMsg("教學失敗：可繼續查看、重開章節或退出。");
            return;
        }
        G.lives = 0; G.cpuHp = 0; G.phase = Game::GAMEOVER;
        if (G.score > G.highScore) G.highScore = G.score;

        int totalKills = 0;
        for (const auto& tower : G.towers) {
            totalKills += tower.kills;
        }
        G.highScoreMgr.AddScore(TowerDefenseScore("Player", G.wave, totalKills, 0.f));
        G.highScoreMgr.SaveToFile();

        G.SetMsg("防線崩潰 — 遊戲結束！");
        G.Shake(25.f, 1.f);
    }
}
