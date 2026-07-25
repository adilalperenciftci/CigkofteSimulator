#!/usr/bin/env python3
"""Diyalog üretim hattının ikinci adımı: istemleri modele gönderip CSV üretir.

Girdi : Saved/Dialogue/prompts.jsonl   (editörde `CigGenerateDialogue` üretir)
Çıktı : Config/Dialogue/Lines.csv      (oyunun okuduğu tablo)

Neden ayrı bir betik: bu adım para harcar, saatler sürebilir ve yarıda kalabilir.
Ayrı olunca koşu durdurulup kaldığı yerden sürdürülebilir, çıktı commit'lenmeden
önce gözden geçirilebilir ve editör oturumu kilitlenmez.

Kullanım:
    set ANTHROPIC_API_KEY=...            (Windows)
    export ANTHROPIC_API_KEY=...         (bash)
    python Tools/generate_dialogue.py --limit 20      # önce küçük bir deneme
    python Tools/generate_dialogue.py                 # tamamı

Var olan Lines.csv'deki kovalar atlanır; yani koşu kesilirse tekrar çalıştırmak
kaldığı yerden devam eder.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROMPTS = ROOT / "Saved" / "Dialogue" / "prompts.jsonl"
OUT_CSV = ROOT / "Config" / "Dialogue" / "Lines.csv"

MODEL = "claude-haiku-4-5-20251001"
COLUMNS = ["Key", "Variant", "TR", "EN"]


def load_existing() -> tuple[dict[str, list[dict]], set[str]]:
    """Daha önce üretilmiş satırlar ve tamamlanmış kova anahtarları."""
    if not OUT_CSV.exists():
        return {}, set()
    rows: dict[str, list[dict]] = {}
    with OUT_CSV.open(encoding="utf-8-sig", newline="") as fh:
        for row in csv.DictReader(fh):
            rows.setdefault(row["Key"], []).append(row)
    return rows, set(rows.keys())


def write_csv(rows_by_key: dict[str, list[dict]]) -> None:
    OUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    with OUT_CSV.open("w", encoding="utf-8", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=COLUMNS, quoting=csv.QUOTE_MINIMAL)
        writer.writeheader()
        for key in sorted(rows_by_key):
            for row in rows_by_key[key]:
                writer.writerow({c: row.get(c, "") for c in COLUMNS})


def request_lines(client, prompt: str, variants: int) -> list[dict[str, str]]:
    """Modelden bir kova için replikleri ister. Biçim bozuksa boş liste döner."""
    message = client.messages.create(
        model=MODEL,
        max_tokens=1024,
        messages=[{"role": "user", "content": prompt}],
    )
    text = "".join(block.text for block in message.content if block.type == "text").strip()

    # The model sometimes wraps the JSON in a code fence.
    if text.startswith("```"):
        text = text.strip("`")
        text = text.split("\n", 1)[-1] if "\n" in text else text
        text = text.rsplit("```", 1)[0]

    try:
        data = json.loads(text)
    except json.JSONDecodeError:
        return []

    out = []
    for item in (data.get("lines") or [])[:variants]:
        tr = (item.get("tr") or "").strip()
        en = (item.get("en") or "").strip()
        if tr:
            out.append({"TR": tr, "EN": en})
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--limit", type=int, default=0,
                        help="yalnız ilk N kovayı üret (deneme koşusu için)")
    parser.add_argument("--sleep", type=float, default=0.0,
                        help="istekler arası bekleme saniyesi (hız sınırı için)")
    args = parser.parse_args()

    if not os.environ.get("ANTHROPIC_API_KEY"):
        print("ANTHROPIC_API_KEY tanımlı değil.", file=sys.stderr)
        return 2

    if not PROMPTS.exists():
        print(f"{PROMPTS} yok. Önce editörde `CigGenerateDialogue` çalıştır.", file=sys.stderr)
        return 2

    try:
        import anthropic
    except ImportError:
        print("anthropic paketi yok:  pip install anthropic", file=sys.stderr)
        return 2

    client = anthropic.Anthropic()
    rows_by_key, done = load_existing()

    buckets = [json.loads(line) for line in PROMPTS.read_text(encoding="utf-8").splitlines() if line.strip()]
    todo = [b for b in buckets if b["key"] not in done]
    if args.limit:
        todo = todo[:args.limit]

    print(f"{len(buckets)} kova, {len(done)} tamam, {len(todo)} işlenecek.")

    processed = 0
    try:
        for i, bucket in enumerate(todo, 1):
            key = bucket["key"]
            try:
                lines = request_lines(client, bucket["prompt"], int(bucket.get("variants", 4)))
            except Exception as exc:  # ağ/hız sınırı hatası koşuyu bitirmesin
                print(f"  [{i}/{len(todo)}] {key}: HATA {exc}")
                continue

            if not lines:
                print(f"  [{i}/{len(todo)}] {key}: biçim bozuk, atlandı")
                continue

            rows_by_key[key] = [
                {"Key": key, "Variant": str(n), "TR": ln["TR"], "EN": ln["EN"]}
                for n, ln in enumerate(lines)
            ]
            processed += 1
            if processed % 25 == 0:
                write_csv(rows_by_key)   # ara kayıt: koşu kesilirse iş kaybolmasın
                print(f"  ...{processed} kova yazıldı")
            if args.sleep:
                time.sleep(args.sleep)
    except KeyboardInterrupt:
        print("\nKesildi; o ana kadarki iş yazılıyor.")

    write_csv(rows_by_key)
    total = sum(len(v) for v in rows_by_key.values())
    print(f"\n{OUT_CSV.relative_to(ROOT)}: {len(rows_by_key)} kova, {total} replik.")
    print("Çıktıyı gözden geçirip commit etmeyi unutma.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
