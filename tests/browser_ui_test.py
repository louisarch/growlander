#!/usr/bin/env python3
"""Real WASM/browser regression; run against a local server or the published Pages URL.
Requires Python Playwright, Chromium and Pillow. Uses isolated, disposable saves.
"""
import argparse
import io
import math
from pathlib import Path
from PIL import Image
from playwright.sync_api import sync_playwright

parser = argparse.ArgumentParser()
parser.add_argument('base_url')
parser.add_argument('--evidence', default='/tmp/growlander-browser-evidence')
args = parser.parse_args()
out = Path(args.evidence)
out.mkdir(parents=True, exist_ok=True)
checks = 0

def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)
    print('PASS:', label, flush=True)

with sync_playwright() as pw:
    browser = pw.chromium.launch(executable_path='/usr/bin/chromium', args=['--no-sandbox'])
    for mode in ('smoke', 'touch', 'wide'):
        context = browser.new_context(viewport={'width': 1280, 'height': 720})
        page = context.new_page()
        page.goto(args.base_url.rstrip('/') + '/?test=' + mode)
        page.wait_for_function("document.getElementById('status').textContent.includes('=>')", timeout=60000)
        result = page.locator('#status').inner_text()
        check('=> PASS' in result, 'WASM ' + mode + ': ' + result)
        context.close()

    context = browser.new_context(viewport={'width': 1366, 'height': 768})
    page = context.new_page()
    errors = []
    page.on('pageerror', lambda error: errors.append(str(error)))
    page.goto(args.base_url)
    page.wait_for_function("document.title.includes('Kebun Pertama')", timeout=60000)
    page.wait_for_timeout(800)
    canvas = page.locator('#canvas')
    canvas.focus()

    def state():
        page.keyboard.press('F5')
        page.wait_for_timeout(100)
        text = page.evaluate("localStorage.getItem('growlander.world.v1')")
        check(bool(text), 'save browser tersedia')
        records = {}
        for line in text.splitlines():
            if line and line[0] in 'PIRSD' and line[1:2] == ' ':
                records[line[0]] = line.split()[1:]
        return records

    def click_logical(x, y, button='left'):
        box = canvas.bounding_box()
        page.mouse.click(box['x'] + x * box['width'] / 1280,
                         box['y'] + y * box['height'] / 720, button=button)
        page.wait_for_timeout(100)

    start = state()
    page.keyboard.down('d'); page.wait_for_timeout(300); page.keyboard.up('d')
    moved = state()
    check(float(moved['P'][0]) > float(start['P'][0]) + 1, 'desktop D bergerak')
    page.keyboard.down('Space'); page.wait_for_timeout(120); page.keyboard.up('Space')
    jumped = state()
    check(float(jumped['P'][1]) < float(start['P'][1]) - .3, 'desktop Spasi melompat')
    page.wait_for_timeout(1000)
    page.keyboard.press('i')
    frozen = state()
    page.keyboard.down('d'); page.wait_for_timeout(300); page.keyboard.up('d')
    still = state()
    check(still['P'][:2] == frozen['P'][:2] and still['S'][1] == frozen['S'][1], 'tas menjeda posisi dan waktu')
    page.screenshot(path=str(out / 'desktop-inventory.png'))
    click_logical(840,455)
    page.wait_for_timeout(200)
    # A selection closes the inventory without placing a tile through it.
    selected = state()
    check(selected['R'][2] == '0', 'pilih benih dari tas tidak beraksi menembus panel')
    tx = math.floor(float(selected['P'][0])) + 3
    ty = 23
    cx = max(0, min(2720, float(selected['P'][0]) * 40 - 512))
    cy = max(0, min(1680, float(selected['P'][1]) * 40 - 381.6))
    sx, sy = tx*40+20-cx, ty*40+20-cy
    click_logical(sx, sy, 'right')
    planted = state()
    check(planted['R'][2] == '1' and planted['I'][5] == '7', 'klik kanan menanam item pilihan dari tas')
    print('Menunggu 18 detik pertumbuhan aktif WASM...', flush=True)
    page.wait_for_timeout(18400)
    click_logical(sx, sy)
    harvested = state()
    print('State panen:', {k: harvested.get(k) for k in ('P','R','D','S')}, 'state tanam:', planted.get('S'), flush=True)
    check(harvested['R'][3] == '1' and int(harvested['D'][0]) >= 1, 'panen menghasilkan drop world, bukan inventori instan')
    page.screenshot(path=str(out / 'desktop-drops.png'))
    page.keyboard.down('d'); page.wait_for_timeout(260); page.keyboard.up('d')
    page.wait_for_timeout(500)
    picked = state()
    check(picked['I'][3] == '22' and picked['I'][5] == '8' and picked['S'][0] == '1', 'mendekat mengambil kayu, benih dan permata')
    page.keyboard.down('d'); page.wait_for_timeout(220); page.keyboard.up('d')
    page.keyboard.press('F9'); page.wait_for_timeout(200)
    loaded = state()
    check(abs(float(loaded['P'][0]) - float(picked['P'][0])) < .02, 'F9 memuat save browser')
    page.keyboard.press('Escape')
    click_logical(640,338)
    page.wait_for_timeout(150)
    # Reload must restore the same local state, including the v2 save format.
    page.reload(); page.wait_for_function("document.title.includes('Kebun Pertama')")
    page.wait_for_timeout(400); canvas.focus()
    restored = state()
    check(restored['I'][3] == '22' and restored['S'][0] == '1', 'save tetap hidup setelah reload halaman')
    page.screenshot(path=str(out / 'desktop-gameplay.png'))
    check(not errors, 'desktop tanpa error JavaScript: ' + str(errors))
    context.close()

    context = browser.new_context(viewport={'width': 844, 'height': 390}, is_mobile=True,
                                  has_touch=True, device_scale_factor=2)
    page = context.new_page()
    page.goto(args.base_url)
    page.wait_for_function("document.title.includes('Kebun Pertama')", timeout=60000)
    page.wait_for_timeout(500)
    canvas = page.locator('#canvas')
    cdp = context.new_cdp_session(page)
    def points(coords):
        box=canvas.bounding_box()
        return [{'x':box['x']+x*box['width']/1280,
                 'y':box['y']+y*box['height']/720,'id':i+1} for i,(x,y) in enumerate(coords)]
    def touch(kind, coords):
        cdp.send('Input.dispatchTouchEvent', {'type':kind, 'touchPoints':points(coords)})
    def tap(x,y):
        touch('touchStart',[(x,y)]); page.wait_for_timeout(40); touch('touchEnd',[])
        page.wait_for_timeout(100)
    tap(910,30); page.screenshot(path=str(out/'mobile-inventory.png'))
    tap(840,455)
    # Plant the near empty tile with one world tap.
    tap(500,427)
    mobile = state()
    check(mobile['R'][2]=='1' and mobile['I'][5]=='7', 'mobile satu tap tile menanam')
    # Keep right held, add a jump finger, release jump independently.
    touch('touchStart',[(160,630)]); page.wait_for_timeout(180)
    touch('touchStart',[(160,630),(1190,630)]); page.wait_for_timeout(140)
    touch('touchEnd',[(160,630)]); page.wait_for_timeout(120)
    touch('touchEnd',[])
    mobile_move=state()
    check(float(mobile_move['P'][0])>10.8 and float(mobile_move['P'][1])<22, 'dua jari mobile: gerak dan lompat bersamaan')
    page.wait_for_timeout(1100)
    mobile_pos=state()
    tx=math.floor(float(mobile_pos['P'][0]))+2
    cx=max(0,min(2720,float(mobile_pos['P'][0])*40-512))
    cy=max(0,min(1680,float(mobile_pos['P'][1])*40-381.6))
    sx,sy=tx*40+20-cx,24*40+20-cy
    tap(475,660)
    touch('touchStart',[(sx,sy)]); page.wait_for_timeout(650); touch('touchEnd',[])
    punched=state()
    check(punched['R'][0]=='1', 'tahan tile langsung memukul berulang hingga pecah')
    tap(535,660)
    touch('touchStart',[(sx,sy)]); page.wait_for_timeout(650); touch('touchEnd',[])
    placed=state()
    check(placed['R'][0]=='1' and placed['R'][1]=='1', 'tahan pasang tidak menghancurkan blok yang baru dipasang')
    # Dismiss via outside without an accidental world hit/placement.
    tap(910,30); before=state(); tap(260,350); after=state()
    check(before['R']==after['R'], 'tap luar menutup tas tanpa aksi tembus')
    page.screenshot(path=str(out/'mobile-landscape.png'))
    image=Image.open(io.BytesIO(canvas.screenshot())).convert('RGB')
    sky=image.getpixel((image.width//2,int(image.height*.25)))
    check(sky[2]>190 and sky[2]-sky[0]>65, 'palet langit jenuh terbaca pada mobile landscape')
    # Wider landscape still has correct logical touch coordinates and safe letterboxing.
    page.set_viewport_size({'width': 960,'height': 432})
    tap(910,30); tap(840,455)
    page.screenshot(path=str(out/'mobile-wide.png'))
    check(canvas.bounding_box()['width']<=960, 'kanvas landscape lebar tidak keluar viewport')
    context.close()
    browser.close()
print(f'{checks} browser checks, 0 failures', flush=True)
