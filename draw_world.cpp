// ================================================================
//  draw_world.cpp — 世界層繪圖函式
//  DrawRoundBox, DrawHex, DrawStars, DrawPath, DrawThreatMap,
//  DrawAIHints, DrawConnections, DrawPulses,
//  DrawTower, DrawGhostTower, DrawEnemies,
//  DrawBullets, DrawParticles, DrawFloats
// ================================================================
#include "draw.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

static const char* DrawEnemyTagName(EnemyTag tag) {
    switch (tag) {
        case EnemyTag::BRUTE:  return "BRUTE";
        case EnemyTag::SWIFT:  return "SWIFT";
        case EnemyTag::BOUNTY: return "BOUNTY";
        default:               return "";
    }
}

static Color DrawEnemyTagColor(EnemyTag tag) {
    switch (tag) {
        case EnemyTag::BRUTE:  return Color{255, 120, 120, 255};
        case EnemyTag::SWIFT:  return Color{120, 255, 200, 255};
        case EnemyTag::BOUNTY: return Color{255, 220, 90, 255};
        default:               return WHITE;
    }
}

Color LerpColor(Color a, Color b, float t) {
    t = Clamp(t, 0.f, 1.f);
    return Color{
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

static Vector2 RouteAdvanceDir(PathEntrySide side) {
    switch (side) {
        case PathEntrySide::LEFT:   return { 1.f,  0.f};
        case PathEntrySide::TOP:    return { 0.f,  1.f};
        case PathEntrySide::RIGHT:  return {-1.f,  0.f};
        case PathEntrySide::BOTTOM: return { 0.f, -1.f};
    }
    return {1.f, 0.f};
}

static const char* RouteSideLabel(PathEntrySide side) {
    switch (side) {
        case PathEntrySide::LEFT:   return "西側入口";
        case PathEntrySide::TOP:    return "北側入口";
        case PathEntrySide::RIGHT:  return "東側入口";
        case PathEntrySide::BOTTOM: return "南側入口";
    }
    return "入口";
}

static const char* RouteShortLabel(PathEntrySide side) {
    switch (side) {
        case PathEntrySide::LEFT:   return "WEST";
        case PathEntrySide::TOP:    return "NORTH";
        case PathEntrySide::RIGHT:  return "EAST";
        case PathEntrySide::BOTTOM: return "SOUTH";
    }
    return "ROUTE";
}

RouteVisualTheme GetRouteTheme(const PathPreset& preset, int laneSlot) {
    Color accent{};
    Color glow{};
    Color fill{};

    switch (preset.entrySide) {
        case PathEntrySide::LEFT:
            accent = {  70, 220, 255, 255};
            glow   = { 150, 245, 255, 255};
            fill   = {   8,  34,  52, 255};
            break;
        case PathEntrySide::TOP:
            accent = { 255, 196,  84, 255};
            glow   = { 255, 229, 145, 255};
            fill   = {  54,  32,  12, 255};
            break;
        case PathEntrySide::RIGHT:
            accent = { 220, 118, 255, 255};
            glow   = { 244, 184, 255, 255};
            fill   = {  34,  17,  52, 255};
            break;
        case PathEntrySide::BOTTOM:
            accent = { 255, 110,  96, 255};
            glow   = { 255, 168, 150, 255};
            fill   = {  52,  16,  20, 255};
            break;
    }

    float familyMix = 0.05f + 0.04f * (float)(preset.family % 3);
    Color familyTint = (preset.family % 2 == 0) ? COL_AI : COL_STAR;

    RouteVisualTheme theme{};
    theme.accent    = LerpColor(accent, familyTint, familyMix);
    theme.glow      = LerpColor(glow, WHITE, 0.06f * (float)std::max(1, preset.dualWeight - 1));
    theme.fill      = LerpColor(fill, accent, 0.08f + 0.02f * (float)preset.dualWeight);
    theme.fillSoft  = AlphaOf(LerpColor(theme.fill, BG, 0.45f), 255);
    theme.text      = LerpColor(theme.glow, WHITE, 0.12f);
    theme.sideLabel = RouteSideLabel(preset.entrySide);
    theme.shortLabel= RouteShortLabel(preset.entrySide);

    static const Color LANE_TINTS[MAX_LANES] = {
        Color{ 70, 220, 255, 255},
        Color{255, 230, 120, 255},
        Color{255, 125, 190, 255},
        Color{125, 255, 165, 255},
        Color{170, 145, 255, 255},
        Color{255, 155,  90, 255}
    };
    int lane = std::max(0, std::min(MAX_LANES - 1, laneSlot));
    if (lane > 0) {
        float tintMix = 0.08f + 0.03f * (float)(lane % 3);
        theme.accent   = LerpColor(theme.accent, LANE_TINTS[lane], tintMix);
        theme.glow     = LerpColor(theme.glow, LANE_TINTS[lane], tintMix * 0.6f);
        theme.fill     = LerpColor(theme.fill, BG, 0.08f + 0.03f * (float)lane);
        theme.fillSoft = AlphaOf(LerpColor(theme.fillSoft, BG, 0.08f + 0.02f * (float)lane), 255);
    }

    return theme;
}

// ══════════════════════════════════════════════════════════════════
//  繪圖輔助
// ══════════════════════════════════════════════════════════════════
void DrawRoundBox(float x, float y, float w, float h, float r,
                  Color fill, Color border, float bw) {
    float rr = r / std::min(w, h) * 2;
    DrawRectangleRounded({x, y, w, h}, rr, 8, fill);
    DrawRectangleRoundedLines({x, y, w, h}, rr, 8, bw, border);
}

void DrawHex(Vector2 c, float r, Color fill, Color border) {
    Vector2 pts[6];
    for (int i = 0; i < 6; i++) {
        float a = i * 60.f * DEG2RAD - 30.f * DEG2RAD;
        pts[i] = { c.x + r * cosf(a), c.y + r * sinf(a) };
    }
    for (int i = 0; i < 6; i++) DrawTriangle(c, pts[i], pts[(i + 1) % 6], fill);
    for (int i = 0; i < 6; i++) DrawLineV(pts[i], pts[(i + 1) % 6], border);
}

static void DrawCornerBrackets(float x, float y, float w, float h, float len, Color col) {
    DrawLineEx({x, y}, {x + len, y}, 2.f, col);
    DrawLineEx({x, y}, {x, y + len}, 2.f, col);
    DrawLineEx({x + w, y}, {x + w - len, y}, 2.f, col);
    DrawLineEx({x + w, y}, {x + w, y + len}, 2.f, col);
    DrawLineEx({x, y + h}, {x + len, y + h}, 2.f, col);
    DrawLineEx({x, y + h}, {x, y + h - len}, 2.f, col);
    DrawLineEx({x + w, y + h}, {x + w - len, y + h}, 2.f, col);
    DrawLineEx({x + w, y + h}, {x + w, y + h - len}, 2.f, col);
}

static void DrawScanSweep(float x, float y, float w, float h, float phase, Color col) {
    float sweep = fmodf(phase, 1.f);
    float sy = y + sweep * h;
    DrawRectangleGradientV((int)x, (int)(sy - 18.f), (int)w, 36,
        AlphaOf(col, 0), AlphaOf(col, 30));
    DrawLineEx({x + 18.f, sy}, {x + w - 18.f, sy}, 1.6f, AlphaOf(col, 86));
    DrawLineEx({x + 48.f, sy + 5.f}, {x + w - 110.f, sy + 5.f}, 1.f, AlphaOf(WHITE, 28));
}

static void DrawMicroTicks(float x, float y, float w, float h, Color col) {
    for (int i = 0; i < 4; i++) {
        float px = x + 12.f + i * 18.f;
        DrawLineEx({px, y + 8.f}, {px + 8.f, y + 8.f}, 1.f, AlphaOf(col, 62));
        DrawLineEx({x + w - 12.f - i * 18.f, y + h - 8.f}, {x + w - 20.f - i * 18.f, y + h - 8.f}, 1.f, AlphaOf(col, 46));
    }
}

static void DrawWorldBackdrop(Vector2 o) {
    float t = (float)GetTime();
    DrawRectangleGradientV((int)o.x, (int)o.y, MAP_W, MAP_H,
        Color{5, 12, 24, 255}, Color{1, 4, 10, 255});
    DrawRectangle((int)o.x, (int)o.y, MAP_W, MAP_H, AlphaOf(COL_CPU, 5));

    for (int x = 0; x < MAP_W; x += CELL * 2) {
        int alpha = ((x / (CELL * 2)) % 3 == 0) ? 22 : 11;
        DrawLine((int)o.x + x, (int)o.y, (int)o.x + x, (int)o.y + MAP_H, AlphaOf(COL_SENSOR, alpha));
    }
    for (int y = 0; y < MAP_H; y += CELL * 2) {
        int alpha = ((y / (CELL * 2)) % 3 == 0) ? 18 : 9;
        DrawLine((int)o.x, (int)o.y + y, (int)o.x + MAP_W, (int)o.y + y, AlphaOf(COL_SENSOR, alpha));
    }

    for (int y = CELL; y < MAP_H; y += CELL * 4) {
        float fy = o.y + (float)y;
        DrawLineEx({o.x + 28.f, fy}, {o.x + MAP_W - 28.f, fy + 18.f}, 1.2f, AlphaOf(COL_AI, 18));
    }

    for (int i = 0; i < 5; i++) {
        float y = o.y + fmodf(t * (24.f + i * 7.f) + i * 173.f, (float)MAP_H);
        DrawLineEx({o.x + 40.f, y}, {o.x + MAP_W - 40.f, y + 26.f}, 1.f, AlphaOf(COL_SENSOR, 10 + i * 2));
    }

    float nodePulse = 0.55f + 0.45f * sinf(t * 1.8f);
    for (int i = 0; i < 7; i++) {
        float nx = o.x + 110.f + i * 168.f;
        float ny = o.y + 90.f + fmodf(i * 137.f + t * 18.f, (float)MAP_H - 180.f);
        DrawCircleV({nx, ny}, 3.f, AlphaOf(COL_AI, 34 + (int)(34 * nodePulse)));
        DrawCircleLinesV({nx, ny}, 10.f + nodePulse * 4.f, AlphaOf(COL_AI, 18));
        if (i > 0) DrawLineEx({nx - 168.f, ny - 28.f}, {nx, ny}, 1.f, AlphaOf(COL_SENSOR, 12));
    }

    DrawScanSweep(o.x, o.y, (float)MAP_W, (float)MAP_H, fmodf(t * 0.055f, 1.f), COL_SENSOR);

    DrawRectangleLinesEx({o.x + 1.f, o.y + 1.f, (float)MAP_W - 2.f, (float)MAP_H - 2.f}, 2.f, AlphaOf(COL_CPU, 58));
    DrawCornerBrackets(o.x + 9.f, o.y + 9.f, (float)MAP_W - 18.f, (float)MAP_H - 18.f, 34.f, AlphaOf(COL_SENSOR, 68));
}

// ══════════════════════════════════════════════════════════════════
//  DrawStars
// ══════════════════════════════════════════════════════════════════
void DrawStars(Game& G) {
    float t = (float)GetTime();
    for (auto& s : G.stars) {
        float b = s.bright * (0.7f + 0.3f * sinf(t * 1.1f + s.x));
        Color c = {
            (unsigned char)(150 * b),
            (unsigned char)(170 * b),
            (unsigned char)(255 * b),
            255
        };
        DrawCircle((int)s.x, (int)s.y, s.r, c);
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawPath
// ══════════════════════════════════════════════════════════════════
void DrawPath(Game& G) {
    auto  o = G.MapOrigin();
    float t = (float)GetTime();
    DrawWorldBackdrop(o);

    auto laneEnemyCount = [&](int laneSlot) {
        int count = 0;
        for (auto& e : G.enemies) if (e.pathIdx == laneSlot) count++;
        return count;
    };

    auto lanePressure = [&](int laneSlot) {
        const auto& cells = G.LaneCells(laneSlot);
        if (cells.empty()) return 0.f;
        float furthest = 0.f;
        for (auto& e : G.enemies) {
            if (e.pathIdx != laneSlot) continue;
            furthest = std::max(furthest, e.pathPos / (float)std::max(1, (int)cells.size()));
        }
        return Clamp(furthest, 0.f, 1.f);
    };

    constexpr float routeCardW = 308.f;
    constexpr float routeCardH = 46.f;
    constexpr float routeCardGapX = 18.f;
    constexpr float routeCardGapY = 58.f;
    constexpr float routeTextX = 106.f;

    auto drawRouteCard = [&](float x, float y, const char* header, const PathPreset& preset,
                              int laneSlot, float pulse, const char* detail) {
        RouteVisualTheme theme = GetRouteTheme(preset, laneSlot);
        DrawRoundBox(x, y, routeCardW, routeCardH, 9.f,
            AlphaOf(theme.fillSoft, 210),
            AlphaOf(theme.accent, (int)(110 + 70 * pulse)), 1.5f);
        DrawRectangleGradientH((int)x + 2, (int)y + 2, (int)routeCardW - 4, (int)routeCardH - 4,
            AlphaOf(theme.accent, 10), AlphaOf(theme.fill, 2));
        DrawLineEx({x + 16.f, y + 16.f}, {x + 40.f, y + 16.f}, 1.2f, AlphaOf(theme.accent, 96));
        DTX(header, x + 14.f, y + 8.f, FS_TINY, AlphaOf(theme.accent, 224));
        DTX(preset.name, x + routeTextX, y + 9.f, FS_SMALL, theme.text);
        DTX(detail, x + routeTextX, y + 27.f, FS_TINY, AlphaOf(theme.glow, 156));
        DTX(theme.shortLabel, x + 14.f, y + 27.f, FS_TINY, AlphaOf(theme.glow, 194));
    };

    // ── 下波路線預覽 ─────────────────────────────────────────────
    bool showPreview = (G.phase == Game::BUILD || G.phase == Game::TRAINING)
                    && G.hasPlannedRouteChange
                    && (G.wave > 0) && (G.wave % 3 == 0);

    if (showPreview) {
        float pulse = 0.55f + 0.45f * sinf(t * 3.2f);
        auto drawPreviewLane = [&](int presetIdx, int laneSlot) {
            const PathPreset& previewPreset = GetPathPreset(presetIdx);
            RouteVisualTheme  theme = GetRouteTheme(previewPreset, laneSlot);
            const auto& previewCells = G.nextPreviewCells[laneSlot];
            for (auto& pc : previewCells) {
                float px = o.x + pc.gx * CELL;
                float py = o.y + pc.gy * CELL;
                DrawRectangle((int)px + 2, (int)py + 2, CELL - 4, CELL - 4, AlphaOf(theme.fill, (int)(70 + pulse * 45)));
                DrawRectangle((int)px + 8, (int)py + 8, CELL - 16, CELL - 16, AlphaOf(theme.accent, (int)(18 + pulse * 30)));
                DrawRectangleLinesEx({px + 1.f, py + 1.f, (float)CELL - 2.f, (float)CELL - 2.f}, 2.f,
                    AlphaOf(theme.accent, (int)(140 + pulse * 80)));
            }
        };

        int previewCount = std::max(1, std::min(MAX_LANES, G.nextPreviewLaneCount));
        for (int lane = 0; lane < previewCount; lane++) {
            if (G.nextPreviewPaths[lane] >= 0) drawPreviewLane(G.nextPreviewPaths[lane], lane);
        }

        int cols = (previewCount <= 2) ? previewCount : 3;
        int rows = (previewCount + cols - 1) / cols;
        float gridW = cols * routeCardW + (cols - 1) * routeCardGapX;
        float cardY = o.y + MAP_H - 30.f - routeCardH - (rows - 1) * routeCardGapY;
        DTC("下波輪換預告", (int)(o.x + MAP_W * 0.5f), (int)(cardY - 16.f), FS_SMALL,
            AlphaOf({255, 220, 150, 255}, (int)(200 + pulse * 40)));

        for (int lane = 0; lane < previewCount; lane++) {
            if (G.nextPreviewPaths[lane] < 0) continue;
            char header[24];
            snprintf(header, 24, "下波路線%d", lane + 1);
            int col = lane % cols;
            int row = lane / cols;
            const PathPreset& preset = GetPathPreset(G.nextPreviewPaths[lane]);
            drawRouteCard(o.x + MAP_W * 0.5f - gridW * 0.5f + col * (routeCardW + routeCardGapX),
                cardY + row * routeCardGapY, header, preset, lane, pulse,
                GetRouteTheme(preset, lane).sideLabel);
        }
    }

    auto drawLane = [&](int laneSlot) {
        if (!G.IsLaneActive(laneSlot) || G.LaneCells(laneSlot).empty()) return;

        const auto&       cells  = G.LaneCells(laneSlot);
        const PathPreset& preset = G.LanePreset(laneSlot);
        RouteVisualTheme  theme  = GetRouteTheme(preset, laneSlot);
        float             pulse  = 0.55f + 0.45f * sinf(t * 2.4f + preset.family * 0.45f + laneSlot * 0.6f);
        int               count  = laneEnemyCount(laneSlot);
        float             pressure = lanePressure(laneSlot);

        for (auto& pc : cells) {
            float px = o.x + pc.gx * CELL;
            float py = o.y + pc.gy * CELL;
            DrawRectangleRounded({px + 1.f, py + 1.f, (float)CELL - 2.f, (float)CELL - 2.f}, 0.18f, 8, AlphaOf(theme.fill, 215));
            DrawRectangleRounded({px + 5.f, py + 5.f, (float)CELL - 10.f, (float)CELL - 10.f}, 0.20f, 8, AlphaOf(theme.fillSoft, 245));
            DrawRectangleRoundedLines({px + 6.f, py + 6.f, (float)CELL - 12.f, (float)CELL - 12.f}, 0.22f, 8, 1.f, AlphaOf(theme.glow, 40));

            switch (preset.entrySide) {
                case PathEntrySide::LEFT:
                    DrawRectangle((int)px + 3, (int)py + 4, 6, CELL - 8, AlphaOf(theme.accent, 180));
                    break;
                case PathEntrySide::TOP:
                    DrawRectangle((int)px + 4, (int)py + 3, CELL - 8, 6, AlphaOf(theme.accent, 180));
                    break;
                case PathEntrySide::RIGHT:
                    DrawRectangle((int)px + CELL - 9, (int)py + 4, 6, CELL - 8, AlphaOf(theme.accent, 180));
                    break;
                case PathEntrySide::BOTTOM:
                    DrawRectangle((int)px + 4, (int)py + CELL - 9, CELL - 8, 6, AlphaOf(theme.accent, 180));
                    break;
            }

            if (((pc.gx + pc.gy + preset.family) % 5) == 0) {
                DrawLineEx({px + 11.f, py + CELL - 10.f}, {px + CELL - 10.f, py + 11.f}, 2.f,
                    AlphaOf(theme.glow, 32));
            }

            if (((pc.gx * 2 + pc.gy + laneSlot) % 6) == 0) {
                DrawLineEx({px + 12.f, py + 14.f}, {px + CELL - 12.f, py + 14.f}, 1.f, AlphaOf(theme.accent, 46));
                DrawLineEx({px + 12.f, py + CELL - 14.f}, {px + CELL - 12.f, py + CELL - 14.f}, 1.f, AlphaOf(theme.glow, 24));
            }

            DrawCircle((int)(px + CELL * 0.5f), (int)(py + CELL * 0.5f), 2.4f, AlphaOf(theme.accent, 96));
            DrawRectangleLinesEx({px + 1.f, py + 1.f, (float)CELL - 2.f, (float)CELL - 2.f}, 1.5f,
                AlphaOf(theme.glow, 78));
        }

        for (int wi = 0; wi + 1 < preset.count; wi++) {
            int x0 = preset.wx[wi],     y0 = preset.wy[wi];
            int x1 = preset.wx[wi + 1], y1 = preset.wy[wi + 1];
            if (x0 < 0 || x0 >= COLS || y0 < 0 || y0 >= ROWS) continue;
            Vector2 p0 = {o.x + (x0 + 0.5f) * CELL, o.y + (y0 + 0.5f) * CELL};
            Vector2 p1 = {o.x + (x1 + 0.5f) * CELL, o.y + (y1 + 0.5f) * CELL};
            DrawLineEx(p0, p1, CELL - 10.f, AlphaOf(theme.fill, 68));
            DrawLineEx(p0, p1, 12.f, AlphaOf(theme.accent, 72));
            DrawLineEx(p0, p1, 4.f, AlphaOf(theme.glow, 165));

            Vector2 dir = Vector2Normalize(Vector2Subtract(p1, p0));
            Vector2 mid = {
                p0.x + (p1.x - p0.x) * (0.30f + 0.35f * fmodf(t * 0.55f + wi * 0.17f + laneSlot * 0.12f, 1.f)),
                p0.y + (p1.y - p0.y) * (0.30f + 0.35f * fmodf(t * 0.55f + wi * 0.17f + laneSlot * 0.12f, 1.f))
            };
            float ang = atan2f(dir.y, dir.x);
            float al = 13.f;
            float aw = 0.55f;
            Color ac = AlphaOf(theme.glow, (int)(105 + pulse * 85));
            DrawTriangle(mid,
                {mid.x + al * cosf(ang + aw + PI), mid.y + al * sinf(ang + aw + PI)},
                {mid.x + al * cosf(ang - aw + PI), mid.y + al * sinf(ang - aw + PI)}, ac);
        }

        Vector2 entryCenter = {o.x + (cells.front().gx + 0.5f) * CELL, o.y + (cells.front().gy + 0.5f) * CELL};
        Vector2 entryDir = RouteAdvanceDir(preset.entrySide);
        Vector2 entryOuter = {entryCenter.x - entryDir.x * CELL * 0.75f, entryCenter.y - entryDir.y * CELL * 0.75f};
        Vector2 entryTip = {entryCenter.x - entryDir.x * 14.f, entryCenter.y - entryDir.y * 14.f};
        float entryAng = atan2f(entryDir.y, entryDir.x);
        DrawLineEx(entryOuter, entryCenter, 6.f, AlphaOf(theme.accent, (int)(95 + pulse * 70)));
        DrawCircleLinesV(entryCenter, CELL * 0.40f + pulse * 6.f, AlphaOf(theme.glow, 150));
        DrawTriangle(entryTip,
            {entryTip.x - 14.f * cosf(entryAng - 0.55f), entryTip.y - 14.f * sinf(entryAng - 0.55f)},
            {entryTip.x - 14.f * cosf(entryAng + 0.55f), entryTip.y - 14.f * sinf(entryAng + 0.55f)},
            AlphaOf(theme.accent, 210));

        float tagW = MCN(theme.sideLabel, FS_TINY) + 34.f;
        float tagX = entryCenter.x - tagW * 0.5f;
        float tagY = entryCenter.y - CELL * 0.84f;
        DrawRoundBox(tagX, tagY, tagW, 24.f, 7.f, AlphaOf(theme.fill, 228), AlphaOf(theme.accent, 150), 1.2f);
        DTC(theme.sideLabel, (int)entryCenter.x, (int)(tagY + 12.f), FS_TINY, theme.text);

        Vector2 cpu = G.CC(CPU_GX, CPU_GY);
        Vector2 end = {o.x + (cells.back().gx + 0.5f) * CELL, o.y + (cells.back().gy + 0.5f) * CELL};
        float breach = Clamp(pressure * 0.85f + count * 0.06f, 0.f, 1.f);
        DrawLineEx(end, cpu, 10.f, AlphaOf(theme.accent, (int)(28 + breach * 110)));
        if (count > 0 || G.phase == Game::FIGHT) {
            float ringR = CELL * 0.42f + laneSlot * 7.f + breach * 10.f;
            DrawCircleLinesV(cpu, ringR, AlphaOf(theme.glow, (int)(45 + breach * 150 * pulse)));
        }
        if (count > 0) {
            char cb[16];
            snprintf(cb, 16, "x%d", count);
            DrawRoundBox(end.x - 21.f, end.y - 13.f, 42.f, 20.f, 6.f,
                AlphaOf(theme.fill, 220), AlphaOf(theme.accent, 158), 1.2f);
            DTC(cb, (int)end.x, (int)end.y - 2, FS_TINY, theme.text);
        }
    };

    for (int lane = 0; lane < G.ActiveLaneCount(); lane++) drawLane(lane);

    if (G.phase == Game::FIGHT && (G.waveTelegraphTimer > 0.f || G.spawnPulseTimer > 0.f)) {
        auto drawSpawnLane = [&](int laneSlot, float strength) {
            if (!G.IsLaneActive(laneSlot) || G.LaneCells(laneSlot).empty()) return;
            const auto&      cells = G.LaneCells(laneSlot);
            RouteVisualTheme theme = GetRouteTheme(G.LanePreset(laneSlot), laneSlot);
            int count = std::min(5, (int)cells.size());
            for (int i = 0; i < count; i++) {
                float px = o.x + cells[i].gx * CELL;
                float py = o.y + cells[i].gy * CELL;
                DrawRectangle((int)px + 2, (int)py + 2, CELL - 4, CELL - 4, AlphaOf(theme.accent, (int)(48 * strength)));
                DrawRectangleLinesEx({px + 1.f, py + 1.f, (float)CELL - 2.f, (float)CELL - 2.f}, 2.f,
                    AlphaOf(theme.glow, (int)(170 * strength)));
            }
            Vector2 entryCenter = {o.x + (cells.front().gx + 0.5f) * CELL, o.y + (cells.front().gy + 0.5f) * CELL};
            DrawCircleLinesV(entryCenter, CELL * 0.48f + 6.f * strength, AlphaOf(theme.glow, (int)(180 * strength)));
        };

        float telegraphPulse = 0.65f + 0.35f * sinf(t * 7.f);
        float wavePulse = std::max(0.f, G.waveTelegraphTimer) * 0.5f + std::max(0.f, G.spawnPulseTimer) * 2.5f;
        float strength = std::min(1.f, telegraphPulse * wavePulse);
        if (G.waveTelegraphTimer > 0.f) {
            for (int lane = 0; lane < G.ActiveLaneCount(); lane++) {
                drawSpawnLane(lane, strength * std::max(0.60f, 1.f - lane * 0.06f));
            }
        } else {
            int lane = std::max(0, std::min(G.ActiveLaneCount() - 1, G.spawnPulsePath));
            drawSpawnLane(lane, strength);
        }
    }

    // ── BLACKOUT 警示邊框 ────────────────────────────────────────
    if (G.blackoutActive) {
        float flicker=0.5f+0.5f*sinf(t*9.f);
        DrawRectangleLinesEx({(float)PANEL_L,(float)TOPBAR_H,(float)MAP_W,(float)MAP_H},
            4.f,{255,60,60,(unsigned char)(120*flicker)});
        int sensorLoss = (G.ActiveLaneCount() >= 6) ? 45 : (G.ActiveLaneCount() >= 4 ? 55 : 70);
        char blackoutMsg[64];
        snprintf(blackoutMsg, 64, "通訊中斷！感測器效能 -%d%%", sensorLoss);
        DTC(blackoutMsg,PANEL_L+MAP_W/2,TOPBAR_H+24,FS_SMALL,
            AlphaOf({255,100,100,255},(int)(220*flicker)));
    }

    // ── 格線 ─────────────────────────────────────────────────────
    for (int x = 0; x <= COLS; x++) {
        Color c=AlphaOf(LerpColor(COL_SENSOR, COL_CPU, 0.35f),(x%5==0)?20:9);
        DrawLine((int)(o.x+x*CELL),(int)o.y,(int)(o.x+x*CELL),(int)(o.y+ROWS*CELL),c);
    }
    for (int y = 0; y <= ROWS; y++) {
        Color c=AlphaOf(LerpColor(COL_SENSOR, COL_CPU, 0.35f),(y%4==0)?20:9);
        DrawLine((int)o.x,(int)(o.y+y*CELL),(int)(o.x+COLS*CELL),(int)(o.y+y*CELL),c);
    }

    float routeCardY = o.y + 12.f;
    int laneCount = G.ActiveLaneCount();
    int cols = (laneCount <= 3) ? 1 : 2;
    for (int lane = 0; lane < laneCount; lane++) {
        char header[24];
        snprintf(header, 24, "路線%d路由", lane + 1);
        int col = lane % cols;
        int row = lane / cols;
        drawRouteCard(o.x + 8.f + col * (routeCardW + routeCardGapX), routeCardY + row * routeCardGapY, header,
            G.LanePreset(lane), lane, 0.6f + 0.4f * sinf(t * 2.2f + lane * 0.7f),
            GetRouteTheme(G.LanePreset(lane), lane).sideLabel);
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawThreatMap
// ══════════════════════════════════════════════════════════════════
void DrawThreatMap(Game& G) {
    if (!G.showThreat) return;
    float mx = G.threatMap.GetMax();
    if (mx < 0.01f) return;
    auto o = G.MapOrigin();

    for (int lane = 0; lane < G.ActiveLaneCount(); lane++) {
        for (auto& pc : G.LaneCells(lane)) {
            float v = G.threatMap.Get(pc.gx, pc.gy);
            if (v < 0.01f) continue;
            float norm = std::min(1.f, v / mx);
            Color heatCol = {
                (unsigned char)(norm*255),(unsigned char)((1.f-norm)*120),
                (unsigned char)((1.f-norm)*200),(unsigned char)(40+norm*120)
            };
            float px=o.x+pc.gx*CELL,py=o.y+pc.gy*CELL;
            DrawRectangle((int)px+2,(int)py+2,CELL-4,CELL-4,heatCol);
            DrawCircleV({px + CELL * 0.5f, py + CELL * 0.5f}, 9.f + norm * 17.f, AlphaOf(COL_THREAT, (int)(20 + norm * 55)));
            DrawCircleLinesV({px + CELL * 0.5f, py + CELL * 0.5f}, 12.f + norm * 18.f, AlphaOf(WHITE, (int)(22 + norm * 78)));
            DrawLineEx({px + 10.f, py + CELL - 10.f}, {px + CELL - 10.f, py + 10.f}, 1.3f, AlphaOf(COL_THREAT, (int)(35 + norm * 70)));
            if (norm > 0.6f) {
                char buf[8]; snprintf(buf,8,"%.0f",v);
                DTC(buf,(int)(px+CELL*0.5f),(int)(py+CELL*0.5f),FS_TINY,AlphaOf(WHITE,(int)(160*norm)));
            }
        }
    }

    // 圖例
    auto mo=G.MapOrigin();
    int lx=(int)(mo.x+MAP_W-130),ly=(int)(mo.y+MAP_H-30);
    for (int i=0;i<100;i++) {
        float n=i/100.f;
        DrawRectangle(lx+i,ly,1,14,{(unsigned char)(n*255),(unsigned char)((1.f-n)*120),(unsigned char)((1.f-n)*200),200});
    }
    DrawRectangleLinesEx({(float)lx - 2.f, (float)ly - 2.f, 104.f, 18.f}, 1.f, AlphaOf(COL_THREAT, 126));
    DTX("低",(float)(lx-24),(float)ly,FS_TINY,AlphaOf(WHITE,160));
    DTX("高",(float)(lx+102),(float)ly,FS_TINY,AlphaOf(WHITE,160));
}

// ══════════════════════════════════════════════════════════════════
//  DrawAIHints
// ══════════════════════════════════════════════════════════════════
void DrawAIHints(Game& G) {
    if (!G.showAIHints) return;
    if (G.phase != Game::BUILD && G.phase != Game::TRAINING) return;
    float t = (float)GetTime();

    for (auto& h : G.aiHints) {
        float px=(float)PANEL_L+h.gx*CELL, py=(float)TOPBAR_H+h.gy*CELL;
        float pulse=0.55f+0.45f*sinf(t*3.5f+h.gx*0.7f);
        unsigned char alpha=(unsigned char)(pulse*200);

        Color bgCol=TDef(h.suggest).col; bgCol.a=(unsigned char)(pulse*35);
        DrawRectangle((int)px+3,(int)py+3,CELL-6,CELL-6,bgCol);
        Color bdCol=COL_AI; bdCol.a=alpha;
        DrawCircleV({px + CELL * 0.5f, py + CELL * 0.5f}, CELL * (0.38f + 0.06f * pulse), AlphaOf(COL_AI, 14 + (int)(pulse * 20)));
        DrawCircleLinesV({px + CELL * 0.5f, py + CELL * 0.5f}, CELL * (0.42f + 0.05f * pulse), AlphaOf(COL_AI, (int)(90 + pulse * 80)));
        DrawRectangleLinesEx({px+2,py+2,(float)CELL-4,(float)CELL-4},2.f,bdCol);
        DrawCornerBrackets(px + 5.f, py + 5.f, (float)CELL - 10.f, (float)CELL - 10.f, 8.f, AlphaOf(TDef(h.suggest).col, 150));
        DrawLineEx({px + 8.f, py + CELL - 9.f}, {px + CELL - 8.f, py + 9.f}, 1.f, AlphaOf(COL_AI, 70));
        DTX("AI",px+4,py+3,FS_TINY,AlphaOf(COL_AI,(int)(pulse*220)));
        DTC(TDef(h.suggest).sym,(int)(px+CELL/2),(int)(py+CELL/2+4),FS_SMALL,AlphaOf(TDef(h.suggest).col,(int)(pulse*200)));

        Vector2 mp=VirtualMouse();
        if (mp.x>=px && mp.x<px+CELL && mp.y>=py && mp.y<py+CELL) {
            float tw=MCN(h.reason.c_str(),FS_TINY)+12;
            float tx=px+CELL/2-tw/2,ty=py-26;
            DrawRectangle((int)tx-2,(int)ty-2,(int)tw+4,22,AlphaOf({4,9,18,255},220));
            DrawRectangleGradientH((int)tx-2,(int)ty-2,(int)tw+4,22,AlphaOf(COL_AI,30),AlphaOf(TDef(h.suggest).col,18));
            DrawRectangleLinesEx({tx-2,ty-2,tw+4,22},1.f,AlphaOf(COL_AI,180));
            DTX(h.reason.c_str(),tx+4,ty+2,FS_TINY,AlphaOf(COL_AI,230));
        }
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawConnections
// ══════════════════════════════════════════════════════════════════
void DrawConnections(Game& G) {
    float t = (float)GetTime();
    for (auto& tw : G.towers) {
        for (int cid : tw.conns) {
            auto* dst = G.FindTower(cid);
            if (!dst) continue;
            Vector2 src=G.CC(tw.gx,tw.gy), d=G.CC(dst->gx,dst->gy);
            Color sc=TDef(tw.type).col; sc.a=(unsigned char)(60+tw.sig*110);
            DrawLineEx(src,d,8.f+tw.sig*5.f,AlphaOf(sc, 22 + (int)(tw.sig * 30)));
            DrawLineEx(src,d,2.f+tw.sig*2.5f,sc);
            DrawLineEx(src,d,1.f,AlphaOf(WHITE, 24 + (int)(tw.sig * 44)));

            Vector2 dir=Vector2Normalize(Vector2Subtract(d,src));
            float len = Vector2Distance(src, d);
            Vector2 normal={-dir.y, dir.x};
            for (int node = 1; node <= 2; node++) {
                float k = fmodf(t*(0.42f + tw.sig * 0.35f) + tw.id*0.21f + node*0.34f, 1.f);
                Vector2 dot={src.x+(d.x-src.x)*k + normal.x * (node == 1 ? 3.f : -3.f),
                             src.y+(d.y-src.y)*k + normal.y * (node == 1 ? 3.f : -3.f)};
                DrawCircleV(dot, 2.5f + tw.sig * 2.f, AlphaOf(WHITE, 80 + (int)(tw.sig * 120)));
                DrawCircleV(dot, 6.f + tw.sig * 3.f, AlphaOf(sc, 28 + (int)(tw.sig * 55)));
            }
            float aT=fmodf(t*0.7f+tw.id*0.25f,1.f);
            Vector2 mid={src.x+(d.x-src.x)*aT,src.y+(d.y-src.y)*aT};
            float ang=atan2f(dir.y,dir.x),al=11.f,aw=0.45f;
            Color ac=sc; ac.a=(unsigned char)(120+tw.sig*90);
            DrawTriangle(mid,
                {mid.x+al*cosf(ang+aw+3.14f),mid.y+al*sinf(ang+aw+3.14f)},
                {mid.x+al*cosf(ang-aw+3.14f),mid.y+al*sinf(ang-aw+3.14f)},ac);
            if (len > CELL * 1.8f) {
                Vector2 labelPos={src.x+(d.x-src.x)*0.5f+normal.x*10.f, src.y+(d.y-src.y)*0.5f+normal.y*10.f};
                char sb[12]; snprintf(sb, 12, "%.0f%%", tw.sig * 100.f);
                DTC(sb, (int)labelPos.x, (int)labelPos.y, FS_TINY, AlphaOf(sc, 120));
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawPulses
// ══════════════════════════════════════════════════════════════════
void DrawPulses(Game& G) {
    for (auto& p : G.pulses) {
        float fade=1.f-p.t;
        Vector2 pos={p.src.x+(p.dst.x-p.src.x)*p.t,p.src.y+(p.dst.y-p.src.y)*p.t};
        Color c=p.col; c.a=(unsigned char)(240*fade);
        DrawCircleV(pos,7.f,c);
        c.a=(unsigned char)(70*fade); DrawCircleV(pos,13.f,c);
        Color cc=WHITE; cc.a=(unsigned char)(180*fade); DrawCircleV(pos,3.f,cc);
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawTower
// ══════════════════════════════════════════════════════════════════
void DrawTower(Game& G, Tower& t, bool sel) {
    Vector2 ctr=G.CC(t.gx,t.gy);
    float px=(float)PANEL_L+t.gx*CELL, py=(float)TOPBAR_H+t.gy*CELL;
    float time = (float)GetTime();

    // ── CPU 特殊繪製 ─────────────────────────────────────────────
    if (t.type == TType::CPU) {
        float glow=10.f+8.f*sinf((float)GetTime()*2.5f);
        float cpuR=G.cpuHp/100.f;
        Color cpuCol=(cpuR>0.5f)?COL_CPU:(cpuR>0.25f)?ORANGE:RED;
        float danger = 1.f - cpuR;
        for (int ring = 0; ring < 3; ring++) {
            float rr = CELL * (0.56f + ring * 0.16f) + sinf(time * (2.1f + ring) + ring) * 4.f;
            DrawCircleLinesV(ctr, rr, AlphaOf(cpuCol, (int)(55 + danger * 105) / (ring + 1)));
        }
        if (danger > 0.35f) {
            float flick = 0.5f + 0.5f * sinf(time * 12.f);
            DrawRectangleLinesEx({px - 8.f, py - 8.f, (float)CELL + 16.f, (float)CELL + 16.f}, 2.f,
                AlphaOf(RED, (int)(80 + flick * 120)));
        }
        for (int i=3;i>=1;i--) {
            Color gc=cpuCol; gc.a=(unsigned char)(12*i);
            float ext=glow*i*0.5f;
            DrawRectangleRounded({px+3.f-ext,py+3.f-ext,(float)CELL-6+ext*2,(float)CELL-6+ext*2},0.25f,8,gc);
        }
        DrawRectangleRounded({px+4,py+4,(float)CELL-8,(float)CELL-8},0.2f,8,Color{6,20,35,255});
        DrawRectangleRoundedLines({px+4,py+4,(float)CELL-8,(float)CELL-8},0.2f,8,1.f,cpuCol);
        DrawMicroTicks(px + 4.f, py + 4.f, (float)CELL - 8.f, (float)CELL - 8.f, cpuCol);
        DTC("CPU",(int)ctr.x,(int)ctr.y-8,FS_MED,cpuCol);
        char buf[8]; snprintf(buf,8,"%.0f%%",G.cpuHp);
        DTC(buf,(int)ctr.x,(int)ctr.y+14,FS_SMALL,cpuCol);
        return;
    }

    TowerDef& def=TDef(t.type);
    Color fill=def.col; fill.a=(unsigned char)((0.15f+t.sig*0.85f)*65);
    Color border=def.col; border.a=(unsigned char)(150+t.sig*100);

    DrawCircleV(ctr, CELL * 0.48f, AlphaOf(def.col, 16 + (int)(t.sig * 24.f)));
    DrawCircleLinesV(ctr, CELL * 0.48f + 2.f * sinf(time * 2.2f + t.id), AlphaOf(def.col, 45 + (int)(t.sig * 55.f)));
    DrawRoundBox(px + 4.f, py + 4.f, (float)CELL - 8.f, (float)CELL - 8.f, 9.f,
                 AlphaOf(Color{2, 8, 16, 255}, 215), AlphaOf(def.col, 70), 1.2f);
    DrawCornerBrackets(px + 8.f, py + 8.f, (float)CELL - 16.f, (float)CELL - 16.f, 9.f, AlphaOf(def.col, 95));

    if (t.upgradeFlash > 0) {
        Color ufc=WHITE; ufc.a=(unsigned char)(200*t.upgradeFlash);
        DrawRectangleRounded({px+2,py+2,(float)CELL-4,(float)CELL-4},0.2f,8,ufc);
    }

    if (t.sig > 0.05f) {
        for (int i=2;i>=1;i--) {
            float gr=12.f+t.sig*18.f;
            Color gc=def.col; gc.a=(unsigned char)(t.sig*22/i);
            if (t.type == TType::PERCEPTRON) {
                DrawCircle((int)ctr.x,(int)ctr.y,(int)((float)CELL*0.43f+gr*i/2),gc);
            } else {
                DrawRectangle((int)(px+2-gr*i/3),(int)(py+2-gr*i/3),
                    (int)(CELL-4+gr*i*2/3),(int)(CELL-4+gr*i*2/3),gc);
            }
        }
    }

    if (t.type == TType::PERCEPTRON) {
        DrawHex(ctr,(float)CELL*0.42f,AlphaOf(fill, 150),border);
        DrawHex(ctr,(float)CELL*0.25f,AlphaOf(COL_AI, 28 + (int)(t.sig * 55.f)),AlphaOf(def.col, 130));
        int layer=CalcPCTLayer(G,t.id);
        Color layerCol=(layer==1)?COL_PERC:(layer==2)?COL_AI:COL_STAR;
        for (int i = 0; i < layer; i++) {
            float rr = CELL * (0.34f + i * 0.085f) + 2.f * sinf(time * 2.6f + i + t.id);
            DrawCircleLinesV(ctr, rr, AlphaOf(layerCol, 54 - i * 10));
        }
    } else {
        DrawRoundBox(px+8,py+8,(float)CELL-16,(float)CELL-16,8,fill,border,2.f);
        DrawRoundBox(px+14,py+14,(float)CELL-28,(float)CELL-28,6,AlphaOf(BG, 190),AlphaOf(def.col, 92),1.f);
    }

    if (t.type == TType::SENSOR) {
        float scan = 0.55f + 0.45f * sinf(time * 3.4f + t.id);
        DrawCircleLinesV(ctr, CELL * (0.22f + scan * 0.16f), AlphaOf(COL_SENSOR, 90));
        DrawLineEx({ctr.x - 10.f, ctr.y}, {ctr.x + 10.f, ctr.y}, 1.4f, AlphaOf(COL_SENSOR, 135));
        DrawLineEx({ctr.x, ctr.y - 10.f}, {ctr.x, ctr.y + 10.f}, 1.4f, AlphaOf(COL_SENSOR, 135));
    }

    if (t.type == TType::CANNON) {
        Vector2 aim = { cosf(time * 0.7f + t.id), sinf(time * 0.7f + t.id) };
        float best = 9999.f;
        for (auto& e : G.enemies) {
            float d = Dist({(float)t.gx, (float)t.gy}, G.EnemyGrid(e));
            if (d <= t.range && d < best) {
                best = d;
                Vector2 ep = G.EnemyWorld(e);
                aim = Vector2Normalize(Vector2Subtract(ep, ctr));
            }
        }
        Vector2 muzzle = {ctr.x + aim.x * CELL * 0.34f, ctr.y + aim.y * CELL * 0.34f};
        Vector2 back   = {ctr.x - aim.x * CELL * 0.12f, ctr.y - aim.y * CELL * 0.12f};
        DrawLineEx(back, muzzle, 9.f, AlphaOf(COL_CANNON, 70));
        DrawLineEx(back, muzzle, 4.f, AlphaOf(COL_CANNON, 210));
        DrawCircleV(muzzle, 5.f + t.sig * 2.f, AlphaOf(WHITE, 165));
    }

    if (t.type == TType::NAND || t.type == TType::AND || t.type == TType::OR || t.type == TType::XOR) {
        float p = 0.5f + 0.5f * sinf(time * 2.8f + t.id * 0.4f);
        DrawLineEx({px + 14.f, ctr.y}, {px + CELL - 14.f, ctr.y}, 1.4f, AlphaOf(def.col, 85 + (int)(80 * p)));
        DrawLineEx({ctr.x, py + 14.f}, {ctr.x, py + CELL - 14.f}, 1.0f, AlphaOf(def.col, 60));
    }

    if (sel) {
        float pulse=0.85f+0.15f*sinf((float)GetTime()*5.f);
        Color wc=WHITE; wc.a=200;
        DrawCircleLinesV(ctr,(float)CELL*0.54f*pulse,wc);
        if (t.type==TType::SENSOR) {
            DrawCircleV(ctr,t.range*CELL,AlphaOf(COL_SENSOR,30));
            DrawCircleLinesV(ctr,t.range*CELL,AlphaOf(COL_SENSOR,100));
        }
        if (t.type==TType::CANNON) {
            DrawCircleV(ctr,t.range*CELL,AlphaOf(COL_CANNON,25));
            DrawCircleLinesV(ctr,t.range*CELL,AlphaOf(COL_CANNON,80));
        }
    }

    if (t.type == TType::PERCEPTRON) {
        int layer=CalcPCTLayer(G,t.id);
        char lb[8]; snprintf(lb,8,"L%d",layer);
        Color lc=(layer==1)?COL_PERC:(layer==2)?COL_AI:COL_STAR;
        DTX(lb,px+CELL-22,py+4,FS_TINY,AlphaOf(lc,220));
    }

    DTC(def.sym,(int)ctr.x,(int)ctr.y-8,FS_SMALL,AlphaOf(border, 245));
    for (int lv=0;lv<t.level;lv++) DrawPoly({px+8+lv*11.f,py+(float)CELL-16.f},5,5.f,0.f,COL_STAR);

    int bx=(int)px+6,by=(int)py+CELL-10,bw=CELL-12,bh=7;
    DrawRectangle(bx,by,bw,bh,Color{0,0,0,140});
    int fw=(int)(bw*t.sig);
    if (fw > 0) { Color bc=def.col; bc.a=220; DrawRectangle(bx,by,fw,bh,bc); }

    if (HasTowerSkillSlot(t.type)) {
        float maxCd=SKILL_CD[(int)t.type];
        if (maxCd > 0.f) {
            int abx=bx,aby=by-8,abw=bw,abh=4;
            DrawRectangle(abx,aby,abw,abh,Color{0,0,0,100});
            float ready=1.f-t.activeCd/maxCd;
            int afw=(int)(abw*ready);
            Color adc=(t.activeCd<=0)?Color{100,255,180,255}:Color{80,140,255,200};
            if (afw>0) DrawRectangle(abx,aby,afw,abh,adc);
            if (t.activeCd<=0) {
                float p2=0.6f+0.4f*sinf((float)GetTime()*5.f);
                DrawRectangleLinesEx({(float)abx,(float)aby,(float)abw,(float)abh},1.f,{100,255,180,(unsigned char)(180*p2)});
                DrawCircleLinesV(ctr, CELL * (0.53f + p2 * 0.04f), AlphaOf(Color{100,255,180,255}, (int)(70 * p2)));
            }
        }
    }

    if (t.type==TType::PERCEPTRON && t.learner.lastLoss > 0.f) {
        Color lossCol=(t.learner.lastLoss<0.05f)?GREEN:(t.learner.lastLoss<0.15f)?YELLOW:RED;
        DrawCircle((int)(px+CELL-9),(int)(py+9),5.f,lossCol);
        DrawCircleLines((int)(px+CELL-9),(int)(py+9),5.f,AlphaOf(WHITE,120));
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawGhostTower
// ══════════════════════════════════════════════════════════════════
void DrawGhostTower(Game& G, int gx, int gy) {
    if (G.placing==TType::NONE) return;
    if (!IsBuildableTowerType(G.placing)) return;
    if (gx<0||gx>=COLS||gy<0||gy>=ROWS) return;
    if (G.TowerAt(gx,gy)||G.IsPath(gx,gy)) return;

    TowerDef& def=TDef(G.placing);
    float px=(float)PANEL_L+gx*CELL, py=(float)TOPBAR_H+gy*CELL;
    bool can=(G.credits>=def.baseCost);
    Color fill=def.col; fill.a=can?45:18;
    Color bd=def.col;   bd.a=can?160:50;
    DrawRoundBox(px+5,py+5,(float)CELL-10,(float)CELL-10,6,fill,bd);
    DTC(def.sym,(int)(px+CELL/2),(int)(py+CELL/2-5),FS_SMALL,bd);
    if (!can) DTC("不足",(int)(px+CELL/2),(int)(py+CELL/2+14),FS_TINY,RED);

    Vector2 c2={px+CELL*0.5f,py+CELL*0.5f};
    if (G.placing==TType::SENSOR) {
        DrawCircleV(c2,4.5f*CELL,AlphaOf(COL_SENSOR,15));
        DrawCircleLinesV(c2,4.5f*CELL,AlphaOf(COL_SENSOR,70));
    }
    if (G.placing==TType::CANNON) {
        DrawCircleV(c2,7.f*CELL,AlphaOf(COL_CANNON,12));
        DrawCircleLinesV(c2,7.f*CELL,AlphaOf(COL_CANNON,60));
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawEnemies
// ══════════════════════════════════════════════════════════════════
void DrawEnemies(Game& G) {
    float t = (float)GetTime();
    for (auto& e : G.enemies) {
        Vector2 p = G.EnemyWorld(e);
        bool fl = (e.flashTimer > 0);
        const PathPreset& routePreset = G.LanePreset(e.pathIdx);
        RouteVisualTheme laneTheme = GetRouteTheme(routePreset, e.pathIdx);
        float travel = 0.f;
        if (!G.EnemyLaneCells(e).empty()) {
            travel = Clamp(e.pathPos / (float)std::max(1, (int)G.EnemyLaneCells(e).size()), 0.f, 1.f);
        }

        Color base, ring;
        float sz = 14.f;
        switch (e.type) {
            case EType::BASIC:   base=COL_VIRUS;   ring={255, 80,120,200}; sz=14.f; break;
            case EType::FAST:    base=COL_FAST;    ring={255,220, 80,200}; sz=11.f; break;
            case EType::ARMORED: base=COL_ARMORED; ring={160,200,255,200}; sz=17.f; break;
            case EType::ELITE:   base=COL_ELITE;   ring={255,120,210,200}; sz=15.f; break;
            case EType::BOSS:    base=COL_BOSS;    ring={200, 80,255,220}; sz=26.f; break;
        }
        if (fl) { base=WHITE; ring=WHITE; }
        
        if (e.flashTimer > 0) {
            float f = e.flashTimer;
            base = LerpColor(base, WHITE, f);
            ring = LerpColor(ring, WHITE, f);
        }

        if (e.tag == EnemyTag::BRUTE)  { ring = DrawEnemyTagColor(e.tag); sz += 2.f; }
        if (e.tag == EnemyTag::SWIFT)  { base = AlphaOf(base, 220); }
        if (e.tag == EnemyTag::BOUNTY) { ring = DrawEnemyTagColor(e.tag); }

        Vector2 sideDir = RouteAdvanceDir(routePreset.entrySide);
        Vector2 routeDot = {p.x - sideDir.x * (sz + 4.f), p.y - sideDir.y * (sz + 4.f)};
        if (e.type == EType::FAST || e.tag == EnemyTag::SWIFT) {
            for (int tr = 1; tr <= 3; tr++) {
                Vector2 tail = {p.x - sideDir.x * (sz + tr * 9.f), p.y - sideDir.y * (sz + tr * 9.f)};
                DrawCircleV(tail, std::max(2.f, sz * (0.42f - tr * 0.07f)), AlphaOf(base, 42 - tr * 9));
            }
            DrawLineEx({p.x - sideDir.x * (sz + 26.f), p.y - sideDir.y * (sz + 26.f)}, p, 2.f, AlphaOf(base, 72));
        }
        if (e.type == EType::BOSS) {
            float bossPulse = 0.55f + 0.45f * sinf(t * ((e.bossState == BossState::RAMPAGE) ? 7.f : 3.f));
            Color bossAura = (e.bossState == BossState::RAMPAGE) ? RED : (e.bossState == BossState::EVADE) ? YELLOW : COL_BOSS;
            DrawCircleV(p, sz + 30.f + bossPulse * 8.f, AlphaOf(bossAura, 14 + (int)(bossPulse * 18)));
            DrawCircleLinesV(p, sz + 24.f + bossPulse * 10.f, AlphaOf(bossAura, 88 + (int)(bossPulse * 80)));
            DrawCircleLinesV(p, sz + 37.f + bossPulse * 14.f, AlphaOf(bossAura, 44));
        }
        if (e.tag == EnemyTag::BOUNTY) {
            float bp = 0.5f + 0.5f * sinf(t * 5.2f + e.id);
            DrawCircleLinesV(p, sz + 13.f + bp * 4.f, AlphaOf(COL_STAR, 140));
            DrawLineEx({p.x - sz - 8.f, p.y - sz - 8.f}, {p.x - sz + 3.f, p.y - sz - 8.f}, 1.5f, AlphaOf(COL_STAR, 155));
            DrawLineEx({p.x + sz + 8.f, p.y + sz + 8.f}, {p.x + sz - 3.f, p.y + sz + 8.f}, 1.5f, AlphaOf(COL_STAR, 155));
        }
        DrawCircleV(p, sz + 11.f, AlphaOf(laneTheme.accent, 16));
        DrawCircleV(p, sz + 6.f, AlphaOf(base, 34));
        DrawCircleV(routeDot, 4.5f, AlphaOf(laneTheme.accent, 230));
        DrawCircleLinesV(routeDot, 7.f, AlphaOf(laneTheme.glow, 120));
        DrawCircleLinesV(p, sz + 4.f, AlphaOf(laneTheme.accent, 75));

        Vector2 pts[6];
        for (int i=0;i<6;i++) {
            float a=i*60.f*DEG2RAD+e.angle*DEG2RAD;
            pts[i]={p.x+sz*cosf(a),p.y+sz*sinf(a)};
        }
        for (int i=0;i<6;i++) DrawTriangle(p,pts[i],pts[(i+1)%6],AlphaOf(base,fl?220:160));
        for (int i=0;i<6;i++) DrawLineV(pts[i],pts[(i+1)%6],ring);
        DrawCircleV(p, std::max(3.5f, sz * 0.24f), AlphaOf(WHITE, fl ? 190 : 82));
        DrawCircleLinesV(p, sz + 1.5f, AlphaOf(ring, 95));

        if (e.stealth) {
            float st=0.5f+0.5f*sinf(t*4.f+e.id);
            DrawCircleLinesV(p,sz+4.f,AlphaOf(COL_FAST,(int)(80*st)));
            DrawCircleLinesV({p.x + 2.f * sinf(t * 9.f), p.y},sz+9.f,AlphaOf(COL_FAST,(int)(45*st)));
        }
        if (e.marked) {
            float mp2=0.7f+0.3f*sinf(t*6.f);
            DrawCircleLinesV(p,sz+6.f,AlphaOf(COL_XOR,(int)(180*mp2)));
            DrawLineEx({p.x - sz - 6.f, p.y}, {p.x + sz + 6.f, p.y}, 1.2f, AlphaOf(COL_XOR, (int)(120 * mp2)));
            DrawLineEx({p.x, p.y - sz - 6.f}, {p.x, p.y + sz + 6.f}, 1.2f, AlphaOf(COL_XOR, (int)(95 * mp2)));
        }
        if (e.spawnFx > 0.f) {
            float sf = e.spawnFx / 0.65f;
            Color sc = (e.tag == EnemyTag::NONE) ? laneTheme.accent : DrawEnemyTagColor(e.tag);
            DrawCircleLinesV(p, sz + 14.f * (1.f - sf), AlphaOf(sc, (int)(180 * sf)));
            DrawCircleLinesV(p, sz + 20.f * (1.f - sf), AlphaOf(sc, (int)(90 * sf)));
        }
        if (e.shielded && e.shieldHp > 0.f) {
            float sp2=0.6f+0.4f*sinf(t*5.f+e.id);
            float shieldRatio = e.maxHp > 0.f ? Clamp(e.shieldHp / std::max(1.f, e.maxHp * 0.25f), 0.f, 1.f) : 1.f;
            DrawCircleV(p, sz + 14.f, AlphaOf(Color{100,180,255,255}, (int)(18 + shieldRatio * 24)));
            DrawCircleLinesV(p,sz+9.f, AlphaOf(Color{150,220,255,255},(int)(200*sp2)));
            DrawCircleLinesV(p,sz+11.f,AlphaOf(Color{100,180,255,255},(int)( 80*sp2)));
            DrawLineEx({p.x - sz - 8.f, p.y - sz - 2.f}, {p.x + sz + 8.f, p.y + sz + 2.f}, 1.1f,
                AlphaOf(Color{180,235,255,255}, (int)(90 * sp2)));
        }

        int bw = (int)(sz * 2) + 10;
        int bh = 5;
        int bx = (int)(p.x - bw / 2);
        int by = (int)(p.y - sz - 12);
        float hpR = e.hp / e.maxHp;
        DrawRectangle(bx, by, bw, bh, AlphaOf(laneTheme.fill, 205));
        Color hpC = (hpR > 0.6f) ? GREEN : (hpR > 0.3f) ? ORANGE : RED;
        DrawRectangle(bx, by, (int)(bw * hpR), bh, hpC);
        DrawRectangle(bx, by + 7, bw, 3, AlphaOf(laneTheme.fillSoft, 180));
        DrawRectangle(bx, by + 7, (int)(bw * travel), 3, AlphaOf(laneTheme.accent, 220));

        float chipW = MCN(laneTheme.shortLabel, FS_TINY) + 12.f;
        float chipX = p.x - chipW * 0.5f;
        float chipY = (float)by - 17.f;
        DrawRoundBox(chipX, chipY, chipW, 13.f, 4.f,
            AlphaOf(laneTheme.fill, 220), AlphaOf(laneTheme.accent, 160), 1.f);
        DTC(laneTheme.shortLabel, (int)p.x, (int)(chipY + 6.5f), FS_TINY, laneTheme.text);

        if (e.tag != EnemyTag::NONE) {
            const char* tagTxt = DrawEnemyTagName(e.tag);
            Color tagCol = DrawEnemyTagColor(e.tag);
            float tw = MCN(tagTxt, FS_TINY) + 12.f;
            float tx = p.x - tw * 0.5f;
            float ty = chipY - 18.f;
            DrawRoundBox(tx, ty, tw, 14.f, 4.f, AlphaOf(tagCol, 28), AlphaOf(tagCol, 180), 1.f);
            DTC(tagTxt, (int)p.x, (int)(ty + 7.f), FS_TINY, tagCol);
        }

        if (e.type==EType::BOSS) {
            const char* stateLabel=
                (e.bossState==BossState::RAMPAGE)?"RAMPAGE！":
                (e.bossState==BossState::EVADE)  ?"迴避中"  :"BOSS";
            Color stateCol=
                (e.bossState==BossState::RAMPAGE)?RED:
                (e.bossState==BossState::EVADE)  ?YELLOW:COL_BOSS;
            float pulse2=0.8f+0.2f*sinf(t*5.f);
            stateCol.a=(unsigned char)(220*pulse2);
            DTC(stateLabel,(int)p.x,(int)(p.y-sz-22),FS_SMALL,stateCol);
            if (G.showThreat) {
                char tb[24]; snprintf(tb,24,"thr:%.1f",e.localThreat);
                DTC(tb,(int)p.x,(int)(p.y+sz+14),FS_TINY,AlphaOf(COL_THREAT,180));
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════
//  DrawBullets / DrawParticles / DrawFloats
// ══════════════════════════════════════════════════════════════════
void DrawBullets(Game& G) {
    for (auto& b : G.bullets) {
        float speed = sqrtf(b.vel.x * b.vel.x + b.vel.y * b.vel.y);
        Vector2 dir = (speed > 0.001f) ? Vector2{ b.vel.x / speed, b.vel.y / speed } : Vector2{ 1.f, 0.f };
        float trailLen = b.splash ? 50.f : (b.crit ? 46.f : 34.f);
        float outerW   = b.splash ? 12.f : (b.crit ? 9.f : 6.f);
        float coreW    = b.splash ? 4.5f : (b.crit ? 3.5f : 2.5f);
        Vector2 tail   = { b.pos.x - dir.x * trailLen, b.pos.y - dir.y * trailLen };
        Vector2 mid    = { b.pos.x - dir.x * trailLen * 0.55f, b.pos.y - dir.y * trailLen * 0.55f };

        DrawLineEx(tail, b.pos, outerW, AlphaOf(b.col, b.crit ? 82 : 58));
        DrawLineEx(mid,  b.pos, coreW,  AlphaOf(b.col, b.crit ? 230 : 190));
        if (b.crit) DrawLineEx(mid, b.pos, 1.4f, AlphaOf(WHITE, 220));

        DrawCircleV(b.pos, b.splash ? 16.f : (b.crit ? 13.f : 9.f), AlphaOf(b.col, b.crit ? 92 : 70));
        DrawCircleV(b.pos, b.splash ? 7.f  : (b.crit ? 6.f  : 4.f), b.crit ? AlphaOf(WHITE, 235) : b.col);
    }
}

void DrawParticles(Game& G) {
    for (auto& p : G.particles) {
        float alpha = Clamp(p.life / p.maxLife, 0.f, 1.f);
        float grow  = 1.f - alpha;

        if (p.radius >= 14.f) {
            float r = p.radius * (0.38f + grow * 0.92f);
            DrawCircleV(p.pos, r, AlphaOf(p.col, (int)(24 * alpha)));
            DrawCircleLinesV(p.pos, r, AlphaOf(p.col, (int)(155 * alpha)));
            DrawCircleLinesV(p.pos, r * 0.62f, AlphaOf(WHITE, (int)(42 * alpha)));
        } else {
            float glowR = p.radius * (1.25f + grow * 0.75f);
            DrawCircleV(p.pos, glowR, AlphaOf(p.col, (int)(58 * alpha)));
            DrawCircleV(p.pos, std::max(1.5f, p.radius * alpha), AlphaOf(p.col, (int)(210 * alpha)));
        }
    }
}

void DrawFloats(Game& G) {
    for (auto& f : G.floats) {
        float alpha=std::min(1.f,f.life);
        Color c=f.col; c.a=(unsigned char)(230*alpha);
        DTC(f.text.c_str(),(int)f.pos.x,(int)f.pos.y,FS_SMALL,c);
    }
}
