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

## Motor İçeriği
- `/Engine/BasicShapes` primitive mesh'leri ve `Roboto` fontu Unreal Engine ile gelir;
  dünya ve karakterler runtime'da bu primitive'ler + Kenney mesh'leri karışımıyla kurulur.

## Kod
Tüm gameplay sistemleri özgün C++ olarak yazılmıştır (Blueprint gameplay mantığı yoktur).
