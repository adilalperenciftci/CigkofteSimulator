"""Cut the three ambience beds out of the licensed source recordings.

The beds in Content/Audio are not hand-edited; they are produced by this script
so anyone can rebuild them from the same downloads and get the same files. That
matters because the sources are large, licensed packs that cannot live in the
repository - without this, "where did S_AmbNight come from" has no answer.

Two things make a bed usable in game. It has to join back to its own start
without a click, which a tail crossfade gives: the material just past the loop
end is faded over the opening, so the last sample already leads into the first.
And it has to sit at a predictable level, because the mixer multiplies rather
than normalises.

The optional low-pass is how the night bed is derived from the same street the
day bed comes from. The licensed pack has no night city ambience, and deriving
one is better than substituting a different location: night is the same street
heard later, so it should be the same street. Distance and closed windows eat
treble, which is what the filter reproduces.

Sources (see CREDITS.md for the licence and the required attribution):
  Free City & Nature Sounds - Gregor Quendel, Fab, CC BY 4.0.
  Downloaded as city_nature_sounds_unity.unitypackage, which is a gzipped tar;
  each asset sits in a GUID directory next to a `pathname` file holding its
  original name.

Usage:
  python Tools/make_ambience.py <unitypackage> <output_dir>

Then import the three WAVs as looping USoundWaves at /Game/Audio. Looping is not
optional: TickAmbience spawns a bed once and never retriggers it, so a one-shot
import plays for forty-five seconds and then leaves the day silent.
"""

import math
import os
import struct
import subprocess
import sys
import tempfile
import wave

# Which recording each bed is cut from, where, and how long. The night window
# was chosen by scoring every 60 s window of the street recording for loudness
# and spikiness and taking the calmest; the others are representative sections.
BEDS = [
    dict(name="S_AmbStreet",
         source="WAV_City_Ambience_Traffic_Street_Cars_and_tram.wav",
         start=78.0, length=45.0, fade=3.0, lowpass=0.0, gain=1.0),
    dict(name="S_AmbNight",
         source="WAV_City_Ambience_Traffic_Street_Cars_and_tram.wav",
         start=190.0, length=45.0, fade=3.0, lowpass=900.0, gain=0.8),
    dict(name="S_AmbRain",
         source="WAV_Rain_Dropping_on_various_textures.wav",
         start=3.0, length=43.0, fade=3.0, lowpass=0.0, gain=1.0),
]


def unpack(unitypackage, workdir):
    """Extract the archive and map GUID directories back to real filenames."""
    subprocess.run(["tar", "-xzf", unitypackage, "-C", workdir], check=True)

    found = {}
    for entry in os.listdir(workdir):
        d = os.path.join(workdir, entry)
        pathname = os.path.join(d, "pathname")
        asset = os.path.join(d, "asset")
        if os.path.isfile(pathname) and os.path.isfile(asset):
            with open(pathname, "r", encoding="utf-8") as f:
                original = f.read().strip()
            found[os.path.basename(original)] = asset
    return found


def read(path):
    with wave.open(path, "rb") as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width != 2:
        raise SystemExit("expected 16-bit source, got %d-byte samples: %s" % (width, path))
    return list(struct.unpack("<%dh" % (len(raw) // 2), raw)), ch, rate


def lowpass(chans, rate, cutoff):
    # One pole per channel. Gentle on purpose: a steep filter on a wide bed
    # sounds like a blanket, this just moves it further away.
    a = 1.0 - math.exp(-2.0 * math.pi * cutoff / rate)
    out = []
    for c in chans:
        y = 0.0
        acc = []
        for x in c:
            y += a * (x - y)
            acc.append(y)
        out.append(acc)
    return out


def make_loop(src, dst, start_s, len_s, fade_s, cutoff, gain):
    flat, ch, rate = read(src)
    frames = len(flat) // ch
    start, length, fade = int(start_s * rate), int(len_s * rate), int(fade_s * rate)
    if start + length + fade > frames:
        raise SystemExit("%s is only %.1f s; cannot take %.1f s from %.1f s"
                         % (src, frames / rate, len_s + fade_s, start_s))

    chans = [[float(flat[(start + i) * ch + c]) for i in range(length + fade)]
             for c in range(ch)]

    if cutoff > 0:
        chans = lowpass(chans, rate, cutoff)

    # Equal-power crossfade: the tail past the loop point fades down over the
    # opening as the opening fades up, so energy stays flat through the seam.
    for c in chans:
        for i in range(fade):
            t = i / float(fade)
            c[i] = c[i] * math.sqrt(t) + c[length + i] * math.sqrt(1.0 - t)
        del c[length:]

    # Peak-normalise to a fixed headroom, then apply the trim, so the three beds
    # reach the mixer at comparable levels.
    peak = max(max(abs(v) for v in c) for c in chans) or 1.0
    scale = (0.89 * 32767.0 / peak) * gain

    out = []
    for i in range(length):
        for c in chans:
            out.append(max(-32768, min(32767, int(c[i] * scale))))

    with wave.open(dst, "wb") as w:
        w.setnchannels(ch)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(struct.pack("<%dh" % len(out), *out))

    return length / rate, ch, rate


def seam_check(path):
    """A click is a step the waveform never takes anywhere else in the file.

    So the test is comparative: measure the jump from the last sample back to
    the first, and compare it against ordinary sample-to-sample motion.
    """
    with wave.open(path, "rb") as w:
        ch, n = w.getnchannels(), w.getnframes()
        raw = w.readframes(n)
    v = struct.unpack("<%dh" % (len(raw) // 2), raw)

    deltas = sorted(abs(v[i] - v[i - ch]) for i in range(ch, len(v), ch * 37))
    worst = deltas[int(len(deltas) * 0.999)]
    seam = max(abs(v[c] - v[len(v) - ch + c]) for c in range(ch))
    return seam, worst


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__.strip().splitlines()[-4])

    unitypackage, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)

    with tempfile.TemporaryDirectory() as workdir:
        sources = unpack(unitypackage, workdir)

        failed = False
        for bed in BEDS:
            src = sources.get(bed["source"])
            if not src:
                print("MISSING source %s" % bed["source"])
                failed = True
                continue

            dst = os.path.join(outdir, bed["name"] + ".wav")
            dur, ch, rate = make_loop(src, dst, bed["start"], bed["length"],
                                      bed["fade"], bed["lowpass"], bed["gain"])
            seam, worst = seam_check(dst)
            ok = seam <= worst
            if not ok:
                failed = True
            print("%-12s %5.1f s  %d ch  %d Hz  seam %5d vs p99.9 %6d  %s"
                  % (bed["name"], dur, ch, rate, seam, worst, "OK" if ok else "CLICK"))

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
