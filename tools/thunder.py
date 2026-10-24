#!/usr/bin/env python3
"""Synthesizes rolling thunder for the storm backdrop -> sounds/mp3/0011.mp3 .. 0013.mp3
Close crack + long low rumble, all from filtered noise (no samples)."""
import os, sys, subprocess, wave
import numpy as np
from scipy.signal import butter, sosfilt
OUT = sys.argv[1] if len(sys.argv) > 1 else 'sounds/mp3'
SR = 44100
def lp(x, f, o=4): return sosfilt(butter(o, f, 'low', fs=SR, output='sos'), x)
def bp(x, a, b, o=3): return sosfilt(butter(o, [a, b], 'band', fs=SR, output='sos'), x)
def thunder(seed, dur, crack, distance):
    r = np.random.default_rng(seed); n = int(SR * dur); t = np.arange(n) / SR
    noise = r.standard_normal(n)
    brown = np.cumsum(r.standard_normal(n)); brown -= lp(brown, 8); brown /= np.abs(brown).max()
    # rumble: several overlapping rolls with random swells
    env = np.zeros(n)
    for _ in range(int(8 + 10 * dur / 6)):
        c = r.uniform(0.15, 0.85) * dur; w = r.uniform(0.25, 1.4); a = r.uniform(0.3, 1.0)
        env += a * np.exp(-((t - c) / w) ** 2)
    env *= np.clip(t / 0.35, 0, 1) * np.exp(-t / (dur * 0.55))
    env /= env.max()
    rumble = lp(brown, 160 - 60 * distance) * 1.2 + lp(noise, 90) * 0.6
    rumble = rumble * env
    # crack: sharp broadband burst with a ripping tail
    ck = np.zeros(n)
    if crack:
        k = int(SR * 0.9)
        rip = bp(noise[:k], 300, 4000) * np.exp(-np.arange(k) / (SR * 0.12)) * (1 + 0.6 * np.sin(np.arange(k) / SR * 2 * np.pi * 23))
        ck[:k] = rip * crack
        ck[:k] += lp(noise[:k], 1200) * np.exp(-np.arange(k) / (SR * 0.25)) * crack * 0.8
    x = rumble / np.abs(rumble).max() + ck
    x = np.tanh(x * 1.6)                                   # a touch of saturation, like a big speaker
    x[:200] *= np.linspace(0, 1, 200); x[-SR // 2:] *= np.linspace(1, 0, SR // 2)
    return (x / np.abs(x).max() * 0.98)
os.makedirs(OUT, exist_ok=True)
for name, args in (('0011', (1, 6.5, 1.0, 0.0)), ('0012', (2, 7.5, 0.0, 0.6)), ('0013', (3, 5.5, 0.7, 0.3))):
    y = thunder(*args)
    wav = os.path.join(OUT, name + '.wav')
    with wave.open(wav, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((y * 32767).astype(np.int16).tobytes())
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', wav, '-ac', '1', '-ar', '44100', '-b:a', '128k',
                    '-id3v2_version', '0', os.path.join(OUT, name + '.mp3')], check=True)
    os.remove(wav); print(name, round(len(y) / SR, 1), 's')
