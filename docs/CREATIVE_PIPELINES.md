# Yaratıcı üretim hatları

## Görsel, UI ve PBR

- Runtime: resmî ComfyUI v0.28.0, CUDA 13 / PyTorch 2.13, `--lowvram`, yalnız `127.0.0.1:8188`.
- Başlatma: `pwsh -File Scripts/Start-ComfyUI.ps1`; bağlantı health testi geçti.
- Model otomatik indirilmedi. 4 GB VRAM için SD 1.5 sınıfı yaklaşık 4 GB checkpoint ancak lisans kabulü ve disk teyidinden sonra seçilmelidir.
- Başlangıç workflow’u: `AssetWork/Workflows/comfyui-text2image-template.json`.
- Konsept/menü/UI/şeffaf ikon/seamless texture için prompt, boyut, seed ve output prefix uyarlanır.
- Base color dışındaki normal, roughness, metallic, AO ve height haritaları fiziksel doğruluk için ayrı doğrulanır; yalnız renk dönüştürerek “PBR” kabul edilmez.
- Her çıktı `Scripts/New-GeneratedAssetMetadata.ps1` ile metadata alır. Content’e doğrudan yazılmaz.

## Ses, müzik ve seslendirme

- FFmpeg 8.1.2 portable, SHA-256 doğrulanmış proje-local kurulumdur.
- Unreal öncesi WAV, varsayılan 48 kHz; voice çoğunlukla mono, müzik/ambience stereo.
- Ad: `kategori_olay_varyasyon_dil_sürüm`.
- `Scripts/Test-AudioAsset.ps1` sample rate, kanal, süre ve EBU R128 peak raporu üretir.
- Loop başlangıç/bitişi ve kaynak/lisans JSON’u `AssetWork/Audio/Licenses` altında tutulur.
- ElevenLabs MCP resmî paket olarak kurulu; API key/ücretli çağrı olmadan üretim yapılmaz. Gerçek kişi sesi açık rıza olmadan klonlanmaz.
- Yerel music/SFX/TTS modeli otomatik indirilmedi; 4 GB VRAM ve lisans/bakım eşiği nedeniyle placeholder workflow kullanılmalıdır.

## Blender

- Blender Foundation 4.5.10 LTS portable kuruldu; ayarlar uygulama yanındaki `portable` dizinindedir.
- Resmî “Blender Lab MCP Server” bulunmadığı için community MCP kurulmadı.
- `.blend`: `AssetWork/Blender`; export: `AssetWork/Exports`; render: `AssetWork/Renders`.
- Import öncesi scale, pivot, normals, UV, material slots, collision, LOD ve polygon budget kontrol edilir.
