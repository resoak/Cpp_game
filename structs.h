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
    bool    splash{false};
    float   splashR{0.f};
    Color   col;
};

struct SigPulse  { Vector2 src, dst; float t; Color col; };
struct Particle  { Vector2 pos, vel; float life, maxLife, radius; Color col; };
struct FloatText { Vector2 pos; std::string text; Color col; float life; };
struct Star      { float x, y, r, bright; };
