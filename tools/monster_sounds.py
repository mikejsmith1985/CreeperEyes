"""Synthesise the Frankenstein monster sounds for the DFPlayer.
Usage: python3 tools/monster_sounds.py sounds/mp3"""
import numpy as np, subprocess, os, sys
from scipy import signal
from scipy.io import wavfile

SR = 44100
OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(7)

def curve(points, n):
    """points: list of (t0..1, value) -> smooth curve of n samples"""
    t = np.linspace(0, 1, n)
    xs, ys = zip(*points)
    return np.interp(t, xs, ys)

def glottal(f0):
    """band-limited-ish sawtooth pulse train with jitter, from an f0 curve"""
    jit = 1 + 0.02 * signal.lfilter([1], [1, -0.995], rng.standard_normal(len(f0))) * 0.1
    ph = np.cumsum(f0 * jit / SR)
    saw = 2 * (ph % 1) - 1
    sub = np.sin(2 * np.pi * ph * 0.5)          # subharmonic = monstrous
    src = saw + 0.6 * sub
    return signal.lfilter([1], [1, -0.7], src)  # glottal roll-off

def formants(x, fs, bws, gains):
    y = np.zeros_like(x)
    for f, bw, g in zip(fs, bws, gains):
        b, a = signal.iirpeak(f, f / bw, SR)
        y += g * signal.lfilter(b, a, x)
    return y

def voice(dur, f0pts, ampts, vowel, rough=0.0, rough_hz=28, breath=0.15):
    n = int(dur * SR)
    f0 = curve(f0pts, n)
    amp = curve(ampts, n)
    src = glottal(f0)
    if rough:
        t = np.arange(n) / SR
        src *= 1 - rough * (0.5 + 0.5 * np.sin(2 * np.pi * rough_hz * t + rng.uniform(0, 6)))
    noise = rng.standard_normal(n) * breath
    v = formants(src + noise, *vowel)
    v = signal.lfilter(*signal.butter(2, [45, 4000], 'band', fs=SR), v)
    return v * amp

OH  = ([380, 750, 2400], [6, 8, 10], [1.0, 0.6, 0.15])
UH  = ([550, 1000, 2400], [6, 8, 10], [1.0, 0.5, 0.15])
AH  = ([700, 1150, 2500], [6, 8, 10], [1.0, 0.55, 0.2])
MM  = ([250, 1000, 2300], [4, 8, 12], [1.0, 0.08, 0.02])

def save(name, x):
    x = np.tanh(1.8 * x / (np.max(np.abs(x)) + 1e-9))   # gentle saturation
    fade = int(0.03 * SR)
    x[:fade] *= np.linspace(0, 1, fade); x[-fade:] *= np.linspace(1, 0, fade)
    x = 0.89 * x / np.max(np.abs(x))
    pad = np.zeros(int(0.05 * SR))
    x = np.concatenate([pad, x, pad])
    wav = os.path.join(OUT, name + '.wav')
    wavfile.write(wav, SR, (x * 32767).astype(np.int16))
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', wav, '-ac', '1', '-ar', '44100',
                    '-b:a', '128k', '-id3v2_version', '0', os.path.join(OUT, name + '.mp3')], check=True)
    os.remove(wav)

# 0001 wake groan: slow rising then falling "uuh-OHHH"
save('0001', voice(2.8, [(0, 62), (0.35, 88), (0.7, 80), (1, 52)],
                   [(0, 0), (0.15, 0.7), (0.45, 1), (0.8, 0.8), (1, 0)], OH, rough=0.25, rough_hz=22))
# 0002-0005 growls: shorter, rougher, varied
save('0002', voice(1.5, [(0, 75), (0.3, 68), (1, 55)], [(0, 0), (0.1, 1), (0.7, 0.8), (1, 0)], UH, rough=0.6, rough_hz=30))
save('0003', voice(1.9, [(0, 58), (0.5, 72), (1, 50)], [(0, 0), (0.25, 0.9), (0.6, 1), (1, 0)], AH, rough=0.55, rough_hz=26))
save('0004', voice(1.2, [(0, 85), (0.4, 70), (1, 60)], [(0, 0), (0.08, 1), (0.5, 0.9), (1, 0)], OH, rough=0.7, rough_hz=34))
# 0005: two-part "hrr... HRRR"
a = voice(0.7, [(0, 60), (1, 58)], [(0, 0), (0.3, 0.6), (1, 0)], UH, rough=0.6, rough_hz=25)
b = voice(1.4, [(0, 70), (0.4, 82), (1, 52)], [(0, 0), (0.15, 1), (0.7, 0.85), (1, 0)], AH, rough=0.6, rough_hz=29)
save('0005', np.concatenate([a, np.zeros(int(0.18 * SR)), b]))
# 0006 "Mmm..." pleased hum, closed mouth, slight rise then fall
save('0006', voice(1.9, [(0, 92), (0.3, 104), (0.75, 98), (1, 82)],
                   [(0, 0), (0.15, 1), (0.75, 0.9), (1, 0)], MM, rough=0.1, rough_hz=18, breath=0.05))
# 0007 jump-scare ROAR: loud, rough, rising then crashing down
r = voice(2.2, [(0, 85), (0.25, 150), (0.6, 120), (1, 60)], [(0, 0), (0.05, 1), (0.6, 0.95), (1, 0)], AH,
          rough=0.85, rough_hz=38, breath=0.5)
save('0007', r)
# 0008 SNORE: noisy rumbling inhale, pause, airy exhale
n1 = int(1.4 * SR)
inh = voice(1.4, [(0, 42), (1, 48)], [(0, 0), (0.3, 0.8), (0.85, 1), (1, 0)], UH, rough=0.9, rough_hz=24, breath=0.9)
exh = rng.standard_normal(int(1.2 * SR)) * curve([(0, 0), (0.15, 0.5), (1, 0)], int(1.2 * SR))
exh = signal.lfilter(*signal.butter(2, [300, 1800], 'band', fs=SR), exh) * 0.6
save('0008', np.concatenate([inh, np.zeros(int(0.35 * SR)), exh]))
# 0009 friendly "hmm?" with a rising, curious pitch
save('0009', voice(1.3, [(0, 95), (0.6, 105), (1, 150)], [(0, 0), (0.15, 1), (0.8, 0.9), (1, 0)], MM,
                   rough=0.05, rough_hz=18, breath=0.04))
# 0010 chuckle: "heh-heh-heh" falling
parts = []
for i, f0 in enumerate([118, 108, 98, 90]):
    parts.append(voice(0.22, [(0, f0 + 10), (1, f0)], [(0, 0), (0.2, 1), (1, 0)], AH, rough=0.3, rough_hz=30, breath=0.35))
    parts.append(np.zeros(int(0.09 * SR)))
save('0010', np.concatenate(parts))
print(sorted(os.listdir(OUT)))
