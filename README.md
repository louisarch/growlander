# Growlander

Growlander adalah game platformer 2D lokal berbasis C++17, SDL2, dan SDL2_ttf. Dunia awal dibuat secara prosedural (100 × 60 tile); permainan tidak memakai server atau akun. Aset visual dibuat sendiri dan tidak menyalin aset berhak cipta Growtopia.

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

Opsi `--screenshot <path>` menyimpan tangkapan layar; `--save <path>` memilih lokasi save yang menimpa lokasi bawaan. Tanpa override, save lokal menggunakan `SDL_GetPrefPath("Growlander", "Growlander")/world.glw`.

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
