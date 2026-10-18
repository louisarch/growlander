#include "../src/game.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
int tests = 0;
int failures = 0;

void check(bool condition, const char *label) {
  ++tests;
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << label << '\n';
  }
}

bool near(float a, float b, float tolerance = .001f) {
  return std::fabs(a - b) <= tolerance;
}

void physicsTests() {
  gl::Game game;
  check(near(game.player.x, 10.f) && near(game.player.y, 22.35f),
        "spawn position");
  check(game.player.grounded, "spawn starts grounded");
  game.update(.1f, 0.f, true);
  const float jumpedY = game.player.y;
  check(jumpedY < 22.35f && game.player.vy < 0.f, "jump moves upward");
  for (int i = 0; i < 100; ++i)
    game.update(.1f, 0.f, true);
  check(game.player.grounded && near(game.player.y, 22.35f),
        "held jump lands without auto-hop");
  const float landedY = game.player.y;
  game.update(.1f, 0.f, true);
  check(near(game.player.y, landedY), "held jump remains edge-triggered");
  game.update(.1f, 0.f, false);
  game.update(.1f, 0.f, true);
  check(game.player.y < landedY, "released jump can jump again");

  gl::Game bounds;
  for (int i = 0; i < 80; ++i)
    bounds.update(.1f, -1.f, false);
  check(bounds.player.x >= 0.f && bounds.player.x < .01f,
        "left world edge blocks player");
  for (int i = 0; i < 300; ++i)
    bounds.update(.1f, 1.f, false);
  check(bounds.player.x + gl::Game::PlayerWidth <= gl::WorldWidth + .001f,
        "right world edge blocks player");
  const float before = bounds.player.x;
  bounds.update(100.f, 0.f, false);
  check(bounds.player.x >= before &&
            bounds.player.x + gl::Game::PlayerWidth <= gl::WorldWidth + .001f,
        "large dt is capped and remains collision-safe");

  gl::Game wall;
  check(wall.place(11, 23, gl::Item::Brick),
        "wall obstacle can be built next to player");
  for (int i = 0; i < 20; ++i)
    wall.update(.1f, 1.f, false);
  check(wall.player.x + gl::Game::PlayerWidth <= 11.001f,
        "player collides with a placed wall");

  gl::Game ceiling;
  check(ceiling.place(10, 20, gl::Item::Brick),
        "ceiling obstacle can be built overhead");
  ceiling.update(.1f, 0.f, true);
  ceiling.update(.1f, 0.f, true);
  check(ceiling.player.y >= 21.f && ceiling.player.vy >= 0.f,
        "jump collides with ceiling and cancels upward velocity");

  gl::Game recovery;
  recovery.update(.1f, 1.f, false);
  recovery.update(.1f, 1.f, false);
  check(recovery.place(10, 22, gl::Item::Brick) &&
            recovery.place(10, 23, gl::Item::Brick),
        "can build over the original spawn while standing elsewhere");
  recovery.player = gl::Player{10.f, 22.35f, 0.f, 0.f, true};
  recovery.update(.01f, 0.f, false);
  check(recovery.player.x != 10.f && recovery.player.x >= 0.f &&
            recovery.player.x + gl::Game::PlayerWidth <= gl::WorldWidth &&
            recovery.player.y + gl::Game::PlayerHeight <= gl::WorldHeight,
        "respawn searches for a safe location when spawn is blocked");
  for (int i = 0; i < 20; ++i)
    recovery.update(.1f, 0.f, false);
  check(recovery.player.x != 10.f,
        "recovered player does not remain stuck at blocked spawn");
}

void interactionTests() {
  gl::Game game;
  check(!game.punch(30, 24), "unreachable block cannot be punched");
  check(!game.place(30, 23, gl::Item::Dirt),
        "unreachable cell cannot be placed");
  check(!game.place(-1, 23, gl::Item::Dirt),
        "out-of-world placement is rejected");
  check(game.cell(-1, 0).tile == gl::Tile::Bedrock,
        "out-of-world lookup is solid boundary");
  check(!game.place(10, 22, gl::Item::Dirt),
        "placement overlapping player is rejected");
  check(!game.place(12, 23, static_cast<gl::Item>(-1)),
        "negative invalid item enum is rejected");
  check(!game.place(12, 23, static_cast<gl::Item>(99)),
        "out-of-range item enum is rejected");
  check(game.inventory[static_cast<int>(gl::Item::Dirt)] == 40,
        "failed placements do not consume items");
  check(std::string(gl::Game::itemName(gl::Item::Fist)) == "Tangan" &&
            std::string(gl::Game::itemName(gl::Item::Dirt)) == "Tanah" &&
            std::string(gl::Game::itemName(gl::Item::Stone)) == "Batu" &&
            std::string(gl::Game::itemName(gl::Item::Wood)) == "Kayu" &&
            std::string(gl::Game::itemName(gl::Item::Brick)) == "Bata" &&
            std::string(gl::Game::itemName(gl::Item::Seed)) == "Benih",
        "item names are localized to Indonesian");

  check(game.punch(10, 24), "first dirt punch registers");
  check(game.cell(10, 24).tile == gl::Tile::Dirt && game.cell(10, 24).hits == 1,
        "dirt takes two hits");
  check(game.punch(10, 24), "second dirt punch registers");
  check(game.cell(10, 24).tile == gl::Tile::Air && game.progress.broken == 1,
        "dirt breaks and increments progress");
  check(game.inventory[static_cast<int>(gl::Item::Dirt)] == 40 &&
            game.drops().size() == 1 &&
            game.drops()[0].item == gl::Item::Dirt &&
            game.drops()[0].count == 1,
        "broken block creates a world drop, not an immediate reward");
  game.update(.2f, 0.f, false);
  check(game.inventory[static_cast<int>(gl::Item::Dirt)] == 40 &&
            game.drops().size() == 1,
        "nearby drop observes grace period before pickup");
  game.update(.11f, 0.f, false);
  check(game.inventory[static_cast<int>(gl::Item::Dirt)] == 40 &&
            game.drops().size() == 1,
        "pickup grace is measured in capped simulation time");
  game.update(.1f, 0.f, false);
  check(game.inventory[static_cast<int>(gl::Item::Dirt)] == 41 &&
            game.drops().empty(),
        "nearby drop is automatically picked up after grace period");

  gl::Game farDrop;
  farDrop.punch(13, 24);
  farDrop.punch(13, 24);
  farDrop.update(.1f, 0.f, false);
  farDrop.update(.1f, 0.f, false);
  farDrop.update(.1f, 0.f, false);
  farDrop.update(.1f, 0.f, false);
  check(farDrop.inventory[static_cast<int>(gl::Item::Dirt)] == 40 &&
            farDrop.drops().size() == 1,
        "drop outside pickup radius remains in the world");

  gl::Game placing;
  check(placing.place(12, 23, gl::Item::Dirt),
        "reachable valid block placement succeeds");
  check(placing.cell(12, 23).tile == gl::Tile::Dirt &&
            placing.progress.placed == 1,
        "placement updates world and progress");
  check(placing.inventory[static_cast<int>(gl::Item::Dirt)] == 39,
        "placement consumes item");
  check(!placing.place(12, 23, gl::Item::Brick),
        "occupied cell cannot be replaced");
  check(!placing.place(11, 23, gl::Item::Fist), "fist is not placeable");
  placing.inventory[static_cast<int>(gl::Item::Brick)] = 0;
  check(!placing.place(13, 23, gl::Item::Brick),
        "placement with empty inventory is rejected");
  placing.player = gl::Player{10.f, 54.7f, 0.f, 0.f, false};
  check(placing.inReach(10, 59) && !placing.punch(10, 59),
        "bedrock is immutable when within reach");
}

void farmingTests() {
  gl::Game game;
  check(!game.place(12, 22, gl::Item::Seed),
        "seed without solid support is rejected");
  check(game.inventory[static_cast<int>(gl::Item::Seed)] == 8,
        "unsupported seed does not consume seed");
  check(game.place(12, 23, gl::Item::Seed), "seed plants on solid ground");
  check(game.cell(12, 23).tile == gl::Tile::Air &&
            game.cell(12, 23).growth == 0.f,
        "plant is represented as growing non-solid air");
  check(game.progress.planted == 1 &&
            game.inventory[static_cast<int>(gl::Item::Seed)] == 7,
        "planting updates count and inventory");
  check(!game.punch(12, 23), "unripe plant cannot be harvested");
  check(!game.punch(12, 24),
        "support cannot be broken while plant is above it");
  check(game.cell(12, 24).tile == gl::Tile::Dirt,
        "protected support remains intact");
  for (int i = 0; i < 180; ++i)
    game.update(.1f, 0.f, false);
  check(near(game.cell(12, 23).growth, gl::Game::GrowSeconds),
        "plant matures after grow interval");
  check(game.punch(12, 23), "mature plant harvests");
  check(game.cell(12, 23).growth < 0.f && game.progress.harvested == 1,
        "harvest clears plant and records progress");
  check(game.inventory[static_cast<int>(gl::Item::Wood)] == 20 &&
            game.inventory[static_cast<int>(gl::Item::Seed)] == 7 &&
            game.gems == 0 && game.drops().size() == 3,
        "harvest spawns wood, seed, and gem drops without immediate awards");
  game.player = gl::Player{11.5f, 22.35f, 0.f, 0.f, true};
  game.update(.1f, 0.f, false);
  game.update(.1f, 0.f, false);
  game.update(.11f, 0.f, false);
  check(game.inventory[static_cast<int>(gl::Item::Wood)] == 22 &&
            game.inventory[static_cast<int>(gl::Item::Seed)] == 8 &&
            game.gems == 1 && game.drops().empty(),
        "harvest rewards are granted by actual proximity pickup");
  check(game.punch(12, 24) && game.punch(12, 24),
        "support becomes breakable after harvest");
}

void serializationTests() {
  const auto dir =
      std::filesystem::temp_directory_path() / "growlander_core_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string path = (dir / "state.save").string();
  const std::string corruptPath = (dir / "state.corrupt").string();

  gl::Game source;
  source.place(12, 23, gl::Item::Seed);
  source.update(.37f, 1.f, false);
  source.punch(10, 24);
  source.punch(10, 24);
  source.message = "round trip message";
  check(source.save(path), "save writes valid state");
  check(source.message == "Permainan disimpan.",
        "successful save reports status in Indonesian");
  gl::Game loaded;
  check(loaded.load(path), "valid save loads");
  check(near(loaded.player.x, source.player.x) &&
            near(loaded.player.y, source.player.y) &&
            near(loaded.player.vx, source.player.vx) &&
            near(loaded.player.vy, source.player.vy),
        "player physics state round-trips");
  check(loaded.inventory == source.inventory &&
            loaded.progress.broken == source.progress.broken &&
            loaded.progress.planted == source.progress.planted &&
            loaded.gems == source.gems,
        "inventory and progression round-trip");
  check(loaded.cell(12, 23).growth == source.cell(12, 23).growth &&
            loaded.cell(10, 24).hits == source.cell(10, 24).hits &&
            near(loaded.elapsed, source.elapsed),
        "world, hit and timer state round-trip");
  check(loaded.message == source.message, "message round-trips");
  check(loaded.drops().size() == source.drops().size() &&
            !loaded.drops().empty() &&
            loaded.drops()[0].item == source.drops()[0].item &&
            loaded.drops()[0].count == source.drops()[0].count &&
            near(loaded.drops()[0].x, source.drops()[0].x) &&
            near(loaded.drops()[0].y, source.drops()[0].y) &&
            near(loaded.drops()[0].age, source.drops()[0].age),
        "pending world drops round-trip in v2 save");

  {
    std::ofstream bad(corruptPath, std::ios::trunc);
    bad << "GROWLANDER 1\nP nan 0 0 0 0 0\n";
  }
  gl::Game unchanged;
  unchanged.update(.2f, 1.f, false);
  const float oldX = unchanged.player.x;
  const int oldDirt = unchanged.inventory[static_cast<int>(gl::Item::Dirt)];
  const auto oldMessage = unchanged.message;
  check(!unchanged.load(corruptPath), "truncated and non-finite save rejected");
  check(near(unchanged.player.x, oldX) &&
            unchanged.inventory[static_cast<int>(gl::Item::Dirt)] == oldDirt &&
            unchanged.message == oldMessage,
        "failed load leaves all live state unchanged");
  check(!unchanged.load(path + ".missing"), "missing save rejected");

  {
    std::ofstream truncated(path + ".bad", std::ios::trunc);
    truncated << "GROWLANDER 1\nP 10 22.35 0 0 1 0\nI 0 0 0 0 0 0\n";
  }
  check(!unchanged.load(path + ".bad"), "incomplete save rejected");

  std::ifstream savedIn(path);
  const std::string validText((std::istreambuf_iterator<char>(savedIn)),
                              std::istreambuf_iterator<char>());
  {
    std::string legacy = validText;
    const auto dropSection = legacy.find("\nD ");
    check(dropSection != std::string::npos,
          "v2 save writes pending-drop section");
    if (dropSection != std::string::npos) {
      legacy.replace(0, std::string("GROWLANDER 2").size(), "GROWLANDER 1");
      legacy.erase(dropSection);
      std::ofstream out(dir / "legacy_v1.save", std::ios::trunc);
      out << legacy;
      out.close();
      gl::Game legacyLoaded;
      check(legacyLoaded.load((dir / "legacy_v1.save").string()) &&
                legacyLoaded.drops().empty() &&
                legacyLoaded.progress.broken == source.progress.broken,
            "legacy v1 save loads with no pending drops");
    }
  }
  {
    std::string badDrop = validText;
    const auto dropSection = badDrop.find("\nD 1\n");
    check(dropSection != std::string::npos,
          "saved world has one test drop");
    if (dropSection != std::string::npos) {
      const auto itemStart = dropSection + std::string("\nD 1\n").size();
      badDrop.replace(itemStart, 1, "6");
      std::ofstream out(dir / "bad_drop.save", std::ios::trunc);
      out << badDrop;
    }
  }
  const float atomicX = unchanged.player.x;
  check(!unchanged.load((dir / "bad_drop.save").string()) &&
            near(unchanged.player.x, atomicX) && unchanged.drops().empty(),
        "malformed v2 drop is rejected without partial state mutation");
  const auto cellData = validText.find("\nC 6000\n");
  check(cellData != std::string::npos,
        "saved world includes complete cell section");
  if (cellData != std::string::npos) {
    std::string badTile = validText;
    badTile.replace(cellData + 8, 1, "9");
    std::ofstream out(dir / "bad_tile.save", std::ios::trunc);
    out << badTile;
    out.close();
    check(!unchanged.load((dir / "bad_tile.save").string()),
          "full save with invalid tile is rejected");
  }
  {
    std::string badInventory = validText;
    const auto inventoryLine = badInventory.find("\nI 0 40 ");
    check(inventoryLine != std::string::npos,
          "saved inventory section is present");
    if (inventoryLine != std::string::npos) {
      badInventory.replace(inventoryLine + 3, 1, "-1");
      std::ofstream out(dir / "bad_inventory.save", std::ios::trunc);
      out << badInventory;
    }
  }
  check(!unchanged.load((dir / "bad_inventory.save").string()),
        "full save with negative inventory is rejected");

  {
    std::ofstream existing(path, std::ios::trunc);
    existing << "keep-existing-save";
  }
  std::filesystem::create_directory(path + ".tmp");
  check(!source.save(path),
        "save failure is reported when temp path cannot be written");
  check(source.message == "Gagal menyimpan permainan.",
        "failed save reports status in Indonesian");
  std::ifstream existingIn(path);
  std::string preserved;
  existingIn >> preserved;
  check(preserved == "keep-existing-save",
        "failed save preserves existing save file");
  std::filesystem::remove_all(dir);
}
} // namespace

int main() {
  physicsTests();
  interactionTests();
  farmingTests();
  serializationTests();
  std::cout << tests << " checks, " << failures << " failures\n";
  return failures == 0 ? 0 : 1;
}
