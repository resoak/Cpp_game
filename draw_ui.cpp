// ================================================================
//  draw_ui.cpp — UI 層繪圖
//  DrawLeftPanel, DrawRightPanel, DrawTopBar, DrawBotBar
// ================================================================
#include "draw.h"
#include <cstdio>
#include <algorithm>
#include <cstring>

static const char* LaneLabel(int laneSlot) {
    static const char* LABELS[] = { "主線", "路線2", "路線3", "路線4", "路線5", "路線6" };
    if (laneSlot >= 0 && laneSlot < MAX_LANES) return LABELS[laneSlot];
    return "路線";
}

static int CountLaneEnemiesUi(const Game& G, int laneSlot) {
    int count = 0;
    for (auto& e : G.enemies) if (e.pathIdx == laneSlot) count++;
    return count;
}

static float LanePressureUi(const Game& G, int laneSlot) {
    const auto& cells = G.LaneCells(laneSlot);
    if (cells.empty()) return 0.f;

    float furthest = 0.f;
    for (auto& e : G.enemies) {
        if (e.pathIdx != laneSlot) continue;
        furthest = std::max(furthest, e.pathPos / (float)std::max(1, (int)cells.size()));
    }
    return Clamp(furthest, 0.f, 1.f);
}

static void DrawRouteUiCard(int x, int y, int w, const char* header, const PathPreset& preset,
                            int laneSlot, int enemyCount, float pressure, bool preview) {
    RouteVisualTheme theme = GetRouteTheme(preset, laneSlot);
    float pulse = 0.65f + 0.35f * sinf((float)GetTime() * 2.4f + laneSlot * 0.8f + preset.family * 0.35f);

    DrawRoundBox((float)x, (float)y, (float)w, 46.f, 8.f,
        AlphaOf(theme.fillSoft, 220),
        AlphaOf(theme.accent, (int)(120 + pulse * 70)), 1.4f);
    DTX(header, (float)x + 10, (float)y + 6, FS_TINY, AlphaOf(theme.accent, 230));
    DTX(theme.shortLabel, (float)x + 10, (float)y + 21, FS_TINY, AlphaOf(theme.glow, 215));
    DTX(preset.name, (float)x + 78, (float)y + 5, FS_SMALL, theme.text);
    DTX(theme.sideLabel, (float)x + 78, (float)y + 22, FS_TINY, AlphaOf(theme.glow, 165));

    if (!preview) {
        char eb[16];
        snprintf(eb, 16, "x%d", enemyCount);
        DTX(eb, (float)(x + w - 38), (float)y + 6, FS_TINY, AlphaOf(theme.text, 215));
        DrawRectangle(x + 78, y + 35, w - 100, 4, AlphaOf(theme.fill, 170));
        DrawRectangle(x + 78, y + 35, (int)((w - 100) * pressure), 4, AlphaOf(theme.accent, 230));
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawLeftPanel
// ══════════════════════════════════════════════════════════════════
void DrawLeftPanel(Game& G) {
    const int py0 = TOPBAR_H;
    const int ph  = VIRT_H - TOPBAR_H - BOTBAR_H;
    DrawRectangle(0, py0, PANEL_L, ph, PANEL_BG);
    DrawRectangleLines(0, py0, PANEL_L, ph, PANEL_BD);

    float t = (float)GetTime();

    if (G.phase == Game::TRAINING) {
        DTC("TRAINING", PANEL_L / 2, py0 + 38, FS_MED, AlphaOf(COL_PERC, 230));
        DTX("防線凍結中，選擇一項短訓練獎勵", 12, (float)py0 + 58, FS_TINY, AlphaOf(WHITE, 120));

        int startY = py0 + 76;
        for (int i = 0; i < G.trainingChoiceCount; i++) {
            auto& choice = G.trainingChoices[i];
            int by = startY + i * 86;
            Color bd = choice.col;
            bd.a = 170;
            Color bg = AlphaOf(choice.col, 22);
            DrawRoundBox(10, (float)by, PANEL_L - 20, 72, 8, bg, bd, 1.8f);

            char key[8];
            snprintf(key, 8, "[%d]", i + 1);
            DTX(key, 16, (float)by + 8, FS_TINY, AlphaOf(choice.col, 220));
            DTX(choice.title.c_str(), 46, (float)by + 7, FS_SMALL, choice.col);
            DTX(choice.desc.c_str(), 16, (float)by + 31, FS_TINY, AlphaOf(WHITE, 165));
        }

        float pulse = 0.75f + 0.25f * sinf(t * 4.f);
        char tb[32];
        snprintf(tb, 32, "剩餘 %.1fs", std::max(0.f, G.trainingTimer));
        DrawRoundBox(10, (float)(startY + G.trainingChoiceCount * 86 + 8), PANEL_L - 20, 62, 8,
                     AlphaOf(COL_SENSOR, 16), AlphaOf(COL_SENSOR, 90), 1.2f);
        DTC(tb, PANEL_L / 2, startY + G.trainingChoiceCount * 86 + 32, FS_SMALL,
            AlphaOf(COL_SENSOR, (int)(220 * pulse)));
        DTX("未選擇時將自動套用第 1 項", 18, (float)(startY + G.trainingChoiceCount * 86 + 54), FS_TINY, AlphaOf(WHITE, 100));
        return;
    }

    DTC("元件選擇", PANEL_L / 2, py0 + 38, FS_MED, AlphaOf(COL_CPU, 220));

    static TType ORDER[] = {
        TType::SENSOR, TType::PERCEPTRON, TType::AND, TType::OR,
        TType::XOR,    TType::NAND,       TType::CANNON
    };

    G.btnY0 = py0 + 68;
    constexpr int BTN_H_SM  = 56;
    constexpr int BTN_GAP_SM = 3;

    for (int i = 0; i < 7; i++) {
        TType     tt  = ORDER[i];
        TowerDef& def = TDef(tt);
        int       by  = G.btnY0 + i * (BTN_H_SM + BTN_GAP_SM);

        bool sel    = (G.placing == tt);
        bool canBuy = (G.credits >= def.baseCost);

        Color bg = PANEL_BG;
        if (sel)          { bg = def.col; bg.a = 55; }
        else if (!canBuy)   bg = {8, 8, 8, 255};

        Color bd = sel ? def.col : AlphaOf(def.col, canBuy ? 80 : 30);
        if (sel) { float p2=0.7f+0.3f*sinf(t*4.f); bd.a=(unsigned char)(200*p2); }

        DrawRoundBox(10, (float)by, PANEL_L - 20, (float)BTN_H_SM, 8, bg, bd, sel ? 2.5f : 1.5f);

        Color sc = sel ? def.col : AlphaOf(def.col, canBuy ? 200 : 80);
        DTC(def.sym, 48, by + BTN_H_SM / 2, FS_MED, sc);

        Color tc = canBuy ? WHITE : AlphaOf(WHITE, 80);
        DTX(def.label, 70, (float)by + 5, FS_TINY, tc);

        char cs[16]; snprintf(cs, 16, "%d CR", def.baseCost);
        DTX(cs, 70, (float)by + 22, FS_TINY, AlphaOf(COL_AND, canBuy ? 200 : 80));
        DTX(def.desc, 12, (float)by + BTN_H_SM - 15, FS_TINY, AlphaOf(WHITE, canBuy ? 100 : 40));
    }

    // ── 發動波次按鈕 ─────────────────────────────────────────────
    G.waveBtnY = G.btnY0 + 7 * (BTN_H_SM + BTN_GAP_SM) + 12;
    if (G.phase == Game::BUILD) {
        float p2   = 0.7f + 0.3f * sinf(t * 3.f);
        Color wbc  = COL_PERC;
        DrawRoundBox(10, (float)G.waveBtnY, PANEL_L - 20, 54, 10, AlphaOf(wbc, 25), wbc, 2.5f);
        DTC("▶ 發動下一波", PANEL_L / 2, G.waveBtnY + 27, FS_MED, AlphaOf(wbc, (int)(220 * p2)));
    }

    // ── 金幣 + 訊息 ──────────────────────────────────────────────
    int infoY = G.waveBtnY + 70;
    char cr[32]; snprintf(cr, 32, "金幣 %d CR", G.credits);
    DTC(cr, PANEL_L / 2, infoY, FS_MED, COL_AND);

    if (G.msgTimer > 0 && !G.msg.empty()) {
        float alpha = std::min(1.f, G.msgTimer);
        int   wrap  = 28;
        if ((int)G.msg.size() > wrap) {
            DTX(G.msg.substr(0, wrap).c_str(),  8, (float)(infoY+28), FS_TINY, AlphaOf(COL_SENSOR, (int)(200*alpha)));
            DTX(G.msg.substr(wrap).c_str(),     8, (float)(infoY+48), FS_TINY, AlphaOf(COL_SENSOR, (int)(200*alpha)));
        } else {
            DTX(G.msg.c_str(), 8, (float)(infoY+28), FS_TINY, AlphaOf(COL_SENSOR, (int)(200*alpha)));
        }
    }

    // ── AI 工具切換列 ────────────────────────────────────────────
    int akY = infoY + 80;
    DrawRoundBox(10, (float)akY, PANEL_L-20, 52, 6, AlphaOf(COL_AI,12), AlphaOf(COL_AI,60), 1.f);
    DTC("AI 工具", PANEL_L/2, akY+12, FS_TINY, AlphaOf(COL_AI,180));
    DTX("[T] 熱圖",  18,           (float)(akY+28), FS_TINY, G.showThreat   ? COL_AI : AlphaOf(COL_AI,100));
    DTX("[A] 提示", PANEL_L/2+4,  (float)(akY+28), FS_TINY, G.showAIHints  ? COL_AI : AlphaOf(COL_AI,100));
}

// ══════════════════════════════════════════════════════════════════
//  DrawRightPanel
// ══════════════════════════════════════════════════════════════════
void DrawRightPanel(Game& G) {
    int rx = VIRT_W - PANEL_R;
    int cx = rx + PANEL_R / 2;

    const int py0 = TOPBAR_H;
    const int ph  = VIRT_H - TOPBAR_H - BOTBAR_H;
    DrawRectangle(rx, py0, PANEL_R, ph, PANEL_BG);
    DrawRectangleLines(rx, py0, PANEL_R, ph, PANEL_BD);
    DTC("元件資訊", cx, py0 + 38, FS_MED, AlphaOf(COL_CPU, 220));

    Tower* sel = G.FindTower(G.selectedId);
    if (!sel) {
        EnemyIntel& I = G.intel;
        int laneCount = G.ActiveLaneCount();
        int iy = py0 + 70;

        DTC("入口態勢", cx, iy, FS_MED, AlphaOf(COL_CPU, 220)); iy += 28;
        for (int lane = 0; lane < laneCount; lane++) {
            char header[24];
            snprintf(header, 24, "當前%s", LaneLabel(lane));
            DrawRouteUiCard(rx + 10, iy, PANEL_R - 20, header, G.LanePreset(lane), lane,
                CountLaneEnemiesUi(G, lane), LanePressureUi(G, lane), false);
            iy += 54;
        }

        if (G.hasPlannedRouteChange) {
            int previewCount = std::max(1, std::min(MAX_LANES, G.nextPreviewLaneCount));
            int previewH = 28 + previewCount * 54 + 8;
            DrawRoundBox((float)rx + 8, (float)iy, (float)PANEL_R - 16, (float)previewH, 8,
                AlphaOf({255, 205, 120, 255}, 9), AlphaOf({255, 205, 120, 255}, 56), 1.3f);
            DTC("下次輪換", cx, iy + 14, FS_TINY, AlphaOf({255, 220, 160, 255}, 220));
            iy += 26;
            for (int lane = 0; lane < previewCount; lane++) {
                if (G.nextPreviewPaths[lane] < 0) continue;
                char header[24];
                snprintf(header, 24, "下波%s", LaneLabel(lane));
                DrawRouteUiCard(rx + 14, iy, PANEL_R - 28, header,
                    GetPathPreset(G.nextPreviewPaths[lane]), lane, 0, 0.f, true);
                iy += 54;
            }
            iy += 6;
        }

        DTC("敵情分析", cx, iy, FS_MED, AlphaOf({255, 100, 100, 255}, 220)); iy += 38;

        if (!I.adapted) {
            DTC("收集中...", cx, iy, FS_SMALL, AlphaOf(WHITE, 80)); iy += 24;
            char wl[32]; snprintf(wl, 32, "已學習 %d / 2 波", I.wavesLearned);
            DTC(wl, cx, iy, FS_TINY, AlphaOf(WHITE, 100));
            return;
        }

        // ── 兵種存活率長條圖 ─────────────────────────────────────
        static const char* TYPE_NAMES[] = { "基礎", "快速", "裝甲", "精英", "BOSS" };
        static const Color TYPE_COLS[]  = {
            COL_VIRUS, COL_FAST, COL_ARMORED, COL_ELITE, COL_BOSS
        };

        DrawRoundBox((float)rx+8, (float)iy, (float)PANEL_R-16, 120, 6,
                     AlphaOf({255,80,80,255}, 8), AlphaOf({255,80,80,255}, 60), 1.5f);
        DTC("兵種適應度", cx, iy+14, FS_TINY, AlphaOf({255,120,120,255}, 200)); iy += 28;

        for (int i = 0; i < 4; i++) {
            float surv = I.typeSurvRate[i];
            int   barW = (int)((PANEL_R - 60) * surv);
            Color bc   = TYPE_COLS[i];

            DTX(TYPE_NAMES[i], (float)rx+12, (float)iy, FS_TINY, AlphaOf(WHITE, 160));
            DrawRectangle(rx+44, iy, PANEL_R-56, 12, AlphaOf(BLACK, 120));
            if (barW > 0) DrawRectangle(rx+44, iy, barW, 12, AlphaOf(bc, 180));

            char pct[12]; snprintf(pct, 12, "%2.0f%%", surv * 100.f);
            DTX(pct, (float)(rx + PANEL_R - 28), (float)iy, FS_TINY, AlphaOf(bc, 220));
            iy += 18;
        }
        iy += 6;

        if (laneCount > 1) {
            int routeBoxH = 34 + laneCount * 28;
            DrawRoundBox((float)rx + 8, (float)iy, (float)PANEL_R - 16, (float)routeBoxH, 6,
                         AlphaOf({255,180,60,255}, 8), AlphaOf({255,180,60,255}, 50), 1.5f);
            DTC("路線威脅", cx, iy + 14, FS_TINY, AlphaOf({255,200,80,255}, 200)); iy += 28;

            int preferredLane = I.PreferredPath(laneCount);
            for (int i = 0; i < laneCount; i++) {
                const PathPreset& preset = G.LanePreset(i);
                RouteVisualTheme theme = GetRouteTheme(preset, i);
                float surv = I.pathSurvRate[i];
                int   barW = (int)((PANEL_R - 138) * surv);
                bool  pref = (preferredLane == i);
                Color pc   = pref ? theme.accent : AlphaOf(theme.accent, 150);

                char label[24]; snprintf(label, 24, "%s%s", LaneLabel(i), pref ? " ▶" : "");
                DTX(label, (float)rx + 12, (float)iy, FS_TINY, pc);
                DTX(preset.name, (float)rx + 72, (float)iy, FS_TINY, theme.text);
                DrawRectangle(rx + 72, iy + 13, PANEL_R - 138, 10, AlphaOf(theme.fill, 190));
                if (barW > 0) DrawRectangle(rx + 72, iy + 13, barW, 10, AlphaOf(pc, 220));

                char pct[12]; snprintf(pct, 12, "%2.0f%%", surv * 100.f);
                DTX(theme.sideLabel, (float)rx + 12, (float)iy + 13, FS_TINY, AlphaOf(theme.glow, 150));
                DTX(pct, (float)(rx + PANEL_R - 34), (float)iy + 11, FS_TINY, AlphaOf(pc, 220));
                iy += 28;
            }
            iy += 6;
        }

        // ── 學習狀態摘要 ─────────────────────────────────────────
        char wlb[32]; snprintf(wlb, 32, "已學習 %d 波", I.wavesLearned);
        DTC(wlb, cx, iy, FS_TINY, AlphaOf(WHITE, 120)); iy += 22;

        char topb[48];
        snprintf(topb, 48, "主攻兵種：%s", I.TopTypeName());
        DTC(topb, cx, iy, FS_TINY, AlphaOf({255, 100, 100, 255}, 200)); iy += 22;

        if (laneCount > 1) {
            int prefLane = I.PreferredPath(laneCount);
            const PathPreset& prefPreset = G.LanePreset(prefLane);
            char pathb[80];
            snprintf(pathb, 80, "偏好路線：%s / %s", GetRouteTheme(prefPreset, prefLane).sideLabel, prefPreset.name);
            DTC(pathb, cx, iy, FS_TINY, AlphaOf(GetRouteTheme(prefPreset, prefLane).accent, 210)); iy += 22;
        }

        // ── 防禦建議神經網路輸出機率 ─────────────────────────────
        if (G.defNN.trainCount > 0) {
            DrawRoundBox((float)rx+8, (float)iy, (float)PANEL_R-16, 88, 6,
                         AlphaOf({60,180,255,255}, 8), AlphaOf({60,180,255,255}, 50), 1.f);
            DTC("防禦建議NN", cx, iy+13, FS_TINY, AlphaOf({100,200,255,255}, 220)); iy += 26;

            static const char* NN_LABELS[] = { "感測器", "AND閘", "XOR閘", "砲塔" };
            static const Color NN_COLS[]   = { COL_SENSOR, COL_AND, COL_XOR, COL_CANNON };
            for (int k = 0; k < 4; k++) {
                float p   = G.defNN.lastProb[k];
                int   bw  = (int)((PANEL_R - 56) * p);
                Color bc  = NN_COLS[k];
                DTX(NN_LABELS[k], (float)rx+12, (float)iy, FS_TINY, AlphaOf(WHITE,160));
                DrawRectangle(rx+52, iy, PANEL_R-64, 11, AlphaOf(BLACK,120));
                if (bw > 0) DrawRectangle(rx+52, iy, bw, 11, AlphaOf(bc, 200));
                char pb[8]; snprintf(pb, 8, "%2.0f%%", p*100.f);
                DTX(pb, (float)(rx+PANEL_R-26), (float)iy, FS_TINY, AlphaOf(bc,220));
                iy += 15;
            }
            char tlb[32]; snprintf(tlb, 32, "loss=%.3f 訓練%d次", G.defNN.lastLoss, G.defNN.trainCount);
            DTX(tlb, (float)rx+12, (float)iy, FS_TINY, AlphaOf(WHITE, 110)); iy += 14;
        }

        // ── 敵方神經網路權重 ─────────────────────────────────────
        if (I.brain.trainCount > 0) {
            DrawRoundBox((float)rx+8, (float)iy, (float)PANEL_R-16, 72, 6,
                         AlphaOf({200,60,60,255}, 8), AlphaOf({200,60,60,255}, 50), 1.f);
            DTC("敵方神經網路", cx, iy+13, FS_TINY, AlphaOf({255,80,80,255}, 200)); iy += 26;

            char w0[40], w1[40], lossb[40];
            snprintf(w0,   40, "路線w: [%.2f, %.2f]", I.brain.wHO[0], I.brain.wHO[1]);
            snprintf(w1,   40, "隱藏b: [%.2f, %.2f]", I.brain.bH[0],  I.brain.bH[1]);
            snprintf(lossb,40, "loss=%.3f  訓練%d次", I.brain.lastLoss, I.brain.trainCount);
            DTX(w0,    (float)rx+12, (float)iy, FS_TINY, AlphaOf({255,120,120,255}, 180)); iy += 16;
            DTX(w1,    (float)rx+12, (float)iy, FS_TINY, AlphaOf({255,120,120,255}, 160)); iy += 16;
            DTX(lossb, (float)rx+12, (float)iy, FS_TINY, AlphaOf(WHITE, 130));
        }
        return;
    }

    TowerDef& def = TDef(sel->type);
        int y = py0 + 70;

    DTC(def.label, cx, y, FS_LARGE, def.col); y += 44;
    for (int lv=0;lv<sel->level;lv++) DrawPoly({(float)(rx+30+lv*22),(float)y},5,9.f,0.f,COL_STAR);
    y += 26;

    DrawRoundBox((float)rx+12,(float)y,(float)PANEL_R-24,18,4,AlphaOf(BLACK,180),AlphaOf(def.col,80));
    int sw=(int)((PANEL_R-28)*sel->sig);
    if (sw>0) DrawRectangle(rx+14,y+2,sw,14,AlphaOf(def.col,200));
    char sb[20]; snprintf(sb,20,"訊號 %.2f",sel->sig);
    DTC(sb,cx,y+9,FS_TINY,WHITE); y += 32;

    DTX(def.desc,                          (float)rx+10,(float)y,FS_TINY,AlphaOf(WHITE,160));    y+=22;
    DTX(def.tiers[sel->level-1].ability,   (float)rx+10,(float)y,FS_TINY,AlphaOf(def.col,200));  y+=22;

    if (sel->type != TType::CPU && sel->level < 3) {
        int  ucost=TDef(sel->type).tiers[sel->level].cost;
        bool canUp=(G.credits>=ucost);
        Color uc=canUp?COL_PERC:AlphaOf(COL_PERC,60);
        DrawRoundBox((float)rx+12,(float)y,(float)PANEL_R-24,36,6,AlphaOf(uc,canUp?25:10),uc,1.5f);
        char ub[40]; snprintf(ub,40,"[U] 升級 Lv.%d  %d CR",sel->level+1,ucost);
        DTC(ub,cx,y+18,FS_TINY,uc); y+=46;
    }

    char kb[32],db[40];
    snprintf(kb,32,"擊殺：%d",sel->kills);
    snprintf(db,40,"總傷：%.0f",sel->totalDmg);
    DTX(kb,(float)rx+12,(float)y,FS_TINY,AlphaOf(WHITE,160)); y+=20;
    DTX(db,(float)rx+12,(float)y,FS_TINY,AlphaOf(WHITE,140)); y+=28;

    // ── 感知器學習狀態 ───────────────────────────────────────────
    if (sel->type == TType::PERCEPTRON) {
        DrawRoundBox((float)rx+8,(float)y,(float)PANEL_R-16,110,6,AlphaOf(COL_AI,10),AlphaOf(COL_AI,80),1.5f);
        DTC("神經元學習狀態",cx,y+14,FS_TINY,AlphaOf(COL_AI,220)); y+=28;
        char w1b[32],w2b[32],bib[32];
        snprintf(w1b,32,"w1  = %+.3f",sel->w1);
        snprintf(w2b,32,"w2  = %+.3f",sel->w2);
        snprintf(bib,32,"bias= %+.3f",sel->bias);
        DTX(w1b,(float)rx+14,(float)y,FS_TINY,AlphaOf(COL_PERC,200)); y+=18;
        DTX(w2b,(float)rx+14,(float)y,FS_TINY,AlphaOf(COL_PERC,200)); y+=18;
        DTX(bib,(float)rx+14,(float)y,FS_TINY,AlphaOf(COL_OR,200));   y+=18;

        float loss=sel->learner.lastLoss;
        Color lc=(loss<0.05f)?GREEN:(loss<0.15f)?YELLOW:RED;
        char  lb2[32],lrd[36];
        snprintf(lb2,32,"loss= %.4f",loss);
        snprintf(lrd,36,"lr衰減= %.3f",sel->learner.lrDecay);
        DTX(lb2,(float)rx+14,(float)y,FS_TINY,lc);                    y+=18;
        DTX(lrd,(float)rx+14,(float)y,FS_TINY,AlphaOf(WHITE,140));    y+=20;

        if (!sel->lossHistory.empty()) {
            y+=4;
            int sparW=PANEL_R-24,sparH=36;
            DrawRectangle(rx+12,y,sparW,sparH,AlphaOf(BLACK,120));
            DrawRectangleLinesEx({(float)rx+12,(float)y,(float)sparW,(float)sparH},1.f,AlphaOf(COL_AI,60));
            DTX("loss趨勢",(float)rx+14,(float)y+2,FS_TINY,AlphaOf(COL_AI,160));
            int   n=(int)sel->lossHistory.size();
            float maxL=*std::max_element(sel->lossHistory.begin(),sel->lossHistory.end());
            maxL=std::max(maxL,0.01f);
            for (int i=0;i<n;i++) {
                float lv=sel->lossHistory[i]/maxL;
                int bwi=std::max(2,(sparW-4)/std::max(n,1));
                int hh=(int)(lv*(sparH-10));
                int bxi=rx+14+(sparW-4)*i/std::max(n,1);
                int byi=y+sparH-2-hh;
                Color bc=(sel->lossHistory[i]<0.05f)?GREEN:(sel->lossHistory[i]<0.15f)?YELLOW:RED;
                DrawRectangle(bxi,byi,std::min(bwi-1,8),hh,AlphaOf(bc,180));
            }
            y+=sparH+6;
        }
    }

    // ── 主動技能資訊 ─────────────────────────────────────────────
    if (sel->type != TType::CPU) {
        int   ti=  (int)sel->type;
        float maxCd=SKILL_CD[ti];
        if (maxCd > 0.f) {
            DrawRoundBox((float)rx+8,(float)y,(float)PANEL_R-16,40,4,AlphaOf(COL_AI,8),AlphaOf(COL_AI,50),1.f);
            char sn[48]; snprintf(sn,48,"[Q] %s",SKILL_NAME[ti]);
            DTX(sn,(float)rx+12,(float)y+4,FS_TINY,AlphaOf(COL_AI,200));
            if (sel->activeCd<=0) {
                DTX("✓ 就緒！",(float)rx+12,(float)y+20,FS_TINY,AlphaOf({100,255,180,255},230));
            } else {
                char cdb[32]; snprintf(cdb,32,"冷卻 %.0f秒",sel->activeCd);
                DTX(cdb,(float)rx+12,(float)y+20,FS_TINY,AlphaOf(ORANGE,200));
            }
            y+=48;
        }
    }

    if (!sel->conns.empty()) {
        char cc[24]; snprintf(cc,24,"連線：%d 條",(int)sel->conns.size());
        DTX(cc,(float)rx+12,(float)y,FS_TINY,AlphaOf(WHITE,120)); y+=20;
    }
    DTX("[C] 連線  [U] 升級",(float)rx+12,(float)y,FS_TINY,AlphaOf(WHITE,90)); y+=18;
    DTX("[DEL] 移除",        (float)rx+12,(float)y,FS_TINY,AlphaOf(WHITE,90));
}

// ══════════════════════════════════════════════════════════════════
//  DrawTopBar
// ══════════════════════════════════════════════════════════════════
void DrawTopBar(Game& G) {
    DrawRectangleGradientH(0, 0, VIRT_W, TOPBAR_H, Color{4, 10, 18, 255}, Color{8, 16, 30, 255});
    DrawRectangleGradientV(0, 0, VIRT_W, TOPBAR_H, AlphaOf(COL_CPU, 10), AlphaOf(BLACK, 0));
    
    if (G.cpuHp < 30.f) {
        float flicker = 0.5f + 0.5f * sinf((float)GetTime() * 12.f);
        DrawRectangle(0, 0, VIRT_W, TOPBAR_H, AlphaOf(RED, (int)(60 * flicker)));
    }
    DrawLine(0, TOPBAR_H, VIRT_W, TOPBAR_H, PANEL_BD);

    float t = (float)GetTime();
    float p2 = 0.8f + 0.2f * sinf(t * 1.5f);
    DTC("邏輯閘防禦戰", VIRT_W / 2, 22, FS_TITLE, AlphaOf(COL_CPU, (int)(220 * p2)));

    char wb[32], lb[20];
    snprintf(wb, 32, "第 %d 波", G.wave);
    snprintf(lb, 20, "命 %d", G.lives);
    DTX(wb, PANEL_L + 20, 10, FS_MED, COL_SENSOR);
    DTX(lb, PANEL_L + 20, 38, FS_MED, G.lives > 10 ? COL_PERC : G.lives > 5 ? ORANGE : RED);

    char sc[32], hs[32];
    snprintf(sc, 32, "分數：%d", G.score);
    snprintf(hs, 32, "最高：%d", G.highScore);
    DTX(sc, PANEL_L + 200, 10, FS_MED, COL_AND);
    DTX(hs, PANEL_L + 200, 38, FS_TINY, AlphaOf(COL_AND, 160));

    auto drawTopRouteBadge = [&](int x, int y, int w, const char* header, const PathPreset& preset, int laneSlot) {
        RouteVisualTheme theme = GetRouteTheme(preset, laneSlot);
        int enemyCount = CountLaneEnemiesUi(G, laneSlot);
        float pressure = LanePressureUi(G, laneSlot);

        DrawRoundBox((float)x, (float)y, (float)w, 24.f, 6.f,
            AlphaOf(theme.fillSoft, 220), AlphaOf(theme.accent, 150), 1.2f);
        DTX(header, (float)x + 8, (float)y + 4, FS_TINY, AlphaOf(theme.accent, 220));
        DTX(preset.name, (float)x + 58, (float)y + 4, FS_TINY, theme.text);

        char eb[12];
        snprintf(eb, 12, "x%d", enemyCount);
        DTX(eb, (float)x + w - 34, (float)y + 4, FS_TINY, AlphaOf(theme.text, 210));
        DrawRectangle(x + 58, y + 18, w - 96, 3, AlphaOf(theme.fill, 170));
        DrawRectangle(x + 58, y + 18, (int)((w - 96) * pressure), 3, AlphaOf(theme.accent, 230));
    };

    int routeCount = G.ActiveLaneCount();
    int badgeGap = 8;
    if (routeCount <= 4) {
        int badgeW = (routeCount <= 2) ? 188 : 150;
        int routeStripW = routeCount * badgeW + (routeCount - 1) * badgeGap;
        int routeStartX = VIRT_W / 2 - routeStripW / 2;
        for (int lane = 0; lane < routeCount; lane++) {
            drawTopRouteBadge(routeStartX + lane * (badgeW + badgeGap), 38, badgeW,
                LaneLabel(lane), G.LanePreset(lane), lane);
        }
    } else {
        int totalEnemies = 0;
        float maxPressure = 0.f;
        int prefLane = G.intel.PreferredPath(routeCount);
        for (int lane = 0; lane < routeCount; lane++) {
            totalEnemies += CountLaneEnemiesUi(G, lane);
            maxPressure = std::max(maxPressure, LanePressureUi(G, lane));
        }
        const PathPreset& preset = G.LanePreset(prefLane);
        RouteVisualTheme theme = GetRouteTheme(preset, prefLane);
        int badgeW = 236;
        int x = VIRT_W / 2 - badgeW / 2;
        DrawRoundBox((float)x, 38.f, (float)badgeW, 24.f, 6.f,
            AlphaOf(theme.fillSoft, 220), AlphaOf(theme.accent, 150), 1.2f);
        char summary[64];
        snprintf(summary, 64, "多線 x%d  敵%d  高壓%s", routeCount, totalEnemies, LaneLabel(prefLane));
        DTX(summary, (float)x + 10, 42.f, FS_TINY, theme.text);
        DrawRectangle(x + 10, 56, badgeW - 20, 3, AlphaOf(theme.fill, 170));
        DrawRectangle(x + 10, 56, (int)((badgeW - 20) * maxPressure), 3, AlphaOf(theme.accent, 230));
    }

    int leftInfoX = VIRT_W / 2 - 360;
    if (G.hasPlannedRouteChange) {
        char preview[96];
        int previewCount = std::max(1, std::min(MAX_LANES, G.nextPreviewLaneCount));
        if (previewCount >= 5) {
            snprintf(preview, 96, "下波輪換：多線 x%d", previewCount);
        } else if (previewCount > 1) {
            char names[72] = "";
            for (int lane = 0; lane < previewCount; lane++) {
                if (G.nextPreviewPaths[lane] < 0) continue;
                if (names[0] != '\0') strncat(names, " / ", sizeof(names) - strlen(names) - 1);
                strncat(names, GetPathPreset(G.nextPreviewPaths[lane]).name, sizeof(names) - strlen(names) - 1);
            }
            snprintf(preview, 96, "下波輪換：%s", names);
        } else {
            snprintf(preview, 96, "下波輪換：%s", GetPathPreset(G.nextPreviewPaths[0]).name);
        }
        DTX(preview, (float)leftInfoX, 10.f, FS_TINY, AlphaOf({255, 210, 150, 255}, 210));
    }

    const char* phaseStr =
        (G.phase == Game::FIGHT) ? "戰鬥中" :
        (G.phase == Game::BUILD) ? "建置" :
        (G.phase == Game::TRAINING) ? "訓練" : "";
    Color phaseCol =
        (G.phase == Game::FIGHT) ? COL_VIRUS : COL_PERC;
    auto drawStatusPill = [&](int x, int y, int w, const char* text, Color border, Color textCol) {
        DrawRoundBox((float)x, (float)y, (float)w, 22.f, 4.f,
                     AlphaOf(border, 28), AlphaOf(border, 170), 1.3f);
        DTC(text, x + w / 2, y + 11, FS_TINY, textCol);
    };
    auto reserveRightText = [&](int& cursor, int y, const char* text, Color col, int fontSize) {
        int width = (int)MCN(text, fontSize) + 2;
        cursor -= width;
        DTX(text, (float)cursor, (float)y, fontSize, col);
        cursor -= 14;
    };

    int rightCursorTop = VIRT_W - PANEL_R - 22;
    int rightCursorBottom = VIRT_W - PANEL_R - 22;

    reserveRightText(rightCursorTop, 18, phaseStr, phaseCol, FS_MED);

    if (G.combo >= 3) {
        char cb[24]; snprintf(cb, 24, "COMBO x%d", G.combo);
        float cp = 0.8f + 0.2f * sinf(t * 8.f);
        Color cc2 = (G.combo >= 10) ? Color{255, 100, 255, 255} : (G.combo >= 5) ? COL_STAR : Color{255, 220, 100, 255};
        cc2.a = (unsigned char)(230 * cp);
        reserveRightText(rightCursorTop, 15, cb, cc2, FS_LARGE);
    }

    if (G.comboSurgeTimer > 0.f) {
        char sb[24]; snprintf(sb, 24, "SURGE %.0fs", ceilf(G.comboSurgeTimer));
        float sp = 0.75f + 0.25f * sinf(t * 10.f);
        int pillW = 108;
        rightCursorTop -= pillW;
        drawStatusPill(rightCursorTop, 8, pillW, sb,
                       Color{255, 140, 90, 255}, AlphaOf(Color{255, 180, 120, 255}, (int)(230 * sp)));
        rightCursorTop -= 10;
    }

    if (G.buffGlobalMark > 0) {
        char ob2[24]; snprintf(ob2, 24, "標記 %.0fs", G.buffGlobalMark);
        int pillW = 90;
        rightCursorBottom -= pillW;
        drawStatusPill(rightCursorBottom, 38, pillW, ob2, COL_XOR, COL_XOR);
        rightCursorBottom -= 6;
    }
    if (G.buffOverfreq > 0) {
        char ob[24]; snprintf(ob, 24, "超頻 %.0fs", G.buffOverfreq);
        int pillW = 90;
        rightCursorBottom -= pillW;
        drawStatusPill(rightCursorBottom, 38, pillW, ob, COL_OR, COL_OR);
        rightCursorBottom -= 6;
    }
    if (G.buffArmorBreak > 0) {
        char ab[24]; snprintf(ab, 24, "破甲 %.0fs", G.buffArmorBreak);
        int pillW = 90;
        rightCursorBottom -= pillW;
        drawStatusPill(rightCursorBottom, 38, pillW, ab, COL_AND, COL_AND);
        rightCursorBottom -= 6;
    }

    if (G.phase == Game::TRAINING) {
        char tb[32]; snprintf(tb, 32, "選擇獎勵 %.1fs", std::max(0.f, G.trainingTimer));
        DTX(tb, (float)leftInfoX, 24.f, FS_TINY, AlphaOf(COL_SENSOR, 220));
    } else if (G.phase == Game::FIGHT && G.waveTelegraphTimer > 0.f) {
        char ib[32]; snprintf(ib, 32, "敵潮接近 %.1fs", std::max(0.f, G.waveTelegraphTimer));
        DTX(ib, (float)leftInfoX, 24.f, FS_TINY, AlphaOf(Color{255, 180, 90, 255}, 230));
    }

    if (G.eventBannerTimer > 0 && G.currentEvent != WaveEvent::NONE) {
        float alpha = std::min(1.f, G.eventBannerTimer);
        float pulse2 = 0.8f + 0.2f * sinf((float)GetTime() * 6.f);
        Color ec = {255, 80, 80, (unsigned char)(230 * alpha * pulse2)};
        DTC(G.eventName.c_str(), VIRT_W / 2, TOPBAR_H + 36, FS_LARGE, ec);
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawBotBar
// ══════════════════════════════════════════════════════════════════
void DrawBotBar(Game& G) {
    int by=VIRT_H-BOTBAR_H;
    DrawRectangle(0,by,VIRT_W,BOTBAR_H,PANEL_BG);
    DrawLine(0,by,VIRT_W,by,PANEL_BD);
    const char* hint=
        (G.phase == Game::TRAINING)
        ? "[1][2][3] 選擇訓練獎勵  [滑鼠] 點選卡片  [P]暫停  [H]說明  [F11]全螢幕"
        : "[空白]發動  [C]連線  [U]升級  [Q]主動技能  [DEL]移除  [T]熱圖  [A]AI提示  [P]暫停  [H]說明  [F11]全螢幕";
    DTC(hint,VIRT_W/2,by+BOTBAR_H/2,FS_TINY,AlphaOf(WHITE,120));
}
