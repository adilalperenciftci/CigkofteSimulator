# SteamPipe hazırlığı

1. Steamworks partner hesabından AppID/DepotID ve güncel Steamworks SDK erişimini edin.
2. Şablonları proje dışındaki güvenli bir yayın çalışma klasörüne kopyala; placeholder değerlerini orada doldur.
3. Kullanıcı adı/şifre/token/VDF auth bilgisini repoya yazma. SteamCMD etkileşimli login ve Steam Guard kullan.
4. Önce `Scripts/Publish-Steam-DryRun.ps1` ile placeholder, dosya ve `preview=1` denetimi yap.
5. Gerçek upload yalnız açık kullanıcı talebi ve `-ConfirmUpload` ile yapılır.
6. `setlive` boş kalır; upload edilen build’i canlı branche taşımak ayrı ve açık talimat gerektirir.
