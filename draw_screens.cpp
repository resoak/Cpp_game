// ================================================================
//  draw_screens.cpp — DrawMenu, DrawHelp, DrawGameOver
// ================================================================
#include "draw.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

// ══════════════════════════════════════════════════════════════════
//  DrawMenu
// ══════════════════════════════════════════════════════════════════
void DrawMenu(Game& G) {
    float t  = (float)GetTime();
    int   cx = VIRT_W / 2, cy = VIRT_H / 2;

    DrawRectangleGradientV(0,0,VIRT_W,VIRT_H,Color{3,7,14,255},Color{8,18,35,255});
    DrawStars(G);

    for (int i=4;i>=1;i--) {
        float glow=30.f*i*(0.8f+0.2f*sinf(t*1.5f));
        DrawCircleV({(float)cx,(float)(cy-120)},glow,AlphaOf(COL_CPU,6/i));
    }

    float tp=0.85f+0.15f*sinf(t*2.f);
    DTC("邏輯閘防禦戰",                          cx,cy-140,FS_BIG, AlphaOf(COL_CPU,   (int)(240*tp)));
    DTC("Logic Gate Defense  v3.0 + AI Edition", cx,cy- 82,FS_MED, AlphaOf(COL_SENSOR,180));

    const char* feats[] = {
        "★ 神經網路訊號傳播","★ 感知器多層學習+loss圖","★ Boss 狀態機 AI",
        "★ 威脅熱圖視覺化",  "★ 雙路徑+預告系統",      "★ 主動技能+NAND閘"
    };
    for (int i=0;i<6;i++) {
        int   col=i%3,row=i/3;
        int   fx=cx-460+col*320, fy=cy-30+row*28;
        float fp=0.6f+0.4f*sinf(t*1.8f+i*0.6f);
        DTC(feats[i],fx,fy,FS_SMALL,AlphaOf(COL_AI,(int)(180*fp)));
    }

    float sp=0.7f+0.3f*sinf(t*3.5f);
    DTC("按 ENTER 或 空白鍵 開始",cx,cy+60, FS_LARGE,AlphaOf(COL_PERC,(int)(230*sp)));
    DTC("[H] 查看說明",             cx,cy+96, FS_MED,  AlphaOf(WHITE,120));

    if (G.highScore > 0) {
        char hs[40]; snprintf(hs,40,"歷史最高分：%d",G.highScore);
        DTC(hs,cx,cy+138,FS_MED,AlphaOf(COL_STAR,200));
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
    DrawRectangle(PANEL_L+80,TOPBAR_H+40,MAP_W-160,MAP_H-80,Color{4,9,18,240});
    DrawRectangleLinesEx(
        {(float)(PANEL_L+80),(float)(TOPBAR_H+40),(float)(MAP_W-160),(float)(MAP_H-80)},
        2.f,COL_SENSOR
    );

    int cx=PANEL_L+MAP_W/2, y=TOPBAR_H+70;
    DTC("── 操作說明 ──",cx,y,FS_LARGE,COL_CPU); y+=46;

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
        int kx=PANEL_L+200, dx=kx+80;
        DTX(r.key, (float)(kx-(int)kw/2-30),(float)y,FS_SMALL,AlphaOf(COL_AND,220));
        DTX(r.desc,(float)dx,               (float)y,FS_SMALL,AlphaOf(r.col, 190));
        y+=28;
    }

    y+=10;
    DTC("── AI 與關卡說明 ──",cx,y,FS_MED,COL_AI); y+=34;

    struct AIRow { const char* title; const char* desc; };
    AIRow aiRows[] = {
        {"感知器學習",  "每波結束以擊殺率為目標，對 w1/w2/bias 執行 Delta Rule 梯度更新"},
        {"感知器層數",  "感知器串接形成多層網路（L1/L2/L3），右側面板顯示 loss 折線圖"},
        {"威脅熱圖",    "擊殺位置累積熱度，跨波衰減 15%，Boss 貢獻 8x"},
        {"Boss 狀態機", "偵測前方高威脅→EVADE加速x2.0；血量<30%永久RAMPAGE x2.8"},
        {"Boss 分裂",   "Boss 降至 50% HP 時立即召喚 3 隻精英，務必保留火力！"},
        {"精英護盾",    "Wave 10+ 精英帶護盾（25% HP），護盾破碎前傷害全吸收"},
        {"波次事件",    "蜂群/裝甲洪流/霧戰/精英突擊/護衛Boss/再生軍/神速"},
        {"通訊中斷",    "感測器範圍縮減至 30%，必須依賴感知器網路辨識目標"},
        {"變異體",      "HP x2、速度 x1.3、無裝甲、帶再生，爆發傷害才能壓制"},
        {"圍城艦",      "單隻 HP x8、裝甲 0.8 的超重坦，需集中全火力穿甲"},
        {"路線輪換",    "每 3 波自動換路線（共 5 條），訓練期間顯示下波路線預告"},
        {"雙路進攻",    "Wave 8 起開通第二條路徑，敵人交替從兩路進攻"},
        {"主動技能",    "選取元件後按 [Q]：SENSOR=EMP暫停，OR=超頻，CANNON=超砲等"},
        {"NAND閘",      "非AND閘：後期反制裝甲利器，配合主動技能可破甲歸零"},
    };
    for (auto& r : aiRows) {
        DTX(r.title,(float)(PANEL_L+100),(float)y,FS_SMALL,AlphaOf(COL_AI,230)); y+=22;
        DTX(r.desc, (float)(PANEL_L+120),(float)y,FS_TINY,  AlphaOf(WHITE, 160)); y+=26;
    }

    DTC("點擊任意處或按 [H]/[ESC] 關閉",cx,TOPBAR_H+MAP_H-60,FS_MED,AlphaOf(WHITE,140));
}

// ══════════════════════════════════════════════════════════════════
//  DrawGameOver
// ══════════════════════════════════════════════════════════════════
void DrawGameOver(Game& G) {
    DrawRectangle(PANEL_L,TOPBAR_H,MAP_W,MAP_H,Color{5,0,0,200});
    int   cx=PANEL_L+MAP_W/2, cy=VIRT_H/2;
    float t=(float)GetTime();

    float rp=0.75f+0.25f*sinf(t*4.f);
    DTC("防線崩潰",cx,cy-110,FS_BIG,AlphaOf(RED,(int)(240*rp)));

    char sbuf[64]; snprintf(sbuf,64,"最終分數：%d",G.score);
    DTC(sbuf,cx,cy-38,FS_TITLE,COL_AND);

    if (G.score>=G.highScore && G.highScore>0) DTC("新紀錄！",cx,cy+10,FS_LARGE,COL_STAR);
    else { char hs[48]; snprintf(hs,48,"最高分：%d",G.highScore); DTC(hs,cx,cy+10,FS_LARGE,AlphaOf(COL_STAR,180)); }

    char wb[48]; snprintf(wb,48,"撐過 %d 波次",G.wave);
    DTC(wb,cx,cy+50,FS_LARGE,AlphaOf(WHITE,180));

    int   pctCount=0;
    float avgLoss=0.f;
    for (auto& tw : G.towers) {
        if (tw.type==TType::PERCEPTRON) { pctCount++; avgLoss+=tw.learner.lastLoss; }
    }
    if (pctCount > 0) {
        avgLoss /= pctCount;
        char aib[64]; snprintf(aib,64,"感知器 %d 個 | 平均 loss %.4f",pctCount,avgLoss);
        DTC(aib,cx,cy+86,FS_MED,AlphaOf(COL_AI,200));
        const char* grade=
            (avgLoss<0.05f)?"神經網路已收斂":
            (avgLoss<0.15f)?"學習中，再多幾波":
                            "需要更多感知器或連線";
        DTC(grade,cx,cy+114,FS_SMALL,AlphaOf(COL_AI,170));
    }

    float pp=0.8f+0.2f*sinf(t*4.f);
    DTC("[R] 重新開始    [ENTER] 主選單",cx,cy+152,FS_LARGE,AlphaOf(Color{210,210,210,255},(int)(210*pp)));
}
