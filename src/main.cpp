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
// Native and deterministic tests retain their original coordinate system.
// Browser play sizes the world viewport independently from the HTML HUD.
int W = 1280, H = 720;
constexpr int T = 40;
struct Color {
  Uint8 r, g, b, a = 255;
};
// Shared high-contrast palette: ocean UI, warm earth, citrus highlights.
const Color Ink{19, 30, 59}, White{247, 251, 255}, Muted{179, 210, 238},
    Gold{255, 217, 66}, Mint{143, 245, 139};
struct Art {
  SDL_Renderer *r;
  TTF_Font *small = nullptr;
  TTF_Font *normal = nullptr;
  TTF_Font *title = nullptr;
  std::map<std::string, SDL_Texture *> cache;
  bool textFilteringOK = true;
  Art(SDL_Renderer *renderer, const std::string &font) : r(renderer) {
    small = TTF_OpenFont(font.c_str(), 16);
    normal = TTF_OpenFont(font.c_str(), 20);
    title = TTF_OpenFont(font.c_str(), 26);
    if (normal) TTF_SetFontStyle(normal, TTF_STYLE_BOLD);
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
    rect(x + 3, y + 4, w, h, {12, 27, 61, 100});
    rect(x, y, w, h, {22, 53, 94, 248});
    border(x, y, w, h, {93, 170, 224}, 2);
    line(x + 2, y + 2, x + w - 3, y + 2, {157, 218, 248});
  }
  void block(gl::Tile type, int x, int y, int s, bool grass = false, int variant = 0) {
    using gl::Tile;
    switch (type) {
    case Tile::Dirt: {
      variant &= 3;
      rect(x, y, s, s, {Uint8(142 + variant * 3), Uint8(76 + variant * 2), 48});
      rect(x, y, s, 3, {179, 101, 52});
      rect(x, y + s - 3, s, 3, {113, 57, 44});
      rect(x + 4, y + s / 2, s / 4, 3, {112, 57, 42});
      rect(x + s * 2 / 3, y + s / 4, 6, 4, {208, 132, 68});
      rect(x + s / 3, y + s * 3 / 4, 4, 3, {188, 107, 57});
      if (grass) {
        rect(x, y, s, 8, {48, 151, 63});
        rect(x, y, s, 4, {127, 225, 60});
        rect(x, y, s, 2, {184, 249, 91});
        for (int i = 3; i < s; i += 10)
          rect(x + i, y + 6, 5, 6, {48, 151, 63});
      }
      break;
    }
    case Tile::Stone:
      rect(x, y, s, s, {102, 116, 159});
      rect(x + 2, y + 2, s - 4, 4, {163, 182, 217});
      line(x + 2, y + s - 2, x + s - 2, y + s - 2, {57, 65, 107});
      line(x + s / 3, y + 7, x + s / 2, y + s / 2, {72, 80, 126});
      line(x + s / 2, y + s / 2, x + s - 4, y + s / 2 + 4, {72, 80, 126});
      break;
    case Tile::Wood:
      rect(x, y, s, s, {170, 114, 62});
      rect(x + 3, y + 2, 4, s - 4, {205, 154, 82});
      rect(x + s / 2, y, 3, s, {121, 76, 48});
      rect(x + s - 5, y, 2, s, {121, 76, 48});
      line(x + 9, y + s / 3, x + s / 2 - 3, y + s / 3 + 5, {121, 76, 48});
      break;
    case Tile::Brick:
      rect(x, y, s, s, {211, 86, 69});
      for (int k = 0; k < 2; k++) {
        rect(x, y + k * s / 2, s, 3, {114, 49, 65});
        rect(x + (k % 2 ? s / 4 : s / 2), y + k * s / 2, 3, s / 2,
             {114, 49, 65});
      }
      rect(x + 3, y + 4, s / 3, 3, {255, 157, 100});
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
              int direction = 1, int scale = 1, float swing = 0) {
    int sway = walking && grounded ? int(std::sin(time * 14) * 3) : 0;
    auto p = [&](int xx, int yy, int ww, int hh, Color c) {
      rect(x + xx * scale, y + yy * scale, ww * scale, hh * scale, c);
    };
    p(2, 49, 24, 7, {16, 36, 45, 55});
    p(4, 38, 8, 15 + sway, {38, 58, 81});
    p(17, 38, 8, 15 - sway, {38, 58, 81});
    p(1, 50 + sway, 12, 6, {31, 42, 58});
    p(17, 50 - sway, 13, 6, {31, 42, 58});
    p(1, 21, 27, 23, Ink);
    p(2, 22, 25, 20, {28, 103, 194});
    p(7, 24, 14, 17, {56, 169, 240});
    p(1, 26, 5, 17, {240, 183, 136});
    p(24, 26, 5, 17, {240, 183, 136});
    if (swing > 0) {
      int handX = direction > 0 ? 29 : -13;
      p(handX, 25, 13, 10, Ink);
      p(handX + 2, 26, 10, 7, {255, 205, 151});
    }
    p(5, 3, 20, 20, {241, 191, 145});
    p(4, 2, 22, 7, {52, 53, 64});
    p(3, 9, 5, 7, {52, 53, 64});
    p(direction > 0 ? 19 : 9, 12, 3, 4, {30, 49, 61});
    p(2, 20, 28, 5, {244, 165, 86});
    p(-6, 22, 10, 5, {224, 132, 71});
  }
  void tree(int x, int y, float growth) {
    const float pct = std::clamp(growth / gl::Game::GrowSeconds, 0.f, 1.f);
    const int ht = 13 + int(62 * pct);
    const int cy = y + 35 - ht, span = 16 + int(37 * pct);
    rect(x + 16, y + 40 - ht, 10, ht, {99, 53, 41});
    rect(x + 19, y + 40 - ht, 4, ht, {193, 122, 55});
    rect(x + 20 - span / 2, cy - 14, span, 28, {22, 111, 71});
    rect(x + 10 - span / 2, cy - 3, span, 24, {38, 159, 66});
    rect(x + 26 - span / 2, cy - 11, span - 4, 26, {79, 199, 67});
    rect(x + 20 - span / 2, cy - 14, span - 8, 7, {145, 232, 81});
    if (pct >= 1) {
      for (const auto &v : std::array<std::array<int,2>,3>{{{{1,4}},{{32,-7}},{{17,-16}}}}) {
        rect(x + v[0], cy + v[1], 10, 11, {217, 124, 37});
        rect(x + v[0] + 1, cy + v[1], 8, 7, Gold);
        rect(x + v[0] + 2, cy + v[1], 3, 3, White);
      }
    } else {
      rect(x + 1, y + 43, 38, 5, {19, 46, 59});
      rect(x + 2, y + 44, int(36 * pct), 3, Mint);
    }
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
#ifndef __EMSCRIPTEN__
  SDL_SetWindowMinimumSize(window, 640, 360);
#endif
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
      bool webOverlay = false;
#ifdef __EMSCRIPTEN__
      webOverlay = !smoke;
#endif
      bool running = true, paused = false, bag = false, help = false,
           left = false, right = false;
      std::array<bool, SDL_NUM_SCANCODES> keys{};
      int selected = 0, mx = 640, my = 400, direction = 1, frame = 0;
      float cameraX = 0, cameraY = 0, cooldown = 0, time = 0;
      bool jump = false;
      gl::TouchState touches;
      bool hintMove = false, hintJump = false, hintAction = false;
      bool targetSet = false, worldPunch = true, pointerSeen = false;
      float frozenElapsed = 0;
      bool panelPauseOK = false, backgroundPauseOK = false;
      bool inventorySelectOK = false, directPlantOK = false;
      bool autoPlantOK = false;
      bool heldPunchOK = false, cameraFollowOK = false;
      int targetX = 12, targetY = 24;
      std::string lastMessage;
      float messageTimer = 0, swingTimer = 0, pickupTimer = 0;
      struct Particle { float x,y,vx,vy,life; Color color; };
      std::vector<Particle> particles;
      std::string pickupText;
      int lastPlaceX = -1, lastPlaceY = -1;
      auto action = [&](int x, int y, bool forcePunch) {
        const auto before = game.cell(x,y);
        const bool punch = forcePunch || before.tile != gl::Tile::Air || before.growth >= 0;
        bool ok = punch ? game.punch(x,y) : game.place(x,y,static_cast<gl::Item>(selected));
        if (punch) { swingTimer = .17f; direction = x+.5f >= game.player.x+.325f ? 1 : -1; }
        if (ok) {
          const bool broken = punch && game.cell(x,y).tile == gl::Tile::Air && game.cell(x,y).growth < 0;
          for (int i=0;i<(broken?18:7);++i) {
            float vx = float((i*37)%97-48) * 2.f;
            particles.push_back({x*T+20.f,y*T+18.f,vx,-float(40+(i*29)%100),.26f+(i%4)*.08f,
              before.growth>=0 ? Mint : (punch ? Color{241,163,78} : Gold)});
          }
        }
        hintAction=true;
        return ok;
      };
      auto clearInput = [&]() {
        touches.clear(); keys.fill(false); left = right = jump = false;
      };
      auto uiClick = [&](int x, int y) {
        if (webOverlay) return bag || help || paused;
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
          if (bag && gl::inside(x,y,912,198,44,44)) {
            bag=false; clearInput(); return true;
          }
          if (bag && gl::inside(x,y,330,288,620,224)) {
            int col=(x-330)/208, row=(y-288)/112;
            selected=std::clamp(row*3+col,0,5);
            bag=false; clearInput(); return true;
          }
          if (bag && !gl::inside(x,y,300,180,680,430)) {
            bag = false; clearInput(); // Consume dismissal; never act through it.
          }
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
        auto role = webOverlay ? gl::TouchRole::World : gl::touchRole(x,y);
        touches.down(id,role);
        if (role == gl::TouchRole::World) {
          targetX=int(std::floor((x+cameraX)/T));
          targetY=int(std::floor((y+cameraY)/T)); targetSet=true;
          // A world press acts immediately; holding repeats, dragging retargets.
          // Occupied cells punch; empty cells use the selected item.
          const auto &cell=game.cell(targetX,targetY);
          worldPunch=selected==0 || cell.tile!=gl::Tile::Air || cell.growth>=0;
          action(targetX,targetY,worldPunch);
          lastPlaceX=targetX; lastPlaceY=targetY;
          cooldown=.22f;
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
#ifdef __EMSCRIPTEN__
        if (webOverlay) {
          int cssW = EM_ASM_INT({ return Math.round(Module.canvas.getBoundingClientRect().width); });
          int cssH = EM_ASM_INT({ return Math.round(Module.canvas.getBoundingClientRect().height); });
          if (cssW > 0 && cssH > 0) {
            float scale = std::clamp(std::min(cssW/1280.f,cssH/720.f), .75f, 1.25f);
            int nextW = std::min(gl::WorldWidth*T-1, int(std::round(cssW/scale)));
            int nextH = std::min(gl::WorldHeight*T-1, int(std::round(cssH/scale)));
            int ww=0,wh=0; SDL_GetWindowSize(window,&ww,&wh);
            if (W!=nextW || H!=nextH || ww!=cssW || wh!=cssH) {
              W=nextW; H=nextH;
              SDL_SetWindowSize(window,cssW,cssH);
              SDL_RenderSetLogicalSize(renderer,W,H);
              clearInput(); targetSet=false;
              cameraX=std::clamp(game.player.x*T-W*.5f,0.f,float(gl::WorldWidth*T-W));
              cameraY=std::clamp(game.player.y*T-H*.48f,0.f,float(gl::WorldHeight*T-H));
            }
          }
          // A bounded, numeric command queue is the only DOM-to-engine API.
          for (int n=0;n<32;++n) {
            int command=EM_ASM_INT({ return window.growlander?.commands.shift() || 0; });
            if (!command) break;
            clearInput();
            if (command==1) { bag=!bag; help=paused=false; }
            else if (command==2) { help=!help; bag=paused=false; }
            else if (command==3) { paused=!paused; bag=help=false; }
            else if (command==4) { paused=bag=help=false; }
            else if (command==5) saveGame();
            else if (command==6) {
              if (!game.load(savePath)) game.message="Belum ada simpanan yang valid.";
              targetSet=false;
              cameraX=std::clamp(game.player.x*T-W*.5f,0.f,float(gl::WorldWidth*T-W));
              cameraY=std::clamp(game.player.y*T-H*.48f,0.f,float(gl::WorldHeight*T-H));
            }
            else if (command==7) {
              game.player={}; game.player.y=20; targetSet=false;
              paused=bag=help=false; game.message="Kembali ke titik awal.";
            }
            else if (command>=10 && command<16) {
              selected=command-10; bag=false;
            }
          }
          int input=EM_ASM_INT({
            const i=window.growlander?.input || {};
            return (i.left?1:0)|(i.right?2:0)|(i.jump?4:0)|(i.punch?8:0)|(i.place?16:0);
          });
          const gl::TouchRole roles[]={gl::TouchRole::Left,gl::TouchRole::Right,
            gl::TouchRole::Jump,gl::TouchRole::Punch,gl::TouchRole::Place};
          for(int n=0;n<5;++n) {
            if(input & (1<<n)) touches.down(-100-n,roles[n]);
            else touches.up(-100-n);
          }
        }
#endif
        time += dt;
        messageTimer = std::max(0.f, messageTimer-dt);
        swingTimer = std::max(0.f, swingTimer-dt);
        pickupTimer = std::max(0.f, pickupTimer-dt);
        cooldown = std::max(0.f, cooldown - dt);
        if(frame==0) {
          cameraX=std::clamp(game.player.x*T-W*(webOverlay?.50f:.40f),0.f,float(gl::WorldWidth*T-W));
          cameraY=std::clamp(game.player.y*T-H*(webOverlay?.48f:.53f),0.f,float(gl::WorldHeight*T-H));
        }
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
          tap(0,30,910,30); // Select Benih from the inventory, not the hotbar.
          tap(1,10,840,455);
          // Selecting an item closes the bag, without acting through it.
          tap(3,12,1090,530); // Auto-target plants above the ground, not inside it.
          if(frame==4) {
            inventorySelectOK = !bag && selected==5;
            autoPlantOK = game.progress.planted==1 && game.inventory[5]==7;
          }
          target(8,13,13,23); tap(10,14,1090,530);
          if(frame==9) directPlantOK = game.progress.planted==2 && game.inventory[5]==6;
          if(frame==13) finger(SDL_FINGERDOWN,1,160,630);
          tap(14,2,1190,630); // second finger jumps while first keeps walking
          if(frame==16) finger(SDL_FINGERUP,1,160,630);
          target(35,15,9,24);
          if(frame==37) finger(SDL_FINGERDOWN,16,1090,630);
          if(frame==38) heldPunchOK = game.cell(9,24).hits==1 || swingTimer>0 || game.cell(9,24).tile==gl::Tile::Air;
          if(frame==42) {
            finger(SDL_FINGERUP,16,1090,630);
            heldPunchOK = heldPunchOK && game.cell(9,24).tile==gl::Tile::Air;
          }
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
          if(frame==235) cameraFollowOK = cameraX>0 && std::abs(game.player.x*T-cameraX-W*.40f)<3.f;
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
            // DOM overlay focus is not application backgrounding.
#ifdef __EMSCRIPTEN__
            if (webOverlay && EM_ASM_INT({ return document.hasFocus() && !document.hidden; })) continue;
#endif
            clearInput();
            if (!smoke) saveGame();
            if (!smoke) paused = true;
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
              clearInput();
            }
            if (sc == SDL_SCANCODE_I) {
              bag = !bag;
              help = paused = false; clearInput();
            }
            if (sc == SDL_SCANCODE_H) {
              help = !help;
              bag = paused = false; clearInput();
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
              if (webOverlay) { paused=false; clearInput(); }
              else if (saveGame()) running = false;
            }
          } else if (e.type == SDL_MOUSEWHEEL) {
            selected = (selected + (e.wheel.y > 0 ? 5 : 1)) % 6;
          } else if (e.type == SDL_MOUSEMOTION && e.motion.which != SDL_TOUCH_MOUSEID) {
            mx = e.motion.x;
            my = e.motion.y; pointerSeen=true;
            if (!mobilePreview && !touches.held(gl::TouchRole::World)) targetSet=false;
            if (webOverlay && touches.held(gl::TouchRole::World) && !paused && !bag && !help) {
              targetX=int(std::floor((mx+cameraX)/T)); targetY=int(std::floor((my+cameraY)/T)); targetSet=true;
            }
          } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.which != SDL_TOUCH_MOUSEID) {
            mx=e.button.x; my=e.button.y; pointerSeen=true;
            if (e.button.button==SDL_BUTTON_LEFT && uiClick(mx,my)) continue;
            auto role=gl::touchRole(mx,my);
            if(!webOverlay && mobilePreview && e.button.button==SDL_BUTTON_LEFT && role!=gl::TouchRole::World) {
              pointerDown(-900,mx,my); continue;
            }
            if((mobilePreview || webOverlay) && e.button.button==SDL_BUTTON_LEFT) {
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
          const auto oldInventory=game.inventory;
          const int oldGems=game.gems;
          game.update(dt, horizontal, jump || touches.held(gl::TouchRole::Jump));
          for (int i=1;i<6;++i) if(game.inventory[i]>oldInventory[i]) {
            pickupText="+"+std::to_string(game.inventory[i]-oldInventory[i])+" "+gl::Game::itemName(static_cast<gl::Item>(i));
            pickupTimer=1.3f;
          }
          if(game.gems>oldGems) { pickupText="+"+std::to_string(game.gems-oldGems)+" permata"; pickupTimer=1.3f; }
          for(auto &p:particles) { p.life-=dt; p.x+=p.vx*dt; p.y+=p.vy*dt; p.vy+=350*dt; }
          particles.erase(std::remove_if(particles.begin(),particles.end(),[](const Particle&p){return p.life<=0;}),particles.end());
          if(horizontal!=0 && !touches.held(gl::TouchRole::World)) targetSet=false;
          hintMove = hintMove || horizontal != 0;
          hintJump = hintJump || jump || touches.held(gl::TouchRole::Jump);
          if (horizontal != 0)
            direction = horizontal > 0 ? 1 : -1;
          if(mobilePreview && !targetSet) {
            targetX=int(game.player.x)+direction*2;
            targetY=int(game.player.y+1.7f)-(selected==0 ? 0 : 1);
          }
          int tx = (mobilePreview || (webOverlay && touches.held(gl::TouchRole::World))) ? targetX : int(std::floor((mx+cameraX)/T));
          int ty = (mobilePreview || (webOverlay && touches.held(gl::TouchRole::World))) ? targetY : int(std::floor((my+cameraY)/T));
          bool hud = !webOverlay && !mobilePreview && (my<60 || gl::inside(mx,my,448,592,384,110));
          if (!hud && cooldown <= 0) {
            if (left || singleLeft || touches.held(gl::TouchRole::Punch)) {
              const int punchY = mobilePreview && !targetSet ? int(game.player.y+1.7f) : ty;
              action(tx,punchY,true); cooldown=.22f;
            } else if (right || singleRight || touches.held(gl::TouchRole::Place)) {
              if (right || singleRight) {
                if(game.place(tx,ty,static_cast<gl::Item>(selected))) {
                  for(int i=0;i<6;++i) particles.push_back({tx*T+20.f,ty*T+20.f,float(i*20-50),-60.f,.3f,Gold});
                }
                hintAction=true;
              } else if(game.cell(tx,ty).tile==gl::Tile::Air && game.cell(tx,ty).growth<0)
                action(tx,ty,false);
              cooldown=.22f;
            } else if (touches.held(gl::TouchRole::World)) {
              if(worldPunch || tx!=lastPlaceX || ty!=lastPlaceY) {
                if(worldPunch) action(tx,ty,true);
                else if(game.cell(tx,ty).tile==gl::Tile::Air && game.cell(tx,ty).growth<0)
                  action(tx,ty,false);
                lastPlaceX=tx; lastPlaceY=ty;
              }
              cooldown=.22f;
            }
          }
        }
        if(!frozen) {
          const float blend=1.f-std::exp(-12.f*dt);
          cameraX+=(std::clamp(game.player.x*T-W*(webOverlay?.50f:.40f),0.f,float(gl::WorldWidth*T-W))-cameraX)*blend;
          cameraY+=(std::clamp(game.player.y*T-H*(webOverlay?.48f:.53f),0.f,float(gl::WorldHeight*T-H))-cameraY)*blend;
          if(std::abs(cameraX)<.1f) cameraX=0;
        }
        jump = false;
        maxJump = std::min(maxJump, game.player.y);
        // Pixel-stepped saturated sky; scenery stays behind the playable layer.
        // Overlap adjacent strips by one logical pixel: SDL's fractional
        // viewport rounding otherwise leaves dark seams between pixel bands.
        for (int y = 0; y < H; y+=4) {
          float k = float(y) / H;
          art.rect(0,y,W,5,{Uint8(44+70*k),Uint8(142+61*k),Uint8(235-11*k)});
        }
        int sunX=int(W*.82f)-int(cameraX*.06f), sunY=int(H*.22f)-int(cameraY*.03f);
        art.rect(sunX-39,sunY-26,78,52,{255,224,100});
        art.rect(sunX-26,sunY-39,52,78,{255,224,100});
        art.rect(sunX-26,sunY-28,43,9,{255,246,180});
        for(int n=0;n<W/200+3;++n) {
          int x=int(n*230-cameraX*.15f)%2070-100;
          int yy=int(H*.22f)+(n%3)*32-int(cameraY*.08f);
          art.rect(x,yy,100,18,{180,222,255});
          art.rect(x+3,yy-5,92,17,{246,252,255});
          art.rect(x+20,yy-18,53,20,{246,252,255});
        }
        for(int layer=0;layer<2;++layer)
          for(int x=0;x<W;x+=8) {
            float wx=x+cameraX*(layer?.25f:.12f);
            int yy=int(H*.60f+layer*63+std::sin(wx*.005f+layer)*48+std::cos(wx*.012f)*18-cameraY*.12f);
            art.rect(x,yy,9,H-yy,layer?Color{52,158,147}:Color{91,180,196});
          }
        for(int n=0;n<24;++n) {
          int x=int(n*127-cameraX*.28f)%3048-60;
          int y=int(H*.57f)+int(std::sin(n*1.8f)*22)-int(cameraY*.12f);
          art.rect(x+13,y,7,65,{43,137,127});
          art.rect(x,y-8,35,25,{43,137,127});
          art.rect(x+6,y-22,24,26,{43,137,127});
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
                            c.tile == gl::Tile::Dirt, x*13+y*7);
              if(c.tile==gl::Tile::Dirt && game.cell(x,y-1).tile==gl::Tile::Air && game.cell(x,y-1).growth<0 && x%3==0) {
                art.rect(px+8,py-5,3,7,{78,179,56});
                art.rect(px+12,py-8,3,9,{137,227,72});
                if(x%9==0) { art.rect(px+27,py-6,3,7,{48,151,63}); art.rect(px+25,py-9,7,5,{255,230,102}); }
              }
              if (c.hits > 0) {
                art.line(px + 20, py + 3, px + 12, py + 20, Ink);
                art.line(px + 12, py + 20, px + 25, py + 30, Ink);
                art.line(px + 12, py + 20, px + 5, py + 29, Ink);
              }
            }
            if (c.growth >= 0)
              art.tree(px, py, c.growth);
          }
        for(const auto &drop:game.drops()) {
          int dx=int(drop.x*T-cameraX),dy=int(drop.y*T-cameraY)-int(std::sin(time*5+drop.x)*2);
          if(dx<-30 || dx>W+30 || dy<-30 || dy>H+30) continue;
          art.rect(dx-12,dy-13,24,24,{16,35,69,170});
          art.border(dx-12,dy-13,24,24,{255,218,90},1);
          if(drop.item==gl::Item::Fist) {
            art.rect(dx-5,dy-8,10,16,{78,202,255});
            art.rect(dx-8,dy-4,16,8,{78,202,255});
            art.rect(dx-3,dy-6,4,6,{201,249,255});
          } else art.icon(drop.item,dx-9,dy-10,18);
          if(drop.count>1) art.text(std::to_string(drop.count),dx+10,dy-5,Gold,14);
        }
        // Spawn marker is decoration, not a copied sprite.
        int sx = int(7 * T - cameraX), sy = int(24 * T - cameraY);
        art.rect(sx + 10, sy - 49, 5, 49, {124, 90, 62});
        art.rect(sx - 9, sy - 56, 74, 25, {255, 228, 159});
        art.text("RUMAH", sx - 6, sy - 52, Ink, 14);
        int px = int(game.player.x * T - cameraX) - 2,
            py = int(game.player.y * T - cameraY) + 9;
        art.avatar(px, py, time, horizontal != 0 && !frozen,
                   game.player.grounded, direction, 1, swingTimer);
        for(const auto &p:particles) art.rect(int(p.x-cameraX),int(p.y-cameraY),4,4,p.color);
        if(pickupTimer>0) {
          art.panel(px-28,py-36,200,29);
          art.text(pickupText,px-20,py-32,Mint,14);
        }
        int tx = (mobilePreview || (webOverlay && touches.held(gl::TouchRole::World))) ? targetX : int(std::floor((mx+cameraX)/T));
        int ty = (mobilePreview || (webOverlay && touches.held(gl::TouchRole::World))) ? targetY : int(std::floor((my+cameraY)/T));
        if (!frozen && (mobilePreview || (pointerSeen && (webOverlay || (my>=60 && !gl::inside(mx,my,448,592,384,110)))))) {
          bool reach=game.inReach(tx,ty);
          art.border(int(tx*T-cameraX),int(ty*T-cameraY),T,T,reach?Gold:Color{235,122,110},2);
        }
        // Browser UI is real HTML over the world, not a scaled video-like HUD.
        if (!webOverlay) {
        // Floating HUD islands leave the middle of the world unobstructed.
        art.panel(16,8,188,46);
        art.text("GROWLANDER",27,18,White,17);
        art.panel(216,8,164,46);
        art.rect(229,22,14,14,{96,207,255}); art.rect(233,18,6,22,{170,243,255});
        art.text(std::to_string(game.gems),253,18,Gold,17);
        auto button = [&](int x,int y,int w,int h,const std::string &label,bool active=false) {
          art.rect(x+2,y+4,w,h,{13,30,61,100});
          art.rect(x,y,w,h,active?Color{34,119,162,250}:Color{22,55,96,225});
          art.border(x,y,w,h,active?Gold:Color{133,195,230},active?3:2);
          art.line(x+3,y+3,x+w-4,y+3,active?Gold:Color{90,152,205});
          const int size=w<96 && label.size()>5 ? 14 : 17;
          int tw=0,th=0;
          TTF_SizeUTF8(size==14?art.small:art.normal,label.c_str(),&tw,&th);
          art.text(label,x+(w-tw)/2,y+(h-th)/2,White,size);
        };
        button(872,6,96,48,"Tas",bag);
        button(976,6,96,48,"Bantu",help);
        button(1080,6,96,48,"Menu",paused);
        if(frozen) button(1184,6,72,48,"Tutup");
        for(int i=0;i<6;i++) {
          int x=448+i*64;
          art.rect(x+2,634,60,72,{13,30,61,100});
          art.rect(x,630,60,72,i==selected?Color{36,112,158,250}:Color{19,48,87,242});
          art.border(x,630,60,72,i==selected?Gold:Color{84,153,206},i==selected?3:2);
          if(i==selected) art.rect(x+4,634,52,3,Gold);
          art.icon(static_cast<gl::Item>(i),x+15,640,30);
          art.text(i==0?"ALAT":std::to_string(game.inventory[i]),x+10,678,White,14);
        }
        art.panel(505,592,270,31);
        art.text(gl::Game::itemName(static_cast<gl::Item>(selected)),518,597,Gold,14);
        if(mobilePreview) {
          button(24,590,88,88,"  <",touches.held(gl::TouchRole::Left));
          button(128,590,88,88,"  >",touches.held(gl::TouchRole::Right));
          button(1152,590,88,88,"Naik",touches.held(gl::TouchRole::Jump));
          art.line(1188,610,1197,600,White); art.line(1197,600,1206,610,White);
          button(1048,590,88,88,"Pukul",touches.held(gl::TouchRole::Punch));
          button(1048,486,88,88,selected==5?"Tanam":"Pasang",touches.held(gl::TouchRole::Place));
        }
        if (!(hintMove && hintJump && hintAction) && !frozen) {
          art.panel(286,76,708,30);
          art.text(mobilePreview?"Sentuh / tahan tile untuk aksi. Pilih bahan di hotbar.":
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
            art.text("Tas perlengkapan",330,203,Gold,26);
            button(912,198,44,44,"X");
            art.text("Pilih item untuk langsung kembali bermain.",330,251,Muted,17);
            for(int i=0;i<6;++i) {
              int x=330+(i%3)*208,y=288+(i/3)*112;
              art.rect(x,y,196,100,i==selected?Color{34,106,149}:Color{17,41,78});
              art.border(x,y,196,100,i==selected?Gold:Color{78,133,185},i==selected?3:2);
              art.icon(static_cast<gl::Item>(i),x+13,y+15,34);
              art.text(gl::Game::itemName(static_cast<gl::Item>(i)),x+59,y+15,White,17);
              art.text(i==0?"Selalu siap":std::to_string(game.inventory[i])+" item",x+59,y+51,Muted,14);
            }
            art.text("Sentuh luar panel atau tekan I untuk menutup.",330,551,Muted,14);
          } else if (help) {
            art.text("Kebun kecil, ide besar.", 330, 203, Gold, 26);
            std::vector<std::string> lines = {
                "Sentuh tile untuk aksi; tahan untuk memukul berulang.",
                "Tile kosong memakai item pilihan. Blok terisi dipukul.",
                "Benih tumbuh 18 detik. Pukul pohon matang untuk panen.",
                "Jangkauan 4 tile. Blok tak bisa dipasang di dalam avatar.",
                "Dekati hasil panen / blok pecah untuk mengambil drop.",
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
        } // Native/test HUD.
#ifdef __EMSCRIPTEN__
        if (webOverlay) {
          // Emscripten EM_ASM supports at most 16 scalar arguments per call.
          EM_ASM({
            if (window.growlander) window.growlander.pendingState=({selected:$0, inventory:[$1,$2,$3,$4,$5,$6],
              gems:$7,broken:$8,placed:$9,planted:$10,harvested:$11,elapsed:$12,
              paused:!!$13,bag:!!$14,help:!!$15});
          }, selected,game.inventory[0],game.inventory[1],game.inventory[2],game.inventory[3],game.inventory[4],game.inventory[5],
            game.gems,game.progress.broken,game.progress.placed,game.progress.planted,game.progress.harvested,game.elapsed,
            paused,bag,help);
          EM_ASM({
            const bridge=window.growlander;
            if (bridge) bridge.render(Object.assign(bridge.pendingState, {
              message:UTF8ToString($0),pickup:UTF8ToString($1),
              x:$2,y:$3,cameraX:$4,cameraY:$5,width:$6,height:$7}));
          }, game.message.c_str(),(pickupTimer>0?pickupText:std::string()).c_str(),
            game.player.x,game.player.y,cameraX,cameraY,W,H);
        }
#endif
        bool lastSmoke = smoke && frame >= 239;
        if (lastSmoke) {
          bool gameplayOK=game.progress.planted>=2 && game.progress.harvested>=1 &&
                    game.progress.broken>=1 && game.progress.placed>=1 && maxJump<21.f && game.player.x>10.5f;
          bool ok = art.textFilteringOK && !art.cache.empty() && gameplayOK && (!touchTest || (hintMove && hintJump && hintAction && selected==4 &&
                    panelPauseOK && backgroundPauseOK && !paused && !bag && !help &&
                    inventorySelectOK && autoPlantOK && directPlantOK && heldPunchOK && cameraFollowOK &&
                    touches.horizontal()==0 && !touches.held(gl::TouchRole::Punch)));

          std::cout << (touchTest?"SDL multitouch: ":"SDL smoke: ") << "planted=" << game.progress.planted
                    << " harvested=" << game.progress.harvested
                    << " broken=" << game.progress.broken
                    << " placed=" << game.progress.placed
                    << " panel_pause=" << panelPauseOK << " background_pause=" << backgroundPauseOK
                    << " text_filter=" << art.textFilteringOK
                    << " inventory_select=" << inventorySelectOK << " direct_plant=" << directPlantOK
                    << " auto_plant=" << autoPlantOK
                    << " held_punch=" << heldPunchOK << " camera_follow=" << cameraFollowOK
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
