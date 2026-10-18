#!/usr/bin/env python3
"""Responsive page-shell checks against local build or GitHub Pages.
Requires the same Playwright/Chromium environment as browser_ui_test.py.
"""
import argparse
from pathlib import Path
from playwright.sync_api import sync_playwright

parser = argparse.ArgumentParser()
parser.add_argument('base_url')
parser.add_argument('--evidence', default='/tmp/growlander-layout-evidence')
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
    for name, width, height, mobile in [('desktop',1440,1000,False), ('laptop',1280,720,False),
            ('mobile-portrait',390,844,True), ('small-mobile',320,640,True),
            ('mobile-landscape',844,390,True)]:
        context = browser.new_context(viewport={'width':width,'height':height},
            is_mobile=mobile, has_touch=mobile, device_scale_factor=1)
        page = context.new_page()
        errors = []
        page.on('pageerror', lambda e: errors.append(str(e)))
        page.goto(args.base_url)
        page.wait_for_function("document.title.includes('Kebun Pertama')", timeout=60000)
        page.wait_for_timeout(400)
        check(page.locator('header').is_visible() and page.locator('h1').is_visible(), name+': struktur situs terlihat')
        check(page.evaluate('document.documentElement.scrollWidth <= innerWidth'), name+': tanpa overflow horizontal')
        canvas = page.locator('#canvas')
        box = canvas.bounding_box()
        check(abs(box['width']/box['height']-16/9)<.015, name+': rasio canvas 16:9')
        check(box['x'] > 0 and box['width'] < width and box['y'] > 60, name+': game berbatas, bukan layar penuh')
        check(page.evaluate('document.documentElement.scrollHeight > innerHeight'), name+': halaman dapat discroll')
        page.screenshot(path=str(out/(name+'.png')), full_page=True)
        canvas.scroll_into_view_if_needed()
        canvas.focus()
        before = page.evaluate('scrollY')
        page.keyboard.press('Space')
        page.keyboard.press('ArrowRight')
        page.wait_for_timeout(150)
        check(page.evaluate('scrollY') == before, name+': tombol game tidak menggulir halaman')
        if not mobile:
            def player_x():
                canvas.focus()
                page.keyboard.press('F5')
                page.wait_for_timeout(80)
                save = page.evaluate('localStorage.getItem("growlander.world.v1")')
                return float(next(line for line in save.splitlines() if line.startswith('P ')).split()[1])
            x = player_x()
            summary = page.locator('summary').first
            summary.focus()
            page.keyboard.down('d'); page.wait_for_timeout(250); page.keyboard.up('d')
            check(abs(player_x()-x)<.02, name+': keyboard di luar canvas tidak menggerakkan pemain')
            summary.focus()
            page.keyboard.press('Enter')
            check(not page.locator('details').first.evaluate('(el) => el.open'), name+': keyboard halaman tetap bekerja')
        page.locator('nav a[href="#cara-main"]').click()
        check(page.evaluate('scrollY') > 0, name+': navigasi panduan bekerja')
        # Real browser fullscreen, not a mocked API.
        page.locator('#fullscreen').click()
        page.wait_for_function('!!document.fullscreenElement')
        box = canvas.bounding_box()
        check(abs(box['width']/box['height']-16/9)<.015 and box['height']<=height+1,
              name+': fullscreen mempertahankan rasio dan muat')
        page.locator('#exit-fullscreen').click()
        page.wait_for_function('!document.fullscreenElement')
        box = canvas.bounding_box()
        check(abs(box['width']/box['height']-16/9)<.015, name+': keluar fullscreen memulihkan layout')
        page.evaluate('document.getElementById("game-frame").requestFullscreen = undefined')
        page.locator('#fullscreen').click()
        check('tidak didukung' in page.locator('#status').inner_text(), name+': fallback fullscreen tersedia')
        page.evaluate('() => { document.getElementById("game-frame").requestFullscreen = () => Promise.reject(new Error("denied")); }')
        page.locator('#fullscreen').click()
        page.wait_for_function('document.getElementById("status").textContent.includes("tidak tersedia")')
        check(True, name+': penolakan fullscreen ditangani')
        check(not errors, name+': tanpa error JavaScript '+str(errors))
        context.close()
    browser.close()
print(f'{checks} layout checks, 0 failures', flush=True)
