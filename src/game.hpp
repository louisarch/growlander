#pragma once

#include <array>
#include <string>

namespace gl {

constexpr int WorldWidth = 100;
constexpr int WorldHeight = 60;

enum class Item : int { Fist = 0, Dirt, Stone, Wood, Brick, Seed, Count };
enum class Tile : int { Air = 0, Dirt, Stone, Wood, Brick, Bedrock };

struct Cell {
  Tile tile = Tile::Air;
  int hits = 0;
  float growth = -1.f;
};

struct Player {
  float x = 10.f, y = 18.f, vx = 0, vy = 0;
  bool grounded = false;
};

struct Progress {
  int broken = 0, placed = 0, planted = 0, harvested = 0;
};

class Game {
public:
  Player player;
  std::array<int, static_cast<int>(Item::Count)> inventory{};
  Progress progress;
  int gems = 0;
  float elapsed = 0;
  std::string message;

  Game();
  void newWorld();
  void update(float dt, float horizontal, bool jump);
  bool punch(int x, int y);
  bool place(int x, int y, Item item);
  bool inReach(int x, int y) const;
  const Cell &cell(int x, int y) const;
  bool save(const std::string &path);
  bool load(const std::string &path);
  static const char *itemName(Item);
  static constexpr float GrowSeconds = 18.f;
  static constexpr float PlayerWidth = .65f, PlayerHeight = 1.65f;

private:
  std::array<Cell, WorldWidth * WorldHeight> cells_{};
  bool jumpWasDown_ = false;

  static constexpr float MoveSpeed = 6.f;
  static constexpr float JumpSpeed = 11.f;
  static constexpr float Gravity = 25.f;
  static constexpr float Reach = 4.f;

  Cell &mutableCell(int x, int y);
  bool solidAt(int x, int y) const;
  bool overlapsSolid(float x, float y) const;
  bool overlapsPlayer(int x, int y) const;
  bool validPlayerState() const;
  void respawn();
  void moveX(float amount);
  void moveY(float amount);
};

} // namespace gl
