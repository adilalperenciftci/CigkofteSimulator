# Performance budget

Every row is still TBD. Nothing on this branch has been measured, which is worth
stating plainly rather than leaving the table to imply otherwise.

| Area | Target | How it is measured | Result |
|---|---:|---|---|
| Frame rate | 60 | Packaged Development benchmark | TBD |
| Frame budget | 16.67 ms | Unreal Insights frame track | TBD |
| Game thread | ≤ 8 ms | Insights CPU track | TBD |
| Render thread | ≤ 8 ms | Insights CPU track | TBD |
| GPU frame | ≤ 16.67 ms | Insights GPU track | TBD |
| 1% low | ≥ 45 FPS | Fixed-scene benchmark | TBD |
| Draw calls | To be set per scene | `stat RHI` | TBD |
| Triangles | View and scene budget to be set | Primitive stats | TBD |
| Texture memory | ≤ 70% of the target GPU's VRAM | `stat streaming` | TBD |
| System memory | Safe headroom on a 16 GB machine | Insights memory | TBD |
| Shader hitches | 0 critical hitches in play | Insights / log | TBD |
| Load time | Main-flow target to be set | Bookmark / trace | TBD |
| Shipping build size | Release target to be set | Verify-Release | TBD |

Every run must record the benchmark resolution, the scalability profile, the map,
the camera route, the build commit and the duration. Binary `.utrace` files are
not carried into a model context; the CSV or stat summary from inside Insights
is.
