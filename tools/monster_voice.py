#!/usr/bin/env python3
"""
Original monster voice synthesiser for CreeperEyes (v2): a big, slow, breathy human-ish voice.
Glottal pulses with jitter/shimmer and vocal fry, a time-varying formant vocal tract scaled for a
giant, breath noise, a little rasp and a short room reverb.

Usage: python3 tools/monster_voice.py sounds/mp3     (writes 0001.mp3 .. 0010.mp3)
"""
import os, subprocess, sys
import numpy as np
from scipy import signal
from scipy.io import wavfile

SR = 44100
OUT = sys.argv[1] if len(sys.argv) > 1 else 'sounds/mp3'
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(1931)
GIANT = 0.80            # formant scale: < 1 = longer vocal tract = bigger creature

VOWELS = {              # F1, F2, F3 (adult male), bandwidths added below
    'AA': (730, 1090, 2440), 'UH': (640, 1190, 2390), 'OW': (570, 840, 2410), 'OO': (300, 870, 2240),
    'ER': (490, 1350, 1690), 'EH': (530, 1840, 2480), 'MM': (260, 1000, 2200), 'HH': (600, 1300, 2500),
}

def env(points, n):
    """points: [(t 0..1, value)] -> n samples, smooth (cosine) interpolation"""
    t = np.linspace(0, 1, n)
    xs = np.array([p[0] for p in points]); ys = np.array([p[1] for p in points])
    out = np.empty(n)
    for i in range(len(xs) - 1):
        m = (t >= xs[i]) & (t <= xs[i + 1])
        u = (t[m] - xs[i]) / max(1e-9, xs[i + 1] - xs[i])
        out[m] = ys[i] + (ys[i + 1] - ys[i]) * (1 - np.cos(np.pi * u)) / 2
    return out

def glottal(f0, fry=None, jitter=0.012, shimmer=0.08):
    """Rosenberg-style pulse train following f0 (Hz per sample). fry: 0..1 per sample (irregular low pulses)."""
    n = len(f0)
    drift = signal.lfilter([1], [1, -0.999], rng.standard_normal(n)) * 0.002
    f = f0 * (1 + jitter * rng.standard_normal(n) * 0.3 + drift)
    if fry is not None:
        f = f * (1 - 0.55 * fry)                                   # fry drops the pitch
    phase = np.cumsum(f / SR)
    ph = phase % 1.0
    open_q = 0.55
    pulse = np.where(ph < open_q * 0.66, 0.5 * (1 - np.cos(np.pi * ph / (open_q * 0.66))),
             np.where(ph < open_q, np.cos(np.pi / 2 * (ph - open_q * 0.66) / (open_q * 0.34)), 0.0))
    # per-cycle amplitude (shimmer), extra irregular for fry
    cyc = np.floor(phase).astype(int)
    amps = 1 + shimmer * rng.standard_normal(cyc.max() + 2)
    if fry is not None:
        amps2 = 1 + 0.5 * rng.standard_normal(cyc.max() + 2)
        a = amps[cyc] * (1 - fry) + np.abs(amps2[cyc]) * fry
    else:
        a = amps[cyc]
    src = np.diff(pulse * a, prepend=0)                            # glottal flow derivative
    return src, ph

def tract(x, vowel_track, n, nasal=0.0):
    """Time-varying formant filter. vowel_track: [(t, 'AA'), ...] interpolated in formant space."""
    blk = 256
    ts = np.array([v[0] for v in vowel_track])
    fs = np.array([VOWELS[v[1]] for v in vowel_track], dtype=float) * GIANT
    bws = np.array([90, 110, 160]) * 1.1
    y = np.zeros(n)
    zi = [np.zeros(2) for _ in range(3)]
    gains = [1.0, 0.55, 0.25]
    for s in range(0, n, blk):
        e = min(n, s + blk); t = s / n
        F = np.array([np.interp(t, ts, fs[:, k]) for k in range(3)])
        seg = np.zeros(e - s)
        for k in range(3):
            r = np.exp(-np.pi * bws[k] / SR); th = 2 * np.pi * F[k] / SR
            a = [1, -2 * r * np.cos(th), r * r]; b = [1 - r]
            o, zi[k] = signal.lfilter(b, a, x[s:e], zi=zi[k])
            seg += gains[k] * o
        y[s:e] = seg
    if nasal:                                                      # nasal hum: damp highs, add 250 Hz murmur
        lp = signal.lfilter(*signal.butter(2, 900 * GIANT / (SR / 2)), y)
        y = y * (1 - nasal) + lp * nasal * 1.6
    return y

def voice(dur, f0pts, ampts, vowels, rasp=0.15, rasp_hz=32, breath=0.12, fry=None, nasal=0.0):
    n = int(dur * SR)
    f0 = env(f0pts, n)
    fr = env(fry, n) if fry else None
    src, ph = glottal(f0, fr)
    # breath: noise, louder during the open phase of each pulse
    noise = signal.lfilter(*signal.butter(2, [400 / (SR / 2), 6000 / (SR / 2)], 'band'), rng.standard_normal(n))
    src = src * 60 + noise * breath * (0.4 + 0.6 * (ph < 0.55))
    if rasp:                                                       # false-fold rasp: slow amplitude flutter
        lfo = 0.5 + 0.5 * np.sin(2 * np.pi * rasp_hz * np.arange(n) / SR + rng.uniform(0, 6))
        src = src * (1 - rasp * lfo) + rasp * 0.6 * np.abs(src) * np.sign(rng.standard_normal(n)) * lfo * 0.3
    y = tract(src, vowels, n, nasal)
    y = np.diff(y, prepend=0) * 0.6 + y * 0.4                      # mouth radiation (partial)
    return y * env(ampts, n)

def breath_noise(dur, amp_pts, lo=300, hi=2500):
    n = int(dur * SR)
    x = signal.lfilter(*signal.butter(2, [lo / (SR / 2), hi / (SR / 2)], 'band'), rng.standard_normal(n))
    return x * env(amp_pts, n)

_ir = None
def reverb(x, mix=0.16):
    global _ir
    if _ir is None:                                                # small stone room
        n = int(0.55 * SR); t = np.arange(n) / SR
        _ir = rng.standard_normal(n) * np.exp(-t / 0.13)
        _ir = signal.lfilter(*signal.butter(2, 3500 / (SR / 2)), _ir); _ir /= np.abs(_ir).sum() ** 0.5 * 8
    wet = signal.fftconvolve(x, _ir)[:len(x) + len(_ir)]
    out = np.zeros(len(wet)); out[:len(x)] = x
    return out * (1 - mix) + wet / (np.abs(wet).max() + 1e-9) * np.abs(x).max() * mix

def save(name, x, loud=0.2):
    x = signal.lfilter(*signal.butter(2, 55 / (SR / 2), 'high'), x)  # small speaker can't do sub-bass
    x = reverb(x)
    x = np.tanh(1.6 * x / (np.abs(x).max() + 1e-9))
    f = int(0.02 * SR); x[:f] *= np.linspace(0, 1, f); x[-f:] *= np.linspace(1, 0, f)
    x = x * min(0.92 / np.abs(x).max(), loud / np.sqrt((x ** 2).mean()))   # even loudness
    x = np.clip(x, -0.95, 0.95)
    x = np.concatenate([np.zeros(int(0.04 * SR)), x])
    wav = os.path.join(OUT, name + '.wav')
    wavfile.write(wav, SR, (x * 32767).astype(np.int16))
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', wav, '-ac', '1', '-ar', '44100', '-b:a', '128k',
                    '-id3v2_version', '0', os.path.join(OUT, name + '.mp3')], check=True)
    os.remove(wav)
    print(name, '%.1fs' % (len(x) / SR))

def cat(*parts):
    return np.concatenate([p if isinstance(p, np.ndarray) else np.zeros(int(p * SR)) for p in parts])

# 0001 wake-up groan: creaky start, swells into a long "uhhh-OHHHH", sinks back down
save('0001', cat(
    breath_noise(0.35, [(0, 0), (0.5, 0.25), (1, 0.1)]),
    voice(3.0, [(0, 58), (0.3, 82), (0.55, 92), (0.8, 74), (1, 52)], [(0, 0), (0.12, 0.7), (0.45, 1), (0.8, 0.85), (1, 0)],
          [(0, 'UH'), (0.35, 'UH'), (0.6, 'OW'), (1, 'OO')], rasp=0.2, breath=0.14,
          fry=[(0, 1), (0.15, 0.2), (0.85, 0.3), (1, 0.9)])))

# 0002 growl: low, rattling "grrrr"
save('0002', voice(1.8, [(0, 70), (0.3, 64), (1, 52)], [(0, 0), (0.08, 1), (0.7, 0.8), (1, 0)],
                   [(0, 'ER'), (1, 'UH')], rasp=0.75, rasp_hz=27, breath=0.25, fry=[(0, 0.6), (1, 0.8)]))

# 0003 growl: breathy "hhhrraaah"
save('0003', cat(breath_noise(0.3, [(0, 0), (1, 0.45)], 500, 3500),
                 voice(2.0, [(0, 62), (0.4, 88), (1, 55)], [(0, 0.5), (0.3, 1), (0.75, 0.85), (1, 0)],
                       [(0, 'HH'), (0.25, 'AA'), (1, 'UH')], rasp=0.6, rasp_hz=31, breath=0.3,
                       fry=[(0, 0.3), (0.5, 0.2), (1, 0.8)])))

# 0004 growl: short grunt then rumble "huh-RRRR"
save('0004', cat(voice(0.28, [(0, 95), (1, 80)], [(0, 0), (0.2, 1), (1, 0)], [(0, 'UH'), (1, 'UH')], rasp=0.2, breath=0.35),
                 0.12,
                 voice(1.2, [(0, 68), (1, 50)], [(0, 0), (0.1, 1), (0.6, 0.85), (1, 0)], [(0, 'ER'), (1, 'ER')],
                       rasp=0.8, rasp_hz=25, breath=0.2, fry=[(0, 0.5), (1, 0.9)])))

# 0005 mournful groan: long, low, falling "ohhhh-uhhh"
save('0005', voice(2.8, [(0, 80), (0.25, 86), (0.7, 66), (1, 48)], [(0, 0), (0.15, 0.9), (0.55, 1), (0.85, 0.6), (1, 0)],
                   [(0, 'OW'), (0.55, 'OW'), (1, 'UH')], rasp=0.18, breath=0.16, fry=[(0, 0.4), (0.3, 0.1), (1, 1)]))

# 0006 pleased "Mmmm" (closed mouth hum, rises then settles)
save('0006', loud=0.17, x=voice(2.1, [(0, 88), (0.3, 104), (0.7, 98), (1, 80)], [(0, 0), (0.15, 1), (0.75, 0.9), (1, 0)],
                   [(0, 'MM'), (1, 'MM')], rasp=0.06, breath=0.05, nasal=0.85, fry=[(0, 0.2), (0.6, 0), (1, 0.7)]))

# 0007 roar: loud, raw, rising then crashing
save('0007', loud=0.26, x=cat(breath_noise(0.18, [(0, 0), (1, 0.6)], 600, 4000),
                 voice(2.3, [(0, 95), (0.2, 165), (0.55, 140), (1, 70)], [(0, 0.6), (0.1, 1), (0.65, 0.95), (1, 0)],
                       [(0, 'AA'), (0.6, 'AA'), (1, 'OW')], rasp=0.9, rasp_hz=41, breath=0.55, fry=[(0, 0), (0.8, 0.2), (1, 0.8)])))

# 0008 snore: rattling inhale, pause, long breathy exhale with a little lip flutter
save('0008', loud=0.12, x=cat(
    voice(1.5, [(0, 40), (1, 46)], [(0, 0), (0.35, 0.8), (0.85, 1), (1, 0)], [(0, 'UH'), (1, 'OO')],
          rasp=0.95, rasp_hz=26, breath=0.9, fry=[(0, 1), (1, 1)]),
    0.4,
    breath_noise(1.4, [(0, 0), (0.15, 0.55), (0.6, 0.3), (1, 0)], 250, 1600)))

# 0009 friendly, curious "hmm?" (rising)
save('0009', loud=0.17, x=voice(1.4, [(0, 92), (0.55, 100), (1, 150)], [(0, 0), (0.15, 1), (0.8, 0.9), (1, 0)],
                   [(0, 'MM'), (1, 'MM')], rasp=0.04, breath=0.04, nasal=0.8, fry=[(0, 0.3), (0.3, 0), (1, 0)]))

# 0010 deep chuckle "heh heh heh heh"
parts = []
for f0 in (112, 104, 96, 88):
    parts += [breath_noise(0.05, [(0, 0), (1, 0.5)], 800, 4000),
              voice(0.2, [(0, f0 + 8), (1, f0)], [(0, 0), (0.2, 1), (1, 0)], [(0, 'EH'), (1, 'UH')],
                    rasp=0.3, breath=0.35, fry=[(0, 0.2), (1, 0.5)]), 0.08]
save('0010', cat(*parts))
