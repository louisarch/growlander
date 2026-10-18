# Growlander — preview mobile pribadi

Gameplay kini memakai landscape dengan kontrol dua jari: gerak di kiri bawah, lompat dan pukul/panen di kanan, serta pasang/tanam di atasnya. Sentuh tile untuk memilih target, pilih item pada hotbar enam slot, lalu gunakan tombol aksi. Tombol gerak dan aksi bisa ditahan.

HUD lebih lega: checklist besar, panel identitas/dunia, label di atas avatar, dan legenda keyboard permanen dihapus. Yang tersisa adalah judul kecil, **PEMAIN TUNGGAL**, permata, hotbar beserta jumlah item, dan tombol panel. Petunjuk satu baris menghilang setelah gerak, lompat, serta aksi dicoba. Pesan hasil aksi menghilang setelah tiga detik.

Inventaris, bantuan, serta menu hanya muncul saat dibuka dan menjeda dunia. Menu menyediakan lanjut, simpan, simpan & keluar, serta kembali ke rumah. Kehilangan fokus atau masuk latar belakang membersihkan input, menjeda permainan, dan menyimpan dunia lokal. Keyboard serta mouse desktop tetap tersedia.

## Lihat tampilannya

[Preview dari renderer game](../screenshots/mobile-preview.png). Ini tangkapan render Linux dari kode gameplay dan HUD yang sama, **bukan screenshot perangkat atau emulator Android**. Rasio 16:9 dipertahankan; layar sangat lebar dapat menampilkan letterbox.

## Yang sudah diuji

- Build C++ Linux dengan `-Wall -Wextra -Wpedantic`: lulus tanpa warning.
- Dua suite CTest: logika inti dan 17 pemeriksaan kontrol sentuh, seluruhnya lulus.
- Simulasi event sentuh SDL: menanam dua benih, memanen satu pohon, memecah dan memasang blok; gerak dan lompat dengan dua jari; inventaris dan background benar-benar menjeda waktu; tidak ada tombol tertahan setelah pelepasan. Lulus pada kanvas 1280 × 720 dan jendela lebar 1560 × 720 dengan letterbox; pemetaan target sentuh tetap akurat.
- Sepuluh pemeriksaan native desktop: gerak, lompat, panel jeda, resize, tanam, panen setelah 18 detik nyata, simpan/muat, serta keluar. Lulus.

Log berada di [`mobile-evidence/`](mobile-evidence/). Tes sentuh berjalan melalui antrean event SDL di Linux, bukan pada digitizer HP Android.

## Batas preview Android

Project Android tersedia di [`../android/`](../android/) dan petunjuk build di [`android-build.md`](android-build.md). **APK belum tersedia** karena lingkungan ini tidak memiliki Android SDK/NDK. Instalasi di HP, kompatibilitas versi Android, performa, area notch, tombol Back native, serta lifecycle Android nyata belum diverifikasi. Project disiapkan untuk dibangun di lingkungan Android yang lengkap; keberhasilan build APK belum diklaim.

Aset prosedural orisinal, logika C++, dan penyimpanan single-player tetap dipertahankan. Tidak ada server, multiplayer, akun, atau izin internet game yang ditambahkan. Setelah tinjauan pribadi, Louis mengizinkan commit dan push preview ini ke repositori privat. Visibilitas repo tidak diubah dan preview tidak dikirim ke Group/QA.
