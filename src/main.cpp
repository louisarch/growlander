#include "game.hpp"
#include "touch.hpp"
#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {
constexpr int W = 1280, H = 720, T = 40;
struct Color {
  Uint8 r, g, b, a = 255;
};
const Color Ink{22, 36, 52}, White{235, 244, 242}, Muted{157, 182, 188},
    Gold{255, 204, 103}, Mint{112, 229, 175};
struct Art {
  SDL_Renderer *r;
  TTF_Font *small = nullptr;
  TTF_Font *normal = nullptr;
  TTF_Font *title = nullptr;
  std::map<std::string, SDL_Texture *> cache;
  bool textFilteringOK = true;
  Art(SDL_Renderer *renderer, const std::string &font) : r(renderer) {
    small = TTF_OpenFont(font.c_str(), 14);
    normal = TTF_OpenFont(font.c_str(), 17);
    title = TTF_OpenFont(font.c_str(), 26);
  }
  ~Art() {
    clear();
    if (small)
      TTF_CloseFont(small);
    if (normal)
      TTF_CloseFont(normal);
    if (title)
      TTF_CloseFont(title);
  }
  void clear() {
    for (auto &p : cache)
      SDL_DestroyTexture(p.second);
    cache.clear();
  }
  void rect(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect q{x, y, w, h};
    SDL_RenderFillRect(r, &q);
  }
  void line(int x, int y, int xx, int yy, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(r, x, y, xx, yy);
  }
  void border(int x, int y, int w, int h, Color c, int thick = 1) {
    for (int i = 0; i < thick; i++) {
      SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
      SDL_Rect q{x + i, y + i, w - i * 2, h - i * 2};
      SDL_RenderDrawRect(r, &q);
    }
  }
  void text(const std::string &s, int x, int y, Color c = White,
            int size = 17) {
    if (s.empty())
      return;
    if (cache.size() > 800)
      clear();
    std::string key = s + "|" + std::to_string(size) + "|" +
                      std::to_string(c.r) + "," + std::to_string(c.g) + "," +
                      std::to_string(c.b);
    auto it = cache.find(key);
    SDL_Texture *tex = nullptr;
    if (it == cache.end()) {
      SDL_Color col{c.r, c.g, c.b, 255};
      SDL_Surface *surf = TTF_RenderUTF8_Blended(size == 14   ? small
                                                 : size == 26 ? title
                                                              : normal,
                                                 s.c_str(), col);
      if (!surf)
        return;
      tex = SDL_CreateTextureFromSurface(r, surf);
      SDL_FreeSurface(surf);
      if (!tex)
        return;
      // Fractional logical-size scaling must not drop thin glyph strokes.
      // Filter only antialiased text, leaving pixel-art primitives unchanged.
      SDL_ScaleMode mode = SDL_ScaleModeNearest;
      if (SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear) != 0 ||
          SDL_GetTextureScaleMode(tex, &mode) != 0 || mode != SDL_ScaleModeLinear)
        textFilteringOK = false;
      cache.emplace(key, tex);
    } else
      tex = it->second;
    int w = 0, h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst{x, y, w, h};
    SDL_RenderCopy(r, tex, nullptr, &dst);
  }
  void panel(int x, int y, int w, int h) {
    rect(x + 3, y + 4, w, h, {8, 23, 36, 55});
    rect(x, y, w, h, {20, 39, 51, 240});
    border(x, y, w, h, {62, 88, 98});
  }
  void block(gl::Tile type, int x, int y, int s, bool grass = false) {
    using gl::Tile;
    switch (type) {
    case Tile::Dirt:
      rect(x, y, s, s, {131, 86, 59});
      rect(x, y, s, 4, {161, 106, 66});
      rect(x + 5, y + s / 2, s / 4, 4, {105, 67, 53});
      rect(x + s * 2 / 3, y + s / 4, 5, 5, {183, 125, 78});
      if (grass) {
        rect(x, y, s, 7, {74, 166, 94});
        rect(x, y, s, 3, {143, 209, 113});
        for (int i = 5; i < s; i += 11)
          rect(x + i, y + 5, 4, 6, {74, 166, 94});
      }
      break;
    case Tile::Stone:
      rect(x, y, s, s, {103, 120, 133});
      rect(x + 2, y + 2, s - 4, 4, {149, 164, 171});
      line(x + 2, y + s - 2, x + s - 2, y + s - 2, {64, 83, 98});
      line(x + s / 3, y + 7, x + s / 2, y + s / 2, {78, 96, 111});
      line(x + s / 2, y + s / 2, x + s - 4, y + s / 2 + 4, {78, 96, 111});
      break;
    case Tile::Wood:
      rect(x, y, s, s, {170, 114, 62});
      rect(x + 3, y + 2, 4, s - 4, {205, 154, 82});
      rect(x + s / 2, y, 3, s, {121, 76, 48});
      rect(x + s - 5, y, 2, s, {121, 76, 48});
      line(x + 9, y + s / 3, x + s / 2 - 3, y + s / 3 + 5, {121, 76, 48});
      break;
    case Tile::Brick:
      rect(x, y, s, s, {190, 107, 85});
      for (int k = 0; k < 2; k++) {
        rect(x, y + k * s / 2, s, 3, {112, 73, 70});
        rect(x + (k % 2 ? s / 4 : s / 2), y + k * s / 2, 3, s / 2,
             {112, 73, 70});
      }
      rect(x + 3, y + 4, s / 3, 3, {224, 145, 109});
      break;
    case Tile::Bedrock:
      rect(x, y, s, s, {43, 55, 74});
      rect(x + 4, y + 5, s / 2, 5, {67, 79, 94});
      rect(x + s / 2, y + s / 2, s / 3, 6, {28, 40, 56});
      break;
    default:
      break;
    }
    if (type != Tile::Air)
      border(x, y, s, s, {24, 39, 49, 45});
  }
  void icon(gl::Item item, int x, int y, int s) {
    if (item == gl::Item::Fist) {
      rect(x + 4, y + s / 3, s - 9, s / 2, {236, 183, 136});
      rect(x + 7, y + s / 3 - 5, s - 14, 6, {255, 212, 164});
      rect(x + s - 11, y + s / 2, 8, s / 3, {198, 136, 99});
      rect(x + 3, y + s * 3 / 4, 9, 5, {60, 157, 158});
    } else if (item == gl::Item::Seed) {
      rect(x + s / 2 - 2, y + s / 2, 4, s / 2, {214, 171, 96});
      rect(x + 2, y + s / 4, s / 2, 9, {99, 204, 126});
      rect(x + s / 2, y + 4, s / 2 - 1, 10, {159, 229, 132});
    } else
      block(static_cast<gl::Tile>(static_cast<int>(item)), x, y, s);
  }
  void avatar(int x, int y, float time, bool walking, bool grounded,
              int direction = 1, int scale = 1) {
    int sway = walking && grounded ? int(std::sin(time * 14) * 3) : 0;
    auto p = [&](int xx, int yy, int ww, int hh, Color c) {
      rect(x + xx * scale, y + yy * scale, ww * scale, hh * scale, c);
    };
    p(2, 49, 24, 7, {16, 36, 45, 55});
    p(4, 38, 8, 15 + sway, {38, 58, 81});
    p(17, 38, 8, 15 - sway, {38, 58, 81});
    p(1, 50 + sway, 12, 6, {31, 42, 58});
    p(17, 50 - sway, 13, 6, {31, 42, 58});
    p(2, 22, 25, 20, {48, 142, 146});
    p(7, 24, 14, 17, {68, 182, 168});
    p(1, 26, 5, 17, {240, 183, 136});
    p(24, 26, 5, 17, {240, 183, 136});
    p(5, 3, 20, 20, {241, 191, 145});
    p(4, 2, 22, 7, {52, 53, 64});
    p(3, 9, 5, 7, {52, 53, 64});
    p(direction > 0 ? 19 : 9, 12, 3, 4, {30, 49, 61});
    p(2, 20, 28, 5, {244, 165, 86});
    p(-6, 22, 10, 5, {224, 132, 71});
  }
  void tree(int x, int y, float growth) {
    float pct = std::clamp(growth / gl::Game::GrowSeconds, 0.f, 1.f);
    int ht = 12 + int(43 * pct);
    int stem = x + 18;
    rect(stem, y + 40 - ht, 6, ht, {147, 101, 59});
    int cy = y + 35 - ht;
    int span = 14 + int(22 * pct);
    rect(x + 20 - span / 2, cy - 9, span, 17, {68, 155, 104});
    rect(x + 12 - span / 2, cy - 2, span, 16, {80, 181, 113});
    rect(x + 24 - span / 2, cy - 7, span, 18, {119, 204, 124});
    if (pct >= 1) {
      rect(x + 6, cy + 3, 7, 8, Gold);
      rect(x + 29, cy - 5, 7, 8, Gold);
      rect(x + 19, cy - 13, 7, 8, Gold);
    }
    rect(x + 1, y + 43, 38, 4, {23, 47, 52, 110});
    rect(x + 1, y + 43, int(38 * pct), 4, pct >= 1 ? Gold : Mint);
  }
};
std::string findFont() {
#ifdef __ANDROID__
  return "DejaVuSans.ttf"; // SDL_RWops reads packaged Android assets.
#endif
  char *base = SDL_GetBasePath();
  std::string path = base ? base : "";
  SDL_free(base);
  std::vector<std::string> candidates{path + "assets/DejaVuSans.ttf",
                                      "assets/DejaVuSans.ttf"};
#ifdef ASSET_DIR
  candidates.emplace_back(std::string(ASSET_DIR) + "/DejaVuSans.ttf");
#endif
  for (const auto &p : candidates)
    if (std::filesystem::exists(p))
      return p;
  return "";
}
bool screenshot(SDL_Renderer *r, const std::string &path) {
  int w, h;
  SDL_GetRendererOutputSize(r, &w, &h);
  SDL_Surface *s =
      SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
  if (!s)
    return false;
  bool ok = SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_RGBA32, s->pixels,
                                 s->pitch) == 0;
  if (ok)
    ok = SDL_SaveBMP(s, path.c_str()) == 0;
  SDL_FreeSurface(s);
  return ok;
}
} // namespace

int main(int argc, char **argv) {
  bool smoke = false, touchTest = false, mobilePreview = false, wideTest = false;
  std::string shot, savePath;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--mobile-preview")
      mobilePreview = true;
    else if (a == "--touch-wide-test")
      wideTest = touchTest = smoke = mobilePreview = true;
    else if (a == "--touch-test")
      touchTest = smoke = mobilePreview = true;
    else if (a == "--smoke-test")
      smoke = true;
    else if ((a == "--screenshot" || a == "--save") && i + 1 < argc) {
      if (a == "--save")
        savePath = argv[++i];
      else
        shot = argv[++i];
    } else {
      std::cerr << "Penggunaan: growlander [--smoke-test] [--screenshot "
                   "file.bmp] [--save world.glw] [--mobile-preview] [--touch-test] [--touch-wide-test]\n";
      return 2;
    }
  }
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#ifdef __ANDROID__
  mobilePreview = true;
#endif
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::cerr << SDL_GetError() << '\n';
    return 1;
  }
  if (TTF_Init() != 0) {
    std::cerr << TTF_GetError() << '\n';
    SDL_Quit();
    return 1;
  }
  SDL_Window *window = SDL_CreateWindow(
      "Growlander | Kebun Pertama - Single-player", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, wideTest ? 1560 : W, H, SDL_WINDOW_RESIZABLE
#ifdef __ANDROID__
      | SDL_WINDOW_FULLSCREEN_DESKTOP
#endif
      );
  if (!window) {
    std::cerr << SDL_GetError() << '\n';
    TTF_Quit();
    SDL_Quit();
    return 1;
  }
  SDL_SetWindowMinimumSize(window, 640, 360);
  SDL_Renderer *renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer)
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) {
    std::cerr << SDL_GetError() << '\n';
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }
  SDL_RenderSetLogicalSize(renderer, W, H);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  int result = 0;
  {
    Art art(renderer, findFont());
    if (!art.small || !art.normal || !art.title) {
      std::cerr << "Font tidak tersedia: " << TTF_GetError() << '\n';
      result = 1;
    } else {
      gl::Game game;
      if (savePath.empty()) {
#ifdef __EMSCRIPTEN__
        savePath = "/growlander-world.glw";
        EM_ASM({
          try {
            const saved = localStorage.getItem('growlander.world.v1');
            if (saved) FS.writeFile(UTF8ToString($0), saved);
          } catch (error) { console.warn('Penyimpanan browser tidak tersedia', error); }
        }, savePath.c_str());
#else
        char *pref = SDL_GetPrefPath("Growlander", "Growlander");
        if (pref) {
          savePath = std::string(pref) + "world.glw";
          SDL_free(pref);
        } else
          savePath = "growlander-world.glw";
#endif
      }
      if (!smoke && std::filesystem::exists(savePath))
        game.load(savePath);
      auto saveGame = [&]() {
        if (!game.save(savePath)) return false;
#ifdef __EMSCRIPTEN__
        const bool persisted = EM_ASM_INT({
          try {
            localStorage.setItem('growlander.world.v1', FS.readFile(UTF8ToString($0), {encoding:'utf8'}));
            return 1;
          } catch (error) { console.warn('Simpan browser gagal', error); return 0; }
        }, savePath.c_str());
        if (!persisted) {
          game.message = "Simpan browser gagal. Izinkan penyimpanan lokal.";
          return false;
        }
#endif
        return true;
      };
      bool running = true, paused = false, bag = false, help = false,
           left = false, right = false;
      std::array<bool, SDL_NUM_SCANCODES> keys{};
      int selected = 0, mx = 640, my = 400, direction = 1, frame = 0;
      float cameraX = 0, cameraY = 0, cooldown = 0, time = 0;
      bool jump = false;
      gl::TouchState touches;
      bool hintMove = false, hintJump = false, hintAction = false;
      bool targetSet = false;
      float frozenElapsed = 0;
      bool panelPauseOK = false, backgroundPauseOK = false;
      int targetX = 12, targetY = 24;
      std::string lastMessage;
      float messageTimer = 0;
      auto clearInput = [&]() {
        touches.clear(); keys.fill(false); left = right = jump = false;
      };
      auto uiClick = [&](int x, int y) {
        if (gl::inside(x,y,1184,6,72,48)) {
          bag = help = paused = false; clearInput(); return true;
        }
        if (gl::inside(x,y,872,6,96,48)) {
          bag = !bag; help = paused = false; clearInput(); return true;
        }
        if (gl::inside(x,y,976,6,96,48)) {
          help = !help; bag = paused = false; clearInput(); return true;
        }
        if (gl::inside(x,y,1080,6,96,48)) {
          paused = !paused; bag = help = false; clearInput(); return true;
        }
        if (bag || help || paused) {
          if (bag && gl::inside(x,y,330,283,620,270))
            selected = std::clamp((y-283)/45,0,5);
          if (paused) {
            if (gl::inside(x,y,330,314,620,48)) { paused=false; clearInput(); }
            else if (gl::inside(x,y,330,370,620,48)) saveGame();
            else if (gl::inside(x,y,330,426,620,48)) { if (saveGame()) running=false; }
            else if (gl::inside(x,y,330,482,620,48)) { game.player={}; game.player.y=20; targetSet=false; paused=false; clearInput(); }
          }
          return true;
        }
        if (gl::inside(x,y,448,630,384,72)) {
          selected = std::clamp((x-448)/64,0,5); return true;
        }
        return y < 60;
      };
      auto pointerDown = [&](std::int64_t id, int x, int y) {
        if (uiClick(x,y)) return;
        auto role = gl::touchRole(x,y);
        touches.down(id,role);
        if (role == gl::TouchRole::World) {
          targetX=int(std::floor((x+cameraX)/T));
          targetY=int(std::floor((y+cameraY)/T)); targetSet=true;
        }
      };
      float maxJump = game.player.y;
      Uint64 previous = SDL_GetPerformanceCounter();
      auto clickAt = [&](int tx, int ty, Uint8 button) {
        SDL_Event ev{};
        ev.type = SDL_MOUSEBUTTONDOWN;
        ev.button.button = button;
        ev.button.x = int(tx * T - cameraX + 20);
        ev.button.y = int(ty * T - cameraY + 20);
        SDL_PushEvent(&ev);
        ev.type = SDL_MOUSEBUTTONUP;
        SDL_PushEvent(&ev);
      };
      auto keyEvent = [&](SDL_Scancode sc, bool down) {
        SDL_Event ev{};
        ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
        ev.key.keysym.scancode = sc;
        ev.key.keysym.sym = SDL_GetKeyFromScancode(sc);
        SDL_PushEvent(&ev);
      };
      while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt =
            smoke
                ? .1f
                : std::min(.05f, float(double(now - previous) /
                                       double(SDL_GetPerformanceFrequency())));
        previous = now;
        time += dt;
        messageTimer = std::max(0.f, messageTimer-dt);
        cooldown = std::max(0.f, cooldown - dt);
        cameraX = std::clamp(game.player.x * T - W * .40f, 0.f,
                             float(gl::WorldWidth * T - W));
        cameraY = std::clamp(game.player.y * T - H * .53f, 0.f,
                             float(gl::WorldHeight * T - H));
        if (touchTest) {
          auto finger = [&](Uint32 type, SDL_FingerID id, int x, int y) {
            SDL_Event ev{}; ev.type=type; ev.tfinger.touchId=1; ev.tfinger.fingerId=id;
            int wx=0,wy=0,ww=0,wh=0;
            SDL_RenderLogicalToWindow(renderer,float(x),float(y),&wx,&wy);
            SDL_GetWindowSize(window,&ww,&wh);
            ev.tfinger.x=float(wx)/ww; ev.tfinger.y=float(wy)/wh; SDL_PushEvent(&ev);
          };
          auto tap = [&](int when, SDL_FingerID id, int x, int y) {
            if(frame==when) finger(SDL_FINGERDOWN,id,x,y);
            if(frame==when+1) finger(SDL_FINGERUP,id,x,y);
          };
          auto target = [&](int when, SDL_FingerID id, int x, int y) {
            tap(when,id,int(x*T-cameraX+20),int(y*T-cameraY+20));
          };
          tap(1,10,790,660); // Benih
          target(3,11,12,23); tap(5,12,1090,530);
          target(8,13,13,23); tap(10,14,1090,530);
          if(frame==13) finger(SDL_FINGERDOWN,1,160,630);
          tap(14,2,1190,630); // second finger jumps while first keeps walking
          if(frame==16) finger(SDL_FINGERUP,1,160,630);
          target(35,15,9,24); tap(37,16,1090,630); tap(41,17,1090,630);
          tap(44,18,730,660); target(46,19,9,24); tap(48,20,1090,530);
          tap(52,21,910,30); // Inventaris pauses simulation
          if(frame==54) frozenElapsed=game.elapsed;
          if(frame==57) panelPauseOK=bag && game.elapsed==frozenElapsed;
          tap(58,22,1220,30);
          if(frame==62) { SDL_Event ev{}; ev.type=SDL_APP_WILLENTERBACKGROUND; SDL_PushEvent(&ev); }
          if(frame==64) frozenElapsed=game.elapsed;
          if(frame==67) backgroundPauseOK=paused && game.elapsed==frozenElapsed;
          tap(68,23,1220,30);
          target(210,24,12,23); tap(212,25,1090,630);
          if(frame==220) finger(SDL_FINGERDOWN,26,160,630);
          if(frame==225) finger(SDL_FINGERUP,26,160,630);
        } else if (smoke) {
          if (frame == 2) {
            keyEvent(SDL_SCANCODE_6, true);
            clickAt(12, 23, SDL_BUTTON_RIGHT);
          }
          if (frame == 6)
            clickAt(13, 23, SDL_BUTTON_RIGHT);
          if (frame == 8)
            keyEvent(SDL_SCANCODE_SPACE, true);
          if (frame == 9)
            keyEvent(SDL_SCANCODE_SPACE, false);
          if (frame == 35 || frame == 39)
            clickAt(9, 24, SDL_BUTTON_LEFT);
          if (frame == 42) {
            keyEvent(SDL_SCANCODE_5, true);
            clickAt(9, 24, SDL_BUTTON_RIGHT);
          }
          if (frame == 205)
            clickAt(12, 23, SDL_BUTTON_LEFT);
          if (frame == 215)
            keyEvent(SDL_SCANCODE_D, true);
          if (frame == 225)
            keyEvent(SDL_SCANCODE_D, false);
          if (frame == 230) {
            SDL_Event move{};
            move.type = SDL_MOUSEMOTION;
            move.motion.x = 1000;
            move.motion.y = 80;
            SDL_PushEvent(&move);
          }
        }
        SDL_Event e;
        bool singleLeft = false, singleRight = false;
        while (SDL_PollEvent(&e)) {
          if (e.type == SDL_QUIT) {
            running = false;
          } else if (e.type == SDL_APP_WILLENTERBACKGROUND) {
            clearInput(); paused = true;
            if (!smoke) saveGame();
          } else if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERUP || e.type == SDL_FINGERMOTION) {
            mobilePreview = true;
            // SDL_RenderSetLogicalSize already normalizes touch to the logical viewport.
            // Applying WindowToLogical again would offset taps on letterboxed phones.
            int x=int(e.tfinger.x*W), y=int(e.tfinger.y*H);
            if(e.type==SDL_FINGERDOWN) pointerDown(e.tfinger.fingerId,x,y);
            else if(e.type==SDL_FINGERUP) touches.up(e.tfinger.fingerId);
            else if(touches.role(e.tfinger.fingerId)==gl::TouchRole::World && !paused && !bag && !help) {
              targetX=int(std::floor((x+cameraX)/T)); targetY=int(std::floor((y+cameraY)/T)); targetSet=true;
            }
          } else if (e.type == SDL_WINDOWEVENT &&
                     e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            clearInput();
            if (!smoke) saveGame();
            if (!smoke)
              paused = true;
          } else if (e.type == SDL_KEYUP) {
            if (e.key.keysym.scancode < SDL_NUM_SCANCODES)
              keys[e.key.keysym.scancode] = false;
          } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            auto sc = e.key.keysym.scancode;
            if (sc < SDL_NUM_SCANCODES)
              keys[sc] = true;
            if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_6)
              selected = int(sc - SDL_SCANCODE_1);
            if (sc == SDL_SCANCODE_SPACE || sc == SDL_SCANCODE_W ||
                sc == SDL_SCANCODE_UP)
              jump = true;
            if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_AC_BACK) {
              if (bag || help) {
                bag = false;
                help = false;
              } else
                paused = !paused;
            }
            if (sc == SDL_SCANCODE_I) {
              bag = !bag;
              help = false;
            }
            if (sc == SDL_SCANCODE_H) {
              help = !help;
              bag = false;
            }
            if (sc == SDL_SCANCODE_F5)
              saveGame();
            if (sc == SDL_SCANCODE_F9) {
              if (!game.load(savePath))
                game.message = "Belum ada simpanan yang valid.";
            }
            if (sc == SDL_SCANCODE_R && !bag && !help) {
              game.player = {};
              game.player.x = 10;
              game.player.y = 20;
              game.message = "Kembali ke titik awal.";
            }
            if (paused && sc == SDL_SCANCODE_RETURN) {
              if (saveGame())
                running = false;
            }
          } else if (e.type == SDL_MOUSEWHEEL) {
            selected = (selected + (e.wheel.y > 0 ? 5 : 1)) % 6;
          } else if (e.type == SDL_MOUSEMOTION && e.motion.which != SDL_TOUCH_MOUSEID) {
            mx = e.motion.x;
            my = e.motion.y;
            if (!mobilePreview) targetSet=false;
          } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.which != SDL_TOUCH_MOUSEID) {
            mx=e.button.x; my=e.button.y;
            if (e.button.button==SDL_BUTTON_LEFT && uiClick(mx,my)) continue;
            auto role=gl::touchRole(mx,my);
            if(e.button.button==SDL_BUTTON_LEFT && role!=gl::TouchRole::World) {
              pointerDown(-900,mx,my); continue;
            }
            if(mobilePreview && e.button.button==SDL_BUTTON_LEFT) {
              pointerDown(-900,mx,my); continue;
            }
            if(e.button.button==SDL_BUTTON_LEFT) { left=true; singleLeft=true; }
            else if(e.button.button==SDL_BUTTON_RIGHT) { right=true; singleRight=true; }
          } else if (e.type == SDL_MOUSEBUTTONUP) {
            touches.up(-900);
            if(e.button.button==SDL_BUTTON_LEFT) left=false;
            if(e.button.button==SDL_BUTTON_RIGHT) right=false;
          }
        }
        bool frozen = paused || bag || help;
        float horizontal =
            std::clamp(float(keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) -
            float(keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) + touches.horizontal(), -1.f, 1.f);
        if (!frozen) {
          game.update(dt, horizontal, jump || touches.held(gl::TouchRole::Jump));
          hintMove = hintMove || horizontal != 0;
          hintJump = hintJump || jump || touches.held(gl::TouchRole::Jump);
          if (horizontal != 0)
            direction = horizontal > 0 ? 1 : -1;
          if(mobilePreview && !targetSet) {
            targetX=int(game.player.x)+direction*2; targetY=int(game.player.y+1.7f);
          }
          int tx = mobilePreview ? targetX : int(std::floor((mx+cameraX)/T));
          int ty = mobilePreview ? targetY : int(std::floor((my+cameraY)/T));
          bool hud = !mobilePreview && gl::touchRole(mx,my)!=gl::TouchRole::World;
          if (!hud && cooldown <= 0) {
            if (left || singleLeft || touches.held(gl::TouchRole::Punch)) {
              game.punch(tx,ty); cooldown=.22f; hintAction=true;
            } else if (right || singleRight || touches.held(gl::TouchRole::Place)) {
              game.place(tx,ty,static_cast<gl::Item>(selected)); cooldown=.22f; hintAction=true;
            }
          }
        }
        jump = false;
        maxJump = std::min(maxJump, game.player.y);
        // Original layered landscape, entirely generated from simple shapes.
        for (int y = 0; y < H; y++) {
          float k = float(y) / H;
          art.rect(
              0, y, W, 1,
              {Uint8(112 + 75 * k), Uint8(175 + 30 * k), Uint8(192 - 13 * k)});
        }
        int sunX = 1040 - int(cameraX * .06f), sunY = 190 - int(cameraY * .03f);
        art.rect(sunX - 36, sunY - 27, 72, 54, {250, 224, 167});
        art.rect(sunX - 27, sunY - 36, 54, 72, {250, 224, 167});
        for (int n = 0; n < 9; n++) {
          int x = int(n * 230 - cameraX * .15f) % 1700 - 100;
          int yy = 190 + (n % 3) * 30 - int(cameraY * .08f);
          art.rect(x, yy, 95, 15, {239, 239, 217, 155});
          art.rect(x + 17, yy - 12, 58, 17, {239, 239, 217, 155});
        }
        for (int layer = 0; layer < 2; layer++)
          for (int x = 0; x < W; x += 4) {
            float worldx = x + cameraX * (layer ? .25f : .12f);
            int yy =
                int(420 + layer * 70 + std::sin(worldx * .005f + layer) * 55 +
                    std::cos(worldx * .013f) * 20 - cameraY * .12f);
            art.rect(x, yy, 4, H - yy,
                     layer ? Color{90, 150, 143, 130}
                           : Color{120, 167, 158, 150});
          }
        int x0 = std::max(0, int(cameraX / T)),
            x1 = std::min(gl::WorldWidth - 1, int((cameraX + W) / T));
        int y0 = std::max(0, int(cameraY / T)),
            y1 = std::min(gl::WorldHeight - 1, int((cameraY + H) / T));
        for (int y = y0; y <= y1; y++)
          for (int x = x0; x <= x1; x++) {
            const auto &c = game.cell(x, y);
            int px = int(x * T - cameraX), py = int(y * T - cameraY);
            if (c.tile != gl::Tile::Air) {
              art.block(c.tile, px, py, T,
                        game.cell(x, y - 1).tile == gl::Tile::Air &&
                            c.tile == gl::Tile::Dirt);
              if (c.hits > 0) {
                art.line(px + 20, py + 3, px + 12, py + 20, Ink);
                art.line(px + 12, py + 20, px + 25, py + 30, Ink);
                art.line(px + 12, py + 20, px + 5, py + 29, Ink);
              }
            }
            if (c.growth >= 0)
              art.tree(px, py, c.growth);
          }
        // Spawn marker is decoration, not a copied sprite.
        int sx = int(7 * T - cameraX), sy = int(24 * T - cameraY);
        art.rect(sx + 10, sy - 49, 5, 49, {124, 90, 62});
        art.rect(sx - 9, sy - 56, 46, 25, {242, 219, 162});
        art.text("RUMAH", sx - 6, sy - 52, Ink, 14);
        int px = int(game.player.x * T - cameraX) - 2,
            py = int(game.player.y * T - cameraY) + 9;
        art.avatar(px, py, time, horizontal != 0 && !frozen,
                   game.player.grounded, direction);
        int tx = mobilePreview ? targetX : int(std::floor((mx+cameraX)/T));
        int ty = mobilePreview ? targetY : int(std::floor((my+cameraY)/T));
        if (!frozen && (mobilePreview || gl::touchRole(mx,my)==gl::TouchRole::World)) {
          bool reach=game.inReach(tx,ty);
          art.border(int(tx*T-cameraX),int(ty*T-cameraY),T,T,reach?Gold:Color{235,122,110},2);
        }
        // Compact HUD: one thin header, six slots, and reachable thumb controls.
        art.rect(0,0,W,60,{16,31,44,220});
        art.text("GROWLANDER",24,10,Gold,17);
        art.text("PEMAIN TUNGGAL",24,33,Mint,14);
        art.text("Permata " + std::to_string(game.gems),220,21,Gold,17);
        auto button = [&](int x,int y,int w,int h,const std::string &label,bool active=false) {
          art.rect(x,y,w,h,active?Color{63,104,103,240}:Color{20,39,51,200});
          art.border(x,y,w,h,active?Gold:Color{98,139,147},active?2:1);
          art.text(label,x+10,y+h/2-10,White,17);
        };
        button(872,6,96,48,"Tas",bag);
        button(976,6,96,48,"Bantu",help);
        button(1080,6,96,48,"Menu",paused);
        if(frozen) button(1184,6,72,48,"Tutup");
        for(int i=0;i<6;i++) {
          int x=448+i*64;
          art.rect(x,630,60,72,i==selected?Color{64,87,89,240}:Color{20,39,51,210});
          art.border(x,630,60,72,i==selected?Gold:Color{64,86,97},i==selected?3:1);
          art.icon(static_cast<gl::Item>(i),x+15,638,30);
          art.text(i==0?"ALAT":std::to_string(game.inventory[i]),x+10,677,White,14);
        }
        art.panel(448,600,384,26);
        art.text(gl::Game::itemName(static_cast<gl::Item>(selected)),460,603,Gold,14);
        button(24,590,88,88,"<",touches.held(gl::TouchRole::Left));
        button(128,590,88,88,">",touches.held(gl::TouchRole::Right));
        button(1152,590,88,88,"Lompat",touches.held(gl::TouchRole::Jump));
        button(1048,590,88,88,"Pukul",touches.held(gl::TouchRole::Punch));
        button(1048,486,88,88,selected==5?"Tanam":"Pasang",touches.held(gl::TouchRole::Place));
        if (!(hintMove && hintJump && hintAction) && !frozen) {
          art.panel(286,76,708,30);
          art.text(mobilePreview?"Gerak kiri/kanan, lompat, sentuh target lalu Pukul atau Pasang.":
            "A/D gerak, Spasi lompat, kiri pukul, kanan pasang; 1-6 pilih item.",300,82,White,14);
        }
        if (game.message != lastMessage) {
          lastMessage=game.message; messageTimer=3.f;
        }
        if(messageTimer>0 && !game.message.empty() && !frozen) {
          std::string msg=game.message;
          if(msg.size()>88) msg=msg.substr(0,85)+"...";
          art.panel(286,116,708,30); art.text(msg,300,122,Mint,14);
        }
        if (frozen) {
          art.rect(0, 60, W, H - 60, {8, 20, 31, 170});
          art.panel(300, 180, 680, 430);
          if (bag) {
            art.text("Inventaris", 330, 203, Gold, 26);
            art.text("Sentuh item untuk memilih. Tutup untuk kembali.",
                     330, 246, Muted, 14);
            for (int i = 0; i < 6; i++) {
              int y = 283 + i * 45;
              art.icon(static_cast<gl::Item>(i), 335, y, 30);
              art.text(std::to_string(i + 1) + "   " +
                           gl::Game::itemName(static_cast<gl::Item>(i)),
                       389, y + 4, i == selected ? Gold : White, 17);
              art.text(i == 0 ? "Tak terbatas"
                              : std::to_string(game.inventory[i]) + " item",
                       770, y + 4, Muted, 17);
            }
          } else if (help) {
            art.text("Kebun kecil, ide besar.", 330, 203, Gold, 26);
            std::vector<std::string> lines = {
                "Sentuh dunia untuk memilih target. Tahan Pukul untuk memecah.",
                "Pilih item di hotbar, lalu Pasang; pilih Benih untuk Tanam.",
                "Benih tumbuh 18 detik bermain aktif. Pukul pohon untuk panen.",
                "Jangkauan 4 tile. Blok tak bisa dipasang di dalam avatar.",
                "Dua jempol: gerak di kiri, lompat dan aksi di kanan.",
                "Desktop: A/D, Spasi, klik kiri/kanan, 1-6 atau roda mouse.",
                "I inventaris, H bantuan, Esc menu; F5 simpan, F9 muat.",
                "Dunia dijeda saat panel dibuka. Simpan lokal otomatis."};
            for (size_t i = 0; i < lines.size(); i++)
              art.text(lines[i], 330, 258 + int(i) * 34, White, 14);
          } else {
            art.text("Istirahat sebentar?", 330, 212, Gold, 26);
            art.text("Dunia dan tanaman sedang dijeda.", 330, 267, Muted, 17);
            art.text("Lanjut bermain", 330, 334, White, 17);
            art.text("Simpan dunia", 330, 381, White, 17);
            art.text("Simpan & keluar", 330, 428, Gold, 17);
            art.text("Kembali ke rumah", 330, 496, White, 17);
            art.text("Disimpan lokal saat aplikasi masuk latar belakang.", 330, 556, Muted,
                     14);
          }
        }
        bool lastSmoke = smoke && frame >= 239;
        if (lastSmoke) {
          bool gameplayOK=game.progress.planted>=2 && game.progress.harvested>=1 &&
                    game.progress.broken>=1 && game.progress.placed>=1 && maxJump<21.f && game.player.x>10.5f;
          bool ok = art.textFilteringOK && !art.cache.empty() && gameplayOK && (!touchTest || (hintMove && hintJump && hintAction && selected==4 &&
                    panelPauseOK && backgroundPauseOK && !paused && !bag && !help &&
                    touches.horizontal()==0 && !touches.held(gl::TouchRole::Punch)));

          std::cout << (touchTest?"SDL multitouch: ":"SDL smoke: ") << "planted=" << game.progress.planted
                    << " harvested=" << game.progress.harvested
                    << " broken=" << game.progress.broken
                    << " placed=" << game.progress.placed
                    << " panel_pause=" << panelPauseOK << " background_pause=" << backgroundPauseOK
                    << " text_filter=" << art.textFilteringOK
                    << " jump_min_y=" << maxJump
                    << " player_x=" << game.player.x << " => "
                    << (ok ? "PASS" : "FAIL") << '\n';
          if (!ok)
            result = 1;
          if (!shot.empty() && !screenshot(renderer, shot)) {
            std::cerr << "Screenshot gagal: " << SDL_GetError() << '\n';
            result = 1;
          }
          running = false;
        }
        SDL_RenderPresent(renderer);
        frame++;
#ifdef __EMSCRIPTEN__
        // Yield to browser rendering/input instead of blocking its event loop.
        // Periodic saves also protect progress if a mobile tab is discarded.
        if (!smoke && frame % 300 == 0) saveGame();
        emscripten_sleep(0);
#endif
      }
      if (!smoke && !saveGame()) {
        std::cerr << "Simpan gagal: " << game.message << '\n';
        result = 1;
      }
      if (!smoke && !shot.empty() && !screenshot(renderer, shot))
        result = 1;
    }
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  TTF_Quit();
  SDL_Quit();
  return result;
}
