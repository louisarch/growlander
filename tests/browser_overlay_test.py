#!/usr/bin/env python3
"""Growlander browser HUD/overlay regressions using isolated Chromium contexts.

Run against a served WASM build, for example:
  python3 tests/browser_overlay_test.py http://127.0.0.1:8000/ --evidence /tmp/growlander-overlay
Add --layout-only for viewport, overlay, and fullscreen checks only.
"""
from __future__ import annotations

import argparse
import math
import re
from pathlib import Path
from typing import Any

from playwright.sync_api import Browser, BrowserContext, Page, sync_playwright

VIEWPORTS = [
    ("desktop", 1366, 768, False),
    ("mobile-portrait", 390, 844, True),
    ("small-portrait", 320, 640, True),
    ("mobile-landscape", 844, 390, True),
    ("wide-landscape", 960, 432, True),
    ("ultrawide", 1920, 800, False),
]
SAVE_KEY = "growlander.world.v1"
TILE = 40

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("base_url", help="HTTP(S) URL for the built Growlander app")
parser.add_argument("--evidence", default="/tmp/growlander-overlay-evidence")
parser.add_argument("--layout-only", action="store_true",
                    help="run viewport, overlay/modal, and fullscreen checks only")
args = parser.parse_args()
OUT = Path(args.evidence)
OUT.mkdir(parents=True, exist_ok=True)
checks = 0


def check(value: Any, label: str) -> None:
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)
    print("PASS:", label, flush=True)


def state(page: Page) -> dict[str, Any]:
    """Read a detached bridge snapshot; never write engine state from the test."""
    result = page.evaluate("""() => {
      const g = window.growlander;
      return g && g.state ? JSON.parse(JSON.stringify(g.state)) : null;
    }""")
    if not isinstance(result, dict):
        raise AssertionError("window.growlander.state is not available")
    return result


def wait_bridge(page: Page) -> None:
    page.wait_for_function(
        "() => !!(window.growlander && window.growlander.state && "
        "document.querySelector('#canvas'))",
        timeout=90_000,
    )


def canvas_box(page: Page) -> dict[str, float]:
    box = page.locator("#canvas").bounding_box()
    if box is None:
        raise AssertionError("#canvas is not visible")
    return box


def visible_item_outside_dialog(page: Page, index: int):
    """Find the hotbar item even when inventory duplicates its data-item value."""
    selector = f'[data-item="{index}"]'
    candidates = page.locator(selector)
    for i in range(candidates.count()):
        item = candidates.nth(i)
        is_hotbar = item.evaluate("el => !el.closest('[role=dialog]') && !!el.getClientRects().length")
        if is_hotbar:
            return item
    raise AssertionError(f"visible hotbar [data-item=\"{index}\"] not found outside dialogs")


def visible_dialog_item(page: Page, index: int):
    item = page.locator(f'[role="dialog"] [data-item="{index}"]').first
    item.wait_for(state="visible", timeout=5_000)
    return item


def counters(snapshot: dict[str, Any]) -> tuple[Any, ...]:
    # These are all read-only counters that should not change when an overlay is
    # merely selected/dismissed. Keep absent optional values stable as None.
    return tuple(snapshot.get(k) for k in ("broken", "placed", "planted", "harvested"))


def assert_viewport_layout(page: Page, name: str, width: int, height: int) -> None:
    dims = page.evaluate("""() => ({
      iw: innerWidth, ih: innerHeight,
      dw: document.documentElement.scrollWidth,
      dh: document.documentElement.scrollHeight
    })""")
    box = canvas_box(page)
    s = state(page)
    check(dims["dw"] <= width and dims["dh"] <= height,
          f"{name}: no document overflow ({dims['dw']}×{dims['dh']} vs {width}×{height})")
    check(abs(box["x"]) <= 1 and abs(box["y"]) <= 1 and
          abs(box["width"] - width) <= 2 and abs(box["height"] - height) <= 2,
          f"{name}: canvas fills viewport without 16:9 letterboxing ({box})")
    logical_w, logical_h = float(s["width"]), float(s["height"])
    check(logical_w > 0 and logical_h > 0 and
          abs(logical_w / logical_h - width / height) < 0.025,
          f"{name}: engine logical world follows viewport aspect ratio")

    # Every visible hotbar entry and touch control must remain within the
    # viewport. Hotbar duplicates inside an open dialog are excluded here.
    hotbar = [visible_item_outside_dialog(page, i) for i in range(6)]
    hotbar_boxes = [item.bounding_box() for item in hotbar]
    check(all(b and b["x"] >= -1 and b["y"] >= -1 and
              b["x"] + b["width"] <= width + 1 and
              b["y"] + b["height"] <= height + 1 for b in hotbar_boxes),
          f"{name}: all six hotbar items are inside the viewport")
    for i, first in enumerate(hotbar_boxes):
        for second in hotbar_boxes[i + 1:]:
            if first and second:
                overlap_x = min(first["x"] + first["width"], second["x"] + second["width"]) - max(first["x"], second["x"])
                overlap_y = min(first["y"] + first["height"], second["y"] + second["height"]) - max(first["y"], second["y"])
                check(overlap_x <= 1 or overlap_y <= 1,
                      f"{name}: hotbar slots do not overlap")

    controls = page.locator('[data-control="left"], [data-control="right"], '
                            '[data-control="jump"], [data-control="punch"], [data-control="place"]')
    visible_controls = []
    for i in range(controls.count()):
        control = controls.nth(i)
        if control.is_visible():
            visible_controls.append((control, control.bounding_box()))
    for control, box_c in visible_controls:
        check(box_c is not None and box_c["width"] >= 44 and box_c["height"] >= 44,
              f"{name}: touch control has a minimum 44px target")
        for hot_box in hotbar_boxes:
            if box_c and hot_box:
                ox=min(box_c["x"]+box_c["width"],hot_box["x"]+hot_box["width"])-max(box_c["x"],hot_box["x"])
                oy=min(box_c["y"]+box_c["height"],hot_box["y"]+hot_box["height"])-max(box_c["y"],hot_box["y"])
                check(ox<=1 or oy<=1, f"{name}: touch control does not cover hotbar")
        check(box_c is not None and box_c["x"] >= -1 and box_c["y"] >= -1 and
              box_c["x"] + box_c["width"] <= width + 1 and
              box_c["y"] + box_c["height"] <= height + 1,
              f"{name}: touch control {control.get_attribute('data-control')} is in bounds")
    for i, (_, first) in enumerate(visible_controls):
        for _, second in visible_controls[i + 1:]:
            if first and second:
                overlap_x = min(first["x"] + first["width"], second["x"] + second["width"]) - max(first["x"], second["x"])
                overlap_y = min(first["y"] + first["height"], second["y"] + second["height"]) - max(first["y"], second["y"])
                check(overlap_x <= 1 or overlap_y <= 1,
                      f"{name}: touch targets do not overlap")


def open_button(page: Page, selector: str) -> None:
    button = page.locator(selector)
    button.wait_for(state="visible", timeout=10_000)
    button.click(timeout=5_000)


def exercise_bag_modal(page: Page, name: str) -> None:
    before = state(page)
    open_button(page, "#bag-button")
    dialog = page.locator('[role="dialog"]').first
    dialog.wait_for(state="visible", timeout=5_000)
    page.wait_for_function("() => (window.growlander.state.paused || window.growlander.state.bag || window.growlander.state.help)", timeout=5_000)
    frozen = state(page)
    page.wait_for_timeout(350)
    held = state(page)
    check((held.get("paused") or held.get("bag") or held.get("help")) and held.get("elapsed") == frozen.get("elapsed"),
          f"{name}: bag pauses simulation time")
    search=dialog.locator('input[type="search"]')
    search.fill('bi')
    search.press('Home')
    search.type('b')
    check(search.input_value()=='bbi' and search.evaluate('el=>document.activeElement===el'),
          f"{name}: inventory search preserves focus and middle-edit caret")
    search.fill('bibit')
    before_select = state(page)
    visible_dialog_item(page, 5).click()
    page.wait_for_function("() => !document.querySelector('[role=dialog]') || "
                           "document.querySelector('[role=dialog]').getClientRects().length === 0",
                           timeout=5_000)
    page.wait_for_function("() => window.growlander.state.paused !== true", timeout=5_000)
    after_select = state(page)
    check(counters(after_select) == counters(before_select),
          f"{name}: selecting from bag closes it without world-action bleed")

    # Re-open and tap outside the dialog over the world. Dismissal must not be
    # interpreted as a punch/place action.
    open_button(page, "#bag-button")
    page.locator('[role="dialog"]').first.wait_for(state="visible", timeout=5_000)
    outside_before = state(page)
    box = canvas_box(page)
    vw = page.evaluate("innerWidth")
    # A top-left canvas point is outside the centered inventory panel on all
    # supported viewports; use genuine mouse/touch pointer dispatch.
    px = max(4, min(box["x"] + 8, vw - 8))
    py = box["y"] + 8
    page.mouse.click(px, py)
    page.wait_for_timeout(100)
    page.wait_for_function("() => !document.querySelector('[role=dialog]') || "
                           "document.querySelector('[role=dialog]').getClientRects().length === 0",
                           timeout=5_000)
    outside_after = state(page)
    check(counters(outside_after) == counters(outside_before),
          f"{name}: outside click dismisses bag without world action")
    check(before.get("paused") is not True,
          f"{name}: gameplay began unpaused before opening bag")


def run_layout_matrix(browser: Browser) -> None:
    for name, width, height, mobile in VIEWPORTS:
        context = browser.new_context(viewport={"width": width, "height": height},
                                      is_mobile=mobile, has_touch=mobile,
                                      device_scale_factor=1)
        page = context.new_page()
        errors: list[str] = []
        page.on("pageerror", lambda error: errors.append(str(error)))
        page.goto(args.base_url, wait_until="domcontentloaded", timeout=60_000)
        wait_bridge(page)
        page.wait_for_timeout(250)
        assert_viewport_layout(page, name, width, height)
        page.screenshot(path=str(OUT / f"layout-{name}.png"), full_page=True)
        exercise_bag_modal(page, name)

        # Exercise actual browser fullscreen where the UI exposes it. Exit via
        # the app's exit affordance when present, otherwise the browser Escape.
        if name == "desktop":
            fullscreen = page.locator("#fullscreen")
            if fullscreen.count() and fullscreen.is_visible() and fullscreen.is_enabled():
                fullscreen.click()
                page.wait_for_function("!!document.fullscreenElement", timeout=5_000)
                fb = canvas_box(page)
                dims = page.evaluate("({w:innerWidth,h:innerHeight})")
                check(abs(fb["width"] - dims["w"]) <= 2 and
                      abs(fb["height"] - dims["h"]) <= 2,
                      "desktop: actual fullscreen canvas resizes to viewport")
                exit_button = page.locator("#exit-fullscreen")
                if exit_button.count() and exit_button.is_visible():
                    exit_button.click()
                else:
                    page.locator("#fullscreen").click()
                page.wait_for_function("!document.fullscreenElement", timeout=5_000)
                check(True, "desktop: fullscreen exits and restores page")
        check(not errors, f"{name}: no uncaught JavaScript errors {errors}")
        context.close()


def run_legacy_modes(browser: Browser) -> None:
    for mode in ("smoke", "touch", "wide"):
        context = browser.new_context(viewport={"width": 1280, "height": 720})
        page = context.new_page()
        page.goto(args.base_url.rstrip("/") + "/?test=" + mode,
                  wait_until="domcontentloaded", timeout=60_000)
        page.wait_for_function(
            "() => document.querySelector('#status')?.textContent.includes('=>')",
            timeout=90_000,
        )
        result = page.locator("#status").inner_text()
        check("=> PASS" in result, f"legacy SDL {mode}: {result}")
        context.close()


def click_world(page: Page, world_x: float, world_y: float, button: str = "left") -> None:
    """Send a real pointer click using bridge camera and logical dimensions."""
    s = state(page)
    box = canvas_box(page)
    lx = world_x - float(s.get("cameraX", 0))
    ly = world_y - float(s.get("cameraY", 0))
    sx = box["x"] + lx * box["width"] / float(s["width"])
    sy = box["y"] + ly * box["height"] / float(s["height"])
    page.mouse.click(sx, sy, button=button)


def touch_event(cdp, kind: str, points: list[tuple[int, float, float]]) -> None:
    cdp.send("Input.dispatchTouchEvent", {
        "type": kind,
        "touchPoints": [{"id": ident, "x": x, "y": y, "radiusX": 2,
                         "radiusY": 2, "force": 1} for ident, x, y in points],
    })


def touch_world(page: Page, cdp, world_x: float, world_y: float,
                hold_ms: int = 50, ident: int = 11) -> None:
    s = state(page)
    box = canvas_box(page)
    x = box["x"] + (world_x - float(s.get("cameraX", 0))) * box["width"] / float(s["width"])
    y = box["y"] + (world_y - float(s.get("cameraY", 0))) * box["height"] / float(s["height"])
    touch_event(cdp, "touchStart", [(ident, x, y)])
    page.wait_for_timeout(hold_ms)
    touch_event(cdp, "touchEnd", [])
    page.wait_for_timeout(120)


def plant_world_target(snapshot: dict[str, Any], page: Page) -> tuple[float, float]:
    tx = math.floor(float(snapshot["x"])) + 2
    ty = 23
    return tx * TILE + TILE / 2, ty * TILE + TILE / 2


def run_desktop_gameplay(browser: Browser) -> None:
    context = browser.new_context(viewport={"width": 1366, "height": 768},
                                  storage_state=None)
    page = context.new_page()
    errors: list[str] = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    page.goto(args.base_url, wait_until="domcontentloaded", timeout=60_000)
    wait_bridge(page)
    page.wait_for_timeout(1000)
    canvas = page.locator("#canvas")
    canvas.focus()

    start = state(page)
    page.keyboard.down("d")
    page.wait_for_timeout(300)
    page.keyboard.up("d")
    moved = state(page)
    check(float(moved["x"]) > float(start["x"]) + 0.15,
          "desktop: focused canvas receives real D movement")
    canvas.focus()
    before_jump = state(page)
    page.keyboard.down("Space")
    page.wait_for_timeout(130)
    jump_peak = state(page)
    page.keyboard.up("Space")
    check(float(jump_peak["y"]) < float(before_jump["y"]),
          "desktop: focused canvas receives real Space jump")

    canvas.focus()
    page.keyboard.down('i'); page.wait_for_timeout(100)
    page.keyboard.down('i'); page.wait_for_timeout(100); page.keyboard.up('i')
    check(state(page).get('bag') is True, 'desktop: repeated I keydown does not toggle bag closed')
    page.keyboard.press('Escape'); page.wait_for_timeout(100)
    # Pick seed through the inventory dialog; the dialog-owned data-item is
    # selected explicitly, avoiding duplicate hotbar data-item selectors.
    open_button(page, "#bag-button")
    page.locator('[role="dialog"]').first.wait_for(state="visible", timeout=5_000)
    baseline = state(page)
    visible_dialog_item(page, 5).click()
    page.wait_for_function("() => !document.querySelector('[role=dialog]') || "
                           "document.querySelector('[role=dialog]').getClientRects().length === 0",
                           timeout=5_000)
    page.wait_for_function("() => window.growlander.state.paused !== true", timeout=5_000)
    selected = state(page)
    check(int(selected["selected"]) == 5 and counters(selected) == counters(baseline),
          "desktop: bag seed selection closes without planting through the panel")

    world_x, world_y = plant_world_target(selected, page)
    before_plant = state(page)
    click_world(page, world_x, world_y, "right")
    page.wait_for_function("(before) => window.growlander.state.planted > before",
                           arg=int(before_plant.get("planted", 0)), timeout=5_000)
    planted = state(page)
    check(int(planted["planted"]) > int(before_plant.get("planted", 0)),
          "desktop: real right-click plants at camera-mapped world coordinate")

    # Rotation/resize must preserve world progress and input mapping without a
    # reload or state injection.
    page.set_viewport_size({"width": 844, "height": 390})
    page.wait_for_timeout(350)
    rotated = state(page)
    assert_viewport_layout(page, "desktop-to-landscape-rotation", 844, 390)
    check(rotated.get("planted") == planted.get("planted"),
          "rotation: planted progress survives viewport resize")
    rotated_target = ((math.floor(float(rotated["x"])) + 3) * TILE + TILE / 2,
                      23 * TILE + TILE / 2)
    click_world(page, *rotated_target, "right")
    page.wait_for_function("(before) => window.growlander.state.planted > before",
                           arg=int(planted.get("planted", 0)), timeout=5_000)
    rotated_plant = state(page)
    check(int(rotated_plant["planted"]) > int(planted.get("planted", 0)),
          "rotation: camera-to-canvas mapping still plants at the resized viewport")
    page.set_viewport_size({"width": 1366, "height": 768})
    page.wait_for_timeout(250)

    print("Waiting 18.5 seconds of active game time for crop growth…", flush=True)
    page.wait_for_timeout(18_500)
    ripe = state(page)
    check(float(ripe.get("elapsed", 0)) > float(rotated_plant.get("elapsed", 0)),
          "desktop: active world time advances during crop growth")
    before_harvest = state(page)
    click_world(page, world_x, world_y, "left")
    page.wait_for_function("(before) => window.growlander.state.harvested > before",
                           arg=int(before_harvest.get("harvested", 0)), timeout=5_000)
    harvested = state(page)
    check(int(harvested["harvested"]) > int(before_harvest.get("harvested", 0)),
          "desktop: real world click harvests mature plant")

    # Move over the nearby drops so the game, not the test, performs collection.
    page.keyboard.down("d")
    page.wait_for_timeout(450)
    page.keyboard.up("d")
    page.wait_for_function(
        "(before) => window.growlander.state.inventory.reduce((a, n) => a + Number(n), 0) > before",
        arg=sum(map(int, harvested.get("inventory", []))), timeout=6_000)
    collected = state(page)
    check(sum(map(int, collected.get("inventory", []))) >
          sum(map(int, harvested.get("inventory", []))),
          "desktop: walking near the harvest drop collects inventory")

    # Menu save must keep the established browser key and survive a fresh page
    # load in this disposable context. No real profile or user storage is used.
    open_button(page, "#menu-button")
    dialog = page.locator('[role="dialog"]').first
    dialog.wait_for(state="visible", timeout=5_000)
    save_button = dialog.locator('button[data-command="5"]')
    if not save_button.count():
        save_button = dialog.get_by_role("button", name=re.compile(r"simpan dunia", re.I))
    save_button.first.click(timeout=5_000)
    page.wait_for_function("() => localStorage.getItem('growlander.world.v1') !== null",
                           timeout=5_000)
    saved = page.evaluate("localStorage.getItem('growlander.world.v1')")
    saved_keys = page.evaluate("Object.keys(localStorage)")
    check(SAVE_KEY in saved_keys and bool(saved),
          "menu save persists using existing growlander.world.v1 key")
    page.screenshot(path=str(OUT / "desktop-saved-world.png"))
    page.reload(wait_until="domcontentloaded", timeout=60_000)
    wait_bridge(page)
    restored = state(page)
    check(SAVE_KEY in page.evaluate("Object.keys(localStorage)") and
          restored.get("planted") == collected.get("planted") and
          restored.get("harvested") == collected.get("harvested") and
          list(restored.get("inventory", [])) == list(collected.get("inventory", [])),
          "reload: world, crop/harvest counters, inventory, and save key persist")
    check(not errors, f"desktop gameplay: no JavaScript errors {errors}")
    context.close()


def run_mobile_input(browser: Browser) -> None:
    context = browser.new_context(viewport={"width": 390, "height": 844}, is_mobile=True,
                                  has_touch=True, device_scale_factor=1)
    page = context.new_page()
    errors: list[str] = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    page.goto(args.base_url, wait_until="domcontentloaded", timeout=60_000)
    wait_bridge(page)
    page.wait_for_timeout(1000)
    cdp = context.new_cdp_session(page)

    # Demonstrate independent release: continue holding right while the jump
    # finger is lifted. Coordinates come from the visible controls, not fixed
    # screen assumptions.
    right = page.locator('[data-control="right"]')
    jump = page.locator('[data-control="jump"]')
    right.wait_for(state="visible", timeout=10_000)
    jump.wait_for(state="visible", timeout=10_000)
    rb, jb = right.bounding_box(), jump.bounding_box()
    if not rb or not jb:
        raise AssertionError("mobile right/jump controls have no visible bounds")
    rp = (rb["x"] + rb["width"] / 2, rb["y"] + rb["height"] / 2)
    jp = (jb["x"] + jb["width"] / 2, jb["y"] + jb["height"] / 2)
    before = state(page)
    touch_event(cdp, "touchStart", [(21, *rp)])
    page.wait_for_timeout(150)
    touch_event(cdp, "touchStart", [(21, *rp), (22, *jp)])
    page.wait_for_timeout(150)
    touch_event(cdp, "touchEnd", [(22, *jp)])
    page.wait_for_function("growlander.input.right && !growlander.input.jump")
    after_jump_release = state(page)
    page.wait_for_timeout(180)
    still_right = state(page)
    check(float(after_jump_release["x"]) > float(before["x"]) and
          float(still_right["x"]) > float(after_jump_release["x"]),
          "mobile: right movement persists after independent jump-touch release")
    check(float(after_jump_release['y']) < float(before['y'])-.3, 'mobile: second finger actually jumps while moving')
    touch_event(cdp, "touchEnd", [])
    page.wait_for_timeout(100)

    open_button(page, "#bag-button")
    page.locator('[role="dialog"]').first.wait_for(state="visible", timeout=5_000)
    visible_dialog_item(page, 5).click()
    page.wait_for_function("() => !document.querySelector('[role=dialog]') || "
                           "document.querySelector('[role=dialog]').getClientRects().length === 0",
                           timeout=5_000)
    page.wait_for_timeout(1100)
    selected = state(page)
    world_x, world_y = plant_world_target(selected, page)
    before_plant = state(page)
    touch_world(page, cdp, world_x, world_y, hold_ms=60)
    planted = state(page)
    check(int(planted.get("planted", 0)) > int(before_plant.get("planted", 0)),
          "mobile: one real world tap plants at camera-mapped target")

    # Resize into landscape and back without reloading; state and world mapping
    # must remain live and consistent across both orientations.
    page.set_viewport_size({"width": 844, "height": 390})
    page.wait_for_timeout(300)
    assert_viewport_layout(page, "mobile-rotated-landscape", 844, 390)
    rotated = state(page)
    check(rotated.get("planted") == planted.get("planted"),
          "mobile rotation: gameplay progress is preserved")
    rotated_target = ((math.floor(float(rotated["x"])) + 3) * TILE + TILE / 2,
                      23 * TILE + TILE / 2)
    touch_world(page, cdp, *rotated_target, hold_ms=60, ident=12)
    rotated_plant = state(page)
    check(int(rotated_plant.get("planted", 0)) > int(planted.get("planted", 0)),
          "mobile rotation: real tap still maps to the world after resize")

    # A long press on empty ground with a block selected may place once, but
    # must not repeat into a punch that removes the freshly placed tile.
    hotbar_soil = visible_item_outside_dialog(page, 1)
    hotbar_soil.click()
    page.wait_for_function("() => window.growlander.state.selected === 1", timeout=5_000)
    before_place = state(page)
    block_x = (math.floor(float(before_place["x"])) + 2) * TILE + TILE / 2
    block_y = 22 * TILE + TILE / 2
    touch_world(page, cdp, block_x, block_y, hold_ms=750, ident=13)
    after_hold_place = state(page)
    check(int(after_hold_place.get("placed", 0)) == int(before_place.get("placed", 0)) + 1 and
          after_hold_place.get("broken") == before_place.get("broken"),
          "mobile: holding place creates one block and does not re-break it")
    before_hold_break = state(page)
    touch_world(page, cdp, block_x, block_y, hold_ms=750, ident=14)
    after_hold_break = state(page)
    check(int(after_hold_break.get("broken", 0)) > int(before_hold_break.get("broken", 0)),
          "mobile: holding a filled tile performs real repeated break input")

    page.set_viewport_size({"width": 390, "height": 844})
    page.wait_for_timeout(250)
    check(state(page).get("planted") == rotated_plant.get("planted"),
          "mobile reverse rotation: gameplay progress remains preserved")
    page.screenshot(path=str(OUT / "mobile-touch-gameplay.png"), full_page=True)
    check(not errors, f"mobile input: no JavaScript errors {errors}")
    # The explicit punch/place buttons must also drive the real engine.
    pos=state(page)
    ground_x=(math.floor(float(pos['x']))-1)*TILE+TILE/2
    touch_world(page,cdp,ground_x,24*TILE+TILE/2,hold_ms=45,ident=31)
    before_punch=state(page)
    button=page.locator('[data-control="punch"]').bounding_box()
    center=(button['x']+button['width']/2,button['y']+button['height']/2)
    touch_event(cdp,'touchStart',[(32,*center)]);page.wait_for_timeout(600);touch_event(cdp,'touchEnd',[]);page.wait_for_timeout(100)
    check(state(page)['broken']>before_punch['broken'],'mobile: explicit punch button finishes a targeted ground block')
    before_restore=state(page)
    button=page.locator('[data-control="place"]').bounding_box()
    center=(button['x']+button['width']/2,button['y']+button['height']/2)
    touch_event(cdp,'touchStart',[(33,*center)]);page.wait_for_timeout(120);touch_event(cdp,'touchEnd',[]);page.wait_for_timeout(100)
    check(state(page)['placed']>before_restore['placed'],'mobile: explicit place button restores the targeted empty block')
    context.close()


with sync_playwright() as pw:
    browser = pw.chromium.launch(executable_path="/usr/bin/chromium", args=["--no-sandbox"])
    if not args.layout_only:
        run_legacy_modes(browser)
    run_layout_matrix(browser)
    if not args.layout_only:
        run_desktop_gameplay(browser)
        run_mobile_input(browser)
    browser.close()

print(f"{checks} browser checks, 0 failures", flush=True)
