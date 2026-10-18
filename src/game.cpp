#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace gl {
namespace {
constexpr int CellCount = WorldWidth * WorldHeight;
constexpr int MaxInventory = 1000000;
constexpr int MaxCounter = 100000000;
constexpr std::size_t MaxDrops = 256;
constexpr float MaxElapsed = 1000000000.f;

int itemIndex(Item item) { return static_cast<int>(item); }
Tile itemTile(Item item) {
  switch (item) {
  case Item::Dirt:
    return Tile::Dirt;
  case Item::Stone:
    return Tile::Stone;
  case Item::Wood:
    return Tile::Wood;
  case Item::Brick:
    return Tile::Brick;
  default:
    return Tile::Air;
  }
}
int hardness(Tile tile) {
  switch (tile) {
  case Tile::Dirt:
    return 2;
  case Tile::Stone:
    return 4;
  case Tile::Wood:
    return 2;
  case Tile::Brick:
    return 3;
  case Tile::Bedrock:
    return std::numeric_limits<int>::max();
  case Tile::Air:
    return 0;
  }
  return 0;
}
Item tileItem(Tile tile) {
  switch (tile) {
  case Tile::Dirt:
    return Item::Dirt;
  case Tile::Stone:
    return Item::Stone;
  case Tile::Wood:
    return Item::Wood;
  case Tile::Brick:
    return Item::Brick;
  default:
    return Item::Fist;
  }
}
bool finite(float value) { return std::isfinite(value); }
} // namespace

Game::Game() { newWorld(); }

void Game::newWorld() {
  for (int y = 0; y < WorldHeight; ++y) {
    for (int x = 0; x < WorldWidth; ++x) {
      Cell &c = cells_[static_cast<std::size_t>(y * WorldWidth + x)];
      c = Cell{};
      if (y == WorldHeight - 1)
        c.tile = Tile::Bedrock;
      else if (y >= 40)
        c.tile = Tile::Stone;
      else if (y >= 24)
        c.tile = Tile::Dirt;
    }
  }
  player = Player{10.f, 22.35f, 0.f, 0.f, true};
  inventory.fill(0);
  inventory[itemIndex(Item::Dirt)] = 40;
  inventory[itemIndex(Item::Stone)] = 20;
  inventory[itemIndex(Item::Wood)] = 20;
  inventory[itemIndex(Item::Brick)] = 30;
  inventory[itemIndex(Item::Seed)] = 8;
  progress = Progress{};
  gems = 0;
  drops_.clear();
  elapsed = 0.f;
  message = "Selamat datang di Growlander!";
  jumpWasDown_ = false;
}

const Cell &Game::cell(int x, int y) const {
  static const Cell boundary{Tile::Bedrock, 0, -1.f};
  if (x < 0 || x >= WorldWidth || y < 0 || y >= WorldHeight)
    return boundary;
  return cells_[static_cast<std::size_t>(y * WorldWidth + x)];
}

Cell &Game::mutableCell(int x, int y) {
  return cells_[static_cast<std::size_t>(y * WorldWidth + x)];
}

bool Game::solidAt(int x, int y) const { return cell(x, y).tile != Tile::Air; }

bool Game::overlapsSolid(float x, float y) const {
  const int left = static_cast<int>(std::floor(x + 0.00001f));
  const int right = static_cast<int>(std::floor(x + PlayerWidth - 0.00001f));
  const int top = static_cast<int>(std::floor(y + 0.00001f));
  const int bottom = static_cast<int>(std::floor(y + PlayerHeight - 0.00001f));
  for (int ty = top; ty <= bottom; ++ty)
    for (int tx = left; tx <= right; ++tx)
      if (solidAt(tx, ty))
        return true;
  return false;
}

bool Game::overlapsPlayer(int x, int y) const {
  return player.x < x + 1.f && player.x + PlayerWidth > x &&
         player.y < y + 1.f && player.y + PlayerHeight > y;
}

bool Game::validPlayerState() const {
  return finite(player.x) && finite(player.y) && finite(player.vx) &&
         finite(player.vy) && player.x >= 0.f && player.y >= 0.f &&
         player.x + PlayerWidth <= WorldWidth &&
         player.y + PlayerHeight <= WorldHeight &&
         !overlapsSolid(player.x, player.y);
}

void Game::respawn() {
  float bestX = 10.f;
  float bestY = 22.35f;
  float bestDistance = std::numeric_limits<float>::max();
  bool found = false;
  for (int groundY = 1; groundY < WorldHeight; ++groundY) {
    for (int tileX = 0; tileX < WorldWidth; ++tileX) {
      if (!solidAt(tileX, groundY))
        continue;
      const float candidateX =
          static_cast<float>(tileX) + (1.f - PlayerWidth) * .5f;
      const float candidateY = static_cast<float>(groundY) - PlayerHeight;
      if (candidateY < 0.f || overlapsSolid(candidateX, candidateY) ||
          overlapsSolid(candidateX, candidateY - .001f))
        continue;
      const float dx = candidateX - 10.f;
      const float dy = candidateY - 22.35f;
      const float distance = dx * dx + dy * dy;
      if (distance < bestDistance) {
        bestDistance = distance;
        bestX = candidateX;
        bestY = candidateY;
        found = true;
      }
    }
  }
  if (!found) {
    // Fully built-over worlds still need a deterministic escape hatch.
    for (int y = 0; y < 2; ++y)
      mutableCell(10, y) = Cell{};
    bestX = 10.f;
    bestY = 0.f;
  }
  player = Player{bestX, bestY, 0.f, 0.f, true};
  jumpWasDown_ = false;
  message = "Kamu dikembalikan ke titik aman.";
}

void Game::moveX(float amount) {
  if (amount == 0.f)
    return;
  const float target = player.x + amount;
  if (!overlapsSolid(target, player.y))
    player.x = target;
  else if (amount > 0.f) {
    const int edge =
        static_cast<int>(std::floor(target + PlayerWidth - 0.00001f));
    player.x = static_cast<float>(edge) - PlayerWidth;
  } else {
    const int edge = static_cast<int>(std::floor(target + 0.00001f));
    player.x = static_cast<float>(edge + 1);
  }
  if (overlapsSolid(player.x, player.y))
    respawn();
}

void Game::moveY(float amount) {
  if (amount == 0.f)
    return;
  const float target = player.y + amount;
  if (!overlapsSolid(player.x, target)) {
    player.y = target;
    if (amount > 0.f)
      player.grounded = false;
    return;
  }
  if (amount > 0.f) {
    const int edge =
        static_cast<int>(std::floor(target + PlayerHeight - 0.00001f));
    player.y = static_cast<float>(edge) - PlayerHeight;
    player.grounded = true;
    player.vy = 0.f;
  } else {
    const int edge = static_cast<int>(std::floor(target + 0.00001f));
    player.y = static_cast<float>(edge + 1);
    player.vy = 0.f;
  }
  if (overlapsSolid(player.x, player.y))
    respawn();
}

void Game::update(float dt, float horizontal, bool jump) {
  if (!finite(dt) || !finite(horizontal) || dt <= 0.f) {
    jumpWasDown_ = jump;
    return;
  }
  if (!validPlayerState())
    respawn();
  dt = std::min(dt, .1f);
  horizontal = std::clamp(horizontal, -1.f, 1.f);
  elapsed = std::min(MaxElapsed, elapsed + dt);
  for (Cell &c : cells_) {
    if (c.growth >= 0.f && c.growth < GrowSeconds)
      c.growth = std::min(GrowSeconds, c.growth + dt);
  }

  if (jump && !jumpWasDown_ && player.grounded) {
    player.vy = -JumpSpeed;
    player.grounded = false;
  }
  player.vx = horizontal * MoveSpeed;
  const int steps =
      std::max(1, static_cast<int>(std::ceil(dt / (1.f / 120.f))));
  const float step = dt / steps;
  for (int i = 0; i < steps; ++i) {
    moveX(player.vx * step);
    player.vy = std::min(30.f, player.vy + Gravity * step);
    moveY(player.vy * step);
  }
  updateDrops(dt);
  jumpWasDown_ = jump;
}

void Game::spawnDrop(Item item, int count, float x, float y) {
  if (count <= 0)
    return;
  for (Drop &drop : drops_) {
    const float dx = drop.x - x;
    const float dy = drop.y - y;
    if (drop.item == item && dx * dx + dy * dy <= .75f * .75f) {
      drop.count = std::min(MaxInventory, drop.count + count);
      drop.age = 0.f;
      return;
    }
  }
  if (drops_.size() >= MaxDrops) {
    // Preserve rewards while keeping the collection bounded by merging into
    // an existing stack of the same kind, even if it is spatially distant.
    for (Drop &drop : drops_) {
      if (drop.item == item) {
        drop.count = std::min(MaxInventory, drop.count + count);
        return;
      }
    }
    return;
  }
  drops_.push_back(Drop{item, std::min(count, MaxInventory), x, y, 0.f});
}

void Game::updateDrops(float dt) {
  constexpr float PickupRadius = 1.5f;
  constexpr float PickupGrace = .3f;
  for (std::size_t i = 0; i < drops_.size();) {
    Drop &drop = drops_[i];
    drop.age = std::min(MaxElapsed, drop.age + dt);
    drop.y = std::min(static_cast<float>(WorldHeight) - .12f,
                      drop.y + 3.f * dt);
    const int tx = static_cast<int>(std::floor(drop.x));
    const int belowY = static_cast<int>(std::floor(drop.y + .12f));
    if (solidAt(tx, belowY))
      drop.y = static_cast<float>(belowY) - .12f;

    const float closestX =
        std::clamp(drop.x, player.x, player.x + PlayerWidth);
    const float closestY =
        std::clamp(drop.y, player.y, player.y + PlayerHeight);
    const float dx = drop.x - closestX;
    const float dy = drop.y - closestY;
    if (drop.age >= PickupGrace &&
        dx * dx + dy * dy <= PickupRadius * PickupRadius) {
      int transferred = 0;
      if (drop.item == Item::Fist) {
        transferred = std::min(drop.count, MaxCounter - gems);
        gems += transferred;
      } else {
        int &owned = inventory[itemIndex(drop.item)];
        transferred = std::min(drop.count, MaxInventory - owned);
        owned += transferred;
      }
      drop.count -= transferred;
      if (drop.count <= 0) {
        drops_.erase(drops_.begin() + static_cast<std::ptrdiff_t>(i));
        continue;
      }
    }
    ++i;
  }
}

bool Game::inReach(int x, int y) const {
  if (x < 0 || x >= WorldWidth || y < 0 || y >= WorldHeight)
    return false;
  const float dx = (x + .5f) - (player.x + PlayerWidth * .5f);
  const float dy = (y + .5f) - (player.y + PlayerHeight * .5f);
  return dx * dx + dy * dy <= Reach * Reach;
}

bool Game::punch(int x, int y) {
  if (!inReach(x, y)) {
    message = "Terlalu jauh.";
    return false;
  }
  Cell &target = mutableCell(x, y);
  if (target.growth >= 0.f) {
    if (target.growth < GrowSeconds) {
      message = "Tanaman belum siap dipanen.";
      return false;
    }
    target = Cell{};
    ++progress.harvested;
    spawnDrop(Item::Wood, 2, x + .5f, y + .5f);
    spawnDrop(Item::Seed, 1, x + .5f, y + .5f);
    spawnDrop(Item::Fist, 1, x + .5f, y + .5f);
    message = "Pohon matang dipanen: hasilnya jatuh ke dunia.";
    return true;
  }
  if (target.tile == Tile::Air) {
    message = "Tidak ada yang bisa dihancurkan.";
    return false;
  }
  if (target.tile == Tile::Bedrock) {
    message = "Batuan dasar tidak bisa dihancurkan.";
    return false;
  }
  if (cell(x, y - 1).growth >= 0.f) {
    message = "Penyangga tidak bisa dihancurkan selama ada pohon di atasnya.";
    return false;
  }
  ++target.hits;
  const int needed = hardness(target.tile);
  if (target.hits >= needed) {
    const Item drop = tileItem(target.tile);
    target = Cell{};
    ++progress.broken;
    spawnDrop(drop, 1, x + .5f, y + .5f);
    message = "Blok dihancurkan.";
  } else {
    message = "Pukulan " + std::to_string(target.hits) + "/" +
              std::to_string(needed) + ".";
  }
  return true;
}

bool Game::place(int x, int y, Item item) {
  const int rawItem = static_cast<int>(item);
  if (rawItem < 0 || rawItem >= static_cast<int>(Item::Count)) {
    message = "Jenis barang tidak dikenal.";
    return false;
  }
  if (!inReach(x, y)) {
    message = "Terlalu jauh.";
    return false;
  }
  if (item == Item::Fist) {
    message = "Tangan tidak bisa ditempatkan.";
    return false;
  }
  Cell &target = mutableCell(x, y);
  if (target.tile != Tile::Air || target.growth >= 0.f) {
    message = "Tempat itu sudah terisi.";
    return false;
  }
  if (overlapsPlayer(x, y)) {
    message = "Blok tidak bisa ditempatkan di dalam pemain.";
    return false;
  }
  if (item == Item::Seed) {
    if (y + 1 >= WorldHeight || !solidAt(x, y + 1)) {
      message = "Benih membutuhkan tanah padat di bawahnya.";
      return false;
    }
  }
  int &count = inventory[itemIndex(item)];
  if (count <= 0) {
    message = "Barang itu tidak tersedia.";
    return false;
  }
  --count;
  if (item == Item::Seed) {
    target.growth = 0.f;
    ++progress.planted;
    message = "Benih ditanam.";
  } else {
    target.tile = itemTile(item);
    target.hits = 0;
    target.growth = -1.f;
    ++progress.placed;
    message = "Blok ditempatkan.";
  }
  return true;
}

const char *Game::itemName(Item item) {
  switch (item) {
  case Item::Fist:
    return "Tangan";
  case Item::Dirt:
    return "Tanah";
  case Item::Stone:
    return "Batu";
  case Item::Wood:
    return "Kayu";
  case Item::Brick:
    return "Bata";
  case Item::Seed:
    return "Benih";
  case Item::Count:
    break;
  }
  return "Tidak dikenal";
}

bool Game::save(const std::string &path) {
  message = "Permainan disimpan.";
  const auto fail = [this]() {
    message = "Gagal menyimpan permainan.";
    return false;
  };
  if (path.empty() || !validPlayerState() || !finite(elapsed) ||
      elapsed < 0.f || elapsed > MaxElapsed)
    return fail();
  for (int value : inventory)
    if (value < 0 || value > MaxInventory)
      return fail();
  if (gems < 0 || gems > MaxCounter || progress.broken < 0 ||
      progress.broken > MaxCounter || progress.placed < 0 ||
      progress.placed > MaxCounter || progress.planted < 0 ||
      progress.planted > MaxCounter || progress.harvested < 0 ||
      progress.harvested > MaxCounter)
    return fail();
  for (const Cell &c : cells_) {
    const int tile = static_cast<int>(c.tile);
    if (tile < 0 || tile > static_cast<int>(Tile::Bedrock) ||
        !finite(c.growth) || c.hits < 0 ||
        (c.tile == Tile::Air
             ? (c.hits != 0 || c.growth < -1.f || c.growth > GrowSeconds)
             : (c.growth != -1.f || c.hits >= hardness(c.tile))))
      return fail();
  }
  if (drops_.size() > MaxDrops)
    return fail();
  for (const Drop &drop : drops_) {
    const int item = itemIndex(drop.item);
    if ((item != itemIndex(Item::Fist) &&
         (item <= itemIndex(Item::Fist) || item >= itemIndex(Item::Count))) ||
        drop.count <= 0 || drop.count > MaxInventory || !finite(drop.x) ||
        !finite(drop.y) || !finite(drop.age) || drop.x < 0.f ||
        drop.x >= WorldWidth || drop.y < 0.f || drop.y >= WorldHeight ||
        drop.age < 0.f || drop.age > MaxElapsed)
      return fail();
  }

  const std::filesystem::path finalPath(path);
  const std::filesystem::path tempPath(path + ".tmp");
  try {
    std::ofstream out(tempPath, std::ios::out | std::ios::trunc);
    if (!out)
      return fail();
    out.imbue(std::locale::classic());
    out << "GROWLANDER 2\n"
        << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << "P " << player.x << ' ' << player.y << ' ' << player.vx << ' '
        << player.vy << ' ' << (player.grounded ? 1 : 0) << ' '
        << (jumpWasDown_ ? 1 : 0) << '\n';
    out << "I";
    for (int value : inventory)
      out << ' ' << value;
    out << "\nR " << progress.broken << ' ' << progress.placed << ' '
        << progress.planted << ' ' << progress.harvested << "\nS " << gems
        << ' ' << elapsed << "\nM " << std::quoted(message) << "\nC "
        << CellCount << '\n';
    for (const Cell &c : cells_)
      out << static_cast<int>(c.tile) << ' ' << c.hits << ' ' << c.growth
          << '\n';
    out << "D " << drops_.size() << '\n';
    for (const Drop &drop : drops_)
      out << itemIndex(drop.item) << ' ' << drop.count << ' ' << drop.x << ' '
          << drop.y << ' ' << drop.age << '\n';
    out.flush();
    if (!out) {
      out.close();
      std::filesystem::remove(tempPath);
      return fail();
    }
    out.close();
    if (!out) {
      std::filesystem::remove(tempPath);
      return fail();
    }
    std::error_code ec;
    std::filesystem::rename(tempPath, finalPath, ec);
    if (ec) {
      std::filesystem::remove(tempPath);
      return fail();
    }
    return true;
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(tempPath, ignored);
    return fail();
  }
}

bool Game::load(const std::string &path) {
  if (path.empty())
    return false;
  try {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > 1024 * 1024)
      return false;
    std::ifstream in(path);
    if (!in)
      return false;
    in.imbue(std::locale::classic());
    std::string magic, tag;
    int version = 0;
    if (!(in >> magic >> version) || magic != "GROWLANDER" ||
        (version != 1 && version != 2))
      return false;

    Player p;
    int grounded = 0, jumpDown = 0;
    if (!(in >> tag) || tag != "P" ||
        !(in >> p.x >> p.y >> p.vx >> p.vy >> grounded >> jumpDown))
      return false;
    if (grounded < 0 || grounded > 1 || jumpDown < 0 || jumpDown > 1)
      return false;
    p.grounded = grounded != 0;
    std::array<int, static_cast<int>(Item::Count)> inv{};
    if (!(in >> tag) || tag != "I")
      return false;
    for (int &value : inv)
      if (!(in >> value) || value < 0 || value > MaxInventory)
        return false;
    Progress prog;
    if (!(in >> tag) || tag != "R" ||
        !(in >> prog.broken >> prog.placed >> prog.planted >> prog.harvested))
      return false;
    if (prog.broken < 0 || prog.broken > MaxCounter || prog.placed < 0 ||
        prog.placed > MaxCounter || prog.planted < 0 ||
        prog.planted > MaxCounter || prog.harvested < 0 ||
        prog.harvested > MaxCounter)
      return false;
    int newGems = 0;
    float newElapsed = 0.f;
    if (!(in >> tag) || tag != "S" || !(in >> newGems >> newElapsed) ||
        newGems < 0 || newGems > MaxCounter || !finite(newElapsed) ||
        newElapsed < 0.f || newElapsed > MaxElapsed)
      return false;
    std::string newMessage;
    if (!(in >> tag) || tag != "M" || !(in >> std::quoted(newMessage)) ||
        newMessage.size() > 512)
      return false;
    int count = 0;
    if (!(in >> tag) || tag != "C" || !(in >> count) || count != CellCount)
      return false;
    std::array<Cell, CellCount> newCells{};
    for (Cell &c : newCells) {
      int tile = 0;
      if (!(in >> tile >> c.hits >> c.growth) || tile < 0 ||
          tile > static_cast<int>(Tile::Bedrock) || c.hits < 0 ||
          !finite(c.growth))
        return false;
      c.tile = static_cast<Tile>(tile);
      if (c.tile == Tile::Air) {
        if (c.hits != 0 || c.growth < -1.f || c.growth > GrowSeconds)
          return false;
      } else if (c.growth != -1.f || c.hits >= hardness(c.tile))
        return false;
    }
    std::vector<Drop> newDrops;
    if (version == 2) {
      std::size_t dropCount = 0;
      if (!(in >> tag >> dropCount) || tag != "D" || dropCount > MaxDrops)
        return false;
      newDrops.reserve(dropCount);
      for (std::size_t i = 0; i < dropCount; ++i) {
        int item = 0;
        Drop drop;
        if (!(in >> item >> drop.count >> drop.x >> drop.y >> drop.age) ||
            item < itemIndex(Item::Fist) || item >= itemIndex(Item::Count) ||
            drop.count <= 0 || drop.count > MaxInventory ||
            !finite(drop.x) || !finite(drop.y) || !finite(drop.age) ||
            drop.x < 0.f || drop.x >= WorldWidth || drop.y < 0.f ||
            drop.y >= WorldHeight || drop.age < 0.f || drop.age > MaxElapsed)
          return false;
        drop.item = static_cast<Item>(item);
        newDrops.push_back(drop);
      }
    }
    std::string extra;
    if (in >> extra)
      return false;
    if (!finite(p.x) || !finite(p.y) || !finite(p.vx) || !finite(p.vy) ||
        std::abs(p.vx) > 100.f || std::abs(p.vy) > 100.f || p.x < 0.f ||
        p.y < 0.f || p.x + PlayerWidth > WorldWidth ||
        p.y + PlayerHeight > WorldHeight)
      return false;
    const int left = static_cast<int>(std::floor(p.x + .00001f));
    const int right = static_cast<int>(std::floor(p.x + PlayerWidth - .00001f));
    const int top = static_cast<int>(std::floor(p.y + .00001f));
    const int bottom =
        static_cast<int>(std::floor(p.y + PlayerHeight - .00001f));
    for (int ty = top; ty <= bottom; ++ty)
      for (int tx = left; tx <= right; ++tx)
        if (newCells[static_cast<std::size_t>(ty * WorldWidth + tx)].tile !=
            Tile::Air)
          return false;

    player = p;
    inventory = inv;
    progress = prog;
    gems = newGems;
    elapsed = newElapsed;
    message = std::move(newMessage);
    cells_ = std::move(newCells);
    drops_ = std::move(newDrops);
    jumpWasDown_ = jumpDown != 0;
    return true;
  } catch (...) {
    return false;
  }
}

} // namespace gl
