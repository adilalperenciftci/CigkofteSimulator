#!/usr/bin/env python3
"""Cigkofte Simulator - lightweight source and data checks.

A full Unreal build is too heavy for a free runner, so CI only looks at things
the compiler cannot catch, or would catch too late:

  * are the source files valid UTF-8 (have the Turkish characters been mangled),
  * does every header have `#pragma once`,
  * do the balance CSVs have the expected columns and row counts,
  * do the CSV row keys line up exactly with the C++ enums,
  * is every asset the code loads by path listed in DirectoriesToAlwaysCook,
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
    "Inspection.csv": (["Key", "Label", "Deger"], 15),
    "Tutorial.csv": (["Key", "Label", "MetinAnahtari", "VurguIstasyon"], 8),
    "Audio.csv": (["Key", "Label", "Deger"], 6),
    "Social.csv": (["Key", "Label", "Deger"], 18),
    "Contracts.csv": (["Key", "Label", "Deger"], 12),
    "Events.csv": (
        ["Key", "Label", "MinGun", "Sans", "Sure", "SpawnCarpani", "SabirCarpani",
         "FiyatCarpani", "TeslimatCarpani", "StokCarpani", "TakvimPeriyodu",
         "OzelTur"], 14),
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

    # The a-flag says whether the customer got the order they asked for. The
    # table used to be written entirely against a1, because the runtime copied
    # the delivered order from the requested one and could never produce a0 -
    # so every line for a wrong order was missing and nobody noticed. A table
    # covering only one side of a flag means half the runtime states fall back
    # to canned lines.
    buckets = {k for k, _ in seen}
    for flag, ne_olur in (("_a1_", "doğru siparişe"), ("_a0_", "yanlış siparişe")):
        if not any(flag in b for b in buckets):
            fail(f"{rel}: {ne_olur} ait hiç replik yok ({flag} kovası boş) — "
                 f"o durumdaki her müşteri hazır cümleye düşer.")

    print(f"  diyalog: {len(rows)} replik, {len(buckets)} kova doğrulandı")


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

# A key held in a variable or a table instead of written at the call site. The
# regex above only sees CigText::Get(TEXT("...")), so the mood line pools in
# CigOfflineDialogueProvider - arrays of keys, indexed at random - were invisible
# to it. A dotted lowercase literal is a key by convention here, and the cost of
# being wrong is one false positive rather than a customer saying
# "dlg.mood.angry.1" out loud.
TEXT_KEY_LITERAL_RE = re.compile(r'TEXT\("((?:[a-z][a-z0-9]*\.){2,}[a-z0-9]+)"\)')


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

    # Keys that never appear inside a CigText:: call. Checked separately because
    # the loop above can only see what is written at the call site.
    for path in sorted(SOURCE.rglob("*.cpp")):
        if path.parent.name == "Tests":
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(ROOT).as_posix()
        called = {m.group(2) for m in TEXT_CALL_RE.finditer(text)}
        for m in TEXT_KEY_LITERAL_RE.finditer(text):
            key = m.group(1)
            if key in called or key in keys:
                continue
            line = text.count("\n", 0, m.start()) + 1
            fail(f"{rel}:{line}: '{key}' metin anahtarı gibi duruyor ama "
                 f"Strings.csv'de yok — ekranda anahtar adı görünür.")
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


# Key-based balance CSVs and the CigBalance.cpp function holding their defaults.
# Index-based tables (Stock, Pricing, Mahalle) are matched by position instead,
# which the row-count check already covers.
CSV_DEFAULTS = {
    "Skills.csv": "DefaultSkills",
    "Upgrades.csv": "DefaultUpgrades",
    "Traits.csv": "DefaultTraits",
    "Achievements.csv": "DefaultAchievements",
    "Inspection.csv": "DefaultInspection",
    "Social.csv": "DefaultSocial",
    "Contracts.csv": "DefaultContracts",
    "Audio.csv": "DefaultAudio",
    "Tutorial.csv": "DefaultTutorial",
    "Events.csv": "DefaultEvents",
    "Staff.csv": "DefaultStaff",
}

DEFAULT_FN_RE = re.compile(
    r"TArray<\w+>\s+(Default\w+)\(\)\s*\{(.*?)\n\t\}", re.DOTALL)
FIRST_TEXT_RE = re.compile(r"Make\w+\(\s*TEXT\(\"([^\"]+)\"\)")


def check_balance_keys() -> None:
    """A CSV row whose Key is not in the C++ defaults does nothing at all.

    CigBalance loads the defaults first and then lets the CSV overwrite rows it
    can find by key. A key that matches nothing is not an error to the loader -
    it simply never applies, so the file looks like it is tuning the game while
    the game keeps using the built-in number. Renaming a key in one place and
    not the other is the easy way to cause that, which is exactly what splitting
    the bulk order into a contract and a large delivery risked.
    """
    src = ROOT / "Source" / "CigkofteSimulator" / "Core" / "CigBalance.cpp"
    if not src.exists():
        fail("CigBalance.cpp yok — varsayılan anahtarlar doğrulanamıyor.")
        return

    text = src.read_text(encoding="utf-8", errors="replace")
    defaults = {m.group(1): set(FIRST_TEXT_RE.findall(m.group(2)))
                for m in DEFAULT_FN_RE.finditer(text)}

    checked = 0
    before = len(problems)
    for name, fn in CSV_DEFAULTS.items():
        known = defaults.get(fn)
        if not known:
            fail(f"CigBalance.cpp içinde {fn}() bulunamadı veya boş — "
                 f"{name} anahtarları doğrulanamıyor.")
            continue

        path = BALANCE / name
        if not path.exists():
            continue
        _, rows = read_csv(path)

        rel = path.relative_to(ROOT).as_posix()
        for n, row in enumerate(rows, start=2):
            key = (row.get("Key") or "").strip()
            if key and key not in known:
                fail(f"{rel}:{n}: '{key}' {fn}() içinde yok — bu satır hiçbir "
                     f"zaman uygulanmaz, oyun varsayılanı kullanır.")

        missing = known - {(r.get("Key") or "").strip() for r in rows}
        if missing:
            fail(f"{rel}: {fn}() içindeki şu anahtarlar CSV'de yok: "
                 f"{', '.join(sorted(missing))}")
        checked += 1

    if len(problems) == before:
        print(f"  anahtarlar: {checked} CSV varsayılanlarla eşleşiyor")


# Calls that must have exactly one caller, because a second one is not a
# compile error - it silently produces a second, divergent answer.
#
#   (pattern, files allowed to contain it, what goes wrong otherwise)
#
# The allowed list holds the one legitimate caller and, where the function is
# defined in a .cpp rather than inline in a header, its definition site.
SINGLE_CALLER_RULES = [
    ("RegisterSale(", ("CigSaleSystem",),
     "Gün hasılatı satış hattı dışından kaydediliyor (UCigSaleSystem üzerinden geç)"),
    ("PolicyPriceMult(", ("CigPricingSystem", "CigEconomySystem"),
     "Fiyat politikası çarpanı fiyatlandırma dışında uygulanıyor "
     "(UCigPricingSystem::EtkinCarpan üzerinden geç)"),
]


def check_single_callers() -> None:
    """Some calls are only correct when nothing else makes them.

    RegisterSale is what the day summary, the best-day record and every
    end-of-day comparison are built on. It used to be called from the counter,
    from the staff system and from deliveries, and each caller booked a slightly
    different set of consequences alongside it - which is how a staff sale ended
    up invisible to bulk orders and achievements.

    PolicyPriceMult is the same shape of problem seen from the other side. The
    price the customer pays is the per-product markup times the shop policy, and
    while the sale path applied the policy itself, everything that judged the
    price read the markup alone - so switching to the expensive policy raised
    every bill by a quarter without demand, the reviews or the tablet noticing.
    A second caller would either double-charge it or reopen that gap.
    """
    ok = True
    for pattern, owners, message in SINGLE_CALLER_RULES:
        offenders = []
        for path in sorted(SOURCE.rglob("*.cpp")):
            rel = path.relative_to(ROOT).as_posix()
            # Tests are exempt too: they may name the pattern in a comment.
            if any(o in path.name for o in owners) or path.parent.name == "Tests":
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for n, line in enumerate(text.splitlines(), 1):
                if pattern in line:
                    offenders.append(f"{rel}:{n}")

        if offenders:
            fail(f"{message}: {', '.join(offenders)}")
            ok = False

    if ok:
        print(f"  tek çağıran: {len(SINGLE_CALLER_RULES)} kural tutuyor")


# How CigMeshLibrary turns a call-site argument into a content folder. Each
# prefix must still appear in CigMeshLibrary.cpp; rename one there without
# touching this and the check would quietly stop covering that pack, which is
# the same class of silence it exists to catch.
MESH_LOADERS = {
    "Park": "CityPark/Meshes",
    "Bazaar": "Scene_Bazaar_Vol1/Assets/MS/3D",
    "Load": "LowPoly",
    "LoadAt": "",  # the caller passes the whole folder
}

# CigMesh::Park(TEXT("Props"), ...) and the PreferPark(...) wrappers alike.
MESH_CALL_RE = re.compile(
    r'(?:CigMesh::|Prefer)(Park|Bazaar|LoadAt|Load)\s*\(\s*TEXT\("([^"]+)"')
# The inline wrappers inside namespace CigMesh call them without the prefix.
MESH_INLINE_RE = re.compile(r'\b(LoadAt|Load)\s*\(\s*TEXT\("([^"]+)"')
# A literal package path: /Game/Audio/S_Knead.S_Knead
GAME_PATH_RE = re.compile(r'TEXT\("(/Game/[^"]*)"')
ANY_TEXT_RE = re.compile(r'TEXT\("([^"]+)"\)')
COOK_DIR_RE = re.compile(r'^\s*\+DirectoriesToAlwaysCook=\(Path="([^"]+)"\)', re.M)


def pack_subfolders(base: Path) -> list[str]:
    """Folder names under a pack root, one and two levels down.

    Two levels because the CityPark trees live at Meshes/Flora/Trees; nothing
    in the project reaches deeper, and walking a 6 GB Megascans tree in full
    would cost more than it finds.
    """
    out: list[str] = []
    for first in base.iterdir() if base.is_dir() else []:
        if not first.is_dir():
            continue
        out.append(first.name)
        for second in first.iterdir():
            if second.is_dir():
                out.append(f"{first.name}/{second.name}")
    return out


def requested_asset_folders() -> dict[str, str]:
    """Every /Game folder the runtime can ask for, mapped to where it is asked.

    Both loaders build their paths with FString::Printf and hand the result to
    LoadObject, so nothing in the reference graph points at these assets and the
    cooker has no way to know they are wanted.

    Folder names are found two ways. Parsing the call sites is precise but only
    sees an argument written at the call: BuildBazaar keeps its produce in a
    static FBazaarGood[] table and passes G.Folder, and an earlier version of
    this check reported all clear while six stalls' worth of goods stayed
    uncooked. So the source is also searched for any string that happens to name
    a real subfolder of a pack the loaders use. That does not care how the
    string reaches the loader, and its failure direction is one folder cooked
    for nothing rather than a prop missing from the shipped game.
    """
    folders: dict[str, str] = {}
    literals: dict[str, str] = {}

    for path in sorted(list(SOURCE.rglob("*.cpp")) + list(SOURCE.rglob("*.h"))):
        if path.parent.name == "Tests":
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(ROOT).as_posix()

        def where(pos: int) -> str:
            return f"{rel}:{text.count(chr(10), 0, pos) + 1}"

        for m in GAME_PATH_RE.finditer(text):
            # Cut at the first format specifier: /Game/LowPoly/%s/%s.%s is only
            # informative as far as /Game/LowPoly.
            literal = m.group(1).split("%")[0]
            folder = literal.rsplit("/", 1)[0]
            if folder and folder != "/Game":
                folders.setdefault(folder, where(m.start()))

        matches = list(MESH_CALL_RE.finditer(text))
        if path.name == "CigMeshLibrary.h":
            matches += MESH_INLINE_RE.finditer(text)
        for m in matches:
            prefix = MESH_LOADERS[m.group(1)]
            arg = m.group(2)
            folder = f"/Game/{prefix}/{arg}" if prefix else f"/Game/{arg}"
            folders.setdefault(folder, where(m.start()))

        for m in ANY_TEXT_RE.finditer(text):
            literals.setdefault(m.group(1), where(m.start()))

    for prefix in sorted({p for p in MESH_LOADERS.values() if p}):
        base = ROOT / "Content" / prefix
        for sub in pack_subfolders(base):
            if sub in literals:
                folders.setdefault(f"/Game/{prefix}/{sub}", literals[sub])

    return folders


def check_cooked_assets() -> None:
    """Assets loaded by path must be named in DirectoriesToAlwaysCook.

    This is the quietest failure the project has: LoadObject returns nullptr,
    the caller falls back to a primitive or to silence, and the packaged game
    starts, logs no error and passes its smoke test. The first packaged build
    shipped with no audio whatsoever; the fix named /Game/Audio and /Game/LowPoly
    and missed the three mesh packs, so the next one shipped a shop rendered as
    grey boxes. Neither was visible without launching the build and looking.

    Two separate claims, and only the first one travels. That the code and the
    cook list agree is true of a checkout on any machine. That a folder exists on
    disk is only answerable where the packs are installed, and twelve of them are
    deliberately not in the repository - they carry uassets over GitHub's 100 MB
    limit (see .gitignore). The first version of this check asserted both
    unconditionally, passed locally and failed CI on all fourteen directories.
    Existence is now asserted per pack, only where the pack root is present.
    """
    ini = ROOT / "Config" / "DefaultGame.ini"
    if not ini.exists():
        fail("Config/DefaultGame.ini yok — cook listesi doğrulanamıyor.")
        return

    lib = SOURCE / "CigkofteSimulator" / "World" / "CigMeshLibrary.cpp"
    lib_text = lib.read_text(encoding="utf-8", errors="replace") if lib.exists() else ""
    for name, prefix in MESH_LOADERS.items():
        if prefix and prefix not in lib_text:
            fail(f"CigMeshLibrary.cpp içinde '{prefix}' yok — CigMesh::{name} "
                 f"başka bir klasöre bakıyor, bu betiğin eşlemesi eskimiş.")

    cook_dirs = COOK_DIR_RE.findall(ini.read_text(encoding="utf-8-sig"))
    if not cook_dirs:
        fail("DefaultGame.ini içinde hiç DirectoriesToAlwaysCook yok — "
             "yol ile yüklenen hiçbir varlık paketlenmez.")
        return

    content = ROOT / "Content"

    def pack_installed(game_path: str) -> bool:
        """Is the pack this /Game path belongs to checked out here at all."""
        rel = game_path[len("/Game/"):]
        return (content / rel.split("/", 1)[0]).is_dir()

    eksik_paket: set[str] = set()
    for d in cook_dirs:
        if not d.startswith("/Game/"):
            fail(f"DefaultGame.ini: '{d}' /Game/ ile başlamıyor.")
        elif not pack_installed(d):
            eksik_paket.add(d[len("/Game/"):].split("/", 1)[0])
        elif not (content / d[len("/Game/"):]).is_dir():
            fail(f"DefaultGame.ini: '{d}' diye bir klasör yok — bu satır "
                 f"hiçbir şey cook etmez.")

    folders = requested_asset_folders()
    for folder, where in sorted(folders.items()):
        # This half needs no assets on disk: it reads the code and the ini.
        if not any(folder == d or folder.startswith(d + "/") for d in cook_dirs):
            fail(f"{where}: '{folder}' cook listesinde değil — paketlenmiş "
                 f"oyunda bu varlıklar bulunamaz, oyun sessizce yedeğe düşer.")
        rel_dir = folder[len("/Game/"):]
        if not pack_installed(folder):
            eksik_paket.add(rel_dir.split("/", 1)[0])
        elif not (content / rel_dir).is_dir():
            fail(f"{where}: '{folder}' diye bir klasör yok — bu çağrı hiçbir "
                 f"zaman varlık bulamaz.")

    kapsam = f"  cook: {len(folders)} varlık klasörü {len(cook_dirs)} kuralla karşılandı"
    if eksik_paket:
        # Named rather than counted: on a machine that is supposed to have the
        # packs, this line is how you find out one of them is missing.
        kapsam += (f"; {len(eksik_paket)} paket bu kopyada yok, "
                   f"varlık kontrolü atlandı ({', '.join(sorted(eksik_paket))})")
    print(kapsam)


def check_repo_files() -> None:
    for name in ("LICENSE", "CREDITS.md", "README.md", ".gitattributes"):
        if not (ROOT / name).exists():
            fail(f"{name} yok.")
    print("  depo: zorunlu dosyalar yerinde")


def main() -> int:
    print("Cigkofte kaynak kontrolu")
    check_sources()
    check_balance()
    check_balance_keys()
    check_dialogue()
    check_text()
    check_decoupling()
    check_single_callers()
    check_cooked_assets()
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
