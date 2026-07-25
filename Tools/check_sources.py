#!/usr/bin/env python3
"""Cigkofte Simulator - lightweight source and data checks.

A full Unreal build is too heavy for a free runner, so CI only looks at things
the compiler cannot catch, or would catch too late:

  * are the source files valid UTF-8 (have the Turkish characters been mangled),
  * does every header have `#pragma once`,
  * do the balance CSVs have the expected columns and row counts,
  * do the CSV row keys line up exactly with the C++ enums,
  * are the licence and credits files still in place.

Runs locally too:  python Tools/check_sources.py
Exits non-zero if it finds a problem.
"""

from __future__ import annotations

import csv
import io
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "Source"
BALANCE = ROOT / "Config" / "Balance"

problems: list[str] = []


def fail(msg: str) -> None:
    problems.append(msg)


# --------------------------------------------------------------- source files

def check_sources() -> None:
    headers = 0
    for path in sorted(SOURCE.rglob("*")):
        if path.suffix not in {".h", ".cpp", ".cs"}:
            continue
        raw = path.read_bytes()
        rel = path.relative_to(ROOT).as_posix()

        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError as exc:
            fail(f"{rel}: UTF-8 değil ({exc}). Türkçe karakterler bozulmuş olabilir.")
            continue

        if raw.startswith(b"\xef\xbb\xbf"):
            fail(f"{rel}: UTF-8 BOM var. Kaynak dosyalarda BOM istemiyoruz.")

        if path.suffix == ".h":
            headers += 1
            if "#pragma once" not in text:
                fail(f"{rel}: '#pragma once' yok.")

    if headers == 0:
        fail("Hiç başlık dosyası bulunamadı — yol yanlış olabilir.")
    else:
        print(f"  kaynak: {headers} başlık tarandı")


# --------------------------------------------------------------- balance CSVs

# Column names and expected row counts. The numbers are the lengths of the C++
# enums; change one and the static_assert in CigBalance.cpp fails at compile
# time while this check fails in CI. Having both is deliberate: the data and the
# code can break independently.
CSV_SCHEMA = {
    "Skills.csv": (["Key", "Name", "Desc", "MaxRank", "EffectPerRank"], 8),
    "Upgrades.csv": (["Key", "Name", "Desc", "Cost", "MinLevel"], 13),
    "Traits.csv": (
        ["Key", "Name", "Weight", "MinDay", "RareChance",
         "PatienceMult", "TipChanceDelta", "TipMultOverride"], 14),
    "Stock.csv": (["Index", "Label", "BaseCost", "StartAmount", "OrderAmount"], 18),
    "Pricing.csv": (
        ["Index", "Label", "TabanFiyat", "Esneklik", "MinCarpan", "MaxCarpan"], 8),
    "Mahalle.csv": (["Index", "Label", "GelirCarpani"], 7),
    "Staff.csv": (
        ["Key", "Label", "Hiz", "Titizlik", "GulerYuz", "MaasBeklentisi"], 6),
    "Inspection.csv": (["Key", "Label", "Deger"], 13),
    "Tutorial.csv": (["Key", "Label", "MetinAnahtari", "VurguIstasyon"], 8),
    "Audio.csv": (["Key", "Label", "Deger"], 6),
    "Social.csv": (["Key", "Label", "Deger"], 18),
    "Events.csv": (
        ["Key", "Label", "MinGun", "Sans", "Sure", "SpawnCarpani", "SabirCarpani",
         "FiyatCarpani", "TeslimatCarpani", "StokCarpani", "TakvimPeriyodu"], 14),
    "Achievements.csv": (["Key", "Name", "Desc", "Stat", "Threshold"], 20),
}

# Must be the same list as StatValue() in CigAchievementSystem.cpp.
KNOWN_STATS = {
    "TotalServed", "TotalEarned", "TotalPerfectOrders", "TotalDeliveries",
    "Rep", "Level", "Money", "Day", "Hygiene", "CatHappy", "PrestigeCount",
    "OwnHouse", "AllUpgrades", "ApprenticeHired",
}


def read_csv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    raw = path.read_bytes()
    # The game side can read a BOM; utf-8-sig accepts either form.
    text = raw.decode("utf-8-sig")
    reader = csv.DictReader(io.StringIO(text))
    rows = list(reader)
    fields = list(reader.fieldnames or [])

    # An unquoted comma inside a field shifts the columns: half the text moves
    # into the next column and that column's real value is silently lost. Both
    # columns still look populated, so the "missing translation" check never
    # fires and someone playing in English sees half a Turkish sentence.
    # A field containing a comma must be quoted.
    rel = path.relative_to(ROOT).as_posix()
    for n, row in enumerate(rows, start=2):
        if row.get(None) is not None:
            key = row.get(fields[0]) if fields else "?"
            fail(f"{rel}:{n}: '{key}' fazladan alan üretiyor — "
                 f"virgül içeren alanı çift tırnak içine al.")

    return fields, rows


def check_balance() -> None:
    if not BALANCE.is_dir():
        fail(f"{BALANCE.relative_to(ROOT)} yok — denge dosyaları kaybolmuş.")
        return

    for name, (columns, expected_rows) in CSV_SCHEMA.items():
        path = BALANCE / name
        rel = path.relative_to(ROOT).as_posix()
        if not path.exists():
            fail(f"{rel}: dosya yok.")
            continue

        try:
            fields, rows = read_csv(path)
        except UnicodeDecodeError as exc:
            fail(f"{rel}: UTF-8 değil ({exc}).")
            continue

        if fields != columns:
            fail(f"{rel}: sütunlar beklenenden farklı.\n"
                 f"    beklenen: {columns}\n"
                 f"    bulunan : {fields}")
            continue

        if len(rows) != expected_rows:
            fail(f"{rel}: {expected_rows} satır bekleniyordu, {len(rows)} var. "
                 f"Enum'a girdi eklendiyse CigBalance.cpp'deki varsayılanı ve "
                 f"bu betikteki sayıyı da güncelle.")

        keys = [r.get("Key") or r.get("Index") for r in rows]
        if len(set(keys)) != len(keys):
            fail(f"{rel}: yinelenen anahtar var — {keys}")

        for row in rows:
            for col in columns:
                if (row.get(col) or "").strip() == "":
                    fail(f"{rel}: '{keys and row.get('Key') or row.get('Index')}' "
                         f"satırında '{col}' boş.")

        if name == "Achievements.csv":
            for row in rows:
                stat = (row.get("Stat") or "").strip()
                if stat not in KNOWN_STATS:
                    fail(f"{rel}: '{row.get('Key')}' bilinmeyen Stat kullanıyor: "
                         f"'{stat}'. Bu başarım oyunda hiç açılmaz.")

    print(f"  denge: {len(CSV_SCHEMA)} CSV doğrulandı")


# --------------------------------------------------------------- repo files

# Bucket key format: must match CigDialogueContext::BucketKey.
BUCKET_KEY_RE = re.compile(r"^m[0-4]_t([0-9]|1[0-4])_v[01]_r[01]_a[01]_h[01]_p[01]$")


def check_dialogue() -> None:
    path = ROOT / "Config" / "Dialogue" / "Lines.csv"
    if not path.exists():
        # The table may not be generated yet; the game falls back to canned lines.
        print("  diyalog: Lines.csv yok (hazir cumleler kullanilir)")
        return

    rel = path.relative_to(ROOT).as_posix()
    try:
        fields, rows = read_csv(path)
    except UnicodeDecodeError as exc:
        fail(f"{rel}: UTF-8 değil ({exc}).")
        return

    expected = ["Key", "Variant", "TR", "EN"]
    if fields != expected:
        fail(f"{rel}: sütunlar {expected} olmalı, {fields} bulundu.")
        return

    seen: set[tuple[str, str]] = set()
    bad_keys = 0
    for row in rows:
        key = (row.get("Key") or "").strip()
        variant = (row.get("Variant") or "").strip()

        # A malformed key silently becomes a line the game can never find.
        if not BUCKET_KEY_RE.match(key):
            bad_keys += 1
            if bad_keys <= 5:
                fail(f"{rel}: kova anahtarı biçimi bozuk: '{key}'")
            continue

        if (key, variant) in seen:
            fail(f"{rel}: yinelenen satır — {key} varyant {variant}")
        seen.add((key, variant))

        if not (row.get("TR") or "").strip():
            fail(f"{rel}: '{key}' varyant {variant} için TR boş.")

    if bad_keys > 5:
        fail(f"{rel}: toplam {bad_keys} bozuk anahtar var (ilk 5'i yukarıda).")

    print(f"  diyalog: {len(rows)} replik, {len({k for k, _ in seen})} kova doğrulandı")


def check_text() -> None:
    """Every UI string must be filled in for both languages.

    A missing translation silently falls back to Turkish; someone playing in
    English gets a mixed-language screen and nobody notices. Fail here instead.
    """
    path = ROOT / "Config" / "Text" / "Strings.csv"
    if not path.exists():
        fail("Config/Text/Strings.csv yok — arayüzde anahtarlar görünür.")
        return

    rel = path.relative_to(ROOT).as_posix()
    fields, rows = read_csv(path)
    expected = ["Key", "TR", "EN"]
    if fields != expected:
        fail(f"{rel}: sütunlar {expected} olmalı, {fields} bulundu.")
        return

    seen: set[str] = set()
    slots: dict[str, set[int]] = {}
    for row in rows:
        key = (row.get("Key") or "").strip()
        if not key:
            continue
        if key in seen:
            fail(f"{rel}: yinelenen anahtar '{key}'")
        seen.add(key)
        for lang in ("TR", "EN"):
            if not (row.get(lang) or "").strip():
                fail(f"{rel}: '{key}' için {lang} çevirisi eksik.")

        # Both languages must carry the same placeholders: CigText::Format hands
        # out arguments in order, so if {1} is absent in one language that value
        # is silently dropped.
        tr, en = slot_set(row.get("TR") or ""), slot_set(row.get("EN") or "")
        if tr != en:
            fail(f"{rel}: '{key}' yer tutucuları uyuşmuyor — "
                 f"TR {sorted(tr)}, EN {sorted(en)}.")
        slots[key] = tr | en

    check_text_usage(seen, slots)
    print(f"  metin: {len(seen)} anahtar, 2 dil doğrulandı")


SLOT_RE = re.compile(r"\{(\d+)\}")
TEXT_CALL_RE = re.compile(r'CigText::(Get|Format)\s*\(\s*TEXT\("([^"]+)"\)')


def slot_set(template: str) -> set[int]:
    return {int(m) for m in SLOT_RE.findall(template)}


def split_args(text: str, start: int) -> int:
    """Number of arguments in a Format call, excluding the key.

    `start` points just past the closing TEXT("..."); from there we count
    top-level commas, tracking bracket depth to the closing parenthesis.
    """
    depth, count, i = 1, 0, start
    while i < len(text) and depth > 0:
        c = text[i]
        if c == '"':  # dize icindeki virgul/parantez sayilmaz
            i += 1
            while i < len(text) and not (text[i] == '"' and text[i - 1] != "\\"):
                i += 1
        elif c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        elif c == "," and depth == 1:
            count += 1
        i += 1
    return count


def check_text_usage(keys: set[str], slots: dict[str, set[int]]) -> None:
    """The CigText calls in the code must line up with the table.

    Format placeholders are resolved at runtime, so the compiler sees neither a
    missing key nor a missing argument. Both end up on screen as a raw "{1}" or
    as the key name - easy to miss in a menu nobody tests.
    """
    bad = 0
    for path in sorted(SOURCE.rglob("*.cpp")):
        if path.parent.name == "Tests":  # testler bilerek olmayan anahtar dener
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(ROOT).as_posix()
        for m in TEXT_CALL_RE.finditer(text):
            kind, key = m.group(1), m.group(2)
            line = text.count("\n", 0, m.start()) + 1
            if key not in keys:
                fail(f"{rel}:{line}: '{key}' Strings.csv'de yok.")
                bad += 1
                continue
            need = max(slots[key]) + 1 if slots[key] else 0
            if kind == "Get":
                if need:
                    fail(f"{rel}:{line}: '{key}' {need} argüman istiyor ama "
                         f"Get() ile çağrılmış — Format kullan.")
                    bad += 1
            else:
                got = split_args(text, m.end())
                if got != need:
                    fail(f"{rel}:{line}: '{key}' {need} yer tutucu bekliyor, "
                         f"{got} argüman verilmiş.")
                    bad += 1

    if bad == 0:
        print("  metin kullanımı: anahtarlar ve argüman sayıları tutuyor")


def check_decoupling() -> None:
    """Direct calls between systems must not creep back in.

    The event bus (Game/CigEventBus.h) exists precisely for this: a publishing
    system should not know its listeners. If someone writes
    GM->Quests->Notify... out of habit the coupling returns silently; this
    catches it.
    """
    offenders = []
    for path in sorted(SOURCE.rglob("*.cpp")):
        rel = path.relative_to(ROOT).as_posix()
        # The quest system itself and the HUD are exempt: the HUD has to read
        # quest state, and that is a query rather than an event. Tests are exempt
        # too - they may mention the pattern in their comments.
        if ("CigQuestSystem" in path.name or "CigkofteHUD" in path.name
                or path.parent.name == "Tests"):
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for n, line in enumerate(text.splitlines(), 1):
            if "Quests->Notify" in line:
                offenders.append(f"{rel}:{n}")

    if offenders:
        fail("Sistemler arası doğrudan görev çağrısı geri gelmiş "
             f"(olay otobüsünü kullan): {', '.join(offenders)}")
    else:
        print("  kuplaj: yayıncılar görev sistemini doğrudan çağırmıyor")


def check_repo_files() -> None:
    for name in ("LICENSE", "CREDITS.md", "README.md", ".gitattributes"):
        if not (ROOT / name).exists():
            fail(f"{name} yok.")
    print("  depo: zorunlu dosyalar yerinde")


def main() -> int:
    print("Cigkofte kaynak kontrolu")
    check_sources()
    check_balance()
    check_dialogue()
    check_text()
    check_decoupling()
    check_repo_files()

    if problems:
        print(f"\n{len(problems)} sorun bulundu:\n")
        for p in problems:
            print(f"  - {p}")
        return 1

    print("\nHepsi temiz.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
