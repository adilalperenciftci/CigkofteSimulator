# Çiğköfte Simulator — Asset Kaynakları ve Lisanslar

Bir **EG Games** yapımı.

## Görsel & Ses Assetleri (Kenney)

Aşağıdaki low poly modeller ve ses efektleri **Kenney** (https://kenney.nl)
tarafından **Creative Commons CC0 1.0 (kamu malı)** lisansıyla yayınlanmıştır.
CC0 ticari kullanıma uygundur ve atıf zorunlu değildir; yine de teşekkür ederiz.

### Modeller (`Content/LowPoly/`)
- **Food Kit** — https://kenney.nl/assets/food-kit
  (lavaş/taco, sub, tabak, domates, soğan, limon, marul, bardak, şişe vb.)
- **Furniture Kit** — https://kenney.nl/assets/furniture-kit
  (masa, sandalye, buzdolabı, ocak, lavabo, dolap, çöp kovası, saksı, radyo, kanepe)

### Sesler (`Content/Audio/`)
- **Interface Sounds** — https://kenney.nl/assets/interface-sounds (UI tık/onay/hata)
- **Impact Sounds** — https://kenney.nl/assets/impact-sounds (yoğurma)
- **RPG Audio** — https://kenney.nl/assets/rpg-audio (doğrama, bıçak, para, kap, kumaş)
- **Music Jingles** — https://kenney.nl/assets/music-jingles (menü/gün başı/gün sonu)

Lisans metni: https://creativecommons.org/publicdomain/zero/1.0/

## Ortam Sesleri (Gregor Quendel)

`Content/Audio/S_AmbStreet`, `S_AmbNight` ve `S_AmbRain` şu paketten türetilmiştir:

- **Free City & Nature Sounds** — Gregor Quendel (Cinematic Sound Design), Fab.
  Lisans: **Creative Commons Attribution 4.0 (CC BY 4.0)**,
  https://creativecommons.org/licenses/by/4.0/

CC BY 4.0 atıf ve yapılan değişikliğin belirtilmesini şart koşar. Değişiklikler:

- `S_AmbStreet` — `WAV_City_Ambience_Traffic_Street_Cars_and_tram.wav` kaydının
  78. saniyesinden 45 saniyelik bölüm; kuyruk çapraz geçişiyle kesintisiz
  döngüye getirildi ve tepe seviyesi normalize edildi.
- `S_AmbNight` — aynı kaydın 190. saniyesinden (kaydın en tenha bölümü)
  45 saniye; 900 Hz alçak geçiren filtreyle uzaklaştırıldı, aynı şekilde
  döngüye getirildi. Gece yatağının gündüzle aynı sokaktan gelmesi kasıtlıdır.
- `S_AmbRain` — `WAV_Rain_Dropping_on_various_textures.wav` kaydının 3. saniyesinden
  43 saniye, aynı döngü işlemiyle.

## Motor İçeriği
- `/Engine/BasicShapes` primitive mesh'leri ve `Roboto` fontu Unreal Engine ile gelir;
  dünya ve karakterler runtime'da bu primitive'ler + Kenney mesh'leri karışımıyla kurulur.

## Kod
Tüm gameplay sistemleri özgün C++ olarak yazılmıştır (Blueprint gameplay mantığı yoktur).
