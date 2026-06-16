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
#include <array>
#include <utility>

// ══════════════════════════════════════════════════════════════════
//  PerceptronLearner（Delta Rule 梯度更新）
// ══════════════════════════════════════════════════════════════════
class PerceptronLearner {
public:
    float LastLoss() const { return lastLoss_; }
    float LearningRateDecay() const { return lrDecay_; }
    int SampleCount() const { return samples_; }

    void BoostLearningRate(float amount) {
        lrDecay_ = std::min(1.f, lrDecay_ + amount);
    }

    void Accumulate(float in1, float in2, float out) {
        accInput1_ += in1;
        accInput2_ += in2;
        accOutput_ += out;
        samples_++;
    }

    void Reset() {
        accInput1_ = accInput2_ = accOutput_ = 0.f;
        samples_ = 0;
    }

    void Update(float target, float& w1, float& w2, float& bias, float baseLR = 0.15f) {
        if (samples_ == 0) return;

        float i1  = accInput1_ / samples_;
        float i2  = accInput2_ / samples_;
        float out = accOutput_ / samples_;
        float lr  = baseLR * lrDecay_;

        float error = target - out;
        float delta = error * SigmoidDeriv(out);

        w1   += lr * delta * i1;
        w2   += lr * delta * i2;
        bias += lr * delta;

        w1   = std::max(-2.f, std::min(2.f, w1));
        w2   = std::max(-2.f, std::min(2.f, w2));
        bias = std::max(-2.f, std::min(2.f, bias));

        lastLoss_ = error * error;
        lrDecay_ *= 0.92f;
        Reset();
    }

private:
    float accInput1_{0.f};
    float accInput2_{0.f};
    float accOutput_{0.f};
    int   samples_{0};
    float lastLoss_{0.f};
    float lrDecay_{1.f};
};

// ══════════════════════════════════════════════════════════════════
//  ThreatMap（擊殺密度熱圖）
// ══════════════════════════════════════════════════════════════════
class ThreatMap {
public:
    void AddKill(int gx, int gy, float value = 1.f) {
        if (gx >= 0 && gx < 24 && gy >= 0 && gy < 20) {
            cell_[gx][gy] += value;
        }
    }

    float Get(int gx, int gy) const {
        if (gx >= 0 && gx < 24 && gy >= 0 && gy < 20) return cell_[gx][gy];
        return 0.f;
    }

    float GetMax() const {
        float mx = 0.f;
        for (int x = 0; x < 24; x++)
            for (int y = 0; y < 20; y++)
                mx = std::max(mx, cell_[x][y]);
        return mx;
    }

    void Decay(float factor = 0.85f) {
        for (int x = 0; x < 24; x++)
            for (int y = 0; y < 20; y++)
                cell_[x][y] *= factor;
    }

private:
    float cell_[24][20]{};
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
class EnemyBrain {
public:
    float LastLoss() const { return lastLoss_; }
    int TrainCount() const { return trainCount_; }
    float OutputWeight(int index) const { return (index >= 0 && index < 2) ? wHO_[index] : 0.f; }
    float HiddenBias(int index) const { return (index >= 0 && index < 2) ? bH_[index] : 0.f; }

    float Eval(float threatAhead, float hpRatio, float survRate) const {
        float in[3] = { threatAhead, hpRatio, survRate };
        float h[2];
        for (int j = 0; j < 2; j++) {
            float s = bH_[j];
            for (int i = 0; i < 3; i++) s += wIH_[i][j] * in[i];
            h[j] = 1.f / (1.f + expf(-s));
        }
        float o = bO_;
        for (int j = 0; j < 2; j++) o += wHO_[j] * h[j];
        return 1.f / (1.f + expf(-o));
    }

    void Learn(float threatAhead, float hpRatio, float survRate, float target, float lr = 0.12f) {
        float in[3] = { threatAhead, hpRatio, survRate };
        float h[2];
        for (int j = 0; j < 2; j++) {
            float s = bH_[j];
            for (int i = 0; i < 3; i++) s += wIH_[i][j] * in[i];
            h[j] = 1.f / (1.f + expf(-s));
        }
        float o_raw = bO_;
        for (int j = 0; j < 2; j++) o_raw += wHO_[j] * h[j];
        float o  = 1.f / (1.f + expf(-o_raw));
        float dO = (target - o) * o * (1.f - o);
        lastLoss_ = (target - o) * (target - o);
        trainCount_++;
        for (int j = 0; j < 2; j++) wHO_[j] += lr * dO * h[j];
        bO_ += lr * dO;
        for (int j = 0; j < 2; j++) {
            float dH = dO * wHO_[j] * h[j] * (1.f - h[j]);
            bH_[j] += lr * dH;
            for (int i = 0; i < 3; i++) wIH_[i][j] += lr * dH * in[i];
        }
        auto clamp3 = [](float v){ return v<-3.f?-3.f:(v>3.f?3.f:v); };
        auto clamp2 = [](float v){ return v<-2.f?-2.f:(v>2.f?2.f:v); };
        for (int j = 0; j < 2; j++) {
            wHO_[j] = clamp3(wHO_[j]); bH_[j] = clamp2(bH_[j]);
            for (int i = 0; i < 3; i++) wIH_[i][j] = clamp3(wIH_[i][j]);
        }
        bO_ = clamp2(bO_);
    }

private:
    float wIH_[3][2] = {{ 0.5f,-0.3f},{-0.4f, 0.6f},{ 0.2f,-0.1f}};
    float bH_[2]     = { 0.f,  0.f };
    float wHO_[2]    = { 0.6f,-0.4f };
    float bO_        = 0.f;
    float lastLoss_  = 0.f;
    int   trainCount_= 0;
};

// ══════════════════════════════════════════════════════════════════
//  EnemyIntel  —  敵方整體學習系統
// ══════════════════════════════════════════════════════════════════
class EnemyIntel {
public:
    EnemyBrain brain;

    float typeSurvRate[5]{ 0.2f, 0.2f, 0.2f, 0.2f, 0.2f };
    std::array<float, MAX_LANES> pathSurvRate{};

    int typeSpawned[5]{};
    int typeSurvived[5]{};
    std::array<int, MAX_LANES> pathSpawned{};
    std::array<int, MAX_LANES> pathSurvived{};
    std::array<float, MAX_LANES> pathAvgThreat{};

    int  wavesLearned{ 0 };
    bool adapted{ false };

    EnemyIntel() {
        pathSurvRate.fill(0.4f);
    }

    static int ClampLaneCount(int activeLaneCount) {
        return std::max(1, std::min(MAX_LANES, activeLaneCount));
    }

    void ResetWave() {
        for (int i = 0; i < 5; i++) { typeSpawned[i] = 0; typeSurvived[i] = 0; }
        pathSpawned.fill(0);
        pathSurvived.fill(0);
        pathAvgThreat.fill(0.f);
    }

    void LearnFromWave(int activeLaneCount = MAX_LANES) {
        constexpr float LR = 0.40f;
        int laneCount = ClampLaneCount(activeLaneCount);

        for (int i = 0; i < 5; i++) {
            if (typeSpawned[i] > 0) {
                float rate = (float)typeSurvived[i] / typeSpawned[i];
                typeSurvRate[i] = typeSurvRate[i]*(1.f-LR) + rate*LR;
                typeSurvRate[i] = std::max(0.05f, std::min(0.95f, typeSurvRate[i]));
            }
        }
        for (int i = 0; i < MAX_LANES; i++) {
            if (pathSpawned[i] > 0) {
                float rate = (float)pathSurvived[i] / pathSpawned[i];
                pathSurvRate[i] = pathSurvRate[i]*(1.f-LR) + rate*LR;
                pathSurvRate[i] = std::max(0.05f, std::min(0.95f, pathSurvRate[i]));
            }
        }

        float total = 0.001f;
        for (int i = 0; i < laneCount; i++) total += pathSurvRate[i];
        for (int i = 0; i < laneCount; i++) {
            float target = pathSurvRate[i] / total;
            brain.Learn(pathAvgThreat[i], 0.5f, pathSurvRate[i], target);
        }

        wavesLearned++;
        if (wavesLearned >= 2) adapted = true;
        ResetWave();
    }

    int BrainPickPath(int activeLaneCount = MAX_LANES) const {
        int laneCount = ClampLaneCount(activeLaneCount);
        int bestLane = 0;
        float bestScore = -1.f;
        for (int lane = 0; lane < laneCount; lane++) {
            float score = brain.Eval(pathAvgThreat[lane], 1.f, pathSurvRate[lane]);
            if (score > bestScore) {
                bestScore = score;
                bestLane = lane;
            }
        }
        return bestLane;
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

    int PickPath(float rand01, int activeLaneCount = MAX_LANES) const {
        int laneCount = ClampLaneCount(activeLaneCount);
        if (laneCount <= 1) return 0;
        if (!adapted) {
            int lane = (int)(rand01 * laneCount);
            return std::max(0, std::min(laneCount - 1, lane));
        }

        float total = 0.f;
        for (int lane = 0; lane < laneCount; lane++) {
            total += std::max(0.01f, pathSurvRate[lane]);
        }
        if (total <= 0.f) return 0;

        float r = rand01 * total;
        float acc = 0.f;
        for (int lane = 0; lane < laneCount; lane++) {
            acc += std::max(0.01f, pathSurvRate[lane]);
            if (r <= acc) return lane;
        }
        return laneCount - 1;
    }

    const char* TopTypeName() const {
        static const char* NAMES[] = { "基礎兵","快速兵","裝甲兵","精英兵","BOSS" };
        int best = 0;
        for (int i = 1; i < 4; i++) if (typeSurvRate[i] > typeSurvRate[best]) best = i;
        return NAMES[best];
    }

    int PreferredPath(int activeLaneCount = MAX_LANES) const {
        int laneCount = ClampLaneCount(activeLaneCount);
        int best = 0;
        for (int lane = 1; lane < laneCount; lane++) {
            if (pathSurvRate[lane] > pathSurvRate[best]) best = lane;
        }
        return best;
    }
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
class DefenseAdvisorNN {
public:
    float Probability(int index) const {
        return (index >= 0 && index < 4) ? lastProb_[index] : 0.f;
    }

    float LastLoss() const { return lastLoss_; }
    int TrainCount() const { return trainCount_; }

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
                wIH_[i][j] = K[(i * 4 + j) % 24] * 0.5f;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                wHO_[i][j] = K[(i * 4 + j) % 24] * 0.3f;
    }

    // ── 前向傳播（tanh 隱藏層 + softmax 輸出）───────────────────
    void Forward(const float in[6], float prob[4]) const {
        float h[4];
        for (int j = 0; j < 4; j++) {
            float s = bH_[j];
            for (int i = 0; i < 6; i++) s += wIH_[i][j] * in[i];
            h[j] = tanhf(s);
        }
        float mx = -1e9f;
        for (int j = 0; j < 4; j++) {
            float s = bO_[j];
            for (int i = 0; i < 4; i++) s += wHO_[i][j] * h[i];
            prob[j] = s;
            if (s > mx) mx = s;
        }
        float sum = 0.f;
        for (int j = 0; j < 4; j++) { prob[j] = expf(prob[j] - mx); sum += prob[j]; }
        for (int j = 0; j < 4; j++) prob[j] /= sum;
    }

    // ── 推薦：回傳最高機率的類型索引 ────────────────────────────
    int Recommend(const float in[6]) {
        Forward(in, lastProb_);
        int best = 0;
        for (int j = 1; j < 4; j++) if (lastProb_[j] > lastProb_[best]) best = j;
        return best;
    }

    // ── 反向傳播（交叉熵損失）────────────────────────────────────
    void Learn(const float in[6], int targetClass, float lr = 0.08f) {
        float h[4];
        for (int j = 0; j < 4; j++) {
            float s = bH_[j];
            for (int i = 0; i < 6; i++) s += wIH_[i][j] * in[i];
            h[j] = tanhf(s);
        }
        float prob[4], mx = -1e9f;
        for (int j = 0; j < 4; j++) {
            float s = bO_[j];
            for (int i = 0; i < 4; i++) s += wHO_[i][j] * h[i];
            prob[j] = s;
            if (s > mx) mx = s;
        }
        float sum = 0.f;
        for (int j = 0; j < 4; j++) { prob[j] = expf(prob[j] - mx); sum += prob[j]; }
        for (int j = 0; j < 4; j++) { prob[j] /= sum; lastProb_[j] = prob[j]; }

        lastLoss_ = -logf(prob[targetClass] + 1e-7f);
        trainCount_++;

        // 輸出層梯度（softmax + 交叉熵）
        float dO[4];
        for (int j = 0; j < 4; j++) dO[j] = prob[j] - (j == targetClass ? 1.f : 0.f);

        // 更新輸出層
        for (int j = 0; j < 4; j++) {
            bO_[j] -= lr * dO[j];
            for (int i = 0; i < 4; i++) wHO_[i][j] -= lr * dO[j] * h[i];
        }

        // 隱藏層梯度（tanh 導數）
        float dH[4] = {};
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) dH[i] += dO[j] * wHO_[i][j];
            dH[i] *= (1.f - h[i] * h[i]);
        }
        for (int i = 0; i < 4; i++) {
            bH_[i] -= lr * dH[i];
            for (int k = 0; k < 6; k++) wIH_[k][i] -= lr * dH[i] * in[k];
        }

        // 權重裁剪
        auto cl = [](float v, float m){ return v < -m ? -m : (v > m ? m : v); };
        for (int i = 0; i < 4; i++) {
            bO_[i] = cl(bO_[i], 3.f); bH_[i] = cl(bH_[i], 2.f);
            for (int j = 0; j < 4; j++) { wHO_[j][i] = cl(wHO_[j][i], 3.f); }
            for (int j = 0; j < 6; j++) { wIH_[j][i] = cl(wIH_[j][i], 3.f); }
        }
    }

private:
    float wIH_[6][4]{};    // 輸入 → 隱藏
    float bH_[4]{};
    float wHO_[4][4]{};    // 隱藏 → 輸出
    float bO_[4]{};
    float lastProb_[4]{ 0.25f, 0.25f, 0.25f, 0.25f };
    float lastLoss_{ 0.f };
    int   trainCount_{ 0 };
};

// ══════════════════════════════════════════════════════════════════
//  遊戲物件繼承架構：格子物件、路徑物件與視覺特效共用基底
// ══════════════════════════════════════════════════════════════════
class GameEntity {
public:
    virtual ~GameEntity() = default;
};

class GridEntity : public GameEntity {
public:
    Vector2 GridCenter() const {
        return { static_cast<float>(gx), static_cast<float>(gy) };
    }

    int GridX() const { return gx; }
    int GridY() const { return gy; }

protected:
    int gx{0};
    int gy{0};
};

class PathEntity : public GameEntity {
public:
    float PathPosition() const { return pathPos; }
    int LaneIndex() const { return pathIdx; }

protected:
    float pathPos{0.f};
    int   pathIdx{0};
};

class VisualEffect : public GameEntity {
public:
    virtual ~VisualEffect() = default;
    virtual void Update(float dt) { life -= dt; }
    bool IsAlive() const { return life > 0.f; }

protected:
    float life{0.f};
};

// ── AI 顧問提示 ───────────────────────────────────────────────────
class AIHint {
public:
    int         gx, gy;
    TType       suggest;
    float       score;
    std::string reason;
    float       flashT{0.f};
};

// ══════════════════════════════════════════════════════════════════
//  Tower（防禦塔）
// ══════════════════════════════════════════════════════════════════
class Tower : public GridEntity {
public:
    int   id;
    TType type;
    using GridEntity::gx;
    using GridEntity::gy;
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
class Enemy : public PathEntity {
public:
    int   id;
    EType type;

    using PathEntity::pathIdx;
    using PathEntity::pathPos;
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
class Bullet : public GameEntity {
public:
    Bullet() = default;

    Bullet(Vector2 position, Vector2 velocity, int target, int source, float damageValue,
           bool isCrit, bool hasSplash, float splashRadius, Color color)
        : pos(position), vel(velocity), targetId(target), sourceId(source), dmg(damageValue),
          crit(isCrit), splash(hasSplash), splashR(splashRadius), col(color) {}

    Vector2 pos, vel;
    int     targetId;
    int     sourceId{-1};
    float   dmg;
    bool    crit{false};
    bool    splash{false};
    float   splashR{0.f};
    Color   col;
};

class SigPulse : public VisualEffect {
public:
    SigPulse() = default;
    SigPulse(Vector2 source, Vector2 destination, float progress, Color color)
        : src(source), dst(destination), t(progress), col(color) { life = 1.f - progress; }

    void Update(float dt) override {
        t += dt * 3.5f;
        if (t >= 1.f) life = 0.f;
    }

    Vector2 src, dst;
    float t;
    Color col;
};

class Particle : public VisualEffect {
public:
    Particle() = default;
    Particle(Vector2 position, Vector2 velocity, float currentLife, float fullLife, float effectRadius, Color color)
        : pos(position), vel(velocity), maxLife(fullLife), radius(effectRadius), col(color) { life = currentLife; }

    void Update(float dt) override {
        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
        vel.y += 55.f * dt;  // 重力
        life -= dt;
    }

    using VisualEffect::life;
    Vector2 pos, vel;
    float maxLife, radius;
    Color col;
};

class FloatText : public VisualEffect {
public:
    FloatText() = default;
    FloatText(Vector2 position, std::string value, Color color, float currentLife)
        : pos(position), text(std::move(value)), col(color) { life = currentLife; }

    void Update(float dt) override {
        pos.y -= 28.f * dt;
        life -= dt;
    }

    using VisualEffect::life;
    Vector2 pos;
    std::string text;
    Color col;
};

class Star : public GameEntity {
public:
    Star() = default;
    Star(float xPos, float yPos, float radius, float brightness)
        : x(xPos), y(yPos), r(radius), bright(brightness) {}

    float x, y, r, bright;
};
