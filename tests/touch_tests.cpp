#include "touch.hpp"
#include <cstdlib>
#include <iostream>

void check(bool ok, const char* label) {
  if (!ok) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
int main() {
  using namespace gl;
  check(touchRole(24,590)==TouchRole::Left,"left boundary included");
  check(touchRole(112,630)==TouchRole::World,"left boundary excluded");
  check(touchRole(150,630)==TouchRole::Right,"right control");
  check(touchRole(1190,630)==TouchRole::Jump,"jump control");
  check(touchRole(1090,630)==TouchRole::Punch,"punch control");
  check(touchRole(1090,530)==TouchRole::Place,"place control");
  check(touchRole(790,660)==TouchRole::Ui,"hotbar consumed");
  check(touchRole(910,30)==TouchRole::Ui,"header consumed");
  check(touchRole(600,400)==TouchRole::World,"world target");
  TouchState state;
  state.down(1,TouchRole::Right); state.down(2,TouchRole::Jump);
  check(state.horizontal()==1 && state.held(TouchRole::Jump),"two simultaneous fingers");
  state.up(2);
  check(state.horizontal()==1 && !state.held(TouchRole::Jump),"jump release preserves movement");
  state.down(3,TouchRole::Left);
  check(state.horizontal()==0,"opposite directions cancel");
  state.up(1); check(state.horizontal()==-1,"independent finger release");
  state.down(4,TouchRole::Punch); state.down(5,TouchRole::Punch); state.up(4);
  check(state.held(TouchRole::Punch),"second action finger retained");
  state.clear();
  check(state.horizontal()==0 && !state.held(TouchRole::Punch),"lifecycle cancellation clears every finger");
  state.up(999); check(state.role(999)==TouchRole::Ui,"unknown release harmless");
  std::cout << "17 touch control checks, 0 failures\n";
}
