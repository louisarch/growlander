# Growlander

Growlander adalah game platformer 2D lokal berbasis C++17, SDL2 (minimal 2.0.18), dan SDL2_ttf. Dunia awal dibuat secara prosedural (100 × 60 tile); permainan tidak memakai server atau akun. Aset visual dibuat sendiri dan tidak menyalin aset berhak cipta Growtopia.

## Preview Android — untuk tinjauan pribadi

Revisi ini memakai landscape 1280 × 720, kontrol sentuh dua jari, hotbar enam slot, dan HUD ringkas. Checklist besar, detail dunia, nama di atas avatar, serta legenda keyboard permanen dihilangkan. Petunjuk satu baris menghilang setelah gerak, lompat, dan aksi dicoba; pesan aksi hanya tampil sementara. Inventaris, bantuan, dan menu tampil sebagai panel yang menjeda dunia.

- Kiri bawah: gerak kiri/kanan. Kanan bawah: lompat dan pukul/panen; di atasnya: pasang/tanam.
- Sentuh tile untuk memilih target, pilih item di hotbar, lalu tahan tombol aksi. Gerak dan lompat bisa dilakukan bersama dengan dua jari.
- `Tas`, `Bantu`, dan `Menu` membuka panel; `Tutup` kembali bermain. Menu menyediakan simpan, lanjut, simpan & keluar, serta kembali ke rumah.
- Saat aplikasi kehilangan fokus atau masuk latar belakang, input dibersihkan, game dijeda, dan dunia disimpan lokal. Pertumbuhan tidak berjalan saat jeda.
- Keyboard dan mouse desktop tetap bekerja. Tampilan menjaga rasio landscape; perangkat sangat lebar dapat memiliki letterbox.

Jalur build Android ada di [`android/`](android/). Catatan singkat preview dan batas uji ada di [`docs/mobile-preview.md`](docs/mobile-preview.md); instruksi Android ada di [`docs/android-build.md`](docs/android-build.md). Tidak ada multiplayer, server, akun, ataupun izin internet yang ditambahkan. APK Android belum diverifikasi.

Untuk melihat tata letak sentuh di Linux:

```sh
./build/growlander --mobile-preview
# Uji alur gameplay lewat event sentuh SDL, tanpa save pemain:
xvfb-run -a ./build/growlander --touch-test --screenshot touch.bmp
```

## Dependensi

Di Ubuntu/Debian:

```sh
sudo apt update
sudo apt install build-essential cmake pkg-config libsdl2-dev libsdl2-ttf-dev
```

## Build dan jalankan

Dari direktori repositori:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/growlander
```

Build membuat executable `growlander`, library `growlander_core`, dan (saat `BUILD_TESTING` aktif) `growlander_tests`. Folder `assets/` disalin ke samping executable setelah build.

## Build web

Web build memakai Emscripten SDK **3.1.64** (sama dengan workflow Pages) dengan port SDL2 dan SDL2_ttf. Gunakan SDK tersebut, bukan paket Emscripten distro lama. Aktifkan Emscripten di shell, lalu jalankan:

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-web --parallel
```

Hasilnya `build-web/index.html` beserta JavaScript, WebAssembly, dan data aset. Serve folder `build-web/` lewat HTTP (bukan `file://`), misalnya `python3 -m http.server 8000 --directory build-web`. Build Emscripten menampilkan preview kontrol sentuh pada perangkat sentuh; keyboard dan mouse desktop tetap tersedia. Versi GitHub Pages yang dituju: [louisarch.github.io/growlander](https://louisarch.github.io/growlander/). Browser menyimpan progres secara lokal pada browser/perangkat yang digunakan; data tidak tersinkron ke perangkat lain dan dapat hilang bila data situs dibersihkan atau sesi privat berakhir.

Untuk tes integrasi SDL dalam browser, buka `/?test=smoke` atau `/?test=touch`. Hasil `PASS`/`FAIL` ditulis ke konsol dan status halaman; mode ini tidak membaca atau menulis simpanan pemain. Gunakan landscape pada ponsel. Workflow hanya mengunggah `index.html`, `index.js`, `index.wasm`, `index.data`, dan lisensi font; aktifkan Pages dengan sumber **GitHub Actions** di pengaturan repo.

## Mode otomatis

Smoke test menjalankan 240 frame simulasi terakselerasi melalui antrean input SDL: lompat, bergerak, pecah/pasang blok, tanam dua benih, tunggu matang, lalu panen. Program keluar dengan kode bukan nol bila pemeriksaan fitur gagal. Mode ini tidak membaca atau menulis save pemain. Untuk mesin tanpa display, pasang `xvfb` dan `xauth`, lalu jalankan:

```sh
xvfb-run -a ./build/growlander --smoke-test --screenshot smoke.bmp
```

Renderer SDL software dapat dipilih jika diperlukan:

```sh
SDL_RENDER_DRIVER=software xvfb-run -a ./build/growlander --smoke-test --screenshot smoke.bmp
```

Untuk uji keyboard/mouse native, resize, jeda, save/load, dan panen **18 detik waktu nyata**:

```sh
sudo apt install xvfb xauth xdotool python3
SDL_RENDER_DRIVER=software xvfb-run -a python3 tests/native_ui_test.py build/growlander
```

Opsi tambahan `--mobile-preview` menampilkan petunjuk sentuh; `--touch-test` menjalankan simulasi multitouch deterministik tanpa membaca/menulis save pemain. Opsi `--screenshot <path>` menyimpan tangkapan layar; `--save <path>` memilih lokasi save yang menimpa lokasi bawaan. Tanpa override, save lokal menggunakan `SDL_GetPrefPath("Growlander", "Growlander")/world.glw`.

## Kontrol

| Input | Aksi |
| --- | --- |
| A/D atau panah kiri/kanan | Bergerak |
| Space, W, atau panah atas | Melompat |
| Tahan tombol kiri mouse | Memukul / memanen |
| Tombol kanan mouse | Menempatkan seed atau blok |
| Tombol 1–6 / klik hotbar | Pilih Tangan, Tanah, Batu, Kayu, Bata, Benih |
| Roda mouse | Ganti item terpilih |
| I | Tampilkan/tutup inventaris; simulasi dijeda |
| H | Tampilkan/tutup bantuan; simulasi dijeda |
| Esc | Menu jeda: lanjutkan atau simpan dan keluar |
| F5 / F9 | Simpan / muat |
| R | Respawn posisi (tidak membuat dunia baru) |

## Mulai bermain

Pecahkan tanah dengan dua pukulan untuk menambah bahan ke inventaris. Pilih blok lalu klik kanan di ruang kosong dalam jangkauan empat tile; blok tidak bisa ditempatkan di tubuh avatar. Lapisan batu dasar tidak dapat dipecahkan.

Untuk berkebun, tekan **6 (Benih)** lalu klik kanan ruang kosong tepat di atas tanah. Batang dan bilah pertumbuhan berubah selama **18 detik bermain aktif**. Saat buah kuning muncul, klik kiri pada tile pangkal pohon untuk panen; hasil otomatis masuk inventaris. Blok penopang pohon dilindungi sampai tanaman dipanen. Tidak ada pertumbuhan saat menu dijeda atau aplikasi ditutup.

Inventaris, progres, dunia, serta waktu tumbuh disimpan lokal dengan F5 atau saat keluar dan dimuat saat game dibuka kembali. F9 memuat simpanan terakhir; perubahan sejak simpan terakhir akan diganti. Jika penyimpanan gagal, pesan kesalahan ditampilkan. File save memiliki versi dan divalidasi sebelum mengganti state permainan.

## Aset

Avatar, tile, tanaman, latar, dan ikon digambar secara prosedural dengan bentuk sederhana yang orisinal; tidak ada aset Growtopia yang disalin. Font lokal DejaVu Sans disertakan agar tidak perlu mengunduh saat bermain; lisensinya terdapat di [`assets/LICENSE-font.txt`](assets/LICENSE-font.txt).

## Platform dan status

Build dan tes lokal telah dijalankan pada Linux Ubuntu 24.04, GCC 13.3, SDL2 2.30, dan SDL2_ttf 2.22. Ringkasan perintah dan hasil aktual ada di [`docs/catatan-build.md`](docs/catatan-build.md). Screenshot game yang benar-benar dirender ada di [`screenshots/gameplay.png`](screenshots/gameplay.png).

Windows memiliki konfigurasi CMake untuk paket SDL2 dan SDL2_ttf berbasis CMake, misalnya melalui vcpkg toolchain, tetapi **belum diverifikasi**. Ini prototipe single-player: satu dunia lokal, tanpa server, akun, perdagangan online, pembayaran, crafting/splicing, atau katalog item besar. Workflow GitHub Actions disertakan; hasil CI jarak jauh terpisah dari hasil pengujian lokal.
