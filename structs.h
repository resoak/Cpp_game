#pragma once
// ================================================================
//  structs.h — PerceptronLearner、ThreatMap、Tower、Enemy 等結構體
// ================================================================
#include "constants.h"
#include "raylib.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <random>

// ══════════════════════════════════════════════════════════════════
//  PerceptronLearner（Delta Rule 梯度更新）
// ══════════════════════════════════════════════════════════════════
struct PerceptronLearner {
    float accInput1{0.f};
    float accInput2{0.f};
    float accOutput{0.f};
    int   samples{0};
    float lastLoss{0.f};
    float lrDecay{1.f};

    void Accumulate(float in1, float in2, float out) {
        accInput1 += in1;
        accInput2 += in2;
        accOutput += out;
        samples++;
    }

    void Reset() {
        accInput1 = accInput2 = accOutput = 0.f;
        samples = 0;
    }

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

        w1   = std::max(-2.f, std::min(2.f, w1));
        w2   = std::max(-2.f, std::min(2.f, w2));
        bias = std::max(-2.f, std::min(2.f, bias));

        lastLoss  = error * error;
        lrDecay  *= 0.92f;
        Reset();
    }
};

// ══════════════════════════════════════════════════════════════════
//  ThreatMap（擊殺密度熱圖）
// ══════════════════════════════════════════════════════════════════
struct ThreatMap {
    float cell[24][20]{};

    void AddKill(int gx, int gy, float value = 1.f) {
        if (gx >= 0 && gx < 24 && gy >= 0 && gy < 20) {
            cell[gx][gy] += value;
        }
    }

    float Get(int gx, int gy) const {
        if (gx >= 0 && gx < 24 && gy >= 0 && gy < 20) return cell[gx][gy];
        return 0.f;
    }

    float GetMax() const {
        float mx = 0.f;
        for (int x = 0; x < 24; x++)
            for (int y = 0; y < 20; y++)
                mx = std::max(mx, cell[x][y]);
        return mx;
    }

    void Decay(float factor = 0.85f) {
        for (int x = 0; x < 24; x++)
            for (int y = 0; y < 20; y++)
                cell[x][y] *= factor;
    }
};

// ══════════════════════════════════════════════════════════════════
//  EnemyIntel  —  敵人學習系統（追蹤存活率，自適應兵種與路線）
// ══════════════════════════════════════════════════════════════════
// ══════════════════════════════════════════════════════════════════
//  EnemyBrain  —  敵方路線評估神經網路（3 輸入 → 2 隱藏 → 1 輸出）
//
//  輸入：threatAhead（前方威脅）、hpRatio（血量比）、survRate（歷史存活率）
//  輸出：路線安全分數 [0,1]
//  學習：波末根據實際存活率做梯度下降
// ══════════════════════════════════════════════════════════════════
struct EnemyBrain {
    float wIH[3][2] = {{ 0.5f,-0.3f},{-0.4f, 0.6f},{ 0.2f,-0.1f}};
    float bH[2]     = { 0.f,  0.f };
    float wHO[2]    = { 0.6f,-0.4f };
    float bO        = 0.f;
    float lastLoss  = 0.f;
    int   trainCount= 0;

    float Eval(float threatAhead, float hpRatio, float survRate) const {
        float in[3] = { threatAhead, hpRatio, survRate };
        float h[2];
        for (int j = 0; j < 2; j++) {
            float s = bH[j];
            for (int i = 0; i < 3; i++) s += wIH[i][j] * in[i];
            h[j] = 1.f / (1.f + expf(-s));
        }
        float o = bO;
        for (int j = 0; j < 2; j++) o += wHO[j] * h[j];
        return 1.f / (1.f + expf(-o));
    }

    void Learn(float threatAhead, float hpRatio, float survRate, float target, float lr = 0.12f) {
        float in[3] = { threatAhead, hpRatio, survRate };
        float h[2];
        for (int j = 0; j < 2; j++) {
            float s = bH[j];
            for (int i = 0; i < 3; i++) s += wIH[i][j] * in[i];
            h[j] = 1.f / (1.f + expf(-s));
        }
        float o_raw = bO;
        for (int j = 0; j < 2; j++) o_raw += wHO[j] * h[j];
        float o  = 1.f / (1.f + expf(-o_raw));
        float dO = (target - o) * o * (1.f - o);
        lastLoss = (target - o) * (target - o);
        trainCount++;
        for (int j = 0; j < 2; j++) wHO[j] += lr * dO * h[j];
        bO += lr * dO;
        for (int j = 0; j < 2; j++) {
            float dH = dO * wHO[j] * h[j] * (1.f - h[j]);
            bH[j] += lr * dH;
            for (int i = 0; i < 3; i++) wIH[i][j] += lr * dH * in[i];
        }
        auto clamp3 = [](float v){ return v<-3.f?-3.f:(v>3.f?3.f:v); };
        auto clamp2 = [](float v){ return v<-2.f?-2.f:(v>2.f?2.f:v); };
        for (int j = 0; j < 2; j++) {
            wHO[j] = clamp3(wHO[j]); bH[j] = clamp2(bH[j]);
            for (int i = 0; i < 3; i++) wIH[i][j] = clamp3(wIH[i][j]);
        }
        bO = clamp2(bO);
    }
};

// ══════════════════════════════════════════════════════════════════
//  EnemyIntel  —  敵方整體學習系統
// ══════════════════════════════════════════════════════════════════
struct EnemyIntel {
    EnemyBrain brain;

    float typeSurvRate[5]{ 0.2f, 0.2f, 0.2f, 0.2f, 0.2f };
    float pathSurvRate[2]{ 0.4f, 0.4f };

    int typeSpawned[5]{};
    int typeSurvived[5]{};
    int pathSpawned[2]{};
    int pathSurvived[2]{};
    float pathAvgThreat[2]{ 0.f, 0.f };

    int  wavesLearned{ 0 };
    bool adapted{ false };

    void ResetWave() {
        for (int i = 0; i < 5; i++) { typeSpawned[i] = 0; typeSurvived[i] = 0; }
        for (int i = 0; i < 2; i++) { pathSpawned[i] = 0; pathSurvived[i] = 0; pathAvgThreat[i] = 0.f; }
    }

    void LearnFromWave() {
        constexpr float LR = 0.40f;
        for (int i = 0; i < 5; i++) {
            if (typeSpawned[i] > 0) {
                float rate = (float)typeSurvived[i] / typeSpawned[i];
                typeSurvRate[i] = typeSurvRate[i]*(1.f-LR) + rate*LR;
                typeSurvRate[i] = std::max(0.05f, std::min(0.95f, typeSurvRate[i]));
            }
        }
        for (int i = 0; i < 2; i++) {
            if (pathSpawned[i] > 0) {
                float rate = (float)pathSurvived[i] / pathSpawned[i];
                pathSurvRate[i] = pathSurvRate[i]*(1.f-LR) + rate*LR;
                pathSurvRate[i] = std::max(0.05f, std::min(0.95f, pathSurvRate[i]));
            }
        }
        float total = pathSurvRate[0] + pathSurvRate[1] + 0.001f;
        for (int i = 0; i < 2; i++) {
            float target = pathSurvRate[i] / total;
            brain.Learn(pathAvgThreat[i], 0.5f, pathSurvRate[i], target);
        }
        wavesLearned++;
        if (wavesLearned >= 2) adapted = true;
        ResetWave();
    }

    int BrainPickPath(float threat0, float threat1) const {
        float s0 = brain.Eval(threat0, 1.f, pathSurvRate[0]);
        float s1 = brain.Eval(threat1, 1.f, pathSurvRate[1]);
        return (s0 >= s1) ? 0 : 1;
    }

    EType WeightedType(float rand01, int wave) const {
        if (wave < 3) return (rand01 < 0.33f) ? EType::FAST : EType::BASIC;
        float weights[4];
        for (int i = 0; i < 4; i++) weights[i] = 0.15f + 0.85f * typeSurvRate[i];
        if (wave < 5) { weights[2] *= 0.3f; weights[3] *= 0.1f; }
        else if (wave < 8) { weights[3] *= 0.5f; }
        float total = 0.f;
        for (int i = 0; i < 4; i++) total += weights[i];
        float r = rand01 * total, acc = 0.f;
        for (int i = 0; i < 4; i++) { acc += weights[i]; if (r <= acc) return (EType)i; }
        return EType::BASIC;
    }

    int PickPath(float rand01, bool dualPath) const {
        if (!dualPath || !adapted) return (rand01 < 0.5f) ? 0 : 1;
        float total = pathSurvRate[0] + pathSurvRate[1];
        if (total <= 0.f) return (rand01 < 0.5f) ? 0 : 1;
        return (rand01 * total < pathSurvRate[0]) ? 0 : 1;
    }

    const char* TopTypeName() const {
        static const char* NAMES[] = { "基礎兵","快速兵","裝甲兵","精英兵","BOSS" };
        int best = 0;
        for (int i = 1; i < 4; i++) if (typeSurvRate[i] > typeSurvRate[best]) best = i;
        return NAMES[best];
    }

    int PreferredPath() const { return (pathSurvRate[1] > pathSurvRate[0]) ? 1 : 0; }
};


// ══════════════════════════════════════════════════════════════════
//  DefenseAdvisorNN  —  防禦建議神經網路（6 輸入 → 4 隱藏 → 4 輸出）
//
//  輸入（6個）：
//    [0] armor_surv     裝甲兵存活率
//    [1] fast_surv      快速兵存活率
//    [2] elite_surv     精英兵存活率
//    [3] front_cov      前段路線覆蓋率（0~1）
//    [4] mid_cov        中段路線覆蓋率
//    [5] rear_cov       後段路線覆蓋率
//
//  輸出（softmax，4類）：
//    0=SENSOR  1=AND  2=XOR  3=CANNON
//
//  學習：波末以「最危險的敵種對應的剋制塔」為目標，交叉熵反向傳播
// ══════════════════════════════════════════════════════════════════
struct DefenseAdvisorNN {
    float wIH[6][4]{};    // 輸入 → 隱藏
    float bH[4]{};
    float wHO[4][4]{};    // 隱藏 → 輸出
    float bO[4]{};
    float lastProb[4]{ 0.25f, 0.25f, 0.25f, 0.25f };
    float lastLoss{ 0.f };
    int   trainCount{ 0 };

    DefenseAdvisorNN() {
        // 固定小值初始化（不依賴 <random>）
        static const float K[24] = {
             0.10f,-0.20f, 0.15f,-0.12f,
            -0.18f, 0.22f,-0.08f, 0.16f,
             0.20f,-0.14f, 0.18f,-0.22f,
            -0.10f, 0.20f,-0.15f, 0.12f,
             0.18f,-0.22f, 0.08f,-0.16f,
            -0.20f, 0.14f,-0.18f, 0.22f
        };
        for (int i = 0; i < 6; i++)
            for (int j = 0; j < 4; j++)
                wIH[i][j] = K[(i * 4 + j) % 24] * 0.5f;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                wHO[i][j] = K[(i * 4 + j) % 24] * 0.3f;
    }

    // ── 前向傳播（tanh 隱藏層 + softmax 輸出）───────────────────
    void Forward(const float in[6], float prob[4]) const {
        float h[4];
        for (int j = 0; j < 4; j++) {
            float s = bH[j];
            for (int i = 0; i < 6; i++) s += wIH[i][j] * in[i];
            h[j] = tanhf(s);
        }
        float mx = -1e9f;
        for (int j = 0; j < 4; j++) {
            float s = bO[j];
            for (int i = 0; i < 4; i++) s += wHO[i][j] * h[i];
            prob[j] = s;
            if (s > mx) mx = s;
        }
        float sum = 0.f;
        for (int j = 0; j < 4; j++) { prob[j] = expf(prob[j] - mx); sum += prob[j]; }
        for (int j = 0; j < 4; j++) prob[j] /= sum;
    }

    // ── 推薦：回傳最高機率的類型索引 ────────────────────────────
    int Recommend(const float in[6]) {
        Forward(in, lastProb);
        int best = 0;
        for (int j = 1; j < 4; j++) if (lastProb[j] > lastProb[best]) best = j;
        return best;
    }

    // ── 反向傳播（交叉熵損失）────────────────────────────────────
    void Learn(const float in[6], int targetClass, float lr = 0.08f) {
        float h[4];
        for (int j = 0; j < 4; j++) {
            float s = bH[j];
            for (int i = 0; i < 6; i++) s += wIH[i][j] * in[i];
            h[j] = tanhf(s);
        }
        float prob[4], mx = -1e9f;
        for (int j = 0; j < 4; j++) {
            float s = bO[j];
            for (int i = 0; i < 4; i++) s += wHO[i][j] * h[i];
            prob[j] = s;
            if (s > mx) mx = s;
        }
        float sum = 0.f;
        for (int j = 0; j < 4; j++) { prob[j] = expf(prob[j] - mx); sum += prob[j]; }
        for (int j = 0; j < 4; j++) { prob[j] /= sum; lastProb[j] = prob[j]; }

        lastLoss = -logf(prob[targetClass] + 1e-7f);
        trainCount++;

        // 輸出層梯度（softmax + 交叉熵）
        float dO[4];
        for (int j = 0; j < 4; j++) dO[j] = prob[j] - (j == targetClass ? 1.f : 0.f);

        // 更新輸出層
        for (int j = 0; j < 4; j++) {
            bO[j] -= lr * dO[j];
            for (int i = 0; i < 4; i++) wHO[i][j] -= lr * dO[j] * h[i];
        }

        // 隱藏層梯度（tanh 導數）
        float dH[4] = {};
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) dH[i] += dO[j] * wHO[i][j];
            dH[i] *= (1.f - h[i] * h[i]);
        }
        for (int i = 0; i < 4; i++) {
            bH[i] -= lr * dH[i];
            for (int k = 0; k < 6; k++) wIH[k][i] -= lr * dH[i] * in[k];
        }

        // 權重裁剪
        auto cl = [](float v, float m){ return v < -m ? -m : (v > m ? m : v); };
        for (int i = 0; i < 4; i++) {
            bO[i] = cl(bO[i], 3.f); bH[i] = cl(bH[i], 2.f);
            for (int j = 0; j < 4; j++) { wHO[j][i] = cl(wHO[j][i], 3.f); }
            for (int j = 0; j < 6; j++) { wIH[j][i] = cl(wIH[j][i], 3.f); }
        }
    }
};

// ── AI 顧問提示 ───────────────────────────────────────────────────
struct AIHint {
    int         gx, gy;
    TType       suggest;
    float       score;
    std::string reason;
    float       flashT{0.f};
};

// ══════════════════════════════════════════════════════════════════
//  Tower（防禦塔）
// ══════════════════════════════════════════════════════════════════
struct Tower {
    int   id;
    TType type;
    int   gx, gy;
    int   level{1};

    float sig{0.f};
    float w1{0.8f};
    float w2{0.6f};
    float bias{-0.5f};

    int   cooldown{0};
    float anim{0.f};
    std::vector<int> conns;

    float range{7.f};
    float damage{36.f};
    int   fireRateTicks{18};

    int   kills{0};
    float totalDmg{0.f};
    float upgradeFlash{0.f};
    bool  pulseBurst{false};
    int   burstCount{0};
    float activeCd{0.f};

    PerceptronLearner learner;

    // 感知器訓練目標：下游砲塔射程內有敵人的幀比例
    float pctTargetAcc{ 0.f };
    int   pctTargetSamples{ 0 };

    float sampledIn1{0.f};
    float sampledIn2{0.f};
    float sampledOut{0.f};
    int   sampleCount{0};

    std::vector<float> lossHistory;

    void SampleSignal(float in1, float in2, float out) {
        sampledIn1 += in1;
        sampledIn2 += in2;
        sampledOut += out;
        sampleCount++;
    }

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

enum class EnemyTag {
    NONE,
    BRUTE,
    SWIFT,
    BOUNTY
};

// ══════════════════════════════════════════════════════════════════
//  Enemy（敵人）
// ══════════════════════════════════════════════════════════════════
struct Enemy {
    int   id;
    EType type;

    float pathPos{0.f};
    float hp;
    float maxHp;
    float armor{0.f};
    float spd;
    int   reward;
    float angle{0.f};

    bool  marked{false};
    float markTimer{0.f};
    bool  stealth{false};
    float flashTimer{0.f};
    float regenTimer{0.f};

    float stunTimer{0.f};
    int   pathIdx{0};

    bool  shielded{false};
    float shieldHp{0.f};

    bool  hasSplit{false};

    EnemyTag tag{EnemyTag::NONE};
    float    spawnFx{0.f};

    BossState bossState{BossState::CHARGE};
    float     bossStateTimer{0.f};
    float     evadeSpdMult{1.f};
    float     localThreat{0.f};

    void TickBoss(float dt, float threat, float hpRatio) {
        bossStateTimer -= dt;

        if (hpRatio < 0.30f) {
            if (bossState != BossState::RAMPAGE) {
                bossState      = BossState::RAMPAGE;
                evadeSpdMult   = 2.8f;
                bossStateTimer = 9999.f;
            }
            return;
        }

        if (bossState == BossState::CHARGE) {
            if (threat > 2.5f && bossStateTimer <= 0.f) {
                bossState      = BossState::EVADE;
                evadeSpdMult   = 2.0f;
                bossStateTimer = 1.8f;
            } else {
                evadeSpdMult = 1.0f;
            }
        } else if (bossState == BossState::EVADE) {
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
    int     sourceId{-1};
    float   dmg;
    bool    crit{false};
    bool    splash{false};
    float   splashR{0.f};
    Color   col;
};

struct SigPulse  { Vector2 src, dst; float t; Color col; };
struct Particle  { Vector2 pos, vel; float life, maxLife, radius; Color col; };
struct FloatText { Vector2 pos; std::string text; Color col; float life; };
struct Star      { float x, y, r, bright; };
