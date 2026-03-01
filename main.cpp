// ================================================================
//  Logic Gate Defense v3.0 + AI Edition
// ================================================================
#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <functional>
#include <array>

constexpr int VIRT_W   = 1920;
constexpr int VIRT_H   = 1080;
constexpr int PANEL_L  = 330;
constexpr int PANEL_R  = 400;
constexpr int TOPBAR_H = 70;
constexpr int BOTBAR_H = 52;
constexpr int MAP_W    = VIRT_W - PANEL_L - PANEL_R;
constexpr int MAP_H    = VIRT_H - TOPBAR_H - BOTBAR_H;
constexpr int CELL     = 52;
constexpr int COLS     = MAP_W / CELL;
constexpr int ROWS     = MAP_H / CELL;
constexpr int CPU_GX   = 11;
constexpr int CPU_GY   = 8;

static float gScale=1.f,gOffX=0.f,gOffY=0.f;
static float gShkX=0.f,gShkY=0.f;
inline Vector2 VirtualMouse(){
    Vector2 m=GetMousePosition();
    return{(m.x-gOffX)/gScale,(m.y-gOffY)/gScale};
}

static Font gFont; static bool gFontLoaded=false;
static void LoadChineseFont(){
    static std::vector<int> cp; cp.clear();
    for(int i=32;i<127;i++) cp.push_back(i);
    for(int i=0x4E00;i<=0x9FFF;i++) cp.push_back(i);
    for(int i=0x3000;i<=0x303F;i++) cp.push_back(i);
    for(int i=0xFF01;i<=0xFF60;i++) cp.push_back(i);
    for(int i=0x2000;i<=0x206F;i++) cp.push_back(i);
    for(int i=0x2190;i<=0x21FF;i++) cp.push_back(i);
    for(int i=0x25A0;i<=0x25FF;i++) cp.push_back(i);
    const char* paths[]={
        "font.ttf","font.ttc","font.otf",
        "C:/Windows/Fonts/msjh.ttc","C:/Windows/Fonts/kaiu.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",nullptr
    };
    for(int i=0;paths[i];i++){
        FILE*f=fopen(paths[i],"rb");if(!f)continue;fclose(f);
        gFont=LoadFontEx(paths[i],60,cp.data(),(int)cp.size());
        if(gFont.texture.id!=0){gFontLoaded=true;SetTextureFilter(gFont.texture,TEXTURE_FILTER_BILINEAR);break;}
    }
    if(!gFontLoaded)gFont=GetFontDefault();
}
constexpr float FS_TINY=20.f,FS_SMALL=24.f,FS_MED=30.f,FS_LARGE=36.f,FS_TITLE=44.f,FS_BIG=60.f;
inline float MCN(const char*t,float sz){return MeasureTextEx(gFont,t,sz,0).x;}
inline void DTX(const char*t,float x,float y,float sz,Color c){DrawTextEx(gFont,t,{x,y},sz,0,c);}
inline void DTC(const char*t,int x,int y,float sz,Color c){
    DTX(t,(float)x-MCN(t,sz)*.5f,(float)y-sz*.5f,sz,c);}

const Color BG         ={3,7,14,255};
const Color COL_PATH   ={10,18,30,255};
const Color COL_SENSOR ={34,211,238,255};
const Color COL_PERC   ={74,222,128,255};
const Color COL_AND    ={250,204,21,255};
const Color COL_OR     ={251,146,60,255};
const Color COL_XOR    ={232,121,249,255};
const Color COL_CANNON ={248,113,113,255};
const Color COL_CPU    ={100,220,255,255};
const Color COL_VIRUS  ={255,51,102,255};
const Color COL_BOSS   ={180,50,255,255};
const Color COL_ARMORED={140,190,255,255};
const Color COL_FAST   ={255,200,50,255};
const Color COL_ELITE  ={255,100,200,255};
const Color COL_STAR   ={255,220,70,255};
const Color COL_AI     ={0,255,200,255};
const Color COL_THREAT ={255,80,30,255};
const Color COL_NAND   ={180,255,80,255};
const Color PANEL_BG   ={4,9,18,255};
const Color PANEL_BD   ={0,170,85,45};
inline Color AlphaOf(Color c,int a){c.a=(unsigned char)a;return c;}
inline float Sigmoid(float x){return 1.f/(1.f+expf(-x));}
inline float SigmoidDeriv(float y){return y*(1.f-y);}
inline float Dist(Vector2 a,Vector2 b){return Vector2Distance(a,b);}

enum class TType{SENSOR,PERCEPTRON,AND,OR,XOR,CANNON,CPU,NAND,NONE};
enum class EType{BASIC,FAST,ARMORED,ELITE,BOSS};

enum class BossState{
    CHARGE,
    EVADE,
    RAMPAGE
};

struct PerceptronLearner {
    float accInput1{0.f};
    float accInput2{0.f};
    float accOutput{0.f};
    int   samples{0};
    float lastLoss{0.f};
    float lrDecay{1.f};

    void Accumulate(float in1, float in2, float out){
        accInput1 += in1; accInput2 += in2;
        accOutput += out; samples++;
    }
    void Reset(){
        accInput1=accInput2=accOutput=0.f; samples=0;
    }
    void Update(float target, float& w1, float& w2, float& bias,
                float baseLR = 0.15f)
    {
        if(samples == 0) return;
        float i1 = accInput1 / samples;
        float i2 = accInput2 / samples;
        float out = accOutput / samples;
        float lr = baseLR * lrDecay;
        float error = target - out;
        float delta = error * SigmoidDeriv(out);
        w1   += lr * delta * i1;
        w2   += lr * delta * i2;
        bias += lr * delta;
        w1   = std::max(-2.f, std::min(2.f, w1));
        w2   = std::max(-2.f, std::min(2.f, w2));
        bias = std::max(-2.f, std::min(2.f, bias));
        lastLoss = error * error;
        lrDecay  *= 0.92f;
        Reset();
    }
};

struct ThreatMap {
    float cell[24][20]{};
    void AddKill(int gx, int gy, float value = 1.f){
        if(gx>=0&&gx<24&&gy>=0&&gy<20) cell[gx][gy] += value;
    }
    float Get(int gx, int gy) const {
        if(gx>=0&&gx<24&&gy>=0&&gy<20) return cell[gx][gy];
        return 0.f;
    }
    float GetMax() const {
        float mx = 0.f;
        for(int x=0;x<24;x++) for(int y=0;y<20;y++) mx=std::max(mx,cell[x][y]);
        return mx;
    }
    void Decay(float factor = 0.85f){
        for(int x=0;x<24;x++) for(int y=0;y<20;y++) cell[x][y] *= factor;
    }
};

struct AIHint {
    int gx, gy;
    TType suggest;
    float score;
    std::string reason;
    float flashT{0.f};
};

struct TierInfo{int cost;const char*name;const char*ability;};
struct TowerDef{
    const char*label,*sym;Color col;int baseCost;const char*desc;
    TierInfo tiers[3];
};
static TowerDef TDEFS[]={
    {"感測器","SEN",Color{34,211,238,255},40,
     "偵測病毒距離→輸出0~1",
     {{0,"基礎偵測","範圍4.5格"},{80,"強化偵測","範圍6格+偵測快速"},{160,"全域偵測","範圍8格+隱形"}}},
    {"感知器","PCT",Color{74,222,128,255},90,
     "加權求和+Sigmoid（AI可學習w1,w2,bias）",
     {{0,"單層神經元","0~1連續輸出"},{100,"雙層神經元","訊號強度x1.4"},{200,"自適應網路","訊號強度x2.0"}}},
    {"AND閘","AND",Color{250,204,21,255},60,
     "全輸入>0.5才輸出1(對裝甲+50%)",
     {{0,"基礎AND","裝甲傷害+50%"},{80,"增強AND","傷害乘數x1.5"},{160,"連鎖AND","觸發鄰近AND"}}},
    {"OR閘","OR",Color{251,146,60,255},60,
     "任一>0.5輸出1(對雜兵最佳)",
     {{0,"基礎OR","廣域觸發"},{80,"快速OR","砲塔冷卻-30%"},{160,"脈衝OR","爆發連射x2"}}},
    {"XOR閘","XOR",Color{232,121,249,255},60,
     "恰好一個>0.5(對精英+100%)",
     {{0,"基礎XOR","精英傷害+100%"},{80,"精準XOR","20%暴擊率"},{160,"標記XOR","敵人受傷+50%"}}},
    {"砲塔","GUN",Color{248,113,113,255},80,
     "訊號>0.5→自動開火",
     {{0,"基礎砲塔","36傷 射程7格"},{120,"強化砲塔","60傷 射程9格"},{240,"爆破砲塔","100傷+爆炸"}}},
    {"CPU","CPU",Color{100,220,255,255},0,
     "你的核心，保護它！",
     {{0,"",""},{0,"",""},{0,"",""}}},
    {"NAND","NND",Color{180,255,80,255},70,
     "非AND：只要一個輸入≤0.5就輸出1（後期反制裝甲）",
     {{0,"基礎NAND","護甲減免-20%"},{90,"強化NAND","護甲減免-40%"},{180,"破甲NAND","護甲歸零+標記"}}}
};
inline TowerDef&TDef(TType t){return TDEFS[(int)t];}

// ── 路徑系統 ──────────────────────────────────────────────────
struct PathCell{int gx,gy;};

struct PathPreset{
    int count;
    int wx[16];
    int wy[16];
    const char* name;
};
static const PathPreset PATH_PRESETS[]={
    {13,{-1,3,3,9,9,5,5,13,13,17,17,11,11},
         {8, 8,2,2,5,5,12,12,5, 5,14,14, 8},"S 型迂迴"},
    {11,{-1,2,2,6,6,12,12,18,18,11,11},
         {8, 8,1,1,9, 9, 2, 2, 8, 8, 8},"上方長驅"},
    {11,{-1,4,4,8,8,14,14,19,19,11,11},
         {8, 8,14,14,6,6,15,15,8, 8, 8},"下方包抄"},
    {5, {-1,5,5,11,11},
         {8, 8,4, 4, 8},"中央突破"},
    {15,{-1,2,2,8,8,4,4,10,10,16,16,13,13,11,11},
         {8, 8,3,3,10,10,5, 5,14,14, 4, 4, 8, 8, 8},"Z 型纏繞"},
};
static int  CURRENT_PATH_IDX = 0;
static int  CURRENT_PATH_IDX2 = 1;
static const PathPreset* CUR_PRESET = &PATH_PRESETS[0];
static const PathPreset* CUR_PRESET2 = &PATH_PRESETS[1];

static std::vector<PathCell> PATH_CELLS;
static bool IS_PATH[24][20]={};
static std::vector<PathCell> PATH_CELLS2;
static bool IS_PATH2[24][20]={};

static void BuildPath(int presetIdx = 0){
    CURRENT_PATH_IDX = presetIdx % 5;
    CUR_PRESET = &PATH_PRESETS[CURRENT_PATH_IDX];
    PATH_CELLS.clear();
    memset(IS_PATH,0,sizeof(IS_PATH));
    for(int wi=0;wi+1<CUR_PRESET->count;wi++){
        int x0=CUR_PRESET->wx[wi], y0=CUR_PRESET->wy[wi];
        int x1=CUR_PRESET->wx[wi+1],y1=CUR_PRESET->wy[wi+1];
        int dx=(x1>x0)?1:(x1<x0)?-1:0;
        int dy=(y1>y0)?1:(y1<y0)?-1:0;
        int cx=x0,cy=y0;
        while(cx!=x1||cy!=y1){
            if(cx>=0&&cx<COLS&&cy>=0&&cy<ROWS){
                if(PATH_CELLS.empty()||
                   PATH_CELLS.back().gx!=cx||PATH_CELLS.back().gy!=cy){
                    IS_PATH[cx][cy]=true;
                    PATH_CELLS.push_back({cx,cy});
                }
            }
            cx+=dx;cy+=dy;
        }
    }
    IS_PATH[CPU_GX][CPU_GY]=true;
}

static void BuildDualPath(int presetIdx){
    CURRENT_PATH_IDX2 = presetIdx % 5;
    CUR_PRESET2 = &PATH_PRESETS[CURRENT_PATH_IDX2];
    PATH_CELLS2.clear();
    memset(IS_PATH2,0,sizeof(IS_PATH2));
    for(int wi=0;wi+1<CUR_PRESET2->count;wi++){
        int x0=CUR_PRESET2->wx[wi], y0=CUR_PRESET2->wy[wi];
        int x1=CUR_PRESET2->wx[wi+1],y1=CUR_PRESET2->wy[wi+1];
        int dx=(x1>x0)?1:(x1<x0)?-1:0;
        int dy=(y1>y0)?1:(y1<y0)?-1:0;
        int cx=x0,cy=y0;
        while(cx!=x1||cy!=y1){
            if(cx>=0&&cx<COLS&&cy>=0&&cy<ROWS){
                if(PATH_CELLS2.empty()||
                   PATH_CELLS2.back().gx!=cx||PATH_CELLS2.back().gy!=cy){
                    IS_PATH2[cx][cy]=true;
                    PATH_CELLS2.push_back({cx,cy});
                }
            }
            cx+=dx;cy+=dy;
        }
    }
    IS_PATH2[CPU_GX][CPU_GY]=true;
}

// ── Tower ─────────────────────────────────────────────────────
struct Tower{
    int id; TType type; int gx,gy; int level{1};
    float sig{0},w1{.8f},w2{.6f},bias{-.5f};
    int cooldown{0}; float anim{0};
    std::vector<int>conns;
    float range{7.f},damage{36.f};
    int fireRateTicks{18};
    int kills{0}; float totalDmg{0};
    float upgradeFlash{0};
    bool pulseBurst{false};
    int burstCount{0};
    float activeCd{0.f};        // active skill cooldown (counts down to 0 = ready)
    PerceptronLearner learner;
    float sampledIn1{0.f}, sampledIn2{0.f}, sampledOut{0.f};
    int   sampleCount{0};
    std::vector<float> lossHistory; // last 12 loss values for sparkline
    void SampleSignal(float in1, float in2, float out){
        sampledIn1 += in1; sampledIn2 += in2;
        sampledOut += out; sampleCount++;
    }
    void FlushSample(){
        if(sampleCount > 0 && type == TType::PERCEPTRON){
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

// ── Enemy ─────────────────────────────────────────────────────
struct Enemy{
    int id; EType type;
    float pathPos{0};
    float hp,maxHp,armor{0},spd;
    int reward; float angle{0};
    bool marked{false}; float markTimer{0};
    bool stealth{false};
    float flashTimer{0};
    float regenTimer{0};
    float stunTimer{0.f};   // EMP stun duration
    int   pathIdx{0};       // 0=primary path, 1=secondary path
    BossState bossState{BossState::CHARGE};
    float bossStateTimer{0.f};
    float evadeSpdMult{1.f};
    float localThreat{0.f};

    void TickBoss(float dt, float threat, float hpRatio){
        bossStateTimer -= dt;
        if(hpRatio < 0.30f){
            if(bossState != BossState::RAMPAGE){
                bossState = BossState::RAMPAGE;
                evadeSpdMult = 2.4f;
                bossStateTimer = 9999.f;
            }
            return;
        }
        if(bossState == BossState::CHARGE){
            if(threat > 3.f && bossStateTimer <= 0.f){
                bossState = BossState::EVADE;
                evadeSpdMult = 2.0f;
                bossStateTimer = 1.8f;
            }else{
                evadeSpdMult = 1.0f;
            }
        }
        else if(bossState == BossState::EVADE){
            if(bossStateTimer <= 0.f){
                bossState = BossState::CHARGE;
                evadeSpdMult = 1.0f;
                bossStateTimer = 2.0f;
            }
        }
    }
};

struct Bullet{
    Vector2 pos,vel;
    int targetId;
    float dmg;
    bool splash{false};
    float splashR{0};
    Color col;
};

struct SigPulse{Vector2 src,dst;float t;Color col;};
struct Particle{Vector2 pos,vel;float life,maxLife,radius;Color col;};
struct FloatText{Vector2 pos;std::string text;Color col;float life;};
struct Star{float x,y,r,bright;};

// ═══════════════════════════════════════════════════════════════
//  Game 主狀態
// ═══════════════════════════════════════════════════════════════
struct Game{
    std::vector<Tower>    towers;
    std::vector<Enemy>    enemies;
    std::vector<Bullet>   bullets;
    std::vector<SigPulse> pulses;
    std::vector<Particle> particles;
    std::vector<FloatText> floats;
    std::vector<Star>     stars;

    ThreatMap              threatMap;
    std::vector<AIHint>    aiHints;

    enum class WaveEvent{
        NONE,
        SWARM,
        ARMORED,
        STEALTH,
        ELITE_RUSH,
        BOSS_ESCORT,
        REGEN_ARMY,
        DOUBLE_SPD,
    };
    WaveEvent currentEvent{WaveEvent::NONE};
    std::string eventName;
    float eventBannerTimer{0.f};

    static WaveEvent RollEvent(int wave, std::mt19937& rng){
        if(wave < 3) return WaveEvent::NONE;
        if(wave % 5 == 0) return WaveEvent::BOSS_ESCORT;
        std::uniform_int_distribution<int> d(0,7);
        int r = d(rng);
        if(wave < 6) r %= 3;
        switch(r){
            case 0: return WaveEvent::NONE;
            case 1: return WaveEvent::SWARM;
            case 2: return WaveEvent::ARMORED;
            case 3: return WaveEvent::STEALTH;
            case 4: return WaveEvent::ELITE_RUSH;
            case 5: return WaveEvent::REGEN_ARMY;
            case 6: return WaveEvent::DOUBLE_SPD;
            default:return WaveEvent::NONE;
        }
    }
    static const char* EventName(WaveEvent e){
        switch(e){
            case WaveEvent::SWARM:       return "蜂群入侵！";
            case WaveEvent::ARMORED:     return "裝甲洪流！";
            case WaveEvent::STEALTH:     return "霧戰！";
            case WaveEvent::ELITE_RUSH:  return "精英突擊！";
            case WaveEvent::BOSS_ESCORT: return "護衛 Boss！";
            case WaveEvent::REGEN_ARMY:  return "再生軍！";
            case WaveEvent::DOUBLE_SPD:  return "神速衝鋒！";
            default:                     return "";
        }
    }

    bool showThreat{false};
    bool showAIHints{true};
    bool dualPath{false};       // wave 8+ dual path active
    int  nextPreviewPath{1};    // next path index for preview
    float buffOverfreq{0.f};    // OR skill: +fire rate timer
    float buffArmorBreak{0.f};  // AND/NAND skill: armor pierce timer
    float buffGlobalMark{0.f};  // XOR skill: global mark timer

    int nextId{1},credits{450},score{0},highScore{0};
    int wave{0},lives{20};
    float cpuHp{100.f};
    bool paused{false},showHelp{false};

    enum Phase{MENU,BUILD,FIGHT,TRAINING,GAMEOVER}phase{MENU};
    int waveCount{0},spawned{0};
    float spawnTimer{0};
    float trainingTimer{0};
    constexpr static float TRAIN_TIME=15.f;

    TType placing{TType::NONE};
    int selectedId{-1},connectSrc{-1};
    std::string msg; float msgTimer{0};

    std::mt19937 rng{std::random_device{}()};
    int btnY0{0},waveBtnY{0};
    constexpr static int BTN_H=66,BTN_GAP=5;

    int waveKills{0},waveEscaped{0};
    float waveDmg{0};

    int combo{0};float comboTimer{0};
    constexpr static float COMBO_WINDOW=3.f;

    float shakeT{0},shakePow{0};

    Vector2 MapOrigin(){return{(float)PANEL_L+gShkX,(float)TOPBAR_H+gShkY};}
    Vector2 CC(int gx,int gy){auto o=MapOrigin();return{o.x+(gx+.5f)*CELL,o.y+(gy+.5f)*CELL};}
    Vector2 CC(float gx,float gy){auto o=MapOrigin();return{o.x+(gx+.5f)*CELL,o.y+(gy+.5f)*CELL};}
    Tower*  FindTower(int id){for(auto&t:towers)if(t.id==id)return&t;return nullptr;}
    Tower*  TowerAt(int gx,int gy){for(auto&t:towers)if(t.gx==gx&&t.gy==gy)return&t;return nullptr;}
    Enemy*  FindEnemy(int id){for(auto&e:enemies)if(e.id==id)return&e;return nullptr;}
    bool    IsPath(int gx,int gy){
        if(gx<0||gx>=COLS||gy<0||gy>=ROWS) return false;
        return IS_PATH[gx][gy] || IS_PATH2[gx][gy];
    }

    Vector2 EnemyWorld(const Enemy&e){
        auto& cells = (e.pathIdx==1 && dualPath) ? PATH_CELLS2 : PATH_CELLS;
        int i=(int)e.pathPos; float f=e.pathPos-i;
        if(i>=(int)cells.size()-1)return CC(CPU_GX,CPU_GY);
        if(i<0)return CC(cells[0].gx,cells[0].gy);
        Vector2 a=CC(cells[i].gx,cells[i].gy);
        Vector2 b=CC(cells[i+1].gx,cells[i+1].gy);
        return{a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f};
    }
    Vector2 EnemyGrid(const Enemy&e){
        auto& cells = (e.pathIdx==1 && dualPath) ? PATH_CELLS2 : PATH_CELLS;
        int i=(int)e.pathPos; float f=e.pathPos-i;
        if(i>=(int)cells.size()-1)return{(float)CPU_GX,(float)CPU_GY};
        if(i<0)return{(float)cells[0].gx,(float)cells[0].gy};
        float ax=cells[i].gx,ay=cells[i].gy;
        float bx=cells[i+1].gx,by=cells[i+1].gy;
        return{ax+(bx-ax)*f,ay+(by-ay)*f};
    }

    void ApplyTowerStats(Tower&t){
        if(t.type==TType::SENSOR){
            float rngs[]={4.5f,6.0f,8.0f};
            t.range=rngs[t.level-1];
        }else if(t.type==TType::CANNON){
            float dmgs[]={36.f,60.f,100.f};
            float rngs[]={7.f,9.f,11.f};
            int   frs[]={18,12,8};
            t.damage=dmgs[t.level-1];
            t.range=rngs[t.level-1];
            t.fireRateTicks=frs[t.level-1];
        }
    }

    bool UpgradeTower(Tower&t){
        if(t.type==TType::CPU||t.level>=3)return false;
        int cost=TDef(t.type).tiers[t.level].cost;
        if(credits<cost)return false;
        credits-=cost;t.level++;ApplyTowerStats(t);
        t.upgradeFlash=1.f;
        char buf[64];snprintf(buf,64,"%s 升至 Lv.%d！",TDef(t.type).label,t.level);
        SetMsg(buf);SpawnParticles(CC(t.gx,t.gy),TDef(t.type).col,25,130.f);
        return true;
    }

    static constexpr int MAX_CONNS=8;
    bool WouldCycle(int src,int dst){
        std::vector<int>stk={dst},vis;
        while(!stk.empty()){
            int c=stk.back();stk.pop_back();
            if(c==src)return true;
            if(std::find(vis.begin(),vis.end(),c)!=vis.end())continue;
            vis.push_back(c);
            Tower*t=FindTower(c);if(t)for(int x:t->conns)stk.push_back(x);
        }
        return false;
    }
    std::string ValidateConnect(int src,int dst){
        Tower*s=FindTower(src),*d=FindTower(dst);
        if(!s||!d)return "元件不存在";
        if(src==dst)return "不能連接自己";
        if(s->type==TType::CANNON)return "砲塔不能輸出";
        if(s->type==TType::CPU)return "CPU不能連接";
        if(d->type==TType::SENSOR)return "感測器不能被連入";        if(d->type==TType::CPU)return "CPU不能連入";
        if((int)s->conns.size()>=MAX_CONNS)return "已達上限(8條)";
        if(WouldCycle(src,dst))return "不能形成迴圈";
        return "";
    }
    void SpawnParticles(Vector2 pos,Color col,int n=10,float spd=70.f){
        std::uniform_real_distribution<float>ang(0,6.28f),sp(spd*.3f,spd);
        for(int i=0;i<n;i++){float a=ang(rng),s=sp(rng);
            particles.push_back({pos,{cosf(a)*s,sinf(a)*s},.7f,.7f,5.f,col});}
    }
    void AddFloat(Vector2 pos,const std::string&txt,Color col){
        floats.push_back({pos,txt,col,2.f});
    }
    void SetMsg(const std::string&m){msg=m;msgTimer=5.f;}
    void Shake(float p=8.f,float t=0.35f){shakeT=t;shakePow=p;}

    void InitStars(){
        stars.clear();
        std::uniform_real_distribution<float>rw(0,(float)VIRT_W),
            rh(0,(float)VIRT_H),rr(.8f,2.f),rb(.3f,1.f);
        for(int i=0;i<200;i++)stars.push_back({rw(rng),rh(rng),rr(rng),rb(rng)});
    }

    void GenerateAIHints(){
        aiHints.clear();
        for(auto& pc : PATH_CELLS){
            float minDist = 999.f;
            TType bestSuggest = TType::SENSOR;
            for(auto& t : towers){
                float d = Dist({(float)t.gx,(float)t.gy},{(float)pc.gx,(float)pc.gy});
                if(d < minDist) minDist = d;
            }
            if(minDist < 4.5f) continue;
            int pathIdx = 0;
            for(int pi=0;pi<(int)PATH_CELLS.size();pi++)
                if(PATH_CELLS[pi].gx==pc.gx && PATH_CELLS[pi].gy==pc.gy){pathIdx=pi;break;}
            float progress = (float)pathIdx / PATH_CELLS.size();
            std::string reason;
            float score = (minDist - 4.5f) / 8.f;
            if(progress < 0.3f){
                bestSuggest = TType::SENSOR;
                reason = "前段覆蓋不足，建議感測器";
            } else if(progress < 0.6f){
                bestSuggest = TType::OR;
                reason = "中段需要訊號中繼，建議OR閘";
            } else {
                bestSuggest = TType::CANNON;
                reason = "CPU防線薄弱，建議砲塔";
            }
            for(int dx=-1;dx<=1;dx++) for(int dy=-1;dy<=1;dy++){
                int hx=pc.gx+dx, hy=pc.gy+dy;
                if(hx<0||hx>=COLS||hy<0||hy>=ROWS) continue;
                if(IS_PATH[hx][hy]) continue;
                if(TowerAt(hx,hy)) continue;
                AIHint hint;
                hint.gx=hx; hint.gy=hy;
                hint.suggest=bestSuggest;
                hint.score=std::min(1.f,score);
                hint.reason=reason;
                hint.flashT=0.f;
                aiHints.push_back(hint);
                goto next_cell;
            }
            next_cell:;
            if((int)aiHints.size()>=3) break;
        }
    }

    void TrainPerceptrons(){
        if(waveKills + waveEscaped == 0) return;
        float killRate = (float)waveKills / (waveKills + waveEscaped);
        for(auto& t : towers){
            if(t.type != TType::PERCEPTRON) continue;
            t.FlushSample();
            t.learner.Update(killRate, t.w1, t.w2, t.bias);
            // record loss history for sparkline
            t.lossHistory.push_back(t.learner.lastLoss);
            if((int)t.lossHistory.size()>14) t.lossHistory.erase(t.lossHistory.begin());
            char buf[80];
            snprintf(buf,80,"PCT[%d] loss=%.3f",t.id,t.learner.lastLoss);
            AddFloat(CC(t.gx,t.gy), buf, COL_AI);
        }
    }

    void Reset(){
        towers.clear();enemies.clear();bullets.clear();
        pulses.clear();particles.clear();floats.clear();
        nextId=1;credits=450;score=0;wave=0;
        cpuHp=100.f;lives=20;
        paused=false;showHelp=false;phase=BUILD;
        placing=TType::NONE;selectedId=-1;connectSrc=-1;
        spawned=0;spawnTimer=0;waveCount=0;trainingTimer=0;
        combo=0;comboTimer=0;shakeT=0;
        waveKills=0;waveEscaped=0;waveDmg=0;
        currentEvent=WaveEvent::NONE;
        eventName=""; eventBannerTimer=0.f;
        aiHints.clear();
        dualPath=false; nextPreviewPath=1;
        buffOverfreq=0.f; buffArmorBreak=0.f; buffGlobalMark=0.f;
        BuildPath(0);
        BuildDualPath(1);
        Tower cpu;cpu.id=0;cpu.type=TType::CPU;
        cpu.gx=CPU_GX;cpu.gy=CPU_GY;
        cpu.level=1;cpu.sig=1.f;
        towers.push_back(cpu);nextId=1;
        SetMsg("目標：感測器→邏輯閘→砲塔！按[空白鍵]發動第一波。");
    }
};

bool ScreenToGrid(Game&G,Vector2 sp,int&gx,int&gy){
    auto o=G.MapOrigin();
    int x=(int)((sp.x-o.x)/CELL),y=(int)((sp.y-o.y)/CELL);
    if(x<0||x>=COLS||y<0||y>=ROWS)return false;
    gx=x;gy=y;return true;
}

// ── 波次生成 ──────────────────────────────────────────────────
using WaveEvent = Game::WaveEvent;

void SpawnEnemy(Game&G){
    EType et;
    std::uniform_int_distribution<int>typeR(0,4);
    switch(G.currentEvent){
        case WaveEvent::ARMORED:
            et = EType::ARMORED; break;
        case WaveEvent::STEALTH:
            et = (typeR(G.rng)%3==0)?EType::FAST:EType::BASIC; break;
        case WaveEvent::ELITE_RUSH:
            et = EType::ELITE; break;
        case WaveEvent::BOSS_ESCORT:
            et = (G.spawned==0)?EType::BOSS:EType::BASIC; break;
        case WaveEvent::SWARM:
            et = (typeR(G.rng)%5==0)?EType::FAST:EType::BASIC; break;
        default:{
            int r=typeR(G.rng);
            if(G.wave<3) r%=2;
            switch(r){
                case 0:case 1:et=EType::BASIC;break;
                case 2:et=EType::FAST;break;
                case 3:et=EType::ARMORED;break;
                default:et=EType::ELITE;break;
            }
        }
    }
    Enemy e; e.id=G.nextId++;e.type=et;
    e.pathPos=0.f;e.angle=0;
    e.bossState=BossState::CHARGE;e.evadeSpdMult=1.f;
    switch(et){
        case EType::BASIC:
            e.hp=e.maxHp=55.f+G.wave*20.f;
            e.spd=1.4f+G.wave*.06f;e.reward=10+G.wave;break;
        case EType::FAST:
            e.hp=e.maxHp=30.f+G.wave*12.f;
            e.spd=2.8f+G.wave*.1f;e.reward=8+G.wave;
            e.stealth=(G.wave>=5);break;
        case EType::ARMORED:
            e.hp=e.maxHp=120.f+G.wave*35.f;
            e.armor=0.55f;e.spd=0.85f+G.wave*.04f;
            e.reward=22+G.wave*2;break;
        case EType::ELITE:
            e.hp=e.maxHp=90.f+G.wave*28.f;
            e.spd=1.1f+G.wave*.05f;e.reward=28+G.wave*2;break;
        case EType::BOSS:
            e.hp=e.maxHp=800.f+G.wave*200.f;
            e.armor=0.3f;e.spd=0.55f;
            e.reward=150+G.wave*20;break;
    }
    switch(G.currentEvent){
        case WaveEvent::SWARM:
            e.hp *= 0.6f; e.maxHp = e.hp; break;
        case WaveEvent::STEALTH:
            e.stealth = true; break;
        case WaveEvent::ELITE_RUSH:
            e.spd *= 1.5f; break;
        case WaveEvent::REGEN_ARMY:
            e.regenTimer = -1.f; break;
        case WaveEvent::DOUBLE_SPD:
            e.spd *= 2.f; break;
        default: break;
    }
    G.enemies.push_back(e);
    // assign path index for dual path
    if(G.dualPath && !PATH_CELLS2.empty()){
        G.enemies.back().pathIdx = (G.spawned % 2 == 1) ? 1 : 0;
    }
}

void StartWave(Game&G){
    if(G.phase==Game::FIGHT||G.phase==Game::TRAINING)return;
    G.wave++;
    if(G.wave > 1 && (G.wave-1) % 3 == 0){
        int nextPath = (CURRENT_PATH_IDX + 1) % 5;
        BuildPath(nextPath);
        if(G.dualPath){
            int nextPath2 = (CURRENT_PATH_IDX2 + 2) % 5;
            if(nextPath2 == nextPath) nextPath2 = (nextPath2+1)%5;
            BuildDualPath(nextPath2);
        }
        // Only remove towers that now sit on the new path; refund full cost
        int demolished = 0;
        G.towers.erase(
            std::remove_if(G.towers.begin(), G.towers.end(),
                [&](const Tower& t){
                    if(t.type == TType::CPU) return false;
                    bool onPath = IS_PATH[t.gx][t.gy] || (G.dualPath && IS_PATH2[t.gx][t.gy]);
                    if(onPath){
                        G.credits += TDef(t.type).baseCost; // full refund
                        demolished++;
                    }
                    return onPath;
                }),
            G.towers.end());
        // Also clean up dangling connections
        for(auto& tw : G.towers)
            tw.conns.erase(std::remove_if(tw.conns.begin(), tw.conns.end(),
                [&](int id){ return G.FindTower(id) == nullptr; }), tw.conns.end());
        G.credits += 200;
        char pb[120];
        if(demolished > 0){
            if(G.dualPath)
                snprintf(pb,120,"雙路徑重組！+200CR。%d座塔被路徑占用（已全額退款）。主：%s | 副：%s",demolished,CUR_PRESET->name,CUR_PRESET2->name);
            else
                snprintf(pb,120,"路徑重組！+200CR。%d座塔被占用（已全額退款）。新路線：%s",demolished,CUR_PRESET->name);
        } else {
            if(G.dualPath)
                snprintf(pb,120,"雙路徑重組！+200CR。主：%s | 副：%s",CUR_PRESET->name,CUR_PRESET2->name);
            else
                snprintf(pb,120,"路徑重組！+200CR。新路線：%s",CUR_PRESET->name);
        }
        G.SetMsg(pb);
    }
    // Enable dual path at wave 8
    if(G.wave >= 8 && !G.dualPath){
        G.dualPath = true;
        int p2 = (CURRENT_PATH_IDX + 2) % 5;
        if(p2 == CURRENT_PATH_IDX) p2 = (p2+1)%5;
        BuildDualPath(p2);
        G.SetMsg("警告：第二條路徑開通！雙路進攻！");
    }
    // Compute next preview path
    G.nextPreviewPath = (CURRENT_PATH_IDX + 1) % 5;
    G.currentEvent = Game::RollEvent(G.wave, G.rng);
    G.eventName    = Game::EventName(G.currentEvent);
    G.eventBannerTimer = 4.f;
    bool boss=(G.wave%5==0);
    int baseCount = boss ? 1+(G.wave/5)*2 : 6+G.wave*3;
    if(G.currentEvent==WaveEvent::SWARM)      baseCount = (int)(baseCount*2.2f);
    if(G.currentEvent==WaveEvent::BOSS_ESCORT)baseCount = 1 + G.wave*2;
    if(G.currentEvent==WaveEvent::ELITE_RUSH) baseCount = std::max(4, baseCount/2);
    G.waveCount=baseCount;
    G.spawned=0;G.spawnTimer=0;G.phase=Game::FIGHT;
    G.waveKills=0;G.waveEscaped=0;G.waveDmg=0;
    char buf[96];
    if(G.eventName.empty()||G.currentEvent==WaveEvent::NONE)
        snprintf(buf,96,"第%d波 — %d 個病毒！",G.wave,G.waveCount);
    else
        snprintf(buf,96,"第%d波 %s (%d 個)",G.wave,G.eventName.c_str(),G.waveCount);
    G.SetMsg(buf);
}

float ConnDistance(Tower&src,Tower&dst){
    float dx=(float)(src.gx-dst.gx),dy=(float)(src.gy-dst.gy);
    return sqrtf(dx*dx+dy*dy);
}

void PropagateSignals(Game&G,float dt){
    for(auto&t:G.towers){
        if(t.type!=TType::SENSOR)continue;
        float minD=t.range+1.f;
        for(auto&e:G.enemies){
            if(e.stealth&&t.level<2)continue;
            Vector2 eg=G.EnemyGrid(e);
            float d=Dist({(float)t.gx,(float)t.gy},eg);
            if(d<minD)minD=d;
        }
        float newSig=(minD<=t.range)?std::max(0.f,1.f-minD/t.range):0.f;
        if(t.level==3)newSig=std::min(1.f,newSig*1.3f);
        float prev=t.sig;
        t.sig+=(newSig-t.sig)*10.f*dt;
        if(fabsf(t.sig-prev)>.02f)
            for(int cid:t.conns){auto*d=G.FindTower(cid);if(d)
                G.pulses.push_back({G.CC(t.gx,t.gy),G.CC(d->gx,d->gy),0.f,TDef(t.type).col});}
    }
    for(auto&t:G.towers){
        if(t.type==TType::SENSOR||t.type==TType::CPU||t.type==TType::NONE)continue;
        std::vector<float>ins;
        int inIdx=0;
        for(auto&src:G.towers){
            for(int cid:src.conns){
                if(cid!=t.id)continue;
                float dist=ConnDistance(src,t);
                float atten=powf(0.92f,dist);
                float val=src.sig*atten;
                ins.push_back(val);
                inIdx++;
            }
        }
        if(ins.empty()){t.sig+=(0.f-t.sig)*8.f*dt;continue;}
        float newSig=t.sig;
        switch(t.type){
            case TType::PERCEPTRON:{
                float s1=ins.size()>0?ins[0]:0.f;
                float s2=ins.size()>1?ins[1]:0.f;
                float boost=(t.level==2)?1.4f:(t.level==3)?2.0f:1.0f;
                newSig=Sigmoid((s1*t.w1+s2*t.w2+t.bias)*3.f)*boost;
                newSig=std::min(1.f,newSig);
                static int sampleTick=0; sampleTick++;
                if(sampleTick%30==0) t.SampleSignal(s1, s2, newSig);
                break;
            }
            case TType::AND:{
                bool ok=true;for(float s:ins)if(s<.5f){ok=false;break;}
                float mult=(t.level>=2)?1.5f:1.0f;
                newSig=ok?std::min(1.f,1.f*mult):0.f;break;
            }
            case TType::OR:{
                bool any=false;for(float s:ins)if(s>.5f){any=true;break;}
                newSig=any?1.f:0.f;break;
            }
            case TType::XOR:{
                int cnt=0;for(float s:ins)if(s>.5f)cnt++;
                newSig=(cnt==1)?1.f:0.f;break;
            }
            case TType::NAND:{
                bool allHigh=true;
                for(float s:ins)if(s<.5f){allHigh=false;break;}
                float armorBonus=(G.buffArmorBreak>0)?0.3f:(t.level>=2)?0.2f:0.f;
                newSig = allHigh ? 0.f : std::min(1.f, 1.0f + armorBonus);
                break;
            }
            case TType::CANNON:{
                float mx=0.f;for(float s:ins)mx=std::max(mx,s);
                newSig=mx;break;
            }
            default:break;
        }
        float prev=t.sig;
        t.sig+=(newSig-t.sig)*12.f*dt;
        if(fabsf(t.sig-prev)>.02f)
            for(int cid:t.conns){auto*d=G.FindTower(cid);if(d)
                G.pulses.push_back({G.CC(t.gx,t.gy),G.CC(d->gx,d->gy),0.f,TDef(t.type).col});}
    }
}

float CalcDamageMultiplier(Game&G,Tower&cannon,Enemy&target){
    float mult=1.f;
    for(auto&src:G.towers){
        bool feedsCannon=false;
        for(int cid:src.conns)if(cid==cannon.id){feedsCannon=true;break;}
        if(!feedsCannon)continue;
        if(src.type==TType::AND&&target.type==EType::ARMORED)mult+=.5f;
        if(src.type==TType::XOR&&target.type==EType::ELITE)mult+=1.f;
        if(src.type==TType::NAND&&target.type==EType::ARMORED){
            float bonus = src.level==3?1.2f:src.level==2?0.7f:0.4f;
            mult+=bonus;
        }
    }
    if(target.marked)mult+=.5f;
    if(G.buffGlobalMark>0)mult+=.3f;
    float armorReduction=target.armor;
    // Armor break buff or NAND Lv3 in chain negates armor
    bool armorBroken = (G.buffArmorBreak>0);
    if(!armorBroken){
        bool hasNandL3=false;
        for(auto&src:G.towers)
            for(int cid:src.conns)if(cid==cannon.id&&src.type==TType::NAND&&src.level>=3){hasNandL3=true;break;}
        if(hasNandL3) armorBroken=true;
    }
    if(armorReduction>0 && !armorBroken){
        bool hasAnd=false;
        for(auto&src:G.towers)
            for(int cid:src.conns)if(cid==cannon.id&&src.type==TType::AND){hasAnd=true;break;}
        if(!hasAnd)mult*=(1.f-armorReduction);
    }
    return mult;
}

void UpdateBossAI(Game&G, Enemy&boss, float dt){
    Vector2 eg = G.EnemyGrid(boss);
    int bgx = (int)(eg.x + 0.5f);
    int bgy = (int)(eg.y + 0.5f);
    float threat = G.threatMap.Get(bgx, bgy);
    int nextIdx = (int)boss.pathPos + 2;
    if(nextIdx < (int)PATH_CELLS.size()){
        threat = std::max(threat,
            G.threatMap.Get(PATH_CELLS[nextIdx].gx, PATH_CELLS[nextIdx].gy));
    }
    boss.localThreat = threat;
    float hpRatio = boss.hp / boss.maxHp;
    boss.TickBoss(dt, threat, hpRatio);
    static float rampageFloatTimer = 0.f;
    rampageFloatTimer -= dt;
    if(boss.bossState == BossState::RAMPAGE && rampageFloatTimer <= 0.f){
        G.AddFloat(G.EnemyWorld(boss), "RAMPAGE！", Color{255,50,50,255});
        rampageFloatTimer = 3.f;
    }
}

// ── Active Skill System ──────────────────────────────────────────
static const float SKILL_CD[]={20.f,15.f,22.f,18.f,16.f,12.f,0.f,22.f}; // per TType index
static const char* SKILL_NAME[]={
    "電磁脈衝","急速突觸","裝甲穿透","超頻連射","全域標記","超砲擊","","破甲反轉"};
static const char* SKILL_DESC[]={
    "範圍內敵人暫停2秒","加速w1/w2學習","10秒護甲歸零","10秒射速x2",
    "全域標記所有敵人5秒","立即超砲爆炸","","10秒裝甲全破+標記"};

int CalcPCTLayer(Game&G, int towerId, int depth=0){
    if(depth>6) return 1;
    Tower* t = G.FindTower(towerId);
    if(!t || t->type != TType::PERCEPTRON) return 1;
    int maxLayer = 1;
    for(auto& src : G.towers){
        for(int cid : src.conns){
            if(cid != towerId) continue;
            if(src.type == TType::PERCEPTRON)
                maxLayer = std::max(maxLayer, 1 + CalcPCTLayer(G, src.id, depth+1));
        }
    }
    return maxLayer;
}

void ActivateSkill(Game&G, Tower&t){
    if(t.activeCd > 0.f) return;
    if(t.type == TType::CPU) return;
    int ti = (int)t.type;
    t.activeCd = SKILL_CD[ti];
    Vector2 pos = G.CC(t.gx, t.gy);
    switch(t.type){
        case TType::SENSOR:{
            // EMP: stun all enemies in range for 2s
            int cnt=0;
            for(auto&e:G.enemies){
                float d=Dist({(float)t.gx,(float)t.gy}, G.EnemyGrid(e));
                if(d <= t.range*1.5f){ e.stunTimer=2.f; cnt++; }
            }
            G.SpawnParticles(pos, COL_SENSOR, 20, 120.f);
            char b[48];snprintf(b,48,"EMP！暫停 %d 個",cnt);G.AddFloat(pos,b,COL_SENSOR);
            break;
        }
        case TType::PERCEPTRON:{
            // Force positive gradient step
            t.w1 = std::min(2.f, t.w1 + 0.15f);
            t.w2 = std::min(2.f, t.w2 + 0.15f);
            t.learner.lrDecay = std::min(1.f, t.learner.lrDecay + 0.2f);
            G.SpawnParticles(pos, COL_AI, 16, 110.f);
            G.AddFloat(pos, "突觸強化！", COL_AI);
            break;
        }
        case TType::AND:{
            G.buffArmorBreak = 10.f;
            G.SpawnParticles(pos, COL_AND, 20, 140.f);
            G.AddFloat(pos, "裝甲穿透！10秒", COL_AND);
            break;
        }
        case TType::OR:{
            G.buffOverfreq = 10.f;
            G.SpawnParticles(pos, COL_OR, 20, 140.f);
            G.AddFloat(pos, "超頻！射速x2 10秒", COL_OR);
            break;
        }
        case TType::XOR:{
            G.buffGlobalMark = 5.f;
            for(auto&e:G.enemies){ e.marked=true; e.markTimer=5.f; }
            G.SpawnParticles(pos, COL_XOR, 24, 130.f);
            G.AddFloat(pos, "全域標記！", COL_XOR);
            break;
        }
        case TType::CANNON:{
            // Super shot: 500 dmg splash
            Enemy* tgt=nullptr; float best=999.f;
            for(auto&e:G.enemies){
                float d=Dist({(float)t.gx,(float)t.gy},G.EnemyGrid(e));
                if(d<best){best=d;tgt=&e;}
            }
            if(tgt){
                Vector2 tp=G.EnemyWorld(*tgt),sp=G.CC(t.gx,t.gy);
                Vector2 dir=Vector2Normalize(Vector2Subtract(tp,sp));
                G.bullets.push_back({sp,{dir.x*600.f,dir.y*600.f},tgt->id,500.f,true,80.f,RED});
                G.AddFloat(pos, "超砲！", RED);
            }
            G.SpawnParticles(pos, RED, 16, 160.f);
            break;
        }
        case TType::NAND:{
            G.buffArmorBreak = 10.f;
            G.buffGlobalMark = 10.f;
            for(auto&e:G.enemies){ e.marked=true; e.markTimer=10.f; }
            G.SpawnParticles(pos, COL_NAND, 24, 150.f);
            G.AddFloat(pos, "破甲反轉！全標記10秒", COL_NAND);
            break;
        }
        default: break;
    }
    G.Shake(6.f, 0.2f);
}

void Update(Game&G,float dt){
    if(G.paused||G.phase==Game::GAMEOVER||G.phase==Game::MENU)return;
    if(G.msgTimer>0)G.msgTimer-=dt;
    if(G.shakeT>0)G.shakeT-=dt;
    if(G.comboTimer>0){G.comboTimer-=dt;if(G.comboTimer<=0)G.combo=0;}
    for(auto&h:G.aiHints) h.flashT += dt * 2.5f;
    // Buff timers
    if(G.buffOverfreq>0)   G.buffOverfreq   -= dt;
    if(G.buffArmorBreak>0) G.buffArmorBreak -= dt;
    if(G.buffGlobalMark>0) G.buffGlobalMark -= dt;
    // Active skill cooldowns
    for(auto&t:G.towers) if(t.activeCd>0) t.activeCd=std::max(0.f,t.activeCd-dt);

    if(G.phase==Game::TRAINING){
        G.trainingTimer-=dt;
        if(G.trainingTimer<=0){
            G.phase=Game::BUILD;
            G.SetMsg("訓練完成！準備下一波。");
        }
        return;
    }
    if(G.phase==Game::FIGHT&&G.spawned<G.waveCount){
        G.spawnTimer+=dt;
        float intv=std::max(.15f,.75f-G.wave*.035f);
        if(G.spawnTimer>=intv){G.spawnTimer=0;SpawnEnemy(G);G.spawned++;}
    }
    if(G.phase==Game::FIGHT) PropagateSignals(G,dt);
    for(auto&t:G.towers){
        t.anim+=dt*3.f;if(t.anim>6.28f)t.anim-=6.28f;
        if(t.upgradeFlash>0)t.upgradeFlash-=dt*2.f;
    }
    for(auto&cannon:G.towers){
        if(cannon.type!=TType::CANNON)continue;
        if(cannon.cooldown>0)cannon.cooldown--;
        if(cannon.sig<.5f||cannon.cooldown>0)continue;
        int effectiveCd=cannon.fireRateTicks;
        bool hasOrL2=false;
        for(auto&src:G.towers)
            for(int cid:src.conns)
                if(cid==cannon.id&&src.type==TType::OR&&src.level>=2){hasOrL2=true;break;}
        if(hasOrL2)effectiveCd=(int)(effectiveCd*.7f);
        if(G.buffOverfreq>0) effectiveCd=(int)(effectiveCd*0.5f);
        Enemy*tgt=nullptr;float best=cannon.range;
        for(auto&e:G.enemies){
            Vector2 eg=G.EnemyGrid(e);
            float d=Dist({(float)cannon.gx,(float)cannon.gy},eg);
            if(d<best){best=d;tgt=&e;}
        }
        if(!tgt)continue;
        cannon.cooldown=effectiveCd;
        float dmg=cannon.damage*CalcDamageMultiplier(G,cannon,*tgt);
        bool hasXorL2=false;
        for(auto&src:G.towers)
            for(int cid:src.conns)
                if(cid==cannon.id&&src.type==TType::XOR&&src.level>=2){hasXorL2=true;break;}
        bool crit=hasXorL2&&(G.rng()%5==0);
        if(crit)dmg*=2.f;
        bool hasXorL3=false;
        for(auto&src:G.towers)
            for(int cid:src.conns)
                if(cid==cannon.id&&src.type==TType::XOR&&src.level>=3){hasXorL3=true;break;}
        if(hasXorL3){tgt->marked=true;tgt->markTimer=4.f;}
        Vector2 tp=G.EnemyWorld(*tgt),sp=G.CC(cannon.gx,cannon.gy);
        Vector2 dir=Vector2Normalize(Vector2Subtract(tp,sp));
        bool hasSplash=(cannon.level==3);
        Color bcol=crit?YELLOW:COL_CANNON;
        G.bullets.push_back({sp,{dir.x*420.f,dir.y*420.f},tgt->id,dmg,hasSplash,hasSplash?55.f:0.f,bcol});
        bool hasOrL3=false;
        for(auto&src:G.towers)
            for(int cid:src.conns)
                if(cid==cannon.id&&src.type==TType::OR&&src.level>=3){hasOrL3=true;break;}
        if(hasOrL3){
            Enemy*tgt2=nullptr;float best2=cannon.range;
            for(auto&e:G.enemies){
                if(e.id==tgt->id)continue;
                Vector2 eg=G.EnemyGrid(e);
                float d=Dist({(float)cannon.gx,(float)cannon.gy},eg);
                if(d<best2){best2=d;tgt2=&e;}
            }
            if(tgt2){
                Vector2 tp2=G.EnemyWorld(*tgt2);
                Vector2 dir2=Vector2Normalize(Vector2Subtract(tp2,sp));
                G.bullets.push_back({sp,{dir2.x*420.f,dir2.y*420.f},tgt2->id,dmg*.6f,false,0.f,COL_OR});
            }
        }
    }
    for(auto&b:G.bullets){b.pos.x+=b.vel.x*dt;b.pos.y+=b.vel.y*dt;}
    std::vector<Bullet>nb2;
    for(auto&b:G.bullets){
        bool hit=false;
        for(auto&e:G.enemies){
            Vector2 ep=G.EnemyWorld(e);
            if(Dist(b.pos,ep)<20.f){
                e.hp-=b.dmg;e.flashTimer=.15f;
                G.waveDmg+=b.dmg;
                G.SpawnParticles(b.pos,b.col,6,80.f);
                hit=true;
                if(b.splash){
                    for(auto&e2:G.enemies){
                        if(e2.id==e.id)continue;
                        if(Dist(G.EnemyWorld(e2),b.pos)<b.splashR){
                            e2.hp-=b.dmg*.6f;
                            G.SpawnParticles(G.EnemyWorld(e2),Color{255,180,50,255},4,60.f);
                        }
                    }
                    G.SpawnParticles(b.pos,Color{255,120,40,255},16,100.f);
                }
                break;
            }
        }
        if(!hit){
            auto o=G.MapOrigin();
            if(b.pos.x>o.x&&b.pos.x<o.x+MAP_W&&b.pos.y>o.y&&b.pos.y<o.y+MAP_H)
                nb2.push_back(b);
        }
    }
    G.bullets=nb2;
    for(auto it=G.enemies.begin();it!=G.enemies.end();){
        if(it->hp<=0){
            G.waveKills++;
            G.combo++;G.comboTimer=Game::COMBO_WINDOW;
            Vector2 eg=G.EnemyGrid(*it);
            int kgx=(int)(eg.x+0.5f), kgy=(int)(eg.y+0.5f);
            float killValue=(it->type==EType::BOSS)?8.f:(it->type==EType::ELITE)?3.f:1.f;
            G.threatMap.AddKill(kgx, kgy, killValue);
            int baseReward=it->reward;
            float comboMult=(G.combo>=10)?2.f:(G.combo>=5)?1.5f:(G.combo>=3)?1.2f:1.f;
            int reward=(int)(baseReward*comboMult);
            G.credits+=reward;G.score+=(int)(reward*1.5f);
            if(G.score>G.highScore)G.highScore=G.score;
            Vector2 p=G.EnemyWorld(*it);
            G.SpawnParticles(p,COL_VIRUS,it->type==EType::BOSS?40:12,it->type==EType::BOSS?180.f:100.f);
            std::string ftxt="+"+std::to_string(reward)+" CR";
            if(G.combo>=3){char cb[16];snprintf(cb,16," x%.1f",comboMult);ftxt+=cb;}
            G.AddFloat(p,ftxt,G.combo>=5?COL_STAR:COL_AND);
            if(it->type==EType::BOSS){G.AddFloat({p.x,p.y-50},"BOSS 擊倒！",COL_BOSS);G.Shake(20.f,.6f);}
            if(G.combo==3)G.AddFloat({p.x,p.y-30},"COMBO x3！",Color{255,220,100,255});
            if(G.combo==5)G.AddFloat({p.x,p.y-30},"COMBO x5！",COL_STAR);
            if(G.combo==10)G.AddFloat({p.x,p.y-30},"MEGA COMBO！",Color{255,100,255,255});
            it=G.enemies.erase(it);
        }else ++it;
    }
    for(auto&e:G.enemies){
        e.angle+=180.f*dt*(e.type==EType::BOSS?1.f:2.f);
        if(e.flashTimer>0)e.flashTimer-=dt;
        if(e.marked){e.markTimer-=dt;if(e.markTimer<=0)e.marked=false;}
        if(e.type==EType::ELITE){
            e.regenTimer+=dt;
            if(e.regenTimer>=2.f){e.regenTimer=0;e.hp=std::min(e.maxHp,e.hp+e.maxHp*.05f);}
        }
        if(G.currentEvent==WaveEvent::REGEN_ARMY && e.type!=EType::ELITE){
            e.regenTimer+=dt;
            if(e.regenTimer>=1.f){e.regenTimer=0;e.hp=std::min(e.maxHp,e.hp+e.maxHp*.08f);}
        }
        if(e.type==EType::BOSS) UpdateBossAI(G, e, dt);
        if(e.stunTimer > 0){ e.stunTimer -= dt; continue; } // stunned: skip movement
        float speed = e.spd;
        if(e.type==EType::BOSS) speed *= e.evadeSpdMult;
        e.pathPos += speed * dt;
        auto& pathRef = (e.pathIdx==1 && G.dualPath) ? PATH_CELLS2 : PATH_CELLS;
        if(e.pathPos>=(float)(pathRef.size()-1)){
            int dmg=e.type==EType::BOSS?5:1;
            G.lives-=dmg;
            float cpuDmg=(e.type==EType::BOSS)?25.f:8.f;
            G.cpuHp=std::max(0.f,G.cpuHp-cpuDmg);
            G.SpawnParticles(G.CC(CPU_GX,CPU_GY),COL_VIRUS,16,120.f);
            G.AddFloat(G.CC(CPU_GX,CPU_GY),"-"+std::to_string(dmg)+" 命",RED);
            G.Shake(12.f,.4f);
            G.waveEscaped++;
            e.hp=0;
        }
    }
    G.enemies.erase(std::remove_if(G.enemies.begin(),G.enemies.end(),
        [](const Enemy&e){return e.hp<=0;}),G.enemies.end());
    for(auto&p:G.pulses)p.t+=dt*3.5f;
    G.pulses.erase(std::remove_if(G.pulses.begin(),G.pulses.end(),
        [](const SigPulse&p){return p.t>=1.f;}),G.pulses.end());
    if(G.pulses.size()>500)
        G.pulses.erase(G.pulses.begin(),G.pulses.begin()+G.pulses.size()-500);
    for(auto&p:G.particles){p.pos.x+=p.vel.x*dt;p.pos.y+=p.vel.y*dt;p.vel.y+=55.f*dt;p.life-=dt;}
    G.particles.erase(std::remove_if(G.particles.begin(),G.particles.end(),
        [](const Particle&p){return p.life<=0;}),G.particles.end());
    for(auto&f:G.floats){f.pos.y-=28.f*dt;f.life-=dt;}
    G.floats.erase(std::remove_if(G.floats.begin(),G.floats.end(),
        [](const FloatText&f){return f.life<=0;}),G.floats.end());
    if(G.phase==Game::FIGHT&&G.enemies.empty()&&G.spawned>=G.waveCount){
        int bonus=60+G.wave*12;
        if(G.waveEscaped==0){
            bonus=(int)(bonus*1.5f);
            G.SetMsg("完美防守！+"+std::to_string(bonus)+" CR！進入訓練階段...");
        }else{
            char buf[96];
            snprintf(buf,96,"第%d波結束！+%d CR。%d個病毒突破。",G.wave,bonus,G.waveEscaped);
            G.SetMsg(buf);
        }
        G.credits+=bonus;
        G.phase=Game::TRAINING;
        G.trainingTimer=Game::TRAIN_TIME;
        G.threatMap.Decay(0.85f);
        G.TrainPerceptrons();
        G.GenerateAIHints();
    }
    if(G.lives<=0||G.cpuHp<=0){
        G.lives=0;G.cpuHp=0;G.phase=Game::GAMEOVER;
        if(G.score>G.highScore)G.highScore=G.score;
        G.SetMsg("防線崩潰 — 遊戲結束！");G.Shake(25.f,1.f);
    }
}

// ── Draw Helpers ──────────────────────────────────────────────
void DrawRoundBox(float x,float y,float w,float h,float r,
                  Color fill,Color border,float bw=1.5f){
    float rr=r/std::min(w,h)*2;
    DrawRectangleRounded({x,y,w,h},rr,8,fill);
    DrawRectangleRoundedLinesEx({x,y,w,h},rr,8,bw,border);
}
void DrawHex(Vector2 c,float r,Color fill,Color border){
    Vector2 pts[6];
    for(int i=0;i<6;i++){
        float a=i*60.f*DEG2RAD-30.f*DEG2RAD;
        pts[i]={c.x+r*cosf(a),c.y+r*sinf(a)};
    }
    for(int i=0;i<6;i++) DrawTriangle(c,pts[i],pts[(i+1)%6],fill);
    for(int i=0;i<6;i++) DrawLineV(pts[i],pts[(i+1)%6],border);
}

void DrawStars(Game&G){
    float t=(float)GetTime();
    for(auto&s:G.stars){
        float b=s.bright*(.7f+.3f*sinf(t*1.1f+s.x));
        Color c={(unsigned char)(150*b),(unsigned char)(170*b),(unsigned char)(255*b),255};
        DrawCircle((int)s.x,(int)s.y,s.r,c);
    }
}

void DrawPath(Game&G){
    auto o=G.MapOrigin();
    // Draw path preview (ghost) during training if rotation is imminent
    bool showPreview = (G.phase==Game::TRAINING || G.phase==Game::BUILD) && (G.wave % 3 == 0);
    if(showPreview){
        const PathPreset* nextP = &PATH_PRESETS[G.nextPreviewPath];
        // Draw preview ghost cells
        std::vector<PathCell> previewCells;
        for(int wi=0;wi+1<nextP->count;wi++){
            int x0=nextP->wx[wi],y0=nextP->wy[wi],x1=nextP->wx[wi+1],y1=nextP->wy[wi+1];
            int dx=(x1>x0)?1:(x1<x0)?-1:0,dy=(y1>y0)?1:(y1<y0)?-1:0;
            int cx=x0,cy=y0;
            while(cx!=x1||cy!=y1){
                if(cx>=0&&cx<COLS&&cy>=0&&cy<ROWS) previewCells.push_back({cx,cy});
                cx+=dx;cy+=dy;
            }
        }
        float t=(float)GetTime();
        float pulse=0.4f+0.4f*sinf(t*3.f);
        for(auto&pc:previewCells){
            float px=o.x+pc.gx*CELL,py=o.y+pc.gy*CELL;
            Color pvc={255,200,50,(unsigned char)(pulse*90)};
            DrawRectangle((int)px+3,(int)py+3,CELL-6,CELL-6,pvc);
            DrawRectangleLinesEx({px+2,py+2,(float)CELL-4,(float)CELL-4},1.5f,AlphaOf({255,200,50,255},(int)(pulse*160)));
        }
        // Label
        char pn2[64];snprintf(pn2,64,"⚠ 下波路線：%s",nextP->name);
        DTC(pn2,(int)(o.x+MAP_W/2),(int)(o.y+MAP_H-20),FS_SMALL,AlphaOf({255,200,50,255},(int)(pulse*255)));
    }
    // Draw primary path
    for(auto&pc:PATH_CELLS){
        float px=o.x+pc.gx*CELL,py=o.y+pc.gy*CELL;
        DrawRectangle((int)px,(int)py,CELL,CELL,COL_PATH);
        DrawRectangle((int)px+2,(int)py+2,CELL-4,CELL-4,AlphaOf({15,28,45,255},255));
    }
    // Draw secondary path (dual path)
    if(G.dualPath && !PATH_CELLS2.empty()){
        for(auto&pc:PATH_CELLS2){
            float px=o.x+pc.gx*CELL,py=o.y+pc.gy*CELL;
            Color p2c={20,15,40,255};
            DrawRectangle((int)px,(int)py,CELL,CELL,p2c);
            DrawRectangle((int)px+2,(int)py+2,CELL-4,CELL-4,AlphaOf({30,15,55,255},255));
        }
    }
    float t=(float)GetTime();
    // 路段方向線 (primary)
    for(int wi=0;wi+1<CUR_PRESET->count;wi++){
        int x0=CUR_PRESET->wx[wi],y0=CUR_PRESET->wy[wi];
        int x1=CUR_PRESET->wx[wi+1],y1=CUR_PRESET->wy[wi+1];
        if(x0<0||x0>=COLS)continue;
        Vector2 p0={o.x+(x0+.5f)*CELL,o.y+(y0+.5f)*CELL};
        Vector2 p1={o.x+(x1+.5f)*CELL,o.y+(y1+.5f)*CELL};
        Color lc={0,100,60,35};DrawLineEx(p0,p1,CELL-4.f,lc);
    }
    // 路段方向線 (secondary)
    if(G.dualPath){
        for(int wi=0;wi+1<CUR_PRESET2->count;wi++){
            int x0=CUR_PRESET2->wx[wi],y0=CUR_PRESET2->wy[wi];
            int x1=CUR_PRESET2->wx[wi+1],y1=CUR_PRESET2->wy[wi+1];
            if(x0<0||x0>=COLS)continue;
            Vector2 p0={o.x+(x0+.5f)*CELL,o.y+(y0+.5f)*CELL};
            Vector2 p1={o.x+(x1+.5f)*CELL,o.y+(y1+.5f)*CELL};
            Color lc={80,0,100,35};DrawLineEx(p0,p1,CELL-4.f,lc);
        }
    }
    // 路段箭頭 (primary)
    for(int wi=0;wi+1<CUR_PRESET->count;wi++){
        int x0=CUR_PRESET->wx[wi],y0=CUR_PRESET->wy[wi];
        int x1=CUR_PRESET->wx[wi+1],y1=CUR_PRESET->wy[wi+1];
        if(x0<0)continue;
        Vector2 p0={o.x+(x0+.5f)*CELL,o.y+(y0+.5f)*CELL};
        Vector2 p1={o.x+(x1+.5f)*CELL,o.y+(y1+.5f)*CELL};
        Vector2 mid={(p0.x+p1.x)*.5f,(p0.y+p1.y)*.5f};
        Vector2 dir=Vector2Normalize(Vector2Subtract(p1,p0));
        float ang=atan2f(dir.y,dir.x);
        float al=14.f,aw=.5f;
        float anim=.7f+.3f*sinf(t*2.5f+wi*.8f);
        Color ac={0,180,100,(unsigned char)(80*anim)};
        DrawTriangle(mid,
            {mid.x+al*cosf(ang+aw+3.14f),mid.y+al*sinf(ang+aw+3.14f)},
            {mid.x+al*cosf(ang-aw+3.14f),mid.y+al*sinf(ang-aw+3.14f)},ac);
    }
    // 路段箭頭 (secondary)
    if(G.dualPath){
        for(int wi=0;wi+1<CUR_PRESET2->count;wi++){
            int x0=CUR_PRESET2->wx[wi],y0=CUR_PRESET2->wy[wi];
            int x1=CUR_PRESET2->wx[wi+1],y1=CUR_PRESET2->wy[wi+1];
            if(x0<0)continue;
            Vector2 p0={o.x+(x0+.5f)*CELL,o.y+(y0+.5f)*CELL};
            Vector2 p1={o.x+(x1+.5f)*CELL,o.y+(y1+.5f)*CELL};
            Vector2 mid={(p0.x+p1.x)*.5f,(p0.y+p1.y)*.5f};
            Vector2 dir=Vector2Normalize(Vector2Subtract(p1,p0));
            float ang=atan2f(dir.y,dir.x);
            float al=14.f,aw=.5f;
            float anim=.7f+.3f*sinf(t*2.5f+wi*.8f+1.5f);
            Color ac={200,80,255,(unsigned char)(80*anim)};
            DrawTriangle(mid,
                {mid.x+al*cosf(ang+aw+3.14f),mid.y+al*sinf(ang+aw+3.14f)},
                {mid.x+al*cosf(ang-aw+3.14f),mid.y+al*sinf(ang-aw+3.14f)},ac);
        }
    }
    // 格線
    for(int x=0;x<=COLS;x++){
        Color c=AlphaOf({0,200,100,255},(x%5==0)?18:8);
        DrawLine((int)(o.x+x*CELL),(int)o.y,(int)(o.x+x*CELL),(int)(o.y+ROWS*CELL),c);
    }
    for(int y=0;y<=ROWS;y++){
        Color c=AlphaOf({0,200,100,255},(y%4==0)?18:8);
        DrawLine((int)o.x,(int)(o.y+y*CELL),(int)(o.x+COLS*CELL),(int)(o.y+y*CELL),c);
    }
    // 路線名稱
    char pn[48];
    if(G.dualPath)
        snprintf(pn,48,"主：%s | 副：%s",CUR_PRESET->name,CUR_PRESET2->name);
    else
        snprintf(pn,32,"路線：%s",CUR_PRESET->name);
    DTX(pn,o.x+4,o.y+4,FS_TINY,AlphaOf(COL_SENSOR,120));
}

void DrawThreatMap(Game&G){
    if(!G.showThreat) return;
    float mx = G.threatMap.GetMax();
    if(mx < 0.01f) return;
    auto o = G.MapOrigin();
    for(auto&pc : PATH_CELLS){
        float v = G.threatMap.Get(pc.gx, pc.gy);
        if(v < 0.01f) continue;
        float norm = std::min(1.f, v / mx);
        unsigned char r = (unsigned char)(norm * 255);
        unsigned char g = (unsigned char)((1.f - norm) * 120);
        unsigned char b = (unsigned char)((1.f - norm) * 200);
        unsigned char a = (unsigned char)(40 + norm * 120);
        Color heatCol = {r, g, b, a};
        float px = o.x + pc.gx * CELL;
        float py = o.y + pc.gy * CELL;
        DrawRectangle((int)px+2, (int)py+2, CELL-4, CELL-4, heatCol);
        if(norm > 0.6f){
            char buf[8]; snprintf(buf, 8, "%.0f", v);
            DTC(buf,(int)(px+CELL*.5f),(int)(py+CELL*.5f),FS_TINY,AlphaOf(WHITE,(int)(160*norm)));
        }
    }
    auto mo = G.MapOrigin();
    int lx = (int)(mo.x + MAP_W - 130);
    int ly = (int)(mo.y + MAP_H - 30);
    for(int i = 0; i < 100; i++){
        float n = i / 100.f;
        unsigned char lr=(unsigned char)(n*255);
        unsigned char lg=(unsigned char)((1.f-n)*120);
        unsigned char lb=(unsigned char)((1.f-n)*200);
        DrawRectangle(lx + i, ly, 1, 14, {lr, lg, lb, 200});
    }
    DTX("低", (float)(lx - 24), (float)ly, FS_TINY, AlphaOf(WHITE, 160));
    DTX("高", (float)(lx + 102), (float)ly, FS_TINY, AlphaOf(WHITE, 160));
}

void DrawAIHints(Game&G){
    if(!G.showAIHints) return;
    if(G.phase != Game::BUILD && G.phase != Game::TRAINING) return;
    float t = (float)GetTime();
    for(auto&h : G.aiHints){
        float px = (float)PANEL_L + h.gx * CELL;
        float py = (float)TOPBAR_H + h.gy * CELL;
        float pulse = 0.55f + 0.45f * sinf(t * 3.5f + h.gx * 0.7f);
        unsigned char alpha = (unsigned char)(pulse * 200);
        Color bgCol = TDef(h.suggest).col;
        bgCol.a = (unsigned char)(pulse * 35);
        DrawRectangle((int)px+3, (int)py+3, CELL-6, CELL-6, bgCol);
        Color bdCol = COL_AI; bdCol.a = alpha;
        DrawRectangleLinesEx({px+2, py+2, (float)CELL-4, (float)CELL-4},2.f, bdCol);
        DTX("AI", px+4, py+3, FS_TINY, AlphaOf(COL_AI, (int)(pulse*220)));
        DTC(TDef(h.suggest).sym,(int)(px+CELL/2),(int)(py+CELL/2+4),
            FS_SMALL,AlphaOf(TDef(h.suggest).col,(int)(pulse*200)));
        Vector2 mp = VirtualMouse();
        if(mp.x >= px && mp.x < px+CELL && mp.y >= py && mp.y < py+CELL){
            float tw = MCN(h.reason.c_str(), FS_TINY) + 12;
            float tx = px + CELL/2 - tw/2;
            float ty = py - 26;
            DrawRectangle((int)tx-2, (int)ty-2, (int)tw+4, 22,AlphaOf({4,9,18,255}, 220));
            DrawRectangleLinesEx({tx-2, ty-2, tw+4, 22}, 1.f,AlphaOf(COL_AI, 180));
            DTX(h.reason.c_str(), tx+4, ty+2, FS_TINY,AlphaOf(COL_AI, 230));
        }
    }
}

void DrawConnections(Game&G){
    float t=(float)GetTime();
    for(auto&tw:G.towers){
        for(int cid:tw.conns){
            auto*dst=G.FindTower(cid);if(!dst)continue;
            Vector2 src=G.CC(tw.gx,tw.gy),d=G.CC(dst->gx,dst->gy);
            Color sc=TDef(tw.type).col;sc.a=(unsigned char)(60+tw.sig*110);
            DrawLineEx(src,d,2.f+tw.sig*2.5f,sc);
            Vector2 dir=Vector2Normalize(Vector2Subtract(d,src));
            float aT=fmodf(t*.7f+tw.id*.25f,1.f);
            Vector2 mid={src.x+(d.x-src.x)*aT,src.y+(d.y-src.y)*aT};
            float ang=atan2f(dir.y,dir.x),al=11.f,aw=.45f;
            Color ac=sc;ac.a=(unsigned char)(120+tw.sig*90);
            DrawTriangle(mid,
                {mid.x+al*cosf(ang+aw+3.14f),mid.y+al*sinf(ang+aw+3.14f)},
                {mid.x+al*cosf(ang-aw+3.14f),mid.y+al*sinf(ang-aw+3.14f)},ac);
        }
    }
}

void DrawPulses(Game&G){
    for(auto&p:G.pulses){
        float fade=1.f-p.t;
        Vector2 pos={p.src.x+(p.dst.x-p.src.x)*p.t,p.src.y+(p.dst.y-p.src.y)*p.t};
        Color c=p.col;c.a=(unsigned char)(240*fade);DrawCircleV(pos,7.f,c);
        c.a=(unsigned char)(70*fade);DrawCircleV(pos,13.f,c);
        Color cc=WHITE;cc.a=(unsigned char)(180*fade);DrawCircleV(pos,3.f,cc);
    }
}

void DrawTower(Game&G,Tower&t,bool sel){
    Vector2 ctr=G.CC(t.gx,t.gy);
    float px=(float)PANEL_L+t.gx*CELL,py=(float)TOPBAR_H+t.gy*CELL;
    if(t.type==TType::CPU){
        float glow=10.f+8.f*sinf((float)GetTime()*2.5f);
        float cpuR=G.cpuHp/100.f;
        Color cpuCol=cpuR>.5f?COL_CPU:cpuR>.25f?ORANGE:RED;
        for(int i=3;i>=1;i--){
            Color gc=cpuCol;gc.a=(unsigned char)(12*i);float ext=glow*i*.5f;
            DrawRectangleRounded({px+3.f-ext,py+3.f-ext,(float)CELL-6+ext*2,(float)CELL-6+ext*2},.25f,8,gc);
        }
        DrawRectangleRounded({px+4,py+4,(float)CELL-8,(float)CELL-8},.2f,8,Color{6,20,35,255});
        DrawRectangleRoundedLines({px+4,py+4,(float)CELL-8,(float)CELL-8},.2f,8,cpuCol);
        DTC("CPU",(int)ctr.x,(int)ctr.y-8,FS_MED,cpuCol);
        char buf[8];snprintf(buf,8,"%.0f%%",G.cpuHp);
        DTC(buf,(int)ctr.x,(int)ctr.y+14,FS_SMALL,cpuCol);return;
    }
    TowerDef&def=TDef(t.type);
    Color fill=def.col;fill.a=(unsigned char)((.15f+t.sig*.85f)*65);
    Color border=def.col;border.a=(unsigned char)(150+t.sig*100);
    if(t.upgradeFlash>0){
        Color ufc=WHITE;ufc.a=(unsigned char)(200*t.upgradeFlash);
        DrawRectangleRounded({px+2,py+2,(float)CELL-4,(float)CELL-4},.2f,8,ufc);
    }
    if(t.sig>.05f){
        for(int i=2;i>=1;i--){
            float gr=12.f+t.sig*18.f;Color gc=def.col;gc.a=(unsigned char)(t.sig*22/i);
            if(t.type==TType::PERCEPTRON)
                DrawCircle((int)ctr.x,(int)ctr.y,(int)((float)CELL*.43f+gr*i/2),gc);
            else
                DrawRectangle((int)(px+2-gr*i/3),(int)(py+2-gr*i/3),(int)(CELL-4+gr*i*2/3),(int)(CELL-4+gr*i*2/3),gc);
        }
    }
    if(t.type==TType::PERCEPTRON) DrawHex(ctr,(float)CELL*.42f,fill,border);
    else DrawRoundBox(px+5,py+5,(float)CELL-10,(float)CELL-10,6,fill,border,2.f);
    if(sel){
        float pulse=.85f+.15f*sinf((float)GetTime()*5.f);
        Color wc=WHITE;wc.a=200;DrawCircleLinesV(ctr,(float)CELL*.54f*pulse,wc);
        if(t.type==TType::SENSOR){DrawCircleV(ctr,t.range*CELL,AlphaOf(COL_SENSOR,30));DrawCircleLinesV(ctr,t.range*CELL,AlphaOf(COL_SENSOR,100));}
        if(t.type==TType::CANNON){DrawCircleV(ctr,t.range*CELL,AlphaOf(COL_CANNON,25));DrawCircleLinesV(ctr,t.range*CELL,AlphaOf(COL_CANNON,80));}
    }
    if(t.type == TType::PERCEPTRON){
        int layer = CalcPCTLayer(G, t.id);
        char lb[8];snprintf(lb,8,"L%d",layer);
        Color lc = layer==1?COL_PERC:layer==2?COL_AI:COL_STAR;
        DTX(lb, px+CELL-22, py+4, FS_TINY, AlphaOf(lc, 220));
    }
    DTC(def.sym,(int)ctr.x,(int)ctr.y-8,FS_SMALL,border);
    for(int lv=0;lv<t.level;lv++){float sx=px+8+lv*11.f,sy=py+CELL-16.f;DrawPoly({sx,sy},5,5.f,0.f,COL_STAR);}
    int bx=(int)px+6,by=(int)py+CELL-10,bw=CELL-12,bh=7;
    DrawRectangle(bx,by,bw,bh,Color{0,0,0,140});
    int fw=(int)(bw*t.sig);if(fw>0){Color bc=def.col;bc.a=220;DrawRectangle(bx,by,fw,bh,bc);}
    // Active skill cooldown bar (thin bar above signal bar)
    if(t.type != TType::CPU){
        float maxCd = SKILL_CD[(int)t.type];
        if(maxCd > 0.f){
            int abx=bx,aby=by-8,abw=bw,abh=4;
            DrawRectangle(abx,aby,abw,abh,Color{0,0,0,100});
            float ready = 1.f - t.activeCd/maxCd;
            int afw=(int)(abw*ready);
            Color adc = t.activeCd<=0 ? Color{100,255,180,255} : Color{80,140,255,200};
            if(afw>0) DrawRectangle(abx,aby,afw,abh,adc);
            if(t.activeCd<=0){
                // Pulsing border when ready
                float p2=0.6f+0.4f*sinf((float)GetTime()*5.f);
                Color rc={100,255,180,(unsigned char)(180*p2)};
                DrawRectangleLinesEx({(float)abx,(float)aby,(float)abw,(float)abh},1.f,rc);
            }
        }
    }
    if(t.type == TType::PERCEPTRON && t.learner.lastLoss > 0.f){
        Color lossCol = t.learner.lastLoss < 0.05f ? GREEN : t.learner.lastLoss < 0.15f ? YELLOW : RED;
        DrawCircle((int)(px + CELL - 9), (int)(py + 9), 5.f, lossCol);
        DrawCircleLines((int)(px + CELL - 9), (int)(py + 9), 5.f, AlphaOf(WHITE, 120));
    }
}

void DrawGhostTower(Game&G,int gx,int gy){
    if(G.placing==TType::NONE||gx<0||gx>=COLS||gy<0||gy>=ROWS)return;
    if(G.TowerAt(gx,gy)||G.IsPath(gx,gy))return;
    TowerDef&def=TDef(G.placing);
    float px=(float)PANEL_L+gx*CELL,py=(float)TOPBAR_H+gy*CELL;
    bool can=(G.credits>=def.baseCost);
    Color fill=def.col;fill.a=can?45:18;Color bd=def.col;bd.a=can?160:50;
    DrawRoundBox(px+5,py+5,(float)CELL-10,(float)CELL-10,6,fill,bd);
    DTC(def.sym,(int)(px+CELL/2),(int)(py+CELL/2-5),FS_SMALL,bd);
    if(!can) DTC("不足",(int)(px+CELL/2),(int)(py+CELL/2+14),FS_TINY,RED);
    if(G.placing==TType::SENSOR){float r=4.5f*CELL;Vector2 c2={px+CELL*.5f,py+CELL*.5f};DrawCircleV(c2,r,AlphaOf(COL_SENSOR,15));DrawCircleLinesV(c2,r,AlphaOf(COL_SENSOR,70));}
    if(G.placing==TType::CANNON){float r=7.f*CELL;Vector2 c2={px+CELL*.5f,py+CELL*.5f};DrawCircleV(c2,r,AlphaOf(COL_CANNON,12));DrawCircleLinesV(c2,r,AlphaOf(COL_CANNON,60));}
}

void DrawEnemies(Game&G){
    float t=(float)GetTime();
    for(auto&e:G.enemies){
        Vector2 p=G.EnemyWorld(e);
        bool fl=(e.flashTimer>0);
        Color base,ring;float sz=14.f;
        switch(e.type){
            case EType::BASIC:  base=COL_VIRUS;  ring={255,80,120,200};sz=14.f;break;
            case EType::FAST:   base=COL_FAST;   ring={255,220,80,200};sz=11.f;break;
            case EType::ARMORED:base=COL_ARMORED;ring={160,200,255,200};sz=17.f;break;
            case EType::ELITE:  base=COL_ELITE;  ring={255,120,210,200};sz=15.f;break;
            case EType::BOSS:   base=COL_BOSS;   ring={200,80,255,220};sz=26.f;break;
        }
        if(fl){base=WHITE;ring=WHITE;}
        Vector2 pts[6];
        for(int i=0;i<6;i++){float a=i*60.f*DEG2RAD+e.angle*DEG2RAD;pts[i]={p.x+sz*cosf(a),p.y+sz*sinf(a)};}
        for(int i=0;i<6;i++) DrawTriangle(p,pts[i],pts[(i+1)%6],AlphaOf(base,fl?220:160));
        for(int i=0;i<6;i++) DrawLineV(pts[i],pts[(i+1)%6],ring);
        if(e.stealth){float st=0.5f+0.5f*sinf(t*4.f+e.id);DrawCircleLinesV(p,sz+4.f,AlphaOf(COL_FAST,(int)(80*st)));}
        if(e.marked){float mp2=0.7f+0.3f*sinf(t*6.f);DrawCircleLinesV(p,sz+6.f,AlphaOf(COL_XOR,(int)(180*mp2)));}
        float hpR=e.hp/e.maxHp;
        int bw=sz*2+4,bh=5;int bx=(int)(p.x-bw/2),by=(int)(p.y-sz-10);
        DrawRectangle(bx,by,bw,bh,Color{0,0,0,160});
        Color hpC=hpR>.6f?GREEN:hpR>.3f?ORANGE:RED;
        DrawRectangle(bx,by,(int)(bw*hpR),bh,hpC);
        if(e.type==EType::BOSS){
            const char* stateLabel=
                e.bossState==BossState::RAMPAGE?"RAMPAGE！":
                e.bossState==BossState::EVADE?"迴避中":"BOSS";
            Color stateCol=
                e.bossState==BossState::RAMPAGE?RED:
                e.bossState==BossState::EVADE?YELLOW:COL_BOSS;
            float pulse2=0.8f+0.2f*sinf(t*5.f);
            stateCol.a=(unsigned char)(220*pulse2);
            DTC(stateLabel,(int)p.x,(int)(p.y-sz-22),FS_SMALL,stateCol);
            if(G.showThreat){
                char tb[24];snprintf(tb,24,"thr:%.1f",e.localThreat);
                DTC(tb,(int)p.x,(int)(p.y+sz+14),FS_TINY,AlphaOf(COL_THREAT,180));
            }
        }
    }
}

void DrawBullets(Game&G){
    for(auto&b:G.bullets){
        DrawCircleV(b.pos,b.splash?8.f:5.f,b.col);
        Color gc=b.col;gc.a=80;DrawCircleV(b.pos,b.splash?14.f:9.f,gc);
    }
}
void DrawParticles(Game&G){
    for(auto&p:G.particles){float alpha=p.life/p.maxLife;Color c=p.col;c.a=(unsigned char)(200*alpha);DrawCircleV(p.pos,p.radius*alpha,c);}
}
void DrawFloats(Game&G){
    for(auto&f:G.floats){float a=std::min(1.f,f.life);Color c=f.col;c.a=(unsigned char)(230*a);DTC(f.text.c_str(),(int)f.pos.x,(int)f.pos.y,FS_SMALL,c);}
}

void DrawLeftPanel(Game&G){
    DrawRectangle(0,0,PANEL_L,VIRT_H,PANEL_BG);
    DrawRectangleLines(0,0,PANEL_L,VIRT_H,PANEL_BD);
    float t=(float)GetTime();
    DTC("元件選擇",PANEL_L/2,38,FS_MED,AlphaOf(COL_CPU,220));
    static TType ORDER[]={TType::SENSOR,TType::PERCEPTRON,TType::AND,TType::OR,TType::XOR,TType::NAND,TType::CANNON};
    G.btnY0=68;
    constexpr int BTN_H_SM=56, BTN_GAP_SM=3; // smaller buttons to fit 7
    for(int i=0;i<7;i++){
        TType tt=ORDER[i]; TowerDef&def=TDef(tt);
        int by=G.btnY0+i*(BTN_H_SM+BTN_GAP_SM);
        bool sel=(G.placing==tt);bool canBuy=(G.credits>=def.baseCost);
        Color bg=PANEL_BG;
        if(sel){bg=def.col;bg.a=55;}else if(!canBuy){bg={8,8,8,255};}
        Color bd=sel?def.col:AlphaOf(def.col,canBuy?80:30);
        if(sel){float p2=.7f+.3f*sinf(t*4.f);bd.a=(unsigned char)(200*p2);}
        DrawRoundBox(10,(float)by,PANEL_L-20,(float)BTN_H_SM,8,bg,bd,sel?2.5f:1.5f);
        Color sc=sel?def.col:AlphaOf(def.col,canBuy?200:80);
        DTC(def.sym,48,by+BTN_H_SM/2,FS_MED,sc);
        Color tc=canBuy?WHITE:AlphaOf(WHITE,80);
        DTX(def.label,70,(float)by+5,FS_TINY,tc);
        char cs[16];snprintf(cs,16,"%d CR",def.baseCost);
        DTX(cs,70,(float)by+22,FS_TINY,AlphaOf(COL_AND,canBuy?200:80));
        DTX(def.desc,12,(float)by+BTN_H_SM-15,FS_TINY,AlphaOf(WHITE,canBuy?100:40));
    }
    G.waveBtnY = G.btnY0 + 7*(BTN_H_SM+BTN_GAP_SM) + 12;
    if(G.phase==Game::BUILD||G.phase==Game::TRAINING){
        float p2=.7f+.3f*sinf(t*3.f);
        Color wbc=G.phase==Game::BUILD?COL_PERC:AlphaOf(COL_PERC,120);
        DrawRoundBox(10,(float)G.waveBtnY,PANEL_L-20,54,10,AlphaOf(wbc,25),wbc,2.5f);
        const char* wbTxt=G.phase==Game::BUILD?"▶ 發動下一波":"訓練中...";
        DTC(wbTxt,PANEL_L/2,G.waveBtnY+27,FS_MED,AlphaOf(wbc,(int)(220*p2)));
        if(G.phase==Game::TRAINING){char tb[32];snprintf(tb,32,"%.1f 秒",G.trainingTimer);DTC(tb,PANEL_L/2,G.waveBtnY+46,FS_TINY,AlphaOf(COL_AND,180));}
    }
    int infoY = G.waveBtnY + 70;
    char cr[32];snprintf(cr,32,"金幣 %d CR",G.credits);
    DTC(cr,PANEL_L/2,infoY,FS_MED,COL_AND);
    if(G.msgTimer>0 && !G.msg.empty()){
        float alpha=std::min(1.f,G.msgTimer);
        std::string m=G.msg;int wrap=28;
        if((int)m.size()>wrap){
            DTX(m.substr(0,wrap).c_str(),8,(float)(infoY+28),FS_TINY,AlphaOf(COL_SENSOR,(int)(200*alpha)));
            DTX(m.substr(wrap).c_str(),8,(float)(infoY+48),FS_TINY,AlphaOf(COL_SENSOR,(int)(200*alpha)));
        }else{
            DTX(m.c_str(),8,(float)(infoY+28),FS_TINY,AlphaOf(COL_SENSOR,(int)(200*alpha)));
        }
    }
    int akY = infoY + 80;
    DrawRoundBox(10,(float)akY,PANEL_L-20,52,6,AlphaOf(COL_AI,12),AlphaOf(COL_AI,60),1.f);
    DTC("AI 工具",PANEL_L/2,akY+12,FS_TINY,AlphaOf(COL_AI,180));
    DTX("[T] 熱圖",18,(float)(akY+28),FS_TINY,G.showThreat?COL_AI:AlphaOf(COL_AI,100));
    DTX("[A] 提示",PANEL_L/2+4,(float)(akY+28),FS_TINY,G.showAIHints?COL_AI:AlphaOf(COL_AI,100));
}

void DrawRightPanel(Game&G){
    int rx=VIRT_W-PANEL_R;
    DrawRectangle(rx,0,PANEL_R,VIRT_H,PANEL_BG);
    DrawRectangleLines(rx,0,PANEL_R,VIRT_H,PANEL_BD);
    int cx=rx+PANEL_R/2;
    DTC("元件資訊",cx,38,FS_MED,AlphaOf(COL_CPU,220));
    Tower*sel=G.FindTower(G.selectedId);
    if(!sel){
        DTC("點擊元件查看",cx,VIRT_H/2,FS_SMALL,AlphaOf(WHITE,80));
    } else {
        TowerDef&def=TDef(sel->type);int y=70;
        DTC(def.label,cx,y,FS_LARGE,def.col); y+=44;
        for(int lv=0;lv<sel->level;lv++) DrawPoly({(float)(rx+30+lv*22),(float)y},5,9.f,0.f,COL_STAR);
        y+=26;
        DrawRoundBox((float)rx+12,(float)y,(float)PANEL_R-24,18,4,AlphaOf(BLACK,180),AlphaOf(def.col,80));
        int sw=(int)((PANEL_R-28)*sel->sig);
        if(sw>0) DrawRectangle(rx+14,y+2,sw,14,AlphaOf(def.col,200));
        char sb[20];snprintf(sb,20,"訊號 %.2f",sel->sig);
        DTC(sb,cx,y+9,FS_TINY,WHITE); y+=32;
        DTX(def.desc,(float)rx+10,(float)y,FS_TINY,AlphaOf(WHITE,160));y+=22;
        DTX(def.tiers[sel->level-1].ability,(float)rx+10,(float)y,FS_TINY,AlphaOf(def.col,200));y+=22;
        if(sel->type!=TType::CPU && sel->level<3){
            int ucost=TDef(sel->type).tiers[sel->level].cost;bool canUp=(G.credits>=ucost);
            Color uc=canUp?COL_PERC:AlphaOf(COL_PERC,60);
            DrawRoundBox((float)rx+12,(float)y,(float)PANEL_R-24,36,6,AlphaOf(uc,canUp?25:10),uc,1.5f);
            char ub[40];snprintf(ub,40,"[U] 升級 Lv.%d  %d CR",sel->level+1,ucost);
            DTC(ub,cx,y+18,FS_TINY,uc); y+=46;
        }
        char kb[32];snprintf(kb,32,"擊殺：%d",sel->kills);DTX(kb,(float)rx+12,(float)y,FS_TINY,AlphaOf(WHITE,160));y+=20;
        char db[40];snprintf(db,40,"總傷：%.0f",sel->totalDmg);DTX(db,(float)rx+12,(float)y,FS_TINY,AlphaOf(WHITE,140));y+=28;
        if(sel->type==TType::PERCEPTRON){
            DrawRoundBox((float)rx+8,(float)y,(float)PANEL_R-16,110,6,AlphaOf(COL_AI,10),AlphaOf(COL_AI,80),1.5f);
            DTC("神經元學習狀態",cx,y+14,FS_TINY,AlphaOf(COL_AI,220));y+=28;
            char w1b[32];snprintf(w1b,32,"w1  = %+.3f",sel->w1);
            char w2b[32];snprintf(w2b,32,"w2  = %+.3f",sel->w2);
            char bib[32];snprintf(bib,32,"bias= %+.3f",sel->bias);
            DTX(w1b,(float)rx+14,(float)y,FS_TINY,AlphaOf(COL_PERC,200));y+=18;
            DTX(w2b,(float)rx+14,(float)y,FS_TINY,AlphaOf(COL_PERC,200));y+=18;
            DTX(bib,(float)rx+14,(float)y,FS_TINY,AlphaOf(COL_OR,200));y+=18;
            float loss=sel->learner.lastLoss;
            Color lc=loss<.05f?GREEN:loss<.15f?YELLOW:RED;
            char lb2[32];snprintf(lb2,32,"loss= %.4f",loss);DTX(lb2,(float)rx+14,(float)y,FS_TINY,lc);y+=18;
            char lrd[36];snprintf(lrd,36,"lr衰減= %.3f",sel->learner.lrDecay);
            DTX(lrd,(float)rx+14,(float)y,FS_TINY,AlphaOf(WHITE,140));y+=20;
            // Loss sparkline
            if(!sel->lossHistory.empty()){
                y+=4;
                int sw=PANEL_R-24, sh=36;
                DrawRectangle(rx+12,y,sw,sh,AlphaOf(BLACK,120));
                DrawRectangleLinesEx({(float)rx+12,(float)y,(float)sw,(float)sh},1.f,AlphaOf(COL_AI,60));
                DTX("loss趨勢",(float)rx+14,(float)y+2,FS_TINY,AlphaOf(COL_AI,160));
                int n=(int)sel->lossHistory.size();
                float maxL=*std::max_element(sel->lossHistory.begin(),sel->lossHistory.end());
                maxL=std::max(maxL,0.01f);
                for(int i=0;i<n;i++){
                    float lv=sel->lossHistory[i]/maxL;
                    int bwi=std::max(2,(sw-4)/std::max(n,1));
                    int hh=(int)(lv*(sh-10));
                    int bxi=rx+14+(sw-4)*i/std::max(n,1);
                    int byi=y+sh-2-hh;
                    Color bc=sel->lossHistory[i]<.05f?GREEN:sel->lossHistory[i]<.15f?YELLOW:RED;
                    DrawRectangle(bxi,byi,std::min(bwi-1,8),hh,AlphaOf(bc,180));
                }
                y+=sh+6;
            }
        }
        // Active skill info
        if(sel->type != TType::CPU){
            int ti=(int)sel->type;
            float maxCd = SKILL_CD[ti];
            if(maxCd > 0.f){
                DrawRoundBox((float)rx+8,(float)y,(float)PANEL_R-16,40,4,AlphaOf(COL_AI,8),AlphaOf(COL_AI,50),1.f);
                char sn[48];snprintf(sn,48,"[Q] %s",SKILL_NAME[ti]);
                DTX(sn,(float)rx+12,(float)y+4,FS_TINY,AlphaOf(COL_AI,200));
                if(sel->activeCd<=0){
                    DTX("✓ 就緒！",(float)rx+12,(float)y+20,FS_TINY,AlphaOf({100,255,180,255},230));
                } else {
                    char cdb[32];snprintf(cdb,32,"冷卻 %.0f秒",sel->activeCd);
                    DTX(cdb,(float)rx+12,(float)y+20,FS_TINY,AlphaOf(ORANGE,200));
                }
                y+=48;
            }
        }
        if(!sel->conns.empty()){char cc[24];snprintf(cc,24,"連線：%d 條",(int)sel->conns.size());DTX(cc,(float)rx+12,(float)y,FS_TINY,AlphaOf(WHITE,120));y+=20;}
        DTX("[C] 連線  [U] 升級",(float)rx+12,(float)y,FS_TINY,AlphaOf(WHITE,90));y+=18;
        DTX("[DEL] 移除",(float)rx+12,(float)y,FS_TINY,AlphaOf(WHITE,90));
    }
}

void DrawTopBar(Game&G){
    DrawRectangle(0,0,VIRT_W,TOPBAR_H,PANEL_BG);
    DrawLine(0,TOPBAR_H,VIRT_W,TOPBAR_H,PANEL_BD);
    float t=(float)GetTime();
    float p2=.8f+.2f*sinf(t*1.5f);
    DTC("邏輯閘防禦戰",VIRT_W/2,TOPBAR_H/2,FS_TITLE,AlphaOf(COL_CPU,(int)(220*p2)));
    char wb[32];snprintf(wb,32,"第 %d 波",G.wave);DTX(wb,PANEL_L+20,12,FS_MED,COL_SENSOR);
    char lb[20];snprintf(lb,20,"命 %d",G.lives);DTX(lb,PANEL_L+20,40,FS_MED,G.lives>10?COL_PERC:G.lives>5?ORANGE:RED);
    char sc[32];snprintf(sc,32,"分數：%d",G.score);DTX(sc,PANEL_L+200,12,FS_MED,COL_AND);
    char hs[32];snprintf(hs,32,"最高：%d",G.highScore);DTX(hs,PANEL_L+200,38,FS_TINY,AlphaOf(COL_AND,160));
    // Dual path indicator
    if(G.dualPath){
        float dp=0.7f+0.3f*sinf(t*4.f);
        DTC("⚡雙路進攻",PANEL_L+430,TOPBAR_H/2,FS_SMALL,AlphaOf(COL_BOSS,(int)(220*dp)));
    }
    // Active buffs
    int bx2=PANEL_L+560; int by2=8;
    if(G.buffArmorBreak>0){
        char ab[24];snprintf(ab,24,"破甲 %.0fs",G.buffArmorBreak);
        DrawRoundBox((float)bx2,(float)by2,90,22,4,AlphaOf(COL_AND,30),AlphaOf(COL_AND,180));
        DTC(ab,bx2+45,by2+11,FS_TINY,COL_AND); bx2+=96;
    }
    if(G.buffOverfreq>0){
        char ob[24];snprintf(ob,24,"超頻 %.0fs",G.buffOverfreq);
        DrawRoundBox((float)bx2,(float)by2,90,22,4,AlphaOf(COL_OR,30),AlphaOf(COL_OR,180));
        DTC(ob,bx2+45,by2+11,FS_TINY,COL_OR); bx2+=96;
    }
    if(G.buffGlobalMark>0){
        char ob2[24];snprintf(ob2,24,"標記 %.0fs",G.buffGlobalMark);
        DrawRoundBox((float)bx2,(float)by2,90,22,4,AlphaOf(COL_XOR,30),AlphaOf(COL_XOR,180));
        DTC(ob2,bx2+45,by2+11,FS_TINY,COL_XOR); bx2+=96;
    }
    if(G.combo>=3){
        char cb[24];snprintf(cb,24,"COMBO x%d",G.combo);
        float cp=.8f+.2f*sinf(t*8.f);
        Color cc2=G.combo>=10?Color{255,100,255,255}:G.combo>=5?COL_STAR:Color{255,220,100,255};
        cc2.a=(unsigned char)(230*cp);DTC(cb,VIRT_W-PANEL_R-200,TOPBAR_H/2,FS_LARGE,cc2);
    }
    const char* phaseStr=G.phase==Game::FIGHT?"戰鬥中":G.phase==Game::TRAINING?"訓練中":G.phase==Game::BUILD?"建置":"";
    Color phaseCol=G.phase==Game::FIGHT?COL_VIRUS:G.phase==Game::TRAINING?COL_AI:COL_PERC;
    DTC(phaseStr,VIRT_W-PANEL_R-60,TOPBAR_H/2,FS_MED,phaseCol);
    if(G.eventBannerTimer>0 && G.currentEvent!=WaveEvent::NONE){
        G.eventBannerTimer -= GetFrameTime();
        float alpha = std::min(1.f, G.eventBannerTimer);
        float pulse2 = 0.8f+0.2f*sinf((float)GetTime()*6.f);
        Color ec={255,80,80,(unsigned char)(230*alpha*pulse2)};
        DTC(G.eventName.c_str(),VIRT_W/2,TOPBAR_H+36,FS_LARGE,ec);
    }
}

void DrawBotBar(Game&G){
    int by=VIRT_H-BOTBAR_H;
    DrawRectangle(0,by,VIRT_W,BOTBAR_H,PANEL_BG);
    DrawLine(0,by,VIRT_W,by,PANEL_BD);
    const char*hint="[空白]發動  [C]連線  [U]升級  [Q]主動技能  [DEL]移除  [T]熱圖  [A]AI提示  [P]暫停  [H]說明  [F11]全螢幕";
    DTC(hint,VIRT_W/2,by+BOTBAR_H/2,FS_TINY,AlphaOf(WHITE,120));
}

void DrawMenu(Game&G){
    float t=(float)GetTime();
    int cx=VIRT_W/2,cy=VIRT_H/2;
    DrawRectangleGradientV(0,0,VIRT_W,VIRT_H,Color{3,7,14,255},Color{8,18,35,255});
    DrawStars(G);
    for(int i=4;i>=1;i--){float glow=30.f*i*(.8f+.2f*sinf(t*1.5f));DrawCircleV({(float)cx,(float)(cy-120)},glow,AlphaOf(COL_CPU,6/i));}
    float tp=.85f+.15f*sinf(t*2.f);
    DTC("邏輯閘防禦戰",cx,cy-140,FS_BIG,AlphaOf(COL_CPU,(int)(240*tp)));
    DTC("Logic Gate Defense  v3.0 + AI Edition",cx,cy-82,FS_MED,AlphaOf(COL_SENSOR,180));
    const char* feats[]={"★ 神經網路訊號傳播","★ 感知器多層學習+loss圖","★ Boss 狀態機 AI","★ 威脅熱圖視覺化","★ 雙路徑+預告系統","★ 主動技能+NAND閘"};
    for(int i=0;i<6;i++){
        int col=i%3,row=i/3;int fx=cx-460+col*320;int fy=cy-30+row*28;
        float fp=.6f+.4f*sinf(t*1.8f+i*.6f);
        DTC(feats[i],fx,fy,FS_SMALL,AlphaOf(COL_AI,(int)(180*fp)));
    }
    float sp=.7f+.3f*sinf(t*3.5f);
    DTC("按 ENTER 或 空白鍵 開始",cx,cy+60,FS_LARGE,AlphaOf(COL_PERC,(int)(230*sp)));
    DTC("[H] 查看說明",cx,cy+96,FS_MED,AlphaOf(WHITE,120));
    if(G.highScore>0){char hs[40];snprintf(hs,40,"歷史最高分：%d",G.highScore);DTC(hs,cx,cy+138,FS_MED,AlphaOf(COL_STAR,200));}
    DTX("v3.0+AI  Powered by Raylib",16,(float)(VIRT_H-28),FS_TINY,AlphaOf(WHITE,50));
    static float nodeT=0.f; nodeT+=0.016f;
    const int NN=6;float nx[NN],ny[NN];
    for(int i=0;i<NN;i++){nx[i]=(float)cx-250+i*100.f;ny[i]=(float)(cy+195)+sinf(nodeT*1.4f+i)*18.f;}
    for(int i=0;i<NN-1;i++){
        float sig=0.5f+0.5f*sinf(nodeT*2.f+i*.9f);Color lc=COL_PERC;lc.a=(unsigned char)(60+sig*100);
        DrawLineEx({nx[i],ny[i]},{nx[i+1],ny[i+1]},1.5f+sig*2.f,lc);
        float dot=fmodf(nodeT*0.8f+i*.18f,1.f);
        Vector2 dp={nx[i]+(nx[i+1]-nx[i])*dot,ny[i]+(ny[i+1]-ny[i])*dot};
        DrawCircleV(dp,5.f,AlphaOf(COL_PERC,(int)(180*sig)));
    }
    for(int i=0;i<NN;i++){float sig=0.5f+0.5f*sinf(nodeT*1.6f+i);DrawHex({nx[i],ny[i]},14.f,AlphaOf(COL_PERC,(int)(40*sig)),AlphaOf(COL_PERC,(int)(160*sig)));}
}

void DrawHelp(){
    DrawRectangle(PANEL_L+80,TOPBAR_H+40,MAP_W-160,MAP_H-80,Color{4,9,18,240});
    DrawRectangleLinesEx({(float)(PANEL_L+80),(float)(TOPBAR_H+40),(float)(MAP_W-160),(float)(MAP_H-80)},2.f,COL_SENSOR);
    int cx=PANEL_L+MAP_W/2,y=TOPBAR_H+70;
    DTC("── 操作說明 ──",cx,y,FS_LARGE,COL_CPU); y+=46;
    struct HelpRow{const char*key;const char*desc;Color col;};
    HelpRow rows[]={
        {"點擊左側按鈕","選擇元件（再按取消）",WHITE},
        {"點擊地圖空格","放置元件",WHITE},
        {"[C]","進入連線模式",COL_SENSOR},
        {"[U]","升級選取元件",COL_PERC},
        {"[Q]","發動主動技能（選取元件後）",COL_AI},
        {"[DEL]","移除選取元件（退款50%）",COL_CANNON},
        {"[空白/ENTER]","發動下一波次",COL_PERC},
        {"[T]","切換威脅熱圖",COL_AI},
        {"[A]","切換 AI 顧問提示",COL_AI},
        {"[P]","暫停/繼續",WHITE},
        {"[H]","說明",WHITE},
        {"[F11]","全螢幕切換",WHITE},
    };
    for(auto&r:rows){
        float kw=MCN(r.key,FS_SMALL);int kx=PANEL_L+200;int dx=kx+80;
        DTX(r.key,(float)(kx-(int)kw/2-30),(float)y,FS_SMALL,AlphaOf(COL_AND,220));
        DTX(r.desc,(float)dx,(float)y,FS_SMALL,AlphaOf(r.col,190));y+=28;
    }
    y+=10;DTC("── AI 與關卡說明 ──",cx,y,FS_MED,COL_AI);y+=34;
    struct AIRow{const char*title;const char*desc;};
    AIRow aiRows[]={
        {"感知器學習","每波結束以擊殺率為目標，對 w1/w2/bias 執行 Delta Rule 梯度更新"},
        {"感知器層數","感知器串接形成多層網路（L1/L2/L3），右側面板顯示 loss 折線圖"},
        {"威脅熱圖","擊殺位置累積熱度，跨波衰減 15%，Boss 貢獻 8x"},
        {"Boss 狀態機","偵測前方高威脅→EVADE加速x2.0；血量<30%永久RAMPAGE x2.4"},
        {"波次事件","每波隨機：蜂群/裝甲洪流/霧戰/精英突擊/護衛Boss/再生軍/神速"},
        {"路線輪換","每 3 波自動換路線（共 5 條），訓練期間顯示下波路線預告"},
        {"雙路進攻","Wave 8 起開通第二條路徑，敵人交替從兩路進攻"},
        {"主動技能","選取元件後按 [Q]：SENSOR=EMP暫停，OR=超頻，CANNON=超砲等"},
        {"NAND閘","非AND閘：後期反制裝甲利器，配合主動技能可破甲歸零"},
    };
    for(auto&r:aiRows){
        DTX(r.title,(float)(PANEL_L+100),(float)y,FS_SMALL,AlphaOf(COL_AI,230));y+=22;
        DTX(r.desc,(float)(PANEL_L+120),(float)y,FS_TINY,AlphaOf(WHITE,160));y+=26;
    }
    DTC("點擊任意處或按 [H]/[ESC] 關閉",cx,TOPBAR_H+MAP_H-60,FS_MED,AlphaOf(WHITE,140));
}

void DrawGameOver(Game&G){
    DrawRectangle(PANEL_L,TOPBAR_H,MAP_W,MAP_H,Color{5,0,0,200});
    int cx=PANEL_L+MAP_W/2,cy=VIRT_H/2;float t=(float)GetTime();
    float rp=.75f+.25f*sinf(t*4.f);
    DTC("防線崩潰",cx,cy-110,FS_BIG,AlphaOf(RED,(int)(240*rp)));
    char sbuf[64];snprintf(sbuf,64,"最終分數：%d",G.score);DTC(sbuf,cx,cy-38,FS_TITLE,COL_AND);
    if(G.score>=G.highScore&&G.highScore>0) DTC("新紀錄！",cx,cy+10,FS_LARGE,COL_STAR);
    else{char hs[48];snprintf(hs,48,"最高分：%d",G.highScore);DTC(hs,cx,cy+10,FS_LARGE,AlphaOf(COL_STAR,180));}
    char wb[48];snprintf(wb,48,"撐過 %d 波次",G.wave);DTC(wb,cx,cy+50,FS_LARGE,AlphaOf(WHITE,180));
    int pctCount=0;float avgLoss=0.f;
    for(auto&tw:G.towers) if(tw.type==TType::PERCEPTRON){pctCount++;avgLoss+=tw.learner.lastLoss;}
    if(pctCount>0){
        avgLoss/=pctCount;char aib[64];snprintf(aib,64,"感知器 %d 個 | 平均 loss %.4f",pctCount,avgLoss);
        DTC(aib,cx,cy+86,FS_MED,AlphaOf(COL_AI,200));
        const char* grade=avgLoss<0.05f?"神經網路已收斂":avgLoss<0.15f?"學習中，再多幾波":"需要更多感知器或連線";
        DTC(grade,cx,cy+114,FS_SMALL,AlphaOf(COL_AI,170));
    }
    float pp=.8f+.2f*sinf(t*4.f);
    DTC("[R] 重新開始    [ENTER] 主選單",cx,cy+152,FS_LARGE,AlphaOf(Color{210,210,210,255},(int)(210*pp)));
}

void HandleInput(Game&G){
    Vector2 mp=VirtualMouse();int hgx=-1,hgy=-1;bool onMap=ScreenToGrid(G,mp,hgx,hgy);
    if(G.phase==Game::MENU){
        if(G.showHelp){if(IsKeyPressed(KEY_H)||IsKeyPressed(KEY_ESCAPE)||IsMouseButtonPressed(MOUSE_LEFT_BUTTON))G.showHelp=false;return;}
        if(IsKeyPressed(KEY_H))G.showHelp=true;
        if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE))G.Reset();
        return;
    }
    if(IsKeyPressed(KEY_H)){G.showHelp=!G.showHelp;if(G.showHelp&&G.phase==Game::FIGHT)G.paused=true;else if(!G.showHelp)G.paused=false;return;}
    if(G.showHelp){if(IsKeyPressed(KEY_ESCAPE)||IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){G.showHelp=false;G.paused=false;}return;}
    if(IsKeyPressed(KEY_P)&&G.phase!=Game::GAMEOVER){G.paused=!G.paused;return;}
    if(IsKeyPressed(KEY_R)){G.Reset();return;}
    if(IsKeyPressed(KEY_T)){G.showThreat=!G.showThreat;G.SetMsg(G.showThreat?"熱圖：開啟":"熱圖：關閉");return;}
    if(IsKeyPressed(KEY_A)){G.showAIHints=!G.showAIHints;G.SetMsg(G.showAIHints?"AI 提示：開啟":"AI 提示：關閉");return;}
    if(G.phase==Game::GAMEOVER){if(IsKeyPressed(KEY_ENTER))G.phase=Game::MENU;return;}
    if(G.paused)return;
    if(IsKeyPressed(KEY_Q)&&G.selectedId>=0){
        Tower*st=G.FindTower(G.selectedId);
        if(st&&st->type!=TType::CPU){
            if(st->activeCd>0.f){char b2[48];snprintf(b2,48,"技能冷卻中...%.0f 秒",st->activeCd);G.SetMsg(b2);}
            else{ActivateSkill(G,*st);char b2[48];snprintf(b2,48,"[%s] %s！",TDef(st->type).sym,SKILL_NAME[(int)st->type]);G.SetMsg(b2);}
        }
    }
    if(IsKeyPressed(KEY_U)&&G.selectedId>=0){Tower*st=G.FindTower(G.selectedId);if(st)G.UpgradeTower(*st);}
    static TType ORDER[]={TType::SENSOR,TType::PERCEPTRON,TType::AND,TType::OR,TType::XOR,TType::NAND,TType::CANNON};
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)&&mp.x<PANEL_L){
        for(int i=0;i<7;i++){
            constexpr int BTN_H_SM=56, BTN_GAP_SM=3;
            int by=G.btnY0+i*(BTN_H_SM+BTN_GAP_SM);
            if(mp.y>=by&&mp.y<by+BTN_H_SM){TType tt=ORDER[i];G.placing=(G.placing==tt)?TType::NONE:tt;G.selectedId=-1;G.connectSrc=-1;return;}
        }
        if(mp.y>=G.waveBtnY&&mp.y<G.waveBtnY+58&&G.phase==Game::BUILD){StartWave(G);return;}
    }
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)&&onMap){
        if(G.connectSrc>=0){
            Tower*dst=G.TowerAt(hgx,hgy);
            if(dst&&dst->id!=G.connectSrc){
                Tower*src=G.FindTower(G.connectSrc);
                if(src){
                    auto it=std::find(src->conns.begin(),src->conns.end(),dst->id);
                    if(it!=src->conns.end()){src->conns.erase(it);G.SetMsg("連線已取消！");}
                    else{std::string err=G.ValidateConnect(G.connectSrc,dst->id);if(err.empty()){src->conns.push_back(dst->id);G.SetMsg("連接成功！");}else G.SetMsg("[錯誤] "+err);}
                }
            }
            G.connectSrc=-1;return;
        }
        Tower*ex=G.TowerAt(hgx,hgy);
        if(ex){G.selectedId=ex->id;G.placing=TType::NONE;}
        else if(G.placing!=TType::NONE){
            if(G.IsPath(hgx,hgy)){G.SetMsg("[錯誤] 不能放在路徑上！");}
            else if(G.credits>=TDef(G.placing).baseCost){
                Tower nt;nt.id=G.nextId++;nt.type=G.placing;nt.gx=hgx;nt.gy=hgy;
                nt.level=1;nt.sig=0;nt.w1=.8f;nt.w2=.6f;nt.bias=-.5f;nt.cooldown=0;nt.anim=0;
                G.ApplyTowerStats(nt);G.towers.push_back(nt);G.credits-=TDef(G.placing).baseCost;
                if(G.placing==TType::PERCEPTRON)G.SetMsg("感知器已放置，每波結束會自動學習！");
                else{char b2[48];snprintf(b2,48,"已放置 %s",TDef(G.placing).label);G.SetMsg(b2);}
            }else G.SetMsg("金幣不足！");
        }else{G.selectedId=-1;}
    }
    if(IsKeyPressed(KEY_SPACE)&&G.phase==Game::BUILD)StartWave(G);
    if(IsKeyPressed(KEY_ESCAPE)){G.placing=TType::NONE;G.connectSrc=-1;}
    if(IsKeyPressed(KEY_C)&&G.selectedId>=0){
        auto*st=G.FindTower(G.selectedId);
        if(!st){}
        else if(st->type==TType::CANNON)G.SetMsg("[錯誤] 砲塔是終端節點，不能輸出");
        else if(st->type==TType::CPU)G.SetMsg("[錯誤] CPU不能連接");
        else if((int)st->conns.size()>=Game::MAX_CONNS)G.SetMsg("[錯誤] 已達8條上限");
        else{G.connectSrc=G.selectedId;G.SetMsg("點擊目標建立連線 | 點已連接目標取消 | [ESC]取消");}
    }
    if(IsKeyPressed(KEY_DELETE)&&G.selectedId>=0){
        auto*st=G.FindTower(G.selectedId);
        if(st&&st->type!=TType::CPU){
            int rid=G.selectedId;int refund=TDef(st->type).baseCost/2;G.credits+=refund;
            G.towers.erase(std::remove_if(G.towers.begin(),G.towers.end(),[rid](const Tower&t){return t.id==rid;}),G.towers.end());
            for(auto&t:G.towers)t.conns.erase(std::remove(t.conns.begin(),t.conns.end(),rid),t.conns.end());
            G.selectedId=-1;char b3[48];snprintf(b3,48,"已移除，退款+%d CR",refund);G.SetMsg(b3);
        }
    }
}

int main(){
    const int WIN_W=1280,WIN_H=720;
    SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_VSYNC_HINT|FLAG_WINDOW_RESIZABLE);
    InitWindow(WIN_W,WIN_H,"邏輯閘防禦戰 v3.0 + AI Edition");
    SetExitKey(KEY_NULL);
    int mon=GetCurrentMonitor();int mw=GetMonitorWidth(mon),mh=GetMonitorHeight(mon);
    SetWindowPosition((mw-WIN_W)/2,(mh-WIN_H)/2);
    LoadChineseFont();BuildPath(0);
    bool borderlessFS=false;int savedW=WIN_W,savedH=WIN_H,savedX=(mw-WIN_W)/2,savedY=(mh-WIN_H)/2;
    RenderTexture2D target=LoadRenderTexture(VIRT_W,VIRT_H);
    SetTextureFilter(target.texture,TEXTURE_FILTER_BILINEAR);
    SetTargetFPS(60);
    Game G;G.InitStars();G.phase=Game::MENU;
    while(!WindowShouldClose()){
        float dt=std::min(GetFrameTime(),.05f);
        if(IsKeyPressed(KEY_F11)){
            if(!borderlessFS){
                savedW=GetScreenWidth();savedH=GetScreenHeight();
                savedX=(int)GetWindowPosition().x;savedY=(int)GetWindowPosition().y;
                mon=GetCurrentMonitor();mw=GetMonitorWidth(mon);mh=GetMonitorHeight(mon);
                SetWindowState(FLAG_WINDOW_UNDECORATED);SetWindowSize(mw,mh);SetWindowPosition(0,0);borderlessFS=true;
            }else{ClearWindowState(FLAG_WINDOW_UNDECORATED);SetWindowSize(savedW,savedH);SetWindowPosition(savedX,savedY);borderlessFS=false;}
        }
        int wW=GetScreenWidth(),wH=GetScreenHeight();
        float sx=(float)wW/VIRT_W,sy=(float)wH/VIRT_H;
        gScale=(sx<sy)?sx:sy;gOffX=(wW-VIRT_W*gScale)*.5f;gOffY=(wH-VIRT_H*gScale)*.5f;
        HandleInput(G);Update(G,dt);
        Vector2 vmp=VirtualMouse();int hgx=-1,hgy=-1;ScreenToGrid(G,vmp,hgx,hgy);
        float shakeX=0,shakeY=0;
        if(G.shakeT>0){float sp=G.shakePow*(G.shakeT*3.f);std::uniform_real_distribution<float>sd(-sp,sp);shakeX=sd(G.rng);shakeY=sd(G.rng);}
        BeginTextureMode(target);ClearBackground(BG);
        if(G.phase==Game::MENU){DrawMenu(G);if(G.showHelp)DrawHelp();}
        else{
            DrawStars(G);gShkX=shakeX;gShkY=shakeY;
            BeginScissorMode(PANEL_L,TOPBAR_H,MAP_W,MAP_H);
            DrawPath(G);DrawThreatMap(G);DrawConnections(G);DrawPulses(G);
            for(auto&t:G.towers)DrawTower(G,t,t.id==G.selectedId);
            DrawAIHints(G);DrawGhostTower(G,hgx,hgy);
            DrawEnemies(G);DrawBullets(G);DrawParticles(G);DrawFloats(G);
            EndScissorMode();gShkX=0;gShkY=0;
            DrawLeftPanel(G);DrawRightPanel(G);DrawTopBar(G);DrawBotBar(G);
            if(G.phase==Game::GAMEOVER)DrawGameOver(G);
            if(G.showHelp)DrawHelp();
        }
        EndTextureMode();
        BeginDrawing();ClearBackground(BLACK);
        DrawTexturePro(target.texture,{0.f,0.f,(float)VIRT_W,-(float)VIRT_H},
            {gOffX,gOffY,VIRT_W*gScale,VIRT_H*gScale},{0.f,0.f},0.f,WHITE);
        EndDrawing();
    }
    UnloadRenderTexture(target);if(gFontLoaded)UnloadFont(gFont);CloseWindow();return 0;
}