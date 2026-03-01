// ================================================================
//  邏輯閘防禦：神經網路塔防  v1.0
//  Neural Network Tower Defense
//  Build: CMake + Raylib 5.0  (自動下載)
// ================================================================
#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#include <functional>
#include <sstream>
#include <iomanip>

// ── Constants ─────────────────────────────────────────────────
constexpr int SCREEN_W   = 1280;
constexpr int SCREEN_H   = 720;
constexpr int PANEL_L    = 180;   // left panel width
constexpr int PANEL_R    = 220;   // right panel width
constexpr int MAP_W      = SCREEN_W - PANEL_L - PANEL_R; // 880
constexpr int MAP_H      = SCREEN_H - 40;                // 680
constexpr int CELL        = 40;
constexpr int COLS        = MAP_W / CELL;  // 22
constexpr int ROWS        = MAP_H / CELL;  // 17
constexpr Vector2 CPU_CELL = {11.0f, 8.0f};

// ── Colours ───────────────────────────────────────────────────
Color BG        = {5,  13, 20,  255};
Color GRID_COL  = {0,  255,136, 15};
Color COL_SENSOR= {34, 211,238, 255};
Color COL_PERC  = {74, 222,128, 255};
Color COL_AND   = {250,204, 21, 255};
Color COL_OR    = {251,146, 60, 255};
Color COL_XOR   = {232,121,249, 255};
Color COL_CANNON= {248,113,113, 255};
Color COL_CPU   = {34, 211,238, 255};
Color COL_VIRUS = {255, 51,102, 255};
Color PANEL_BG  = {6,  14, 24, 255};
Color PANEL_BD  = {0,  200,100, 60};

// ── Math helpers ──────────────────────────────────────────────
inline float Sigmoid(float x){ return 1.0f/(1.0f+expf(-x)); }
inline float Dist(Vector2 a, Vector2 b){ return Vector2Distance(a,b); }
inline Color AlphaOf(Color c, int a){ c.a=(unsigned char)a; return c; }
inline Color Lerp3(Color a, Color b, float t){
    return { (unsigned char)((int)a.r+(int)((b.r-a.r)*t)),
             (unsigned char)((int)a.g+(int)((b.g-a.g)*t)),
             (unsigned char)((int)a.b+(int)((b.b-a.b)*t)), 255 };
}

// ── Tower types ───────────────────────────────────────────────
enum class TType { SENSOR, PERCEPTRON, AND, OR, XOR, CANNON, CPU, NONE };

struct TowerDef {
    const char* label;
    const char* sym;
    Color       col;
    int         cost;
    const char* desc;
};

static TowerDef DEFS[] = {
    {"Sensor",     "SEN", COL_SENSOR, 40,  "偵測附近病毒，輸出 0~1 信號"},
    {"Perceptron", "PCT", COL_PERC,   90,  "加權求和 → sigmoid 激活"},
    {"AND",        "AND", COL_AND,    60,  "兩輸入皆 >0.5 才通過"},
    {"OR",         "OR",  COL_OR,     60,  "任一輸入 >0.5 即通過"},
    {"XOR",        "XOR", COL_XOR,    60,  "輸入不同時才通過"},
    {"Cannon",     "GUN", COL_CANNON, 80,  "sig>0.5 時攻擊最近敵人"},
    {"CPU",        "CPU", COL_CPU,    0,   "你的基地"},
};

inline int TypeIdx(TType t){ return (int)t; }
inline TowerDef& Def(TType t){ return DEFS[(int)t]; }

// ── Tower ─────────────────────────────────────────────────────
struct Tower {
    int    id;
    TType  type;
    int    gx, gy;           // grid position
    float  sig{0};           // current signal [0,1]
    float  w1{0.8f}, w2{0.6f}, bias{-0.5f};  // perceptron weights
    int    cooldown{0};
    std::vector<int> conns;  // outgoing connections (dst tower ids)
};

// ── Enemy ─────────────────────────────────────────────────────
struct Enemy {
    int   id;
    float x, y;  // world pos (cells)
    float hp, maxHp;
    float spd;
    int   reward;
    float angle{0};  // rotation for virus spin
};

// ── Bullet ────────────────────────────────────────────────────
struct Bullet {
    Vector2 pos;
    Vector2 vel;
    int     targetId;
    float   dmg;
};

// ── Signal Pulse (visual) ─────────────────────────────────────
struct SigPulse {
    Vector2 src, dst;
    float   t{0};   // 0→1
    Color   col;
};

// ── Particle ─────────────────────────────────────────────────
struct Particle {
    Vector2 pos, vel;
    float   life, maxLife;
    float   radius;
    Color   col;
};

// ── Floating text ─────────────────────────────────────────────
struct FloatText {
    Vector2 pos;
    std::string text;
    Color col;
    float life{1.2f};
};

// ================================================================
//  Game State
// ================================================================
struct Game {
    std::vector<Tower>    towers;
    std::vector<Enemy>    enemies;
    std::vector<Bullet>   bullets;
    std::vector<SigPulse> pulses;
    std::vector<Particle> particles;
    std::vector<FloatText> floats;

    int  nextId{1};
    int  credits{300};
    int  score{0};
    int  wave{0};
    float cpuHp{100.f};

    enum Phase { BUILD, FIGHT, GAMEOVER } phase{BUILD};

    // wave spawning
    int  waveCount{0};
    int  spawned{0};
    float spawnTimer{0};

    TType  placing{TType::NONE};
    int    selectedId{-1};
    int    connectSrc{-1};  // id of tower waiting to connect

    std::string msg{"選擇元件放置，按 [空白鍵] 開始波次"};

    std::mt19937 rng{42};

    // Offset for map drawing
    Vector2 MapOrigin(){ return {(float)PANEL_L, 40.0f}; }

    // Cell → screen centre
    Vector2 CellCenter(int gx, int gy){
        auto o=MapOrigin();
        return {o.x+(gx+0.5f)*CELL, o.y+(gy+0.5f)*CELL};
    }
    Vector2 CellCenter(float gx, float gy){
        auto o=MapOrigin();
        return {o.x+(gx+0.5f)*CELL, o.y+(gy+0.5f)*CELL};
    }

    Tower* FindTower(int id){
        for(auto& t:towers) if(t.id==id) return &t;
        return nullptr;
    }
    Tower* TowerAt(int gx,int gy){
        for(auto& t:towers) if(t.gx==gx&&t.gy==gy) return &t;
        return nullptr;
    }
    Enemy* FindEnemy(int id){
        for(auto& e:enemies) if(e.id==id) return &e;
        return nullptr;
    }

    void SpawnParticles(Vector2 pos, Color col, int n=10, float spd=60.f){
        std::uniform_real_distribution<float> ang(0,6.28f);
        std::uniform_real_distribution<float> sp(spd*0.3f,spd);
        for(int i=0;i<n;i++){
            float a=ang(rng), s=sp(rng);
            particles.push_back({pos,{cosf(a)*s,sinf(a)*s},0.6f,0.6f,3.f,col});
        }
    }

    void AddFloat(Vector2 pos, const std::string& txt, Color col){
        floats.push_back({pos,txt,col,1.2f});
    }
};

// ================================================================
//  Input helpers
// ================================================================
// Map screen point → grid cell (-1 if out of bounds)
bool ScreenToGrid(Game& G, Vector2 sp, int& gx, int& gy){
    auto o=G.MapOrigin();
    int x=(int)((sp.x-o.x)/CELL);
    int y=(int)((sp.y-o.y)/CELL);
    if(x<0||x>=COLS||y<0||y>=ROWS) return false;
    gx=x; gy=y;
    return true;
}

// ================================================================
//  Wave spawning
// ================================================================
static const int SPAWN_GX[] = {0,5,10,16,21, 0,5,10,16,21, 0,21, 0,21};
static const int SPAWN_GY[] = {0,0, 0, 0, 0,16,16,16,16,16, 5, 5,11,11};
static const int NSPAWN = 14;

void StartWave(Game& G){
    if(G.phase==Game::FIGHT) return;
    G.wave++;
    G.waveCount = 5 + G.wave*3;
    G.spawned   = 0;
    G.spawnTimer= 0;
    G.phase     = Game::FIGHT;
    std::ostringstream ss;
    ss<<"波次 "<<G.wave<<" — "<<G.waveCount<<" 個病毒入侵！";
    G.msg = ss.str();
}

void SpawnEnemy(Game& G){
    std::uniform_int_distribution<int> sp(0,NSPAWN-1);
    int i=sp(G.rng);
    float hp=60.f+G.wave*25.f;
    Enemy e;
    e.id     = G.nextId++;
    e.x      = SPAWN_GX[i]+0.5f;
    e.y      = SPAWN_GY[i]+0.5f;
    e.hp     = hp;
    e.maxHp  = hp;
    e.spd    = 1.2f+G.wave*0.12f;   // cells/sec
    e.reward = 10+G.wave*2;
    G.enemies.push_back(e);
}

// ================================================================
//  Signal propagation
// ================================================================
void PropagateSignals(Game& G, float dt){
    // Sensors
    for(auto& t:G.towers){
        if(t.type!=TType::SENSOR) continue;
        float range=4.5f;
        float minD=range+1;
        for(auto& e:G.enemies){
            float d=Dist({t.gx+0.5f,t.gy+0.5f},{e.x,e.y});
            if(d<minD) minD=d;
        }
        float newSig = (minD<=range) ? std::max(0.f, 1.f-minD/range) : 0.f;
        float prev=t.sig;
        t.sig += (newSig-t.sig)*10.f*dt;  // smooth
        if(fabsf(t.sig-prev)>0.02f){
            for(int cid:t.conns){
                auto* dst=G.FindTower(cid);
                if(!dst) continue;
                SigPulse p;
                p.src=G.CellCenter(t.gx,t.gy);
                p.dst=G.CellCenter(dst->gx,dst->gy);
                p.col=Def(t.type).col;
                G.pulses.push_back(p);
            }
        }
    }

    // Gates / Perceptrons
    for(auto& t:G.towers){
        if(t.type==TType::SENSOR||t.type==TType::CPU||t.type==TType::NONE) continue;
        // gather inputs: towers whose conn list includes t.id
        std::vector<float> ins;
        for(auto& src:G.towers){
            for(int cid:src.conns)
                if(cid==t.id){ ins.push_back(src.sig); break; }
        }
        if(ins.empty()) continue;

        float newSig=t.sig;
        switch(t.type){
            case TType::PERCEPTRON:{
                float s1=ins.size()>0?ins[0]:0.f;
                float s2=ins.size()>1?ins[1]:0.f;
                newSig=Sigmoid((s1*t.w1+s2*t.w2+t.bias)*3.f);
                break;
            }
            case TType::AND:{
                bool ok=true; for(float s:ins) if(s<0.5f){ok=false;break;}
                newSig=ok?1.f:0.f; break;
            }
            case TType::OR:{
                bool any=false; for(float s:ins) if(s>0.5f){any=true;break;}
                newSig=any?1.f:0.f; break;
            }
            case TType::XOR:{
                int cnt=0; for(float s:ins) if(s>0.5f) cnt++;
                newSig=(cnt==1)?1.f:0.f; break;
            }
            default: break;
        }
        float prev=t.sig;
        t.sig+=(newSig-t.sig)*12.f*dt;
        if(fabsf(t.sig-prev)>0.02f){
            for(int cid:t.conns){
                auto* dst=G.FindTower(cid);
                if(!dst) continue;
                SigPulse p;
                p.src=G.CellCenter(t.gx,t.gy);
                p.dst=G.CellCenter(dst->gx,dst->gy);
                p.col=Def(t.type).col;
                G.pulses.push_back(p);
            }
        }
    }
}

// ================================================================
//  Game Update
// ================================================================
void Update(Game& G, float dt){
    if(G.phase==Game::GAMEOVER) return;

    // ── Spawning ────────────────────────────────────────────────
    if(G.phase==Game::FIGHT && G.spawned<G.waveCount){
        G.spawnTimer+=dt;
        float interval=0.8f-G.wave*0.04f;
        if(interval<0.2f) interval=0.2f;
        if(G.spawnTimer>=interval){
            G.spawnTimer=0;
            SpawnEnemy(G);
            G.spawned++;
        }
    }

    // ── Signal propagation ──────────────────────────────────────
    if(G.phase==Game::FIGHT)
        PropagateSignals(G,dt);

    // ── Cannon cooldown & fire ───────────────────────────────────
    for(auto& t:G.towers){
        if(t.type!=TType::CANNON) continue;
        if(t.cooldown>0) t.cooldown--;
        if(t.sig<0.5f||t.cooldown>0) continue;
        // find nearest enemy within 6 cells
        Enemy* target=nullptr; float best=6.f;
        for(auto& e:G.enemies){
            float d=Dist({t.gx+0.5f,t.gy+0.5f},{e.x,e.y});
            if(d<best){ best=d; target=&e; }
        }
        if(!target) continue;
        t.cooldown=25;
        Vector2 tp=G.CellCenter(target->x-0.5f,target->y-0.5f);
        Vector2 sp=G.CellCenter(t.gx,t.gy);
        Vector2 dir=Vector2Normalize(Vector2Subtract(tp,sp));
        G.bullets.push_back({sp,{dir.x*300.f,dir.y*300.f},target->id,30.f+G.wave*3.f});
    }

    // ── Move bullets ────────────────────────────────────────────
    for(auto& b:G.bullets){
        b.pos.x+=b.vel.x*dt;
        b.pos.y+=b.vel.y*dt;
    }
    // Hit detection
    std::vector<Bullet> nextBullets;
    for(auto& b:G.bullets){
        bool hit=false;
        for(auto& e:G.enemies){
            Vector2 ep=G.CellCenter(e.x-0.5f,e.y-0.5f);
            if(Dist(b.pos,ep)<14.f){
                e.hp-=b.dmg;
                G.SpawnParticles(b.pos,COL_VIRUS,6,80.f);
                hit=true; break;
            }
        }
        // out of map
        if(!hit && b.pos.x>0 && b.pos.x<SCREEN_W && b.pos.y>0 && b.pos.y<SCREEN_H)
            nextBullets.push_back(b);
    }
    G.bullets=nextBullets;

    // ── Remove dead enemies ─────────────────────────────────────
    for(auto it=G.enemies.begin();it!=G.enemies.end();){
        if(it->hp<=0){
            G.score+=10;
            G.credits+=it->reward;
            Vector2 p=G.CellCenter(it->x-0.5f,it->y-0.5f);
            G.SpawnParticles(p,COL_VIRUS,15,100.f);
            std::ostringstream ss; ss<<"+"<<it->reward<<" CR";
            G.AddFloat(p,ss.str(),COL_AND);
            it=G.enemies.erase(it);
        } else ++it;
    }

    // ── Move enemies toward CPU ─────────────────────────────────
    for(auto& e:G.enemies){
        Vector2 target={CPU_CELL.x+0.5f,CPU_CELL.y+0.5f};
        Vector2 dir={target.x-e.x, target.y-e.y};
        float d=sqrtf(dir.x*dir.x+dir.y*dir.y);
        if(d>0.3f){
            e.x+=dir.x/d*e.spd*dt;
            e.y+=dir.y/d*e.spd*dt;
        } else {
            // reached CPU
            G.cpuHp-=8.f;
            G.SpawnParticles(G.CellCenter(CPU_CELL.x,CPU_CELL.y),COL_VIRUS,12);
            e.hp=0;
        }
        e.angle+=180.f*dt;
    }
    G.enemies.erase(std::remove_if(G.enemies.begin(),G.enemies.end(),[](const Enemy&e){return e.hp<=0;}),G.enemies.end());

    // ── Signal pulses (visual) ──────────────────────────────────
    for(auto& p:G.pulses) p.t+=dt*2.5f;
    G.pulses.erase(std::remove_if(G.pulses.begin(),G.pulses.end(),[](const SigPulse&p){return p.t>=1.f;}),G.pulses.end());
    // cap pulses
    if(G.pulses.size()>200) G.pulses.erase(G.pulses.begin(),G.pulses.begin()+G.pulses.size()-200);

    // ── Particles ───────────────────────────────────────────────
    for(auto& p:G.particles){
        p.pos.x+=p.vel.x*dt;
        p.pos.y+=p.vel.y*dt;
        p.vel.y+=60.f*dt;
        p.life-=dt;
    }
    G.particles.erase(std::remove_if(G.particles.begin(),G.particles.end(),[](const Particle&p){return p.life<=0;}),G.particles.end());

    // ── Float texts ─────────────────────────────────────────────
    for(auto& f:G.floats){ f.pos.y-=30.f*dt; f.life-=dt; }
    G.floats.erase(std::remove_if(G.floats.begin(),G.floats.end(),[](const FloatText&f){return f.life<=0;}),G.floats.end());

    // ── Wave complete? ──────────────────────────────────────────
    if(G.phase==Game::FIGHT && G.enemies.empty() && G.spawned>=G.waveCount){
        G.phase=Game::BUILD;
        G.credits+=60;
        // decay all signals
        for(auto& t:G.towers) t.sig*=0.1f;
        std::ostringstream ss;
        ss<<"波次 "<<G.wave<<" 完成！+60 CR　準備下一波 [空白鍵]";
        G.msg=ss.str();
    }

    // ── CPU dead? ────────────────────────────────────────────────
    if(G.cpuHp<=0){
        G.cpuHp=0;
        G.phase=Game::GAMEOVER;
        G.msg="⚠ CPU 被摧毀！　按 R 重新開始";
    }
}

// ================================================================
//  Drawing helpers
// ================================================================
void DrawRoundBox(float x,float y,float w,float h,float r,Color fill,Color border,float thick=1.5f){
    DrawRectangleRounded({x,y,w,h},r/std::min(w,h)*2,8,fill);
    DrawRectangleRoundedLines({x,y,w,h},r/std::min(w,h)*2,8,border);
    (void)thick;
}

void DrawHex(Vector2 c, float r, Color fill, Color border){
    // Approximate with 6 triangles
    std::vector<Vector2> pts(6);
    for(int i=0;i<6;i++){
        float a=i*60.f*DEG2RAD-30.f*DEG2RAD;
        pts[i]={c.x+r*cosf(a), c.y+r*sinf(a)};
    }
    for(int i=0;i<6;i++)
        DrawTriangle(c,pts[i],pts[(i+1)%6],fill);
    for(int i=0;i<6;i++)
        DrawLineV(pts[i],pts[(i+1)%6],border);
}

void DrawTextCentered(const char* txt, int x,int y, int sz, Color col){
    int tw=MeasureText(txt,sz);
    DrawText(txt,x-tw/2,y-sz/2,sz,col);
}

// ── Draw grid ─────────────────────────────────────────────────
void DrawGrid(Vector2 origin){
    for(int x=0;x<=COLS;x++)
        DrawLine((int)(origin.x+x*CELL),(int)origin.y,(int)(origin.x+x*CELL),(int)(origin.y+ROWS*CELL),GRID_COL);
    for(int y=0;y<=ROWS;y++)
        DrawLine((int)origin.x,(int)(origin.y+y*CELL),(int)(origin.x+COLS*CELL),(int)(origin.y+y*CELL),GRID_COL);
}

// ── Draw connections ──────────────────────────────────────────
void DrawConnections(Game& G){
    for(auto& t:G.towers){
        for(int cid:t.conns){
            auto* dst=G.FindTower(cid);
            if(!dst) continue;
            Vector2 src=G.CellCenter(t.gx,t.gy);
            Vector2 d=G.CellCenter(dst->gx,dst->gy);
            Color sc=Def(t.type).col; sc.a=120;
            DrawLineEx(src,d,2.f,sc);
            // arrowhead
            Vector2 dir=Vector2Normalize(Vector2Subtract(d,src));
            Vector2 mid={src.x+(d.x-src.x)*0.6f,src.y+(d.y-src.y)*0.6f};
            float ang=atan2f(dir.y,dir.x);
            float al=8.f,aw=0.4f;
            Vector2 a1={mid.x+al*cosf(ang+aw+3.14f),mid.y+al*sinf(ang+aw+3.14f)};
            Vector2 a2={mid.x+al*cosf(ang-aw+3.14f),mid.y+al*sinf(ang-aw+3.14f)};
            DrawTriangle(mid,a1,a2,sc);
        }
    }
}

// ── Draw signal pulses ────────────────────────────────────────
void DrawPulses(Game& G){
    for(auto& p:G.pulses){
        float t=p.t;
        Vector2 pos={p.src.x+(p.dst.x-p.src.x)*t, p.src.y+(p.dst.y-p.src.y)*t};
        Color c=p.col; c.a=(unsigned char)(255*(1.f-t));
        DrawCircleV(pos,4.f,c);
        // glow
        c.a=(unsigned char)(80*(1.f-t));
        DrawCircleV(pos,7.f,c);
    }
}

// ── Draw a single tower ───────────────────────────────────────
void DrawTower(Game& G, Tower& t, bool selected){
    Vector2 ctr=G.CellCenter(t.gx,t.gy);
    float px=(float)PANEL_L+t.gx*CELL, py=40.f+t.gy*CELL;

    if(t.type==TType::CPU){
        // CPU — glowing square
        float glow=6.f+4.f*sinf(GetTime()*3.f);
        DrawRectangle((int)px+3,(int)py+3,CELL-6,CELL-6,{5,20,30,255});
        DrawRectangleLinesEx({px+3,py+3,(float)CELL-6,(float)CELL-6},2,COL_CPU);
        // glow
        for(int i=1;i<=3;i++){
            Color gc=COL_CPU; gc.a=(unsigned char)(30-i*8);
            DrawRectangleLinesEx({px+3.f-i*glow/3,py+3.f-i*glow/3,(float)CELL-6+i*glow*2/3,(float)CELL-6+i*glow*2/3},1,gc);
        }
        DrawTextCentered("CPU",(int)ctr.x,(int)ctr.y-5,11,COL_CPU);
        char buf[8]; snprintf(buf,8,"%.0f%%",G.cpuHp);
        DrawTextCentered(buf,(int)ctr.x,(int)ctr.y+7,9,
            G.cpuHp>50?GREEN:G.cpuHp>25?ORANGE:RED);
        return;
    }

    TowerDef& def=Def(t.type);
    float alpha=0.25f+t.sig*0.75f;
    Color fill=def.col; fill.a=(unsigned char)(alpha*80);
    Color border=def.col;

    // Glow based on signal
    if(t.sig>0.2f){
        float gr=10.f+t.sig*12.f;
        for(int i=1;i<=2;i++){
            Color gc=def.col; gc.a=(unsigned char)(t.sig*30/i);
            if(t.type==TType::PERCEPTRON)
                DrawCircle((int)ctr.x,(int)ctr.y,(int)(CELL*0.5f+gr*i/2),gc);
            else
                DrawRectangle((int)(px+2-gr*i/3),(int)(py+2-gr*i/3),(int)(CELL-4+gr*i*2/3),(int)(CELL-4+gr*i*2/3),gc);
        }
    }

    if(t.type==TType::PERCEPTRON){
        DrawHex(ctr,(float)CELL*0.42f,fill,border);
    } else {
        DrawRoundBox(px+4,py+4,(float)CELL-8,(float)CELL-8,4,fill,border);
    }

    // Selected ring
    if(selected){
        DrawCircleLinesV(ctr,(float)CELL*0.55f,WHITE);
    }

    // Label
    DrawTextCentered(def.sym,(int)ctr.x,(int)ctr.y-5,11,def.col);

    // Signal bar (bottom of cell)
    int bx=(int)px+5, by=(int)py+CELL-9, bw=CELL-10, bh=5;
    DrawRectangle(bx,by,bw,bh,{0,0,0,150});
    int filled=(int)(bw*t.sig);
    if(filled>0){
        Color bc=def.col;
        DrawRectangle(bx,by,filled,bh,bc);
    }
}

// ── Draw enemies ─────────────────────────────────────────────
void DrawEnemies(Game& G){
    for(auto& e:G.enemies){
        Vector2 ctr=G.CellCenter(e.x-0.5f,e.y-0.5f);
        float r=(float)CELL*0.32f;
        float pulse=0.9f+0.1f*sinf(GetTime()*6.f+(float)e.id);
        float hpR=e.hp/e.maxHp;

        // Body
        Color body=COL_VIRUS; body.a=180;
        DrawCircleV(ctr,r*pulse,body);
        // Core
        Color core={220,20,60,200};
        DrawCircleV(ctr,r*0.55f*pulse,core);
        // Spikes
        for(int i=0;i<6;i++){
            float a=i*60.f*DEG2RAD+e.angle*DEG2RAD;
            float ir=r*0.9f*pulse, or_=r*1.3f*pulse;
            Vector2 p1={ctr.x+ir*cosf(a),ctr.y+ir*sinf(a)};
            Vector2 p2={ctr.x+or_*cosf(a),ctr.y+or_*sinf(a)};
            DrawLineEx(p1,p2,2.f,COL_VIRUS);
        }

        // HP bar
        int bx=(int)ctr.x-CELL/2+3, by=(int)ctr.y-CELL/2+2;
        DrawRectangle(bx,by,CELL-6,4,{80,0,0,200});
        DrawRectangle(bx,by,(int)((CELL-6)*hpR),4,hpR>0.5f?GREEN:hpR>0.25f?ORANGE:RED);
    }
}

// ── Draw bullets ─────────────────────────────────────────────
void DrawBullets(Game& G){
    for(auto& b:G.bullets){
        DrawCircleV(b.pos,4.f,COL_AND);
        DrawCircleV(b.pos,7.f,{250,204,21,60});
    }
}

// ── Draw particles ────────────────────────────────────────────
void DrawParticles(Game& G){
    for(auto& p:G.particles){
        float ratio=p.life/p.maxLife;
        Color c=p.col; c.a=(unsigned char)(255*ratio);
        DrawCircleV(p.pos,p.radius*ratio,c);
    }
}

// ── Draw float texts ──────────────────────────────────────────
void DrawFloats(Game& G){
    for(auto& f:G.floats){
        Color c=f.col; c.a=(unsigned char)(255.f*f.life/1.2f);
        int tw=MeasureText(f.text.c_str(),12);
        DrawText(f.text.c_str(),(int)f.pos.x-tw/2,(int)f.pos.y,12,c);
    }
}

// ── Left Panel ────────────────────────────────────────────────
void DrawLeftPanel(Game& G){
    DrawRectangle(0,0,PANEL_L,SCREEN_H,PANEL_BG);
    DrawRectangle(PANEL_L-1,0,1,SCREEN_H,PANEL_BD);

    int y=10;
    DrawText("NN DEFENSE",8,y,12,COL_PERC); y+=22;
    DrawLine(4,y,PANEL_L-4,y,PANEL_BD); y+=8;

    // Stats
    char buf[64];
    snprintf(buf,64,"Credits: %d",G.credits);
    DrawText(buf,8,y,11,COL_AND); y+=16;
    snprintf(buf,64,"Wave: %d",G.wave);
    DrawText(buf,8,y,11,COL_SENSOR); y+=16;
    snprintf(buf,64,"Score: %d",G.score);
    DrawText(buf,8,y,11,{170,130,255,255}); y+=16;
    // CPU HP bar
    DrawText("CPU HP:",8,y,10,{150,200,150,200}); y+=13;
    DrawRectangle(8,y,PANEL_L-16,8,{20,40,20,255});
    Color hpcol=G.cpuHp>50?GREEN:G.cpuHp>25?ORANGE:RED;
    DrawRectangle(8,y,(int)((PANEL_L-16)*G.cpuHp/100.f),8,hpcol);
    y+=14;

    DrawLine(4,y,PANEL_L-4,y,PANEL_BD); y+=8;
    DrawText("PLACE",8,y,10,{100,180,100,180}); y+=14;

    // Tower buttons
    static TType ORDER[]={TType::SENSOR,TType::PERCEPTRON,TType::AND,TType::OR,TType::XOR,TType::CANNON};
    for(TType tt:ORDER){
        TowerDef& def=Def(tt);
        bool active=(G.placing==tt);
        Color bg=active?AlphaOf(def.col,50):Color{10,20,30,255};
        Color bd=active?def.col:AlphaOf(def.col,100);
        DrawRoundBox(4,(float)y,(float)(PANEL_L-8),32,4,bg,bd);
        DrawTextCentered(def.sym,26,y+10,12,def.col);
        DrawText(def.label,40,y+4,11,def.col);
        char cr[16]; snprintf(cr,16,"%d CR",def.cost);
        int tw=MeasureText(cr,9);
        DrawText(cr,PANEL_L-10-tw,y+4,9,AlphaOf(COL_AND,180));
        y+=35;
    }

    y+=4;
    DrawLine(4,y,PANEL_L-4,y,PANEL_BD); y+=8;

    // Wave button
    bool canStart=(G.phase==Game::BUILD);
    Color bbg=canStart?Color{10,50,30,255}:Color{20,20,20,255};
    Color bbd=canStart?COL_PERC:Color{60,60,60,255};
    Color btxt=canStart?COL_PERC:Color{80,80,80,255};
    DrawRoundBox(4,(float)y,(float)(PANEL_L-8),34,4,bbg,bbd);
    const char* blbl=canStart?"▶ 發動波次":"⚔ 戰鬥中...";
    DrawTextCentered(blbl,PANEL_L/2,y+17,11,btxt);
    y+=38;

    // Connect hint
    if(G.connectSrc>=0){
        DrawRectangle(4,y,PANEL_L-8,24,{30,10,40,255});
        DrawRectangleLinesEx({4.f,(float)y,(float)PANEL_L-8,24},1,COL_XOR);
        DrawTextCentered("點選目標元件",PANEL_L/2,y+12,10,COL_XOR);
        y+=28;
        DrawRoundBox(4,(float)y,(float)(PANEL_L-8),24,3,{40,10,10,255},{200,100,100,200});
        DrawTextCentered("取消連線 [ESC]",PANEL_L/2,y+12,9,{200,120,120,255});
    } else if(G.selectedId>=0){
        auto* st=G.FindTower(G.selectedId);
        if(st && st->type!=TType::CPU){
            DrawRoundBox(4,(float)y,(float)(PANEL_L-8),24,3,{10,25,40,255},AlphaOf(COL_SENSOR,150));
            DrawTextCentered("C: 新增連線",PANEL_L/2,y+12,10,COL_SENSOR);
            y+=28;
            DrawRoundBox(4,(float)y,(float)(PANEL_L-8),24,3,{40,10,10,255},{200,80,80,150});
            DrawTextCentered("DEL: 移除",PANEL_L/2,y+12,10,{220,100,100,255});
        }
    }

    // Controls hint at bottom
    DrawText("[Space]=波次",6,SCREEN_H-58,9,{80,120,80,200});
    DrawText("[C]=連線  [DEL]=刪除",6,SCREEN_H-44,9,{80,120,80,200});
    DrawText("[R]=重置  [ESC]=取消",6,SCREEN_H-30,9,{80,120,80,200});
}

// ── Right Panel (Inspector) ───────────────────────────────────
void DrawRightPanel(Game& G){
    int x0=PANEL_L+MAP_W;
    DrawRectangle(x0,0,PANEL_R,SCREEN_H,PANEL_BG);
    DrawRectangle(x0,0,1,SCREEN_H,PANEL_BD);

    int x=x0+8, y=10;
    DrawText("INSPECTOR",x,y,11,COL_PERC); y+=22;
    DrawLine(x0+4,y,SCREEN_W-4,y,PANEL_BD); y+=8;

    auto* st=(G.selectedId>=0)?G.FindTower(G.selectedId):nullptr;

    if(!st){
        DrawText("點選地圖上的元件",x,y,10,{100,150,100,180}); y+=16;
        DrawText("查看/編輯屬性",x,y,10,{100,150,100,180}); y+=24;
        DrawLine(x0+4,y,SCREEN_W-4,y,PANEL_BD); y+=8;
        // Quick guide
        DrawText("建議配置:",x,y,10,AlphaOf(COL_SENSOR,200)); y+=14;
        DrawText("Sensor → Perceptron",x,y,9,{120,160,120,200}); y+=13;
        DrawText("Perceptron → Cannon",x,y,9,{120,160,120,200}); y+=13;
        DrawText("", x,y,9,WHITE); y+=13;
        DrawText("加入邏輯閘可過濾",x,y,9,{120,160,120,200}); y+=13;
        DrawText("不同種類的信號！",x,y,9,{120,160,120,200}); y+=13;
        return;
    }

    if(st->type==TType::CPU){
        DrawText("CPU 基地",x,y,12,COL_CPU); y+=18;
        char buf[64];
        snprintf(buf,64,"HP: %.0f%%",G.cpuHp);
        DrawText(buf,x,y,11,G.cpuHp>50?GREEN:G.cpuHp>25?ORANGE:RED); y+=16;
        DrawText("保護好我！",x,y,10,{150,200,150,180});
        return;
    }

    TowerDef& def=Def(st->type);
    DrawText(def.label,x,y,13,def.col); y+=18;
    // Description (word wrap simple)
    DrawText(def.desc,x,y,9,{150,180,150,200}); y+=24;

    // Signal meter
    DrawText("Signal:",x,y,10,{150,180,150,200}); y+=13;
    DrawRectangle(x,y,PANEL_R-16,8,{20,40,20,255});
    DrawRectangle(x,y,(int)((PANEL_R-16)*st->sig),8,def.col);
    char sbuf[16]; snprintf(sbuf,16,"%.3f",st->sig);
    DrawText(sbuf,x,y+10,9,def.col); y+=22;

    // Position
    char pbuf[32]; snprintf(pbuf,32,"位置: (%d, %d)",st->gx,st->gy);
    DrawText(pbuf,x,y,9,{120,150,120,200}); y+=18;

    DrawLine(x0+4,y,SCREEN_W-4,y,PANEL_BD); y+=8;

    // Perceptron weights editor
    if(st->type==TType::PERCEPTRON){
        DrawText("⬡ 感知器權重",x,y,10,COL_PERC); y+=16;

        struct Param { const char* lbl; float* val; };
        Param params[]={{"w1 ",& st->w1},{"w2 ",&st->w2},{"bias",&st->bias}};
        for(auto& p:params){
            DrawText(p.lbl,x,y,10,{150,200,150,200});
            char vbuf[12]; snprintf(vbuf,12,"%.2f",*p.val);
            int tw=MeasureText(vbuf,10);
            DrawText(vbuf,x0+PANEL_R-10-tw,y,10,COL_PERC);
            y+=13;
            // Slider bar (click to adjust)
            DrawRectangle(x,y,PANEL_R-16,6,{20,40,20,200});
            float t=(*p.val+2.f)/4.f; // map -2..2 → 0..1
            DrawRectangle(x,y,(int)((PANEL_R-16)*t),6,COL_PERC);
            // Drag handling
            Rectangle bar={x,(float)y,(float)(PANEL_R-16),10.f};
            if(IsMouseButtonDown(MOUSE_LEFT_BUTTON)){
                Vector2 mp=GetMousePosition();
                if(CheckCollisionPointRec(mp,bar)){
                    float ratio=(mp.x-(float)x)/(float)(PANEL_R-16);
                    *p.val=-2.f+ratio*4.f;
                }
            }
            y+=12;
        }
        DrawText("σ((w1·i1+w2·i2+b)×3)",x,y,8,AlphaOf(COL_PERC,140)); y+=14;
        DrawLine(x0+4,y,SCREEN_W-4,y,PANEL_BD); y+=8;
    }

    // Connections
    DrawText("連線 (Connections):",x,y,10,{150,180,150,200}); y+=14;
    // Incoming
    int inCnt=0;
    for(auto& t2:G.towers)
        for(int cid:t2.conns) if(cid==st->id) inCnt++;
    char cbuf[64];
    snprintf(cbuf,64,"輸入: %d　輸出: %d",inCnt,(int)st->conns.size());
    DrawText(cbuf,x,y,9,{120,160,120,200}); y+=14;

    // List outgoing
    for(int cid:st->conns){
        auto* dst=G.FindTower(cid);
        if(!dst) continue;
        char lbuf[48];
        snprintf(lbuf,48,"→ %s (%d,%d)",Def(dst->type).label,dst->gx,dst->gy);
        DrawText(lbuf,x+4,y,9,Def(dst->type).col); y+=12;
    }

    y+=4;
    DrawText("[C] 新增連線",x,y,9,AlphaOf(COL_SENSOR,180)); y+=13;
    DrawText("[DEL] 移除元件",x,y,9,{200,100,100,180});
}

// ── Top bar ───────────────────────────────────────────────────
void DrawTopBar(Game& G){
    DrawRectangle(0,0,SCREEN_W,40,{4,12,22,255});
    DrawLine(0,40,SCREEN_W,40,PANEL_BD);
    DrawText(G.msg.c_str(),PANEL_L+8,12,11,G.phase==Game::GAMEOVER?RED:AlphaOf(COL_PERC,220));
    // Phase badge
    const char* ph=G.phase==Game::BUILD?"BUILD":G.phase==Game::FIGHT?"FIGHT":"GAME OVER";
    Color phcol=G.phase==Game::BUILD?COL_SENSOR:G.phase==Game::FIGHT?COL_CANNON:RED;
    int tw=MeasureText(ph,11);
    DrawRectangle(SCREEN_W-PANEL_R-tw-16,8,tw+12,22,AlphaOf(phcol,40));
    DrawText(ph,SCREEN_W-PANEL_R-tw-10,12,11,phcol);
}

// ── Hover highlight (ghost tower placement) ───────────────────
void DrawGhostTower(Game& G, int gx, int gy){
    if(G.placing==TType::NONE) return;
    if(gx<0||gx>=COLS||gy<0||gy>=ROWS) return;
    if(G.TowerAt(gx,gy)) return;
    TowerDef& def=Def(G.placing);
    float px=(float)PANEL_L+gx*CELL, py=40.f+gy*CELL;
    Color fill=def.col; fill.a=40;
    Color bd=def.col; bd.a=150;
    DrawRoundBox(px+4,py+4,(float)CELL-8,(float)CELL-8,4,fill,bd);
    DrawTextCentered(def.sym,(int)(px+CELL/2),(int)(py+CELL/2-4),11,bd);
}

// ================================================================
//  Input
// ================================================================
void HandleInput(Game& G){
    Vector2 mp=GetMousePosition();
    int hgx=-1,hgy=-1;
    bool onMap=ScreenToGrid(G,mp,hgx,hgy);

    // Left panel button regions
    // Tower buttons: y starts at ~80 (offset from top)
    // Rough Y of each button row
    static TType ORDER[]={TType::SENSOR,TType::PERCEPTRON,TType::AND,TType::OR,TType::XOR,TType::CANNON};
    int btnY=80; // approx, must match DrawLeftPanel
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && mp.x<PANEL_L){
        // Tower buttons
        for(int i=0;i<6;i++){
            int y=btnY+i*35;
            if(mp.y>=y && mp.y<y+32){
                TType tt=ORDER[i];
                G.placing=(G.placing==tt)?TType::NONE:tt;
                G.selectedId=-1;
                G.connectSrc=-1;
                return;
            }
        }
        // Wave button
        int waveY=btnY+6*35+12;
        if(mp.y>=waveY && mp.y<waveY+34 && G.phase==Game::BUILD){
            StartWave(G);
            return;
        }
    }

    // Map click
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && onMap){
        // Connect mode: pick destination
        if(G.connectSrc>=0){
            Tower* dst=G.TowerAt(hgx,hgy);
            if(dst && dst->id!=G.connectSrc){
                Tower* src=G.FindTower(G.connectSrc);
                if(src){
                    // avoid duplicate
                    if(std::find(src->conns.begin(),src->conns.end(),dst->id)==src->conns.end())
                        src->conns.push_back(dst->id);
                    G.msg="連線完成！";
                }
            }
            G.connectSrc=-1;
            return;
        }

        Tower* existing=G.TowerAt(hgx,hgy);
        if(existing){
            G.selectedId=existing->id;
            G.placing=TType::NONE;
        } else if(G.placing!=TType::NONE){
            // Place tower
            if(G.credits>=Def(G.placing).cost){
                Tower t;
                t.id=G.nextId++;
                t.type=G.placing;
                t.gx=hgx; t.gy=hgy;
                t.sig=0; t.w1=0.8f; t.w2=0.6f; t.bias=-0.5f;
                t.cooldown=0;
                G.towers.push_back(t);
                G.credits-=Def(G.placing).cost;
                G.msg=std::string("放置 ")+Def(G.placing).label;
            } else {
                G.msg="Credits 不足！";
            }
        } else {
            G.selectedId=-1;
        }
    }

    // Keyboard shortcuts
    if(IsKeyPressed(KEY_SPACE)){
        if(G.phase==Game::BUILD) StartWave(G);
    }
    if(IsKeyPressed(KEY_ESCAPE)){
        G.placing=TType::NONE;
        G.connectSrc=-1;
    }
    if(IsKeyPressed(KEY_C) && G.selectedId>=0){
        auto* st=G.FindTower(G.selectedId);
        if(st && st->type!=TType::CPU){
            G.connectSrc=G.selectedId;
            G.msg="點選要連接的目標元件...";
        }
    }
    if(IsKeyPressed(KEY_DELETE) && G.selectedId>=0){
        // Remove tower (not CPU)
        auto* st=G.FindTower(G.selectedId);
        if(st && st->type!=TType::CPU){
            int rid=G.selectedId;
            G.towers.erase(std::remove_if(G.towers.begin(),G.towers.end(),[rid](const Tower&t){return t.id==rid;}),G.towers.end());
            for(auto& t:G.towers)
                t.conns.erase(std::remove(t.conns.begin(),t.conns.end(),rid),t.conns.end());
            G.selectedId=-1;
            G.msg="元件已移除";
        }
    }
    if(IsKeyPressed(KEY_R)){
        // Restart
        G.towers.clear();
        G.enemies.clear();
        G.bullets.clear();
        G.pulses.clear();
        G.particles.clear();
        G.floats.clear();
        G.nextId=1;
        G.credits=300;
        G.score=0;
        G.wave=0;
        G.cpuHp=100.f;
        G.phase=Game::BUILD;
        G.placing=TType::NONE;
        G.selectedId=-1;
        G.connectSrc=-1;
        G.spawned=0;
        G.msg="遊戲重置！選擇元件放置，按 [空白鍵] 開始波次";
        // Re-add CPU
        G.towers.push_back({0,TType::CPU,(int)CPU_CELL.x,(int)CPU_CELL.y,1,1,1,0,0,{}});
        G.nextId=1;
    }

    // Hover ghost for placement
    (void)hgx; (void)hgy;
}

// ================================================================
//  Main
// ================================================================
int main(){
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "邏輯閘防禦：神經網路塔防  v1.0");
    SetTargetFPS(60);

    Game G;
    // Place CPU
    G.towers.push_back({0, TType::CPU, (int)CPU_CELL.x, (int)CPU_CELL.y, 1,1,1,0,0,{}});

    while(!WindowShouldClose()){
        float dt=GetFrameTime();
        dt=std::min(dt,0.05f);  // cap to 50ms

        HandleInput(G);
        Update(G,dt);

        // Hover grid cell
        Vector2 mp=GetMousePosition();
        int hgx=-1,hgy=-1;
        ScreenToGrid(G,mp,hgx,hgy);

        BeginDrawing();
        ClearBackground(BG);

        Vector2 origin=G.MapOrigin();

        // Map area scissor
        BeginScissorMode(PANEL_L,40,MAP_W,MAP_H);
        DrawGrid(origin);
        DrawConnections(G);
        DrawPulses(G);
        for(auto& t:G.towers)
            DrawTower(G,t,t.id==G.selectedId);
        DrawGhostTower(G,hgx,hgy);
        DrawEnemies(G);
        DrawBullets(G);
        DrawParticles(G);
        DrawFloats(G);
        EndScissorMode();

        DrawLeftPanel(G);
        DrawRightPanel(G);
        DrawTopBar(G);

        // GAME OVER overlay
        if(G.phase==Game::GAMEOVER){
            DrawRectangle(PANEL_L,40,MAP_W,MAP_H,{5,0,0,180});
            DrawTextCentered("CPU 已摧毀",PANEL_L+MAP_W/2,SCREEN_H/2-30,32,RED);
            char sbuf[64]; snprintf(sbuf,64,"最終分數: %d",G.score);
            DrawTextCentered(sbuf,PANEL_L+MAP_W/2,SCREEN_H/2+10,20,COL_AND);
            DrawTextCentered("按 R 重新開始",PANEL_L+MAP_W/2,SCREEN_H/2+44,16,{200,200,200,220});
        }

        // FPS (debug, remove if you like)
        // DrawFPS(4, SCREEN_H-18);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
