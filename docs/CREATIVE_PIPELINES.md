# Yaratıcı üretim hatları

Üretim araçlarının çalışma zamanları `Tools/` altında, çıktıları `AssetWork/`
altında durur; ikisi de `.gitignore`'da. Depoya giren şey klasör iskeleti, lisans
kaydı ve bu hattı tekrar edilebilir kılan betiklerdir — üretilmiş dosyalar değil.

Hiçbiri oyunun çalışma zamanına bağlanmaz. Paketlenen oyun bunların hiçbirine
ihtiyaç duymadan çalışır.

## Görsel, UI ve PBR

- Çalışma zamanı: ComfyUI 0.28.0, CUDA 13 / PyTorch 2.13, `--lowvram`, yalnız
  `127.0.0.1:8188` dinler.
- Başlatma: `pwsh -File Scripts/Start-ComfyUI.ps1`. Betik dışarıya açılmaz ve
  otomatik tarayıcı açmaz.
- Checkpoint modeli otomatik indirilmez. 4 GB VRAM'de SD 1.5 sınıfı bir model
  yaklaşık 4 GB yer tutar; lisans kabulü ve disk teyidi olmadan seçilmez.
- Başlangıç workflow'u: `AssetWork/Workflows/comfyui-text2image-template.json`.
  Konsept, menü, UI ikonu ve seamless texture için prompt, boyut, seed ve output
  öneki bu şablondan uyarlanır.
- Base color dışındaki normal, roughness, metallic, AO ve height haritaları
  fiziksel doğruluk için ayrı doğrulanır; yalnız renk kanalını dönüştürüp "PBR"
  demek kabul edilmez.
- Her çıktı `Scripts/New-GeneratedAssetMetadata.ps1` ile prompt, model, model
  lisansı, seed, boyut ve kullanım planını taşıyan bir metadata dosyası alır.
  Metadata'sı olmayan dosya `Content/` içine girmez.

## Ses, müzik ve seslendirme

- FFmpeg 8.1.2 portable, SHA-256'sı doğrulanmış proje-local kurulum.
- Unreal öncesi WAV, varsayılan 48 kHz; seslendirme çoğunlukla mono, müzik ve
  ambience stereo.
- Adlandırma: `kategori_olay_varyasyon_dil_sürüm`.
- `Scripts/Test-AudioAsset.ps1` sample rate, kanal, süre ve EBU R128 tepe
  raporunu üretir.
- Loop başlangıç/bitişi ile kaynak ve lisans kaydı `AssetWork/Audio/Licenses`
  altında JSON olarak tutulur.
- Ticari bir TTS servisi kullanılacaksa API anahtarı ve ücretli çağrı gerekir;
  bu onay alınmadan üretim yapılmaz. Gerçek bir kişinin sesi açık rızası
  olmadan klonlanmaz.
- Yerel müzik/SFX/TTS modeli indirilmedi; 4 GB VRAM ve bakım eşiği nedeniyle
  şimdilik placeholder workflow kullanılır.

## Blender

- Blender 4.5.10 LTS portable kurulum; ayarları uygulamanın yanındaki
  `portable` dizinindedir.
- `.blend` kaynakları `AssetWork/Blender`, export'lar `AssetWork/Exports`,
  render'lar `AssetWork/Renders` altında.
- Import öncesi kontrol listesi: scale, pivot, normals, UV, material slots,
  collision, LOD ve polygon bütçesi.

## Lisans

`AssetWork/Licenses/README.md` her dış asset için doldurulacak alanları
tanımlar. Lisansı doğrulanmamış hiçbir dosya `Content/` içine import edilmez.
Bu üretilmiş varlıklar için de geçerlidir: üreten modelin lisansı çıktının
kullanılabilirliğini belirler.
