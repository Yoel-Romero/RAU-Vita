#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
#include <string>
#include <vector>

struct AnimationStrip {
    SDL_Texture* texture = nullptr;
    int frameWidth = 0;
    int frameHeight = 0;
    int frameCount = 0;
    float fps = 12.0f;
};

struct Platform {
    float x, y, w, h;
};

struct StarCrystal {
    float x, y, w, h;
    bool active = true;
};

struct Fairy {
    float x, y, w, h;
    bool active = true;
    float animTime = 0.0f;
};

struct TrailPiece {
    float x, y, w, h;
    float life = 0.0f;
    float maxLife = 0.0f;
    int colorBand = 0;
};

struct Spark {
    float x, y, vx, vy;
    float life = 0.0f;
    float maxLife = 0.0f;
    int colorBand = 0;
};

static float randf(float a, float b) {
    return a + (b - a) * (float)rand() / (float)RAND_MAX;
}

static SDL_Texture* LoadTexture(SDL_Renderer* renderer, const std::string& path) {
    SDL_Texture* tex = IMG_LoadTexture(renderer, path.c_str());
    return tex;
}

static bool Intersects(const SDL_FRect& a, const SDL_FRect& b) {
    return !(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y);
}

static void DrawTexture(SDL_Renderer* renderer, SDL_Texture* tex, const SDL_Rect* src, const SDL_FRect& dst) {
    SDL_Rect dstI = { (int)dst.x, (int)dst.y, (int)dst.w, (int)dst.h };
    SDL_RenderCopy(renderer, tex, src, &dstI);
}

static void DrawAnim(SDL_Renderer* renderer, const AnimationStrip& anim, float animTime, const SDL_FRect& dst) {
    if (!anim.texture || anim.frameCount <= 0) return;
    int frame = (int)(animTime * anim.fps) % anim.frameCount;
    SDL_Rect src = { frame * anim.frameWidth, 0, anim.frameWidth, anim.frameHeight };
    DrawTexture(renderer, anim.texture, &src, dst);
}

static void SpawnStarBurst(std::vector<Spark>& sparks, float x, float y) {
    for (int i = 0; i < 20; i++) {
        Spark s;
        s.x = x;
        s.y = y;
        s.vx = randf(-240.0f, 240.0f);
        s.vy = randf(-260.0f, 40.0f);
        s.life = 0.45f;
        s.maxLife = 0.45f;
        s.colorBand = i % 6;
        sparks.push_back(s);
    }
}

static SDL_Color RainbowColor(int i) {
    static SDL_Color colors[6] = {
        {255,  60, 120, 255},
        {255, 130,  40, 255},
        {255, 235,  70, 255},
        { 80, 255, 120, 255},
        { 80, 180, 255, 255},
        {180, 100, 255, 255}
    };
    return colors[i % 6];
}

int main(int argc, char* argv[]) {
    srand((unsigned int)time(nullptr));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) return -1;

    SDL_Window* window = SDL_CreateWindow(
        "Robot Unicorn Attack Prototype",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        960, 544, 0
    );
    if (!window) return -1;

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) return -1;

    const float SCREEN_W = 960.0f;
    const float SCREEN_H = 544.0f;
    const float FIXED_DT = 1.0f / 60.0f;

    SDL_Texture* texRun = LoadTexture(renderer, "assets/hdpi_run.png");
    SDL_Texture* texJump = LoadTexture(renderer, "assets/hdpi_jump.png");
    SDL_Texture* texFall = LoadTexture(renderer, "assets/hdpi_fall.png");
    SDL_Texture* texFairy = LoadTexture(renderer, "assets/hdpi_fairy.png");
    SDL_Texture* texStar = LoadTexture(renderer, "assets/hdpi_star.png");
    SDL_Texture* cloud0 = LoadTexture(renderer, "assets/hdpi_clouds_0.png");
    SDL_Texture* cloud1 = LoadTexture(renderer, "assets/hdpi_clouds_1.png");
    SDL_Texture* cloud2 = LoadTexture(renderer, "assets/hdpi_clouds_2.png");
    SDL_Texture* cloud3 = LoadTexture(renderer, "assets/hdpi_clouds_3.png");

    if (!texRun || !texJump || !texFall || !texFairy || !texStar ||
        !cloud0 || !cloud1 || !cloud2 || !cloud3) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Missing assets",
            "Put the extracted APK PNGs in a folder named assets next to the exe.", window);
        return -1;
    }

    AnimationStrip runAnim{ texRun, 135, 72, 8, 14.0f };
    AnimationStrip jumpAnim{ texJump, 126, 88, 9, 18.0f };
    AnimationStrip fallAnim{ texFall, 126, 88, 12, 16.0f };
    AnimationStrip fairyAnim{ texFairy, 132, 60, 4, 10.0f };

    bool running = true;
    SDL_Event event;

    Uint64 prevCounter = SDL_GetPerformanceCounter();
    double accumulator = 0.0;

    float worldSpeed = 340.0f;
    const float worldAccel = 7.0f;
    const float maxSpeed = 760.0f;

    float playerX = 180.0f;
    float playerY = 0.0f;
    float playerW = 94.0f;
    float playerH = 94.0f;
    float vy = 0.0f;
    float animTime = 0.0f;

    bool grounded = false;
    int jumpsUsed = 0;
    int wishes = 3;
    int dashCharges = 1;
    bool dead = false;

    bool jumpQueued = false;
    bool dashQueued = false;
    bool isDashing = false;
    float dashTimer = 0.0f;
    const float dashDuration = 0.16f;

    float score = 0.0f;

    std::vector<Platform> platforms;
    std::vector<StarCrystal> stars;
    std::vector<Fairy> fairies;
    std::vector<TrailPiece> trail;
    std::vector<Spark> sparks;

    float cloudOffset0 = 0.0f;
    float cloudOffset1 = 0.0f;
    float cloudOffset2 = 0.0f;
    float cloudOffset3 = 0.0f;

    auto resetLife = [&]() {
        playerX = 180.0f;
        playerY = 300.0f;
        vy = 0.0f;
        grounded = false;
        jumpsUsed = 0;
        dashCharges = 1;
        isDashing = false;
        dashTimer = 0.0f;
        jumpQueued = false;
        dashQueued = false;

        platforms.clear();
        stars.clear();
        fairies.clear();
        trail.clear();
        sparks.clear();

        float x = -40.0f;
        for (int i = 0; i < 10; i++) {
            Platform p;
            p.x = x;
            p.y = 430.0f + randf(-22.0f, 18.0f);
            p.w = randf(180.0f, 280.0f);
            p.h = 28.0f;
            platforms.push_back(p);
            x += p.w + randf(70.0f, 145.0f);
        }
        };

    auto killPlayer = [&]() {
        wishes--;
        if (wishes <= 0) {
            dead = true;
        }
        else {
            resetLife();
        }
        };

    resetLife();

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        double frameTime = (double)(now - prevCounter) / SDL_GetPerformanceFrequency();
        prevCounter = now;
        if (frameTime > 0.25) frameTime = 0.25;
        accumulator += frameTime;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;

            if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                if (event.key.keysym.sym == SDLK_SPACE && !dead) jumpQueued = true;
                if (event.key.keysym.sym == SDLK_LSHIFT && !dead) dashQueued = true;

                if (event.key.keysym.sym == SDLK_r && dead) {
                    dead = false;
                    wishes = 3;
                    score = 0.0f;
                    worldSpeed = 340.0f;
                    resetLife();
                }
            }
        }

        while (accumulator >= FIXED_DT) {
            if (!dead) {
                worldSpeed = std::min(maxSpeed, worldSpeed + worldAccel * FIXED_DT);
                score += worldSpeed * 0.010f;
                animTime += FIXED_DT;

                cloudOffset0 += worldSpeed * 0.08f * FIXED_DT;
                cloudOffset1 += worldSpeed * 0.13f * FIXED_DT;
                cloudOffset2 += worldSpeed * 0.18f * FIXED_DT;
                cloudOffset3 += worldSpeed * 0.25f * FIXED_DT;

                if (dashQueued && dashCharges > 0 && !isDashing) {
                    isDashing = true;
                    dashTimer = dashDuration;
                    dashCharges--;
                }

                if (jumpQueued && jumpsUsed < 2) {
                    vy = (jumpsUsed == 0) ? -760.0f : -700.0f;
                    grounded = false;
                    jumpsUsed++;
                }

                jumpQueued = false;
                dashQueued = false;

                if (isDashing) {
                    dashTimer -= FIXED_DT;
                    if (dashTimer <= 0.0f) {
                        isDashing = false;
                        dashTimer = 0.0f;
                    }
                }

                vy += 2000.0f * FIXED_DT;
                playerY += vy * FIXED_DT;

                grounded = false;
                SDL_FRect playerRect = { playerX, playerY, playerW, playerH };

                for (auto& p : platforms) {
                    SDL_FRect plat = { p.x, p.y, p.w, p.h };
                    bool fallingIntoTop =
                        (playerRect.y + playerRect.h >= plat.y) &&
                        (playerRect.y + playerRect.h <= plat.y + 20.0f) &&
                        (playerRect.x + playerRect.w > plat.x + 8.0f) &&
                        (playerRect.x < plat.x + plat.w - 8.0f) &&
                        (vy >= 0.0f);

                    if (fallingIntoTop) {
                        playerY = plat.y - playerH;
                        vy = 0.0f;
                        grounded = true;
                        jumpsUsed = 0;
                        break;
                    }
                }

                if (playerY > SCREEN_H + 80.0f) {
                    killPlayer();
                }

                for (auto& p : platforms) {
                    p.x -= worldSpeed * FIXED_DT;
                }

                for (size_t i = 0; i < platforms.size(); i++) {
                    if (platforms[i].x + platforms[i].w < -80.0f) {
                        float maxRight = 0.0f;
                        for (auto& p : platforms) maxRight = std::max(maxRight, p.x + p.w);

                        platforms[i].w = randf(180.0f, 280.0f);
                        platforms[i].h = 28.0f;
                        platforms[i].x = maxRight + randf(75.0f, 150.0f);
                        platforms[i].y = 420.0f + randf(-28.0f, 22.0f);

                        if (randf(0.0f, 1.0f) < 0.80f) {
                            StarCrystal s;
                            s.w = 52.0f;
                            s.h = 52.0f;
                            s.x = platforms[i].x + platforms[i].w * randf(0.35f, 0.75f);
                            s.y = platforms[i].y - 62.0f;
                            s.active = true;
                            stars.push_back(s);
                        }

                        if (randf(0.0f, 1.0f) < 0.40f) {
                            Fairy f;
                            f.w = 58.0f;
                            f.h = 58.0f;
                            f.x = platforms[i].x + platforms[i].w * randf(0.40f, 0.70f);
                            f.y = platforms[i].y - 135.0f;
                            f.active = true;
                            f.animTime = randf(0.0f, 1.0f);
                            fairies.push_back(f);
                        }
                    }
                }

                if (isDashing) {
                    TrailPiece t;
                    t.x = playerX + 8.0f;
                    t.y = playerY + playerH * 0.45f;
                    t.w = 68.0f;
                    t.h = 12.0f;
                    t.life = 0.22f;
                    t.maxLife = 0.22f;
                    t.colorBand = (int)(animTime * 40.0f);
                    trail.push_back(t);
                }

                for (auto& t : trail) {
                    t.x -= worldSpeed * 0.65f * FIXED_DT;
                    t.life -= FIXED_DT;
                }
                trail.erase(
                    std::remove_if(trail.begin(), trail.end(),
                        [](const TrailPiece& t) { return t.life <= 0.0f; }),
                    trail.end()
                );

                for (auto& s : sparks) {
                    s.x += s.vx * FIXED_DT;
                    s.y += s.vy * FIXED_DT;
                    s.vy += 520.0f * FIXED_DT;
                    s.life -= FIXED_DT;
                }
                sparks.erase(
                    std::remove_if(sparks.begin(), sparks.end(),
                        [](const Spark& s) { return s.life <= 0.0f; }),
                    sparks.end()
                );

                playerRect = { playerX + 10.0f, playerY + 8.0f, playerW - 18.0f, playerH - 14.0f };

                for (auto& s : stars) {
                    if (!s.active) continue;
                    s.x -= worldSpeed * FIXED_DT;
                    SDL_FRect r = { s.x, s.y, s.w, s.h };

                    if (Intersects(playerRect, r)) {
                        if (isDashing) {
                            s.active = false;
                            score += 100.0f;
                            SpawnStarBurst(sparks, s.x + s.w * 0.5f, s.y + s.h * 0.5f);
                        }
                        else {
                            killPlayer();
                            break;
                        }
                    }
                }

                for (auto& f : fairies) {
                    if (!f.active) continue;
                    f.x -= worldSpeed * FIXED_DT;
                    f.animTime += FIXED_DT;
                    SDL_FRect r = { f.x, f.y, f.w, f.h };

                    if (Intersects(playerRect, r)) {
                        f.active = false;
                        dashCharges = std::min(3, dashCharges + 1);
                        score += 50.0f;
                    }
                }

                stars.erase(
                    std::remove_if(stars.begin(), stars.end(),
                        [](const StarCrystal& s) { return !s.active || s.x + s.w < -100.0f; }),
                    stars.end()
                );

                fairies.erase(
                    std::remove_if(fairies.begin(), fairies.end(),
                        [](const Fairy& f) { return !f.active || f.x + f.w < -100.0f; }),
                    fairies.end()
                );
            }

            accumulator -= FIXED_DT;
        }

        SDL_SetRenderDrawColor(renderer, 163, 199, 255, 255);
        SDL_RenderClear(renderer);

        SDL_Rect skyBand = { 0, 0, (int)SCREEN_W, 300 };
        SDL_SetRenderDrawColor(renderer, 193, 220, 255, 255);
        SDL_RenderFillRect(renderer, &skyBand);

        SDL_Rect horizon = { 0, 260, (int)SCREEN_W, 284 };
        SDL_SetRenderDrawColor(renderer, 255, 210, 240, 255);
        SDL_RenderFillRect(renderer, &horizon);

        auto drawCloudLayer = [&](SDL_Texture* tex, float offset, float y, float alpha) {
            SDL_SetTextureAlphaMod(tex, (Uint8)alpha);
            const float tileW = 256.0f;
            float start = -fmod(offset, tileW);
            for (int i = -1; i < 6; i++) {
                SDL_FRect d = { start + i * tileW, y, tileW, 140.0f };
                DrawTexture(renderer, tex, nullptr, d);
            }
            SDL_SetTextureAlphaMod(tex, 255);
            };

        drawCloudLayer(cloud0, cloudOffset0, 40.0f, 170);
        drawCloudLayer(cloud1, cloudOffset1, 80.0f, 185);
        drawCloudLayer(cloud2, cloudOffset2, 140.0f, 210);
        drawCloudLayer(cloud3, cloudOffset3, 195.0f, 235);

        for (const auto& p : platforms) {
            SDL_Rect body = { (int)p.x, (int)p.y, (int)p.w, (int)p.h };
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &body);

            SDL_Rect topGlow = { (int)p.x, (int)p.y, (int)p.w, 6 };
            SDL_SetRenderDrawColor(renderer, 255, 180, 230, 255);
            SDL_RenderFillRect(renderer, &topGlow);
        }

        for (const auto& t : trail) {
            float alpha = t.life / t.maxLife;
            SDL_Color c = RainbowColor(t.colorBand);
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, (Uint8)(alpha * 180.0f));
            SDL_Rect r = { (int)t.x, (int)t.y, (int)(t.w * alpha), (int)t.h };
            SDL_RenderFillRect(renderer, &r);
        }

        for (const auto& s : stars) {
            SDL_FRect d = { s.x, s.y, s.w, s.h };
            DrawTexture(renderer, texStar, nullptr, d);
        }

        for (const auto& f : fairies) {
            SDL_FRect d = { f.x, f.y + std::sin(f.animTime * 6.0f) * 5.0f, f.w, f.h };
            DrawAnim(renderer, fairyAnim, f.animTime, d);
        }

        SDL_FRect unicornDst = { playerX, playerY, playerW, playerH };

        if (isDashing) {
            SDL_Color c = RainbowColor((int)(animTime * 24.0f));
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 110);
            SDL_Rect glow = { (int)(playerX - 12), (int)(playerY - 8), (int)(playerW + 24), (int)(playerH + 16) };
            SDL_RenderFillRect(renderer, &glow);
        }

        if (!grounded && vy < -20.0f) {
            DrawAnim(renderer, jumpAnim, animTime, unicornDst);
        }
        else if (!grounded && vy >= -20.0f) {
            DrawAnim(renderer, fallAnim, animTime, unicornDst);
        }
        else {
            DrawAnim(renderer, runAnim, animTime, unicornDst);
        }

        for (const auto& s : sparks) {
            float alpha = s.life / s.maxLife;
            SDL_Color c = RainbowColor(s.colorBand);
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, (Uint8)(255 * alpha));
            SDL_Rect r = { (int)s.x, (int)s.y, (int)(8 * alpha + 2), (int)(8 * alpha + 2) };
            SDL_RenderFillRect(renderer, &r);
        }

        for (int i = 0; i < wishes; i++) {
            SDL_SetRenderDrawColor(renderer, 255, 120, 200, 255);
            SDL_Rect r = { 24 + i * 24, 20, 16, 16 };
            SDL_RenderFillRect(renderer, &r);
        }

        for (int i = 0; i < dashCharges; i++) {
            SDL_SetRenderDrawColor(renderer, 255, 240, 80, 255);
            SDL_Rect r = { 24 + i * 24, 46, 16, 10 };
            SDL_RenderFillRect(renderer, &r);
        }

        if (dead) {
            SDL_SetRenderDrawColor(renderer, 40, 0, 20, 180);
            SDL_Rect overlay = { 0, 0, (int)SCREEN_W, (int)SCREEN_H };
            SDL_RenderFillRect(renderer, &overlay);
        }

        std::ostringstream title;
        title << "Robot Unicorn Attack Prototype  |  Score: " << (int)score
            << "  |  Wishes: " << wishes
            << "  |  Dash: " << dashCharges;
        if (dead) title << "  |  GAME OVER - Press R";
        SDL_SetWindowTitle(window, title.str().c_str());

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texRun);
    SDL_DestroyTexture(texJump);
    SDL_DestroyTexture(texFall);
    SDL_DestroyTexture(texFairy);
    SDL_DestroyTexture(texStar);
    SDL_DestroyTexture(cloud0);
    SDL_DestroyTexture(cloud1);
    SDL_DestroyTexture(cloud2);
    SDL_DestroyTexture(cloud3);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}