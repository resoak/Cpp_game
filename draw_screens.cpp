// ================================================================
//  draw_screens.cpp — DrawMenu, DrawHelp, DrawGameOver
// ================================================================
#include "draw.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

static float LanePressureScreen(const Game& G, int laneSlot) {
    const auto& cells = G.LaneCells(laneSlot);
    if (cells.empty()) return 0.f;

    float furthest = 0.f;
    for (auto& e : G.enemies) {
        if (e.pathIdx != laneSlot) continue;
        furthest = std::max(furthest, e.pathPos / (float)std::max(1, (int)cells.size()));
    }
    return Clamp(furthest, 0.f, 1.f);
}

static void DrawHudFrame(float x, float y, float w, float h, float r,
                         Color fill, Color border, Color glow) {
    DrawRoundBox(x, y, w, h, r, fill, border, 1.8f);
    DrawRoundBox(x + 8.f, y + 8.f, w - 16.f, h - 16.f, std::max(4.f, r - 4.f),
                 AlphaOf(fill, 0), AlphaOf(glow, 30), 1.f);

    DrawLineV({x + 18.f, y + 16.f}, {x + 84.f, y + 16.f}, AlphaOf(glow, 170));
    DrawLineV({x + 16.f, y + 18.f}, {x + 16.f, y + 60.f}, AlphaOf(glow, 140));
    DrawLineV({x + w - 84.f, y + 16.f}, {x + w - 18.f, y + 16.f}, AlphaOf(glow, 170));
    DrawLineV({x + w - 16.f, y + 18.f}, {x + w - 16.f, y + 60.f}, AlphaOf(glow, 140));
    DrawLineV({x + 18.f, y + h - 16.f}, {x + 84.f, y + h - 16.f}, AlphaOf(glow, 120));
    DrawLineV({x + w - 84.f, y + h - 16.f}, {x + w - 18.f, y + h - 16.f}, AlphaOf(glow, 120));
}

static void DrawScreenGrid(int x, int y, int w, int h, float t, Color accent) {
    for (int gx = x; gx <= x + w; gx += 64) {
        int alpha = ((gx - x) % 192 == 0) ? 18 : 10;
        DrawLine(gx, y, gx, y + h, AlphaOf(accent, alpha));
    }
    for (int gy = y; gy <= y + h; gy += 64) {
        int alpha = ((gy - y) % 192 == 0) ? 16 : 9;
        DrawLine(x, gy, x + w, gy, AlphaOf(accent, alpha));
    }

    for (int i = 0; i < 4; i++) {
        float sweep = fmodf(t * (120.f + i * 28.f), (float)(w + 280)) - 140.f;
        float ly = (float)y + 28.f + i * 18.f;
        DrawLineEx({(float)x + sweep, ly}, {(float)x + sweep + 180.f, ly}, 1.2f,
                   AlphaOf(COL_SENSOR, 70 - i * 12));
    }
}

static void DrawRouteSummaryCard(int x, int y, int w, const char* header, const PathPreset& preset,
                                 int laneSlot, float metric, bool showMetric, bool highlight) {
    RouteVisualTheme theme = GetRouteTheme(preset, laneSlot);
    float pulse = 0.65f + 0.35f * sinf((float)GetTime() * 2.4f + laneSlot * 0.7f + preset.family * 0.35f);
    Color accent = highlight ? theme.accent : AlphaOf(theme.accent, 170);

    DrawRoundBox((float)x, (float)y, (float)w, 82.f, 10.f,
                 AlphaOf(theme.fillSoft, 226),
                 AlphaOf(accent, (int)(100 + pulse * (highlight ? 110.f : 70.f))), 1.5f);
    DTX(header, (float)x + 12, (float)y + 8, FS_TINY, AlphaOf(accent, 230));
    DTX(theme.shortLabel, (float)x + 12, (float)y + 26, FS_TINY, AlphaOf(theme.glow, 190));
    DTX(preset.name, (float)x + 90, (float)y + 8, FS_SMALL, theme.text);
    DTX(theme.sideLabel, (float)x + 90, (float)y + 34, FS_TINY, AlphaOf(theme.glow, 165));

    if (showMetric) {
        float clamped = Clamp(metric, 0.f, 1.f);
        char mb[24];
        snprintf(mb, 24, "存活 %2.0f%%", clamped * 100.f);
        DTX(mb, (float)(x + w - 112), (float)y + 8, FS_TINY, AlphaOf(accent, 220));
        DrawRectangle(x + 90, y + 58, w - 112, 6, AlphaOf(theme.fill, 185));
        DrawRectangle(x + 90, y + 58, (int)((w - 112) * clamped), 6, AlphaOf(accent, 230));
    } else {
        DTX("採樣中", (float)(x + w - 72), (float)y + 8, FS_TINY, AlphaOf(WHITE, 120));
        DrawRectangle(x + 90, y + 58, w - 112, 6, AlphaOf(theme.fill, 120));
    }

    if (highlight) {
        DTX("HIGH", (float)(x + w - 58), (float)y + 34, FS_TINY, AlphaOf(theme.glow, 220));
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawMenu
// ══════════════════════════════════════════════════════════════════
void DrawMenu(Game& G) {
    float t  = (float)GetTime();
    int   cx = VIRT_W / 2, cy = VIRT_H / 2;

    DrawRectangleGradientV(0,0,VIRT_W,VIRT_H,Color{3,7,14,255},Color{8,18,35,255});
    DrawStars(G);
    DrawScreenGrid(0, 0, VIRT_W, VIRT_H, t, COL_SENSOR);

    int frameW = 1120, frameH = 390;
    int fx = cx - frameW / 2;
    int fy = cy - 250;

    DrawLine(96, fy - 28, VIRT_W - 96, fy - 28, AlphaOf(COL_SENSOR, 28));
    DrawLine(96, fy + frameH + 28, VIRT_W - 96, fy + frameH + 28, AlphaOf(COL_SENSOR, 20));
    DrawHudFrame((float)fx, (float)fy, (float)frameW, (float)frameH, 16.f,
                 AlphaOf(BG, 212), AlphaOf(COL_CPU, 82), COL_SENSOR);

    DrawRoundBox((float)fx + 22, (float)fy + 92, (float)frameW - 44, 150.f, 10.f,
                 AlphaOf(COL_SENSOR, 10), AlphaOf(COL_SENSOR, 36), 1.1f);
    int actionY = fy + 264;
    int actionH = 72;
    int actionGap = 14;
    int actionW = (frameW - 44 - actionGap) / 2;
    int startX = fx + 22;
    int tutorialX = startX + actionW + actionGap;
    DrawRoundBox((float)startX, (float)actionY, (float)actionW, (float)actionH, 12.f,
                 AlphaOf(COL_PERC, 14), AlphaOf(COL_PERC, 96), 1.7f);
    DrawRoundBox((float)tutorialX, (float)actionY, (float)actionW, (float)actionH, 12.f,
                 AlphaOf(COL_AI, 12), AlphaOf(COL_AI, 88), 1.5f);

    DTX("SYSTEM BOOT", (float)fx + 28, (float)fy + 18, FS_TINY, AlphaOf(COL_SENSOR, 180));
    DTX("NEURAL FORTRESS // MENU", (float)fx + frameW - 324, (float)fy + 18, FS_TINY, AlphaOf(COL_AI, 180));
    DrawHex({(float)fx + 74.f, (float)fy + 56.f}, 18.f, AlphaOf(COL_SENSOR, 24), AlphaOf(COL_SENSOR, 160));
    DrawHex({(float)fx + frameW - 74.f, (float)fy + 56.f}, 18.f, AlphaOf(COL_AI, 22), AlphaOf(COL_AI, 150));

    for (int i=4;i>=1;i--) {
        float glow=30.f*i*(0.8f+0.2f*sinf(t*1.5f));
        DrawCircleV({(float)cx,(float)(fy+64)},glow,AlphaOf(COL_CPU,6/i));
    }

    float tp=0.90f+0.10f*sinf(t*1.8f);
    DTC("邏輯閘防禦戰",                          cx,fy+54,FS_BIG, AlphaOf(COL_CPU,   (int)(244*tp)));
    DTC("Logic Gate Defense  v3.0 + AI Edition", cx,fy+114,FS_MED, AlphaOf(COL_SENSOR,156));

    const char* feats[] = {
        "★ 神經網路訊號傳播","★ 感知器多層學習+loss圖","★ Boss 狀態機 AI",
        "★ 威脅熱圖視覺化",  "★ 雙路徑+預告系統",      "★ 主動技能+NAND閘"
    };
    for (int i=0;i<6;i++) {
        int   col=i%3,row=i/3;
        int   bx=fx+54+col*344, by=fy+120+row*54;
        float fp=0.72f+0.28f*sinf(t*1.6f+i*0.6f);
        Color fc = (row == 0) ? COL_AI : COL_SENSOR;
        DrawLineEx({(float)bx, (float)by + 13.f}, {(float)bx + 22.f, (float)by + 13.f}, 1.8f, AlphaOf(fc, 128));
        DrawCircleV({(float)bx + 28.f, (float)by + 13.f}, 2.5f, AlphaOf(fc, (int)(138 * fp)));
        DTX(feats[i], (float)bx + 42, (float)by, FS_SMALL, AlphaOf(fc, (int)(148 * fp)));
    }

    float sp=0.82f+0.18f*sinf(t*3.2f);
    DTC("開始遊戲", startX + actionW / 2, fy + 292, FS_LARGE, AlphaOf(COL_PERC, (int)(238 * sp)));
    DTC("[ENTER / SPACE]", startX + actionW / 2, fy + 326, FS_TINY, AlphaOf(WHITE, 112));
    DTC("教學關卡", tutorialX + actionW / 2, fy + 292, FS_LARGE, AlphaOf(COL_AI, (int)(224 * sp)));
    DTC("[T] 選擇章節", tutorialX + actionW / 2, fy + 326, FS_TINY, AlphaOf(WHITE, 112));
    DTC("[H] 查看說明", cx, fy + 358, FS_MED, AlphaOf(WHITE, 98));

    if (G.highScore > 0) {
        char hs[40]; snprintf(hs,40,"歷史最高分：%d",G.highScore);
        DTC(hs,cx,fy+372,FS_MED,AlphaOf(COL_STAR,172));
    }

    DTX("v3.0+AI  Powered by Raylib",16,(float)(VIRT_H-28),FS_TINY,AlphaOf(WHITE,50));

    // ── 底部神經網路動畫 ─────────────────────────────────────────
    static float nodeT = 0.f;
    nodeT += GetFrameTime();
    const int NN=6;
    float nx[NN],ny[NN];
    for (int i=0;i<NN;i++) {
        nx[i]=(float)cx-250+i*100.f;
        ny[i]=(float)(cy+195)+sinf(nodeT*1.4f+i)*18.f;
    }
    for (int i=0;i<NN-1;i++) {
        float sig=0.5f+0.5f*sinf(nodeT*2.f+i*0.9f);
        Color lc=COL_PERC; lc.a=(unsigned char)(60+sig*100);
        DrawLineEx({nx[i],ny[i]},{nx[i+1],ny[i+1]},1.5f+sig*2.f,lc);
        float dot=fmodf(nodeT*0.8f+i*0.18f,1.f);
        Vector2 dp={nx[i]+(nx[i+1]-nx[i])*dot,ny[i]+(ny[i+1]-ny[i])*dot};
        DrawCircleV(dp,5.f,AlphaOf(COL_PERC,(int)(180*sig)));
    }
    for (int i=0;i<NN;i++) {
        float sig=0.5f+0.5f*sinf(nodeT*1.6f+i);
        DrawHex({nx[i],ny[i]},14.f,AlphaOf(COL_PERC,(int)(40*sig)),AlphaOf(COL_PERC,(int)(160*sig)));
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawHelp
// ══════════════════════════════════════════════════════════════════
void DrawHelp() {
    int boxX = PANEL_L + 80;
    int boxY = TOPBAR_H + 40;
    int boxW = MAP_W - 160;
    int boxH = MAP_H - 80;

    DrawRectangle(boxX, boxY, boxW, boxH, Color{4,9,18,240});
    DrawRectangleLinesEx(
        {(float)boxX,(float)boxY,(float)boxW,(float)boxH},
        2.f,COL_SENSOR
    );

    int cx=PANEL_L+MAP_W/2, y=TOPBAR_H+72;
    DTC("── 操作說明 ──",cx,y,FS_LARGE,COL_CPU); y+=40;
    DrawLine(boxX + 42, y - 10, boxX + boxW - 42, y - 10, AlphaOf(COL_SENSOR, 44));

    struct HelpRow { const char* key; const char* desc; Color col; };
    HelpRow rows[] = {
        {"點擊左側按鈕","選擇元件（再按取消）",        WHITE     },
        {"點擊地圖空格","放置元件",                     WHITE     },
        {"[C]",         "進入連線模式",                 COL_SENSOR},
        {"[U]",         "升級選取元件",                 COL_PERC  },
        {"[Q]",         "發動主動技能（選取元件後）",   COL_AI    },
        {"[DEL]",       "移除選取元件（退款50%）",      COL_CANNON},
        {"[空白/ENTER]","發動下一波次",                 COL_PERC  },
        {"[T]",         "切換威脅熱圖",                 COL_AI    },
        {"[A]",         "切換 AI 顧問提示",             COL_AI    },
        {"[P]",         "暫停/繼續",                    WHITE     },
        {"[H]",         "說明",                         WHITE     },
        {"[F11]",       "全螢幕切換",                   WHITE     },
    };
    for (auto& r : rows) {
        float kw=MCN(r.key,FS_SMALL);
        int kx=PANEL_L+196, dx=kx+80;
        DTX(r.key, (float)(kx-(int)kw/2-30),(float)y,FS_SMALL,AlphaOf(COL_AND,220));
        DTX(r.desc,(float)dx,               (float)y+2.f,FS_TINY,AlphaOf(r.col, 174));
        y+=26;
    }

    y+=8;
    DTC("── AI 與關卡說明 ──",cx,y,FS_MED,COL_AI); y+=30;
    DrawLine(boxX + 42, y - 8, boxX + boxW - 42, y - 8, AlphaOf(COL_AI, 38));

    struct AIRow { const char* title; const char* desc; };
    AIRow aiRows[] = {
        {"感知器學習",  "每波結束以下游砲塔射程覆蓋率為目標，對 w1/w2/bias 執行 Delta Rule 更新"},
        {"感知器層數",  "感知器串接形成多層網路（L1/L2/L3），右側面板顯示 loss 折線圖"},
        {"威脅熱圖",    "擊殺位置累積熱度，跨波衰減 15%，Boss 貢獻 8x"},
        {"Boss 狀態機", "偵測前方高威脅→EVADE加速x2.0；血量<30%永久RAMPAGE x2.8"},
        {"Boss 分裂",   "Boss 降至 50% HP 時立即召喚 3 隻精英，務必保留火力！"},
        {"精英護盾",    "Wave 10+ 精英帶護盾（25% HP），護盾破碎前傷害全吸收"},
        {"波次事件",    "蜂群/裝甲洪流/霧戰/精英突擊/護衛Boss/再生軍/神速"},
        {"通訊中斷",    "感測器範圍縮減至 30%，必須依賴感知器網路辨識目標"},
        {"變異體",      "HP x2、速度 x1.3、無裝甲、帶再生，爆發傷害才能壓制"},
        {"圍城艦",      "單隻 HP x8、裝甲 0.8 的超重坦，需集中全火力穿甲"},
        {"路線輪換",    "每 3 波自動換路線（共 11 條），訓練期間顯示下波路線預告"},
        {"雙路進攻",    "Wave 8 起開通第二條路徑，敵人交替從兩路進攻"},
        {"主動技能",    "選取元件後按 [Q]：SENSOR=EMP暫停，OR=超頻，CANNON=超砲等"},
        {"NAND閘",      "非AND閘：後期反制裝甲利器，配合主動技能可破甲歸零"},
    };
    int leftColX = PANEL_L + 100;
    int rightColX = PANEL_L + MAP_W / 2 + 26;
    for (int i = 0; i < (int)(sizeof(aiRows) / sizeof(aiRows[0])); i++) {
        int col = (i < 7) ? 0 : 1;
        int row = (i < 7) ? i : (i - 7);
        int ax = (col == 0) ? leftColX : rightColX;
        int ay = y + row * 42;
        DTX(aiRows[i].title, (float)ax, (float)ay, FS_TINY, AlphaOf(COL_AI, 220));
        DTX(aiRows[i].desc,  (float)(ax + 18), (float)(ay + 18), FS_TINY, AlphaOf(WHITE, 148));
    }

    DTC("點擊任意處或按 [H]/[ESC] 關閉",cx,TOPBAR_H+MAP_H-60,FS_MED,AlphaOf(WHITE,118));
}

// ══════════════════════════════════════════════════════════════════
//  DrawGameOver
// ══════════════════════════════════════════════════════════════════
void DrawGameOver(Game& G) {
    int   cx=PANEL_L+MAP_W/2;
    float t=(float)GetTime();
    int   ox=PANEL_L, oy=TOPBAR_H;

    DrawRectangleGradientV(ox, oy, MAP_W, MAP_H, Color{5, 8, 16, 220}, Color{28, 3, 6, 238});
    DrawRectangle(ox, oy, MAP_W, MAP_H, AlphaOf(RED, 24));
    DrawScreenGrid(ox, oy, MAP_W, MAP_H, t, RED);

    int panelW = 760, panelH = 540;
    int px = cx - panelW / 2;
    int py = oy + 118;
    DrawHudFrame((float)px, (float)py, (float)panelW, (float)panelH, 16.f,
                 AlphaOf(BG, 224), AlphaOf(RED, 108), Color{255, 168, 148, 255});
    DTX("SYSTEM FAILURE", (float)px + 28, (float)py + 18, FS_TINY, AlphaOf(RED, 210));
    DTX("CORE BREACHED // DEFENSE GRID LOST", (float)px + panelW - 418, (float)py + 18, FS_TINY, AlphaOf(COL_SENSOR, 150));

    float rp=0.75f+0.25f*sinf(t*4.f);
    DTC("防線崩潰",cx,py+68,FS_BIG,AlphaOf(RED,(int)(240*rp)));
    DTC("核心節點失守，戰術網路離線",cx,py+116,FS_SMALL,AlphaOf(COL_SENSOR,150));

    int statY = py + 148;
    int statW = 214;
    DrawRoundBox((float)px + 28, (float)statY, (float)statW, 90.f, 10.f,
                 AlphaOf(COL_AND, 10), AlphaOf(COL_AND, 72), 1.5f);
    DrawRoundBox((float)px + 273, (float)statY, (float)statW, 90.f, 10.f,
                 AlphaOf(COL_PERC, 10), AlphaOf(COL_PERC, 72), 1.5f);
    DrawRoundBox((float)px + 518, (float)statY, (float)statW, 90.f, 10.f,
                 AlphaOf(COL_STAR, 10), AlphaOf(COL_STAR, 72), 1.5f);

    char sbuf[64]; snprintf(sbuf,64,"%d",G.score);
    DTX("最終分數", (float)px + 44, (float)statY + 12, FS_TINY, AlphaOf(COL_AND, 220));
    DTX(sbuf,       (float)px + 44, (float)statY + 38, FS_TITLE, COL_AND);

    char wb[48]; snprintf(wb,48,"%d 波",G.wave);
    DTX("撐過波次", (float)px + 289, (float)statY + 12, FS_TINY, AlphaOf(COL_PERC, 220));
    DTX(wb,        (float)px + 289, (float)statY + 38, FS_TITLE, AlphaOf(COL_PERC, 230));

    DTX("紀錄狀態", (float)px + 534, (float)statY + 12, FS_TINY, AlphaOf(COL_STAR, 220));
    if (G.score>=G.highScore && G.highScore>0) {
        DTX("新紀錄！", (float)px + 534, (float)statY + 40, FS_LARGE, COL_STAR);
    } else {
        char hs[48]; snprintf(hs,48,"最高 %d",G.highScore);
        DTX(hs, (float)px + 534, (float)statY + 40, FS_LARGE, AlphaOf(COL_STAR,180));
    }

    int routeY = py + 268;
    DTC("路線威脅摘要", cx, routeY, FS_MED, AlphaOf(COL_CPU, 220));
    routeY += 30;

    bool showRouteMetric = G.intel.adapted;
    int laneCount = G.ActiveLaneCount();
    int preferredLane = (laneCount > 1) ? G.intel.PreferredPath(laneCount) : 0;
    if (laneCount > 1) {
        int cols = (laneCount <= 2) ? laneCount : 3;
        int rows = (laneCount + cols - 1) / cols;
        int gap = 12;
        int cardW = (panelW - 56 - gap * (cols - 1)) / cols;
        for (int lane = 0; lane < laneCount; lane++) {
            char header[16];
            snprintf(header, 16, "路線%d", lane + 1);
            int col = lane % cols;
            int row = lane / cols;
            DrawRouteSummaryCard(px + 28 + col * (cardW + gap), routeY + row * 92, cardW, header,
                                 G.LanePreset(lane), lane, G.intel.pathSurvRate[lane],
                                 showRouteMetric, showRouteMetric && preferredLane == lane);
        }
        routeY += rows * 92;
    } else {
        float livePressure = LanePressureScreen(G, 0);
        float routeMetric = showRouteMetric ? G.intel.pathSurvRate[0] : livePressure;
        DrawRouteSummaryCard(px + 28, routeY, panelW - 56, "當前路線", G.LanePreset(0), 0,
                             routeMetric, showRouteMetric, true);
        routeY += 96;
    }

    if (showRouteMetric) {
        const PathPreset& prefPreset = G.LanePreset(preferredLane);
        RouteVisualTheme prefTheme = GetRouteTheme(prefPreset, preferredLane);
        char rb[128];
        if (laneCount > 1) {
            snprintf(rb, 128, "高壓入口：路線%d %s / %s  ·  學習 %d 波",
                     preferredLane + 1, prefTheme.sideLabel,
                     prefPreset.name, G.intel.wavesLearned);
        } else {
            snprintf(rb, 128, "單線資料完成：%s / %s  ·  學習 %d 波",
                     prefTheme.sideLabel, prefPreset.name, G.intel.wavesLearned);
        }
        DTC(rb, cx, routeY, FS_TINY, AlphaOf(prefTheme.accent, 202));
    } else {
        char rb[96];
        if (laneCount > 1) {
            float maxPressure = 0.f;
            for (int lane = 0; lane < laneCount; lane++) {
                maxPressure = std::max(maxPressure, LanePressureScreen(G, lane));
            }
            snprintf(rb, 96, "路線資料採樣中 · 最高壓力 %.0f%% / %d 路線",
                     maxPressure * 100.f, laneCount);
        } else {
            snprintf(rb, 96, "路線資料採樣中 · 目前壓力 %.0f%%",
                     LanePressureScreen(G, 0) * 100.f);
        }
        DTC(rb, cx, routeY, FS_TINY, AlphaOf(WHITE, 108));
    }

    int   pctCount=0;
    float avgLoss=0.f;
    for (auto& tw : G.towers) {
        if (tw.type==TType::PERCEPTRON) { pctCount++; avgLoss+=tw.learner.lastLoss; }
    }
    int aiY = py + 404;
    DrawRoundBox((float)px + 28, (float)aiY, (float)panelW - 56, 78.f, 10.f,
                 AlphaOf(COL_AI, 10), AlphaOf(COL_AI, 70), 1.3f);
    DTX("AI 收斂摘要", (float)px + 44, (float)aiY + 12, FS_TINY, AlphaOf(COL_AI, 210));
    if (pctCount > 0) {
        avgLoss /= pctCount;
        char aib[64]; snprintf(aib,64,"感知器 %d 個 | 平均 loss %.4f",pctCount,avgLoss);
        DTX(aib,(float)px + 44,(float)aiY + 34,FS_SMALL,AlphaOf(COL_AI,188));
        const char* grade=
            (avgLoss<0.05f)?"神經網路已收斂":
            (avgLoss<0.15f)?"學習中，再多幾波":
                            "需要更多感知器或連線";
        DTX(grade,(float)px + 44,(float)aiY + 58,FS_TINY,AlphaOf(COL_AI,158));
    } else {
        DTC("未部署感知器節點",cx,aiY+39,FS_SMALL,AlphaOf(COL_AI,150));
    }

    float pp=0.86f+0.14f*sinf(t*4.f);
    DrawRoundBox((float)cx - 278.f, (float)py + panelH - 60.f, 556.f, 44.f, 10.f,
                 AlphaOf(WHITE, 10), AlphaOf(WHITE, 92), 1.5f);
    DTC("[R] 重新開始    [ENTER] 主選單",cx,py+panelH-38,FS_LARGE,AlphaOf(Color{220,220,220,255},(int)(228*pp)));
}
