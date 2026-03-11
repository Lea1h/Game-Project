#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>


enum GameState { STATE_MENU, STATE_PLAYING, STATE_WIN, STATE_LOSE };

std::string formatTime(Uint32 ms) {
    if (ms == 0) return "00:00";
    Uint32 totalSeconds = ms / 1000;
    Uint32 minutes = totalSeconds / 60;
    Uint32 seconds = totalSeconds % 60;
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << minutes << ":" << std::setfill('0') << std::setw(2) << seconds;
    return ss.str();
}

void drawText(SDL_Renderer* renderer, TTF_Font* font, std::string text, int x, int y, SDL_Color color, bool center = true) {
    if (!font) return; 
    SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect rect = { center ? x - (surf->w / 2) : x, y, surf->w, surf->h };
    SDL_RenderCopy(renderer, tex, NULL, &rect);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

int main(int argc, char* argv[]) {

    srand(time(NULL));
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    Mix_Init(MIX_INIT_MP3);
    Mix_AllocateChannels(32); 

    int windowWidth = 1280;
    int windowHeight = 720;
    SDL_Window* window = SDL_CreateWindow("Hungry Bunny", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("res/cute_font.ttf", 24);
    TTF_Font* timerFont = TTF_OpenFont("res/cute_font.ttf", 45); 
    TTF_Font* titleFont = TTF_OpenFont("res/cute_font.ttf", 90); 
    if(!font) font = TTF_OpenFont("res/cute_font.ttf", 24);
    if(!timerFont) timerFont = TTF_OpenFont("res/cute_font.ttf", 45);
    if(!titleFont) titleFont = TTF_OpenFont("res/cute_font.ttf", 90);

    auto LoadMusic = [](std::string n) { Mix_Music* m = Mix_LoadMUS(("res/"+n).c_str()); if(!m) m=Mix_LoadMUS(("res/"+n).c_str()); return m; };
    auto LoadChunk = [](std::string n) { Mix_Chunk* c = Mix_LoadWAV(("res/"+n).c_str()); if(!c) c=Mix_LoadWAV(("res/"+n).c_str()); return c; };

    Mix_Music* bgMusic = LoadMusic("backgroundmusic.mp3");
    Mix_Music* menuMusic = LoadMusic("menumusic.mp3"); 
    Mix_Chunk* runSound = LoadChunk("running.mp3");
    Mix_Chunk* eatSound = LoadChunk("eating.mp3");
    Mix_Chunk* winSound = LoadChunk("win.mp3");
    Mix_Chunk* loseSound = LoadChunk("lose.mp3");

    GameState state = STATE_MENU;
    if(menuMusic) Mix_PlayMusic(menuMusic, -1);

    SDL_Rect worldBoundary = { -2000, -2000, 4000, 4000 }; 
    SDL_FRect bunny = { 0.0f, 0.0f, 60.0f, 60.0f };
    float baseSpeed = 0.75f; 
    SDL_FRect camera = { 0.0f, 0.0f, 1280.0f, 720.0f };
    
    int eatenCount = 10;
    int runChannel = -1;
    Uint32 gameStartTime = 0, hungerTimer = 0, finalScoreTime = 0;
    Uint32 bestWinTime = 0, bestLoseTime = 0; 

    std::vector<SDL_FRect> orangeCubes, darkOrangeCubes;

    auto ResetGame = [&]() {
        eatenCount = 10; bunny.x = 0; bunny.y = 0;
        orangeCubes.clear(); darkOrangeCubes.clear();
        for (int i = 0; i < 45; i++) orangeCubes.push_back({ (float)(rand()%3800-1900), (float)(rand()%3800-1900), 40.0f, 15.0f }); 
        for (int i = 0; i < 25; i++) darkOrangeCubes.push_back({ (float)(rand()%3800-1900), (float)(rand()%3800-1900), 40.0f, 15.0f });
        state = STATE_PLAYING;
        gameStartTime = SDL_GetTicks(); hungerTimer = SDL_GetTicks();
        Mix_HaltMusic(); if(bgMusic) Mix_PlayMusic(bgMusic, -1);
    };

    bool running = true; SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mx, my; SDL_GetMouseState(&mx, &my);
                if (state == STATE_MENU && mx > 540 && mx < 740 && my > 400 && my < 460) ResetGame();
                else if (state != STATE_PLAYING) {
                    if (mx > 540 && mx < 740 && my > 410 && my < 470) ResetGame();
                    if (mx > 540 && mx < 740 && my > 480 && my < 540) { 
                        state = STATE_MENU; Mix_HaltMusic(); if(menuMusic) Mix_PlayMusic(menuMusic, -1);
                    }
                }
            }
        }

        if (state == STATE_PLAYING) {
            if (SDL_GetTicks() - hungerTimer >= 5000) { eatenCount--; hungerTimer = SDL_GetTicks(); }

            bunny.w = 50.0f + (eatenCount * 1.5f);
            bunny.h = 50.0f + (eatenCount * 1.5f);
            float currentSpeed = (eatenCount >= 20) ? baseSpeed * 1.35f : baseSpeed;

            const Uint8* keys = SDL_GetKeyboardState(NULL);
            bool moving = false;
            if (keys[SDL_SCANCODE_UP] && bunny.y > worldBoundary.y) { bunny.y -= currentSpeed; moving = true; }
            if (keys[SDL_SCANCODE_DOWN] && bunny.y + bunny.h < worldBoundary.y + worldBoundary.h) { bunny.y += currentSpeed; moving = true; }
            if (keys[SDL_SCANCODE_LEFT] && bunny.x > worldBoundary.x) { bunny.x -= currentSpeed; moving = true; }
            if (keys[SDL_SCANCODE_RIGHT] && bunny.x + bunny.w < worldBoundary.x + worldBoundary.w) { bunny.x += currentSpeed; moving = true; }

            if (moving) {
                if (runChannel == -1 || Mix_Playing(1) == 0) runChannel = Mix_PlayChannel(1, runSound, -1);
            } else { Mix_HaltChannel(1); runChannel = -1; }

            camera.x = bunny.x - 640 + (bunny.w / 2); camera.y = bunny.y - 360 + (bunny.h / 2);

            for (auto it = orangeCubes.begin(); it != orangeCubes.end(); ) {
                if (SDL_HasIntersectionF(&bunny, &(*it))) { 
                    Mix_PlayChannel(-1, eatSound, 0); 
                    it = orangeCubes.erase(it); eatenCount++; 
                } else ++it;
            }
            for (auto it = darkOrangeCubes.begin(); it != darkOrangeCubes.end(); ) {
                if (SDL_HasIntersectionF(&bunny, &(*it))) { 
                    Mix_PlayChannel(-1, eatSound, 0); 
                    it = darkOrangeCubes.erase(it); eatenCount--; 
                } else ++it;
            }

            if (eatenCount >= 30) { 
                state = STATE_WIN; finalScoreTime = SDL_GetTicks() - gameStartTime;
                if (bestWinTime == 0 || finalScoreTime < bestWinTime) bestWinTime = finalScoreTime;
                Mix_HaltMusic(); Mix_HaltChannel(-1); Mix_PlayChannel(-1, winSound, 0); 
            }
            if (eatenCount <= 0) { 
                state = STATE_LOSE; finalScoreTime = SDL_GetTicks() - gameStartTime;
                if (bestLoseTime == 0 || finalScoreTime < bestLoseTime) bestLoseTime = finalScoreTime; 
                Mix_HaltMusic(); Mix_HaltChannel(-1); Mix_PlayChannel(-1, loseSound, 0); 
            }
        }

        if (state == STATE_MENU) {
            SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255); SDL_RenderClear(renderer); 
            SDL_Rect ground = {0, 450, 1280, 270}; SDL_SetRenderDrawColor(renderer, 80, 170, 80, 255); SDL_RenderFillRect(renderer, &ground);
            SDL_Color white = {255,255,255,255};
            drawText(renderer, titleFont, "Hungry Bunny", 640, 150, white);
            
            SDL_Rect recordBox = {50, 530, 420, 150}; 
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 50); SDL_RenderFillRect(renderer, &recordBox); 
            drawText(renderer, font, "RECORDS:", 80, 545, white, false);
            drawText(renderer, font, "BEST SCORE WINNING: " + formatTime(bestWinTime), 80, 585, white, false);
            drawText(renderer, font, "BEST SCORE LOSING: " + formatTime(bestLoseTime), 80, 625, white, false);

            SDL_Rect bunnyImg = { 950, 300, 160, 160 }; SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &bunnyImg);
            SDL_Rect btn = { 540, 400, 200, 60 }; SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255); SDL_RenderFillRect(renderer, &btn);
            drawText(renderer, font, "PLAY", 640, 415, white);
        } else {
            
            SDL_SetRenderDrawColor(renderer, 30, 60, 30, 255); SDL_RenderClear(renderer);

            
            SDL_SetRenderDrawColor(renderer, 100, 180, 100, 255);
            SDL_FRect world = { worldBoundary.x - camera.x, worldBoundary.y - camera.y, 4000, 4000 }; 
            SDL_RenderFillRectF(renderer, &world);
            
            for (auto& c : orangeCubes) {
                SDL_FRect d = { c.x-camera.x, c.y-camera.y, c.w, c.h }; SDL_SetRenderDrawColor(renderer, 255,140,0,255); SDL_RenderFillRectF(renderer, &d);
            }
            for (auto& c : darkOrangeCubes) {
                SDL_FRect d = { c.x-camera.x, c.y-camera.y, c.w, c.h }; SDL_SetRenderDrawColor(renderer, 139,69,19,255); SDL_RenderFillRectF(renderer, &d);
            }

            Uint8 bCol = 255; if (eatenCount < 10) bCol = 80 + (eatenCount * 17);
            SDL_SetRenderDrawColor(renderer, bCol, bCol, bCol, 255);
            SDL_FRect bDraw = { bunny.x-camera.x, bunny.y-camera.y, bunny.w, bunny.h }; SDL_RenderFillRectF(renderer, &bDraw);

            SDL_Color timerCol = (eatenCount < 5) ? SDL_Color{255,50,50,255} : SDL_Color{255,255,255,255};
            std::string curT = formatTime(state == STATE_PLAYING ? SDL_GetTicks()-gameStartTime : finalScoreTime);
            drawText(renderer, timerFont, curT, 640, 10, timerCol); 

            for (int i = 0; i < 30; i++) {
                SDL_FRect ui = { 30.0f + (i * 41.0f), 70.0f, 38.0f, 15.0f };
                if (i < eatenCount) SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
                else SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderFillRectF(renderer, &ui);
            }

            if (state != STATE_PLAYING) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160); SDL_Rect full = {0,0,1280,720}; SDL_RenderFillRect(renderer, &full);
                SDL_Rect box = { 415, 180, 450, 420 }; SDL_SetRenderDrawColor(renderer, 255, 250, 240, 255); SDL_RenderFillRect(renderer, &box);
                SDL_Color orange = {255, 140, 0, 255};
                if (state == STATE_WIN) drawText(renderer, font, "Congrat, the bunny is full!!", 640, 230, orange);
                else drawText(renderer, font, "the bunny died from hunger T-T.", 640, 230, orange);
                drawText(renderer, font, "YOUR SCORE IS " + formatTime(finalScoreTime), 640, 310, orange);
                
                SDL_Rect rbtn = { 540, 410, 200, 60 }; SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255); SDL_RenderFillRect(renderer, &rbtn);
                drawText(renderer, font, "PLAY AGAIN", 640, 425, {255,255,255,255});
                SDL_Rect mbtn = { 540, 485, 200, 60 }; SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255); SDL_RenderFillRect(renderer, &mbtn);
                drawText(renderer, font, "MAIN MENU", 640, 500, {255,255,255,255});
            }
        }
        SDL_RenderPresent(renderer);
    }
        
    Mix_FreeMusic(bgMusic);
    Mix_FreeMusic(menuMusic);
    Mix_FreeChunk(runSound);
    Mix_FreeChunk(eatSound);
    Mix_FreeChunk(winSound);
    Mix_FreeChunk(loseSound);
    TTF_CloseFont(font);
    TTF_CloseFont(timerFont);
    TTF_CloseFont(titleFont);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    Mix_CloseAudio();
    TTF_Quit();
    SDL_Quit();
    return 0;
}