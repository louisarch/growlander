# Build Android Growlander

Project Android ada di `android/` dari root repositori. Ini tetap game C++ SDL2: `android/app/src/main/jni/CMakeLists.txt` membangun satu `libmain.so` C++17 dari `src/main.cpp` dan `src/game.cpp`, tanpa menyalin atau mengubah logika `game.cpp`. SDL Activity Java hanya glue resmi upstream SDL2; logika permainan tetap di C++.

## Versi yang dipatok

| Komponen | Versi |
| --- | --- |
| SDL2 | 2.30.11 |
| SDL2_ttf | 2.22.0 (arsip rilis menyertakan FreeType) |
| Gradle | 8.9; launcher `./gradlew` mengunduh distribusi resmi dan memverifikasi SHA-256 |
| Android Gradle Plugin | 8.7.3 |
| Android compile/target SDK | 35 |
| Android min SDK | 23 |
| Android NDK | 27.2.12479018 |
| CMake Android | 3.22.1 |
| ABI APK | `arm64-v8a`, `armeabi-v7a`, `x86_64` |

JDK 17+ diperlukan; Java 21 terpasang di mesin ini. Gradle pertama kali perlu jaringan untuk mengambil distribusi dan plugin; SDK Manager perlu mengunduh SDK/NDK/CMake jika belum terpasang.

## Siapkan dependensi lalu build

Dari direktori `android/` pada repositori:

```sh
./bootstrap.sh
./gradlew assembleDebug
```

`bootstrap.sh` men-download rilis upstream SDL2 dan SDL2_ttf yang dipatok, memeriksa SHA-256 arsip, menaruh source di `app/src/main/jni/SDL{,_ttf}`, menyalin SDLActivity Java upstream, dan menyalin font permainan ke `app/src/main/assets/DejaVuSans.ttf`. Sumber C++ permainan selalu dipakai langsung dari direktori `src/` pada root repositori.

Di Android Studio SDK Manager, pasang **Android SDK Platform 35**, **NDK 27.2.12479018 (Side by side)**, dan **CMake 3.22.1**. Atau pasang paket-paket persis tersebut dengan `sdkmanager`. Setelah itu build APK debug ada di:

```text
app/build/outputs/apk/debug/app-debug.apk
```

Instalasi pada perangkat/emulator, bila sudah tersedia: `./gradlew installDebug`. App bersifat single-player/offline; manifest mengunci orientasi landscape, fullscreen, dan tidak meminta `INTERNET`.

## Font dan SDL Android

Font ada di root APK `assets/DejaVuSans.ttf`. SDL2 Android mendukung pembacaan asset APK lewat SDL RWops (`SDL_RWFromFile`); pada Android gunakan nama relatif `DejaVuSans.ttf` untuk `TTF_OpenFont`/`TTF_OpenFontRW`, bukan pemeriksaan keberadaan filesystem terhadap file yang masih berada di dalam APK. `main.cpp` memakai cabang `__ANDROID__` untuk mengembalikan nama tersebut. SDL Activity memang meneruskan input sentuh SDL dan menyediakan lifecycle Android; game dan render tetap C++.

## Status verifikasi dan batasan mesin ini

- Pada penyiapan preview, bootstrap dijalankan berhasil; arsip SDL2 2.30.11 dan SDL2_ttf 2.22.0 lolos pemeriksaan hash, source Android Activity terpasang, dan font hasil salinan SHA-256-nya identik dengan `assets/DejaVuSans.ttf` pada root repositori. Arsip cache dan source dependensi hasil unduhan tidak disertakan dalam Git; jalankan bootstrap setelah clone.
- Konfigurasi Gradle, manifest landscape, daftar ABI/versi, CMake source references, serta tidak adanya permission `INTERNET` telah diperiksa. Logika `game.cpp` tidak berubah. Configure dan build target `main` terhadap dependensi vendored berhasil di host Linux; ini bukan cross-build Android. CMake root hanya ditambah tes sentuh dan syarat SDL minimum.
- **APK belum dibangun dan tidak ada klaim pengujian di Android.** Saat pemeriksaan, Java 21 dan CMake tersedia, sedangkan `gradle`, `sdkmanager`, `adb`, Android SDK, dan NDK tidak terdeteksi. SDK/NDK/Gradle toolchain tidak dipasang agar tidak memulai unduhan besar; akibatnya build Gradle, instalasi perangkat, dan emulator belum dapat diuji. Mesin ini juga tidak menyediakan emulator Android yang terdeteksi.

Rujukan implementasi: [panduan Android SDL2](https://github.com/libsdl-org/SDL/blob/release-2.30.11/docs/README-android.md), [rilis SDL2 2.30.11](https://github.com/libsdl-org/SDL/releases/tag/release-2.30.11), dan [rilis SDL2_ttf 2.22.0](https://github.com/libsdl-org/SDL_ttf/releases/tag/release-2.22.0).
