# Performans bütçesi

| Alan | Hedef | Ölçüm | Sonuç |
|---|---:|---|---|
| Hedef FPS | 60 | Packaged Development benchmark | TBD |
| Frame budget | 16.67 ms | Unreal Insights frame track | TBD |
| Game thread | ≤ 8 ms | Insights CPU track | TBD |
| Render thread | ≤ 8 ms | Insights CPU track | TBD |
| GPU frame | ≤ 16.67 ms | Insights GPU track | TBD |
| 1% low | ≥ 45 FPS | Sabit sahne benchmark | TBD |
| Draw call | Sahneye göre belirlenecek | `stat RHI` | TBD |
| Triangle | Görüş/sahne bütçesi belirlenecek | Primitive stats | TBD |
| Texture memory | Hedef GPU VRAM’in ≤ %70’i | `stat streaming` | TBD |
| System memory | 16 GB sistemde güvenli pay | Insights memory | TBD |
| Shader hitch | Oynanışta 0 kritik hitch | Insights/log | TBD |
| Loading time | Ana akış hedefi belirlenecek | Bookmark/trace | TBD |
| Shipping build size | Release hedefi belirlenecek | Verify-Release | TBD |

Benchmark çözünürlüğü, scalability profili, map, kamera rotası, build commit’i ve süre her koşuda kaydedilmelidir. Binary `.utrace` modele aktarılmaz; Insights içinden CSV/stat özeti kullanılır.
