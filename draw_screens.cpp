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

    int frameW = MENU_FRAME_W, frameH = MENU_FRAME_H;
    int fx = cx - frameW / 2;
    int fy = cy - MENU_FRAME_OFFSET_Y;

    DrawLine(96, fy - 34, VIRT_W - 96, fy - 34, AlphaOf(COL_SENSOR, 28));
    DrawLine(96, fy + frameH + 34, VIRT_W - 96, fy + frameH + 34, AlphaOf(COL_SENSOR, 20));
    DrawHudFrame((float)fx, (float)fy, (float)frameW, (float)frameH, 16.f,
                 AlphaOf(BG, 212), AlphaOf(COL_CPU, 82), COL_SENSOR);

    DrawRoundBox((float)fx + 26, (float)fy + 98, (float)frameW - 52, 182.f, 10.f,
                 AlphaOf(COL_SENSOR, 10), AlphaOf(COL_SENSOR, 36), 1.1f);
    int actionY = fy + MENU_ACTION_Y_OFFSET;
    int actionH = MENU_ACTION_H;
    int actionGap = MENU_ACTION_GAP;
    int actionW = (frameW - MENU_ACTION_INSET * 2 - actionGap) / 2;
    int startX = fx + MENU_ACTION_INSET;
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
    DTC("邏輯閘防禦戰", cx, fy + 58, FS_BIG, AlphaOf(COL_CPU, (int)(244 * tp)));
    DTC("用邏輯閘與感知器排出自動防線，擋下衝向 CPU 的病毒。", cx, fy + 114, FS_MED, AlphaOf(COL_SENSOR, 156));

    struct MenuFeature {
        const char* title;
        const char* desc;
        Color       col;
    };
    MenuFeature feats[] = {
        {"訊號鏈防線", "SENSOR → 邏輯閘 → 砲塔", COL_SENSOR},
        {"感知器學習", "跨波調整權重與 loss", COL_PERC},
        {"多線路口", "輪換路線與入口壓力", COL_AI},
        {"主動技能", "EMP、超頻、破甲、標記", COL_CANNON},
    };
    int featX = fx + 42;
    int featY = fy + 152;
    int featGap = 20;
    int featW = (frameW - 84 - featGap) / 2;
    int featH = 56;
    for (int i = 0; i < 4; i++) {
        int col = i % 2;
        int row = i / 2;
        int bx = featX + col * (featW + featGap);
        int by = featY + row * (featH + 20);
        float fp = 0.72f + 0.28f * sinf(t * 1.6f + i * 0.6f);
        Color fc = feats[i].col;
        DrawRoundBox((float)bx, (float)by, (float)featW, (float)featH, 9.f,
                     AlphaOf(fc, 10), AlphaOf(fc, 72), 1.2f);
        DrawLineEx({(float)bx + 18.f, (float)by + 27.f}, {(float)bx + 42.f, (float)by + 27.f}, 1.8f,
                   AlphaOf(fc, (int)(120 * fp)));
        DTX(feats[i].title, (float)bx + 56.f, (float)by + 10.f, FS_SMALL, AlphaOf(fc, 220));
        DTX(feats[i].desc, (float)bx + 56.f, (float)by + 32.f, FS_TINY, AlphaOf(WHITE, 150));
    }

    float sp=0.82f+0.18f*sinf(t*3.2f);
    DTC("開始遊戲", startX + actionW / 2, actionY + 29, FS_LARGE, AlphaOf(COL_PERC, (int)(238 * sp)));
    DTC("[ENTER / SPACE]", startX + actionW / 2, actionY + 62, FS_TINY, AlphaOf(WHITE, 112));
    DTC("教學關卡", tutorialX + actionW / 2, actionY + 29, FS_LARGE, AlphaOf(COL_AI, (int)(224 * sp)));
    DTC("[T] 選擇章節", tutorialX + actionW / 2, actionY + 62, FS_TINY, AlphaOf(WHITE, 112));
    if (G.highScore > 0) {
        char hs[96]; snprintf(hs,96,"[H] 查看說明  ·  歷史最高分：%d",G.highScore);
        DTC(hs,cx,fy+404,FS_MED,AlphaOf(COL_STAR,172));
    } else {
        DTC("[H] 查看說明", cx, fy + 404, FS_MED, AlphaOf(WHITE, 98));
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
    int boxX = PANEL_L + 52;
    int boxY = TOPBAR_H + 30;
    int boxW = MAP_W - 104;
    int boxH = MAP_H - 60;
    int cx = boxX + boxW / 2;

    DrawHudFrame((float)boxX, (float)boxY, (float)boxW, (float)boxH, 14.f,
                 AlphaOf(BG, 238), AlphaOf(COL_SENSOR, 94), COL_SENSOR);
    DrawScreenGrid(boxX + 12, boxY + 12, boxW - 24, boxH - 24, (float)GetTime(), COL_SENSOR);

    DTX("HELP", (float)boxX + 26.f, (float)boxY + 18.f, FS_TINY, AlphaOf(COL_SENSOR, 210));
    DTX("TACTICAL REFERENCE", (float)boxX + boxW - 298.f, (float)boxY + 18.f, FS_TINY, AlphaOf(COL_AI, 180));
    DTC("操作與系統重點", cx, boxY + 58, FS_LARGE, COL_CPU);
    DTC("先建立訊號鏈，再觀察路線與敵情，補強高壓入口。", cx, boxY + 96, FS_TINY, AlphaOf(WHITE, 154));

    struct HelpRow {
        const char* key;
        const char* desc;
        Color       col;
    };
    struct HelpBullet {
        const char* title;
        const char* desc;
        Color       col;
    };
    auto drawSectionCard = [&](int x, int y, int w, int h, const char* title, Color accent) {
        DrawRoundBox((float)x, (float)y, (float)w, (float)h, 10.f,
                     AlphaOf(accent, 10), AlphaOf(accent, 74), 1.3f);
        DTX(title, (float)x + 20.f, (float)y + 16.f, FS_SMALL, AlphaOf(accent, 222));
        DrawLine(x + 20, y + 50, x + w - 20, y + 50, AlphaOf(accent, 40));
    };
    auto drawControlCard = [&](int x, int y, int w, int h, const char* title,
                               const HelpRow* rows, int count, Color accent) {
        drawSectionCard(x, y, w, h, title, accent);
        for (int i = 0; i < count; i++) {
            int rowY = y + 68 + i * 34;
            DTX(rows[i].key, (float)x + 20.f, (float)rowY, FS_TINY, AlphaOf(rows[i].col, 220));
            DTX(rows[i].desc, (float)x + 160.f, (float)rowY, FS_TINY, AlphaOf(WHITE, 164));
        }
    };
    auto drawBulletCard = [&](int x, int y, int w, int h, const char* title,
                              const HelpBullet* bullets, int count, Color accent) {
        drawSectionCard(x, y, w, h, title, accent);
        for (int i = 0; i < count; i++) {
            int rowY = y + 68 + i * 34;
            DrawCircleV({(float)x + 24.f, (float)rowY + 10.f}, 3.5f, AlphaOf(bullets[i].col, 220));
            DTX(bullets[i].title, (float)x + 38.f, (float)rowY, FS_TINY, AlphaOf(bullets[i].col, 220));
            DTX(bullets[i].desc, (float)x + 154.f, (float)rowY, FS_TINY, AlphaOf(WHITE, 154));
        }
    };

    HelpRow buildRows[] = {
        {"左側按鈕", "選元件；再按一次可取消", WHITE},
        {"已有元件", "點擊查看資訊", WHITE},
        {"地圖空格", "放置元件", WHITE},
        {"[C]", "進入連線模式", COL_SENSOR},
        {"[U]", "升級選取元件", COL_PERC},
        {"[DEL]", "移除並退回 50%", COL_CANNON},
        {"[ESC]", "取消放置或連線", WHITE},
        {"[空白]", "發動下一波", COL_PERC},
    };
    HelpRow battleRows[] = {
        {"[Q]", "發動選取元件的主動技能", COL_AI},
        {"[T]", "切換威脅熱圖", COL_AI},
        {"[A]", "切換 AI 顧問提示", COL_AI},
        {"[P]", "暫停 / 繼續", WHITE},
        {"[H]", "開關說明頁", WHITE},
        {"[F11]", "切換全螢幕", WHITE},
    };
    HelpBullet systemRows[] = {
        {"感知器", "每波依覆蓋率更新權重，右側可看 loss", COL_PERC},
        {"多層網路", "感知器可串接成多層訊號鏈", COL_AI},
        {"熱圖", "擊殺熱度會累積，跨波逐漸衰減", COL_CANNON},
        {"路線", "每 3 波輪換；Wave 7 開放入口，10/16/30 逐步多線", COL_SENSOR},
        {"入口態勢", "上方看路線，右側看入口壓力與敵情", COL_CPU},
        {"訓練", "短暫凍結戰場，從左欄三選一獎勵", COL_AND},
    };
    HelpBullet enemyRows[] = {
        {"精英", "Wave 10 起帶護盾，需集中火力", COL_ELITE},
        {"Boss", "50% 召精英，30% 進入狂暴", COL_BOSS},
        {"事件", "蜂群、裝甲、霧戰、突擊、再生、神速", COL_CANNON},
        {"通訊中斷", "感測器範圍大幅縮小", COL_SENSOR},
        {"變異體", "高血、高速、帶再生，需爆發壓制", COL_OR},
        {"圍城艦", "超重裝甲目標，靠破甲與集火處理", COL_AND},
    };

    int cardGap = 28;
    int cardW = (boxW - 82) / 2;
    int topY = boxY + 136;
    int topH = 308;
    int bottomY = topY + topH + cardGap;
    int bottomH = 270;
    int leftX = boxX + 28;
    int rightX = leftX + cardW + cardGap;

    drawControlCard(leftX, topY, cardW, topH, "建造與指令", buildRows,
                    (int)(sizeof(buildRows) / sizeof(buildRows[0])), COL_AND);
    drawControlCard(rightX, topY, cardW, topH, "戰鬥與介面", battleRows,
                    (int)(sizeof(battleRows) / sizeof(battleRows[0])), COL_AI);
    drawBulletCard(leftX, bottomY, cardW, bottomH, "核心系統", systemRows,
                   (int)(sizeof(systemRows) / sizeof(systemRows[0])), COL_SENSOR);
    drawBulletCard(rightX, bottomY, cardW, bottomH, "敵人與事件", enemyRows,
                   (int)(sizeof(enemyRows) / sizeof(enemyRows[0])), COL_CANNON);

    DTC("按 [H] / [ESC] 或點擊任意處關閉", cx, boxY + boxH - 38, FS_MED, AlphaOf(WHITE, 118));
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

    int panelW = 820, panelH = 748;
    int px = cx - panelW / 2;
    int py = oy + 66;
    DrawHudFrame((float)px, (float)py, (float)panelW, (float)panelH, 16.f,
                 AlphaOf(BG, 224), AlphaOf(RED, 108), Color{255, 168, 148, 255});
    DTX("SYSTEM FAILURE", (float)px + 28, (float)py + 18, FS_TINY, AlphaOf(RED, 210));
    DTX("CORE BREACHED // DEFENSE GRID LOST", (float)px + panelW - 418, (float)py + 18, FS_TINY, AlphaOf(COL_SENSOR, 150));

    float rp=0.75f+0.25f*sinf(t*4.f);
    DTC("防線崩潰",cx,py+74,FS_BIG,AlphaOf(RED,(int)(240*rp)));
    DTC("核心節點失守，戰術網路離線",cx,py+126,FS_SMALL,AlphaOf(COL_SENSOR,150));

    int statY = py + 176;
    int statGap = 26;
    int statW = (panelW - 64 - statGap * 2) / 3;
    int statH = 106;
    int statX = px + 32;
    DrawRoundBox((float)statX, (float)statY, (float)statW, (float)statH, 10.f,
                 AlphaOf(COL_AND, 10), AlphaOf(COL_AND, 72), 1.5f);
    DrawRoundBox((float)(statX + statW + statGap), (float)statY, (float)statW, (float)statH, 10.f,
                 AlphaOf(COL_PERC, 10), AlphaOf(COL_PERC, 72), 1.5f);
    DrawRoundBox((float)(statX + (statW + statGap) * 2), (float)statY, (float)statW, (float)statH, 10.f,
                 AlphaOf(COL_STAR, 10), AlphaOf(COL_STAR, 72), 1.5f);

    char sbuf[64]; snprintf(sbuf,64,"%d",G.score);
    DTX("最終分數", (float)statX + 18.f, (float)statY + 14.f, FS_TINY, AlphaOf(COL_AND, 220));
    DTX(sbuf,       (float)statX + 18.f, (float)statY + 44.f, FS_TITLE, COL_AND);

    char wb[48]; snprintf(wb,48,"%d 波",G.wave);
    DTX("撐過波次", (float)(statX + statW + statGap) + 18.f, (float)statY + 14.f, FS_TINY, AlphaOf(COL_PERC, 220));
    DTX(wb,        (float)(statX + statW + statGap) + 18.f, (float)statY + 44.f, FS_TITLE, AlphaOf(COL_PERC, 230));

    float recordX = (float)(statX + (statW + statGap) * 2) + 18.f;
    DTX("紀錄狀態", recordX, (float)statY + 14.f, FS_TINY, AlphaOf(COL_STAR, 220));
    if (G.score>=G.highScore && G.highScore>0) {
        DTX("新紀錄！", recordX, (float)statY + 46.f, FS_LARGE, COL_STAR);
    } else {
        char hs[48]; snprintf(hs,48,"最高 %d",G.highScore);
        DTX(hs, recordX, (float)statY + 46.f, FS_LARGE, AlphaOf(COL_STAR,180));
    }

    int routeY = py + 324;
    DTC("路線威脅摘要", cx, routeY, FS_MED, AlphaOf(COL_CPU, 220));
    routeY += 36;

    bool showRouteMetric = G.intel.adapted;
    int laneCount = G.ActiveLaneCount();
    int preferredLane = (laneCount > 1) ? G.intel.PreferredPath(laneCount) : 0;
    if (laneCount > 1) {
        int cols = (laneCount <= 2) ? laneCount : 3;
        int rows = (laneCount + cols - 1) / cols;
        int gap = 16;
        int cardW = (panelW - 64 - gap * (cols - 1)) / cols;
        for (int lane = 0; lane < laneCount; lane++) {
            char header[16];
            snprintf(header, 16, "路線%d", lane + 1);
            int col = lane % cols;
            int row = lane / cols;
            DrawRouteSummaryCard(px + 32 + col * (cardW + gap), routeY + row * 98, cardW, header,
                                 G.LanePreset(lane), lane, G.intel.pathSurvRate[lane],
                                 showRouteMetric, showRouteMetric && preferredLane == lane);
        }
        routeY += rows * 98;
    } else {
        float livePressure = LanePressureScreen(G, 0);
        float routeMetric = showRouteMetric ? G.intel.pathSurvRate[0] : livePressure;
        DrawRouteSummaryCard(px + 32, routeY, panelW - 64, "當前路線", G.LanePreset(0), 0,
                             routeMetric, showRouteMetric, true);
        routeY += 102;
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
    int aiY = routeY + 36;
    DrawRoundBox((float)px + 32, (float)aiY, (float)panelW - 64, 78.f, 10.f,
                 AlphaOf(COL_AI, 10), AlphaOf(COL_AI, 70), 1.3f);
    DTX("AI 收斂摘要", (float)px + 50, (float)aiY + 12, FS_TINY, AlphaOf(COL_AI, 210));
    if (pctCount > 0) {
        avgLoss /= pctCount;
        char aib[64]; snprintf(aib,64,"感知器 %d 個 | 平均 loss %.4f",pctCount,avgLoss);
        DTX(aib,(float)px + 50,(float)aiY + 34,FS_SMALL,AlphaOf(COL_AI,188));
        const char* grade=
            (avgLoss<0.05f)?"神經網路已收斂":
            (avgLoss<0.15f)?"學習中，再多幾波":
                            "需要更多感知器或連線";
        DTX(grade,(float)px + 50,(float)aiY + 58,FS_TINY,AlphaOf(COL_AI,158));
    } else {
        DTC("未部署感知器節點",cx,aiY+39,FS_SMALL,AlphaOf(COL_AI,150));
    }

    float pp=0.86f+0.14f*sinf(t*4.f);
    DrawRoundBox((float)cx - 278.f, (float)py + panelH - 66.f, 556.f, 46.f, 10.f,
                 AlphaOf(WHITE, 10), AlphaOf(WHITE, 92), 1.5f);
    DTC("[R] 重新開始    [ENTER] 主選單",cx,py+panelH-42,FS_LARGE,AlphaOf(Color{220,220,220,255},(int)(228*pp)));
}
