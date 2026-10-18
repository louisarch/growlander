# Growlander — catatan build dan serah-terima QA

## Hasil utama

Prototipe desktop C++17/SDL2 single-player sudah dapat dibangun dan dimainkan pada Linux. Dunia 100 × 60 tile berisi tanah, batu, dan lapisan batu dasar. Avatar dapat berjalan, melompat, bertabrakan dengan blok, memukul blok bertahap, membangun dari hotbar, serta menanam dan memanen pohon. HUD, inventaris, bantuan, pesan aksi, dan menu menggunakan bahasa Indonesia. Avatar, ikon, tile, pohon, dan latar merupakan grafis prosedural orisinal. Font DejaVu Sans dibundel beserta lisensi.

## Lingkungan yang benar-benar diuji

- Ubuntu 24.04.4 LTS x86_64, GCC 13.3.0, CMake 3.28.3.
- SDL2 2.30.0 dan SDL2_ttf 2.22.0.
- Renderer SDL software pada Xvfb untuk pengujian otomatis. Aplikasi juga dibuka pada sesi desktop sandbox; uji native lengkap memakai display Xvfb terisolasi supaya tidak terganggu perpindahan fokus desktop.

## Hasil pemeriksaan

| Pemeriksaan | Hasil aktual |
| --- | --- |
| Build Debug | Lulus dengan `-Wall -Wextra -Wpedantic` |
| Build Release | Lulus dengan `-Wall -Wextra -Wpedantic -Werror`, tanpa peringatan compiler |
| CTest Debug dan Release | 1 executable suite lulus; **69 pemeriksaan, 0 kegagalan** |
| Smoke test SDL | **Lulus**: 2 benih ditanam, 1 pohon dipanen, 1 blok dipecahkan, 1 blok dipasang; avatar melompat dan bergerak |
| Keyboard/mouse native melalui xdotool | **10 pemeriksaan, 0 kegagalan** |
| AddressSanitizer + UndefinedBehaviorSanitizer, core | **69 pemeriksaan lulus**, pemeriksaan kebocoran memori aktif |
| AddressSanitizer + UndefinedBehaviorSanitizer, frontend | Smoke test lulus; deteksi kebocoran dimatikan untuk proses frontend/X11, sehingga bukan klaim bebas kebocoran seluruh stack grafis |
| Screenshot | Diambil dari frame game SDL yang benar-benar dirender, bukan mockup |

Suite core mencakup lompat dan pendaratan, pencegahan auto-hop saat tombol ditahan, tepi dunia, tabrakan dinding/plafon, respawn aman, pembatasan jangkauan, punch bertahap, blok batu dasar, penempatan sah/tidak sah, inventaris kosong, enum item invalid, pertumbuhan dan panen, proteksi penopang pohon, serta serialisasi. Simpanan rusak/truncated/invalid ditolak tanpa mengganti state aktif; kegagalan save tidak menimpa file lama.

Uji native memeriksa jendela terbuka, gerak D, lompat Spasi, jeda inventaris, jeda bantuan, pilihan Benih dan ketepatan klik kanan setelah resize ke 1024 × 640, panen setelah **18 detik waktu nyata**, F9 memulihkan posisi, menu Simpan & keluar, dan keberadaan file save untuk sesi berikutnya. Uji ini memakai input X11 nyata; smoke test terakselerasi memakai antrean input SDL.

## Perintah yang dapat diulang

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/growlander_tests
SDL_RENDER_DRIVER=software xvfb-run -a ./build/growlander --smoke-test --screenshot smoke.bmp
SDL_RENDER_DRIVER=software xvfb-run -a python3 tests/native_ui_test.py build/growlander
```

Untuk menjalankan permainan biasa: `./build/growlander`. Dependensi dan kontrol lengkap ada di [README](../README.md). Log build Release ada di [logs/release-build.txt](logs/release-build.txt). Bukti tampilan ada di [screenshots/gameplay.png](../screenshots/gameplay.png).

## Batasan yang perlu diketahui QA

Ini prototipe, bukan rekreasi MMO penuh. Tidak ada jaringan, server, akun, trading online, pembayaran, atau aset Growtopia yang disalin. Tersedia satu dunia lokal, empat bahan bangunan, satu jenis benih, dan alat tangan. Tidak ada audio, crafting/splicing, animasi kompleks, atau katalog item besar. Dunia awal bersifat deterministik dan permukaannya datar agar mudah dipakai mencoba bangun/kebun.

Pertumbuhan hanya berjalan saat simulasi aktif; menu, inventaris, bantuan, hilangnya fokus jendela, dan menutup aplikasi menghentikannya. Save lokal otomatis saat keluar; F9 mengganti perubahan sejak simpan terakhir. Windows/macOS, controller, dan perangkat mobile belum diuji. Konfigurasi Windows CMake disediakan sebagai titik awal, bukan jaminan kompatibilitas. Penyimpanan atomik mengganti file melalui rename pada Linux; perilaku overwrite file yang sudah ada di Windows belum diverifikasi.

Workflow GitHub Actions disertakan, tetapi hasil pengujian lokal tidak sama dengan hasil CI jarak jauh; QA dapat memeriksa workflow pada commit yang diserahkan. Tidak ada perubahan visibilitas repo atau pengaturan akun yang dilakukan.
