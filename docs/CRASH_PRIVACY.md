# Crash gizliliği

- Crash bundle yalnız kullanıcının açıkça seçtiği rapora eklenir; otomatik upload yoktur.
- Log, `CrashContext.runtime-xml`, minidump ve sistem bilgisi kişisel veri içerebilir.
- `Collect-CrashBundle.ps1` API key, token, parola, DSN ve kimlik bilgisi benzeri metni redakte eder; paylaşmadan önce insan incelemesi yine zorunludur.
- Kullanıcı adı, tam dosya yolu, IP, e-posta, makine adı ve serbest metin yorumları ayrıca gözden geçirilmelidir.
- Shipping loglarında auth header, URL query secret, kişisel mesaj ve tam kullanıcı dizini yazılmamalıdır.
- Sentry/başka endpoint ancak açık rıza, gizlilik politikası, saklama süresi ve DSN yapılandırmasından sonra etkinleştirilir.
- Symbol arşivleri public release’e konmaz; erişimi sınırlı tutulur.
- Unreal packaged build varsayılan olarak Epic’e crash göndermez; özel endpoint ayarlanmadan telemetry başlamaz.
