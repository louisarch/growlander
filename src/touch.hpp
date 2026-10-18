#pragma once
#include <cstdint>
#include <map>

namespace gl {
// All hit areas use the same 1280 x 720 logical canvas as the renderer.
enum class TouchRole { World, Left, Right, Jump, Punch, Place, Ui };
inline bool inside(int x, int y, int bx, int by, int bw, int bh) {
  return x >= bx && x < bx + bw && y >= by && y < by + bh;
}
inline TouchRole touchRole(int x, int y) {
  if (inside(x,y,24,590,88,88)) return TouchRole::Left;
  if (inside(x,y,128,590,88,88)) return TouchRole::Right;
  if (inside(x,y,1152,590,88,88)) return TouchRole::Jump;
  if (inside(x,y,1048,590,88,88)) return TouchRole::Punch;
  if (inside(x,y,1048,486,88,88)) return TouchRole::Place;
  if (y < 60 || inside(x,y,448,630,384,72)) return TouchRole::Ui;
  return TouchRole::World;
}
class TouchState {
  std::map<std::int64_t, TouchRole> fingers;
public:
  void down(std::int64_t id, TouchRole role) { fingers[id] = role; }
  void up(std::int64_t id) { fingers.erase(id); }
  void clear() { fingers.clear(); }
  bool held(TouchRole role) const {
    for (const auto &f : fingers) if (f.second == role) return true;
    return false;
  }
  TouchRole role(std::int64_t id) const {
    auto f = fingers.find(id);
    return f == fingers.end() ? TouchRole::Ui : f->second;
  }
  float horizontal() const { return float(held(TouchRole::Right)) - float(held(TouchRole::Left)); }
};
} // namespace gl
