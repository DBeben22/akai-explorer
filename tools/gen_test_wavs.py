#!/usr/bin/env python3
"""Synthesize the reference WAV files used for the AKAI test disk images.

All sounds are generated mathematically (no third-party recordings), so the
resulting WAVs and disk images can be redistributed freely with the project.

Usage: gen_test_wavs.py <output-dir>
"""
import math
import os
import random
import struct
import sys
import wave


def write_wav(path, rate, channels):
    """channels: list of equally long float lists in the range -1..1"""
    n = len(channels[0])
    frames = bytearray()
    for i in range(n):
        for ch in channels:
            v = max(-1.0, min(1.0, ch[i]))
            frames += struct.pack("<h", int(round(v * 32767.0)))
    with wave.open(path, "wb") as w:
        w.setnchannels(len(channels))
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(frames))


def env_exp(n, rate, decay):
    return [math.exp(-i / rate / decay) for i in range(n)]


def sine(rate, secs, freq, amp=0.8):
    n = int(rate * secs)
    return [amp * math.sin(2 * math.pi * freq * i / rate) for i in range(n)]


def sweep(rate, secs, f0, f1, amp=0.8):
    n = int(rate * secs)
    out, phase = [], 0.0
    for i in range(n):
        f = f0 * (f1 / f0) ** (i / n)
        phase += 2 * math.pi * f / rate
        out.append(amp * math.sin(phase))
    return out


def kick(rate, secs=0.45):
    n = int(rate * secs)
    out, phase = [], 0.0
    for i in range(n):
        t = i / rate
        f = 45 + 140 * math.exp(-t / 0.035)
        phase += 2 * math.pi * f / rate
        out.append(0.95 * math.sin(phase) * math.exp(-t / 0.16))
    return out


def snare(rate, rnd, secs=0.35):
    n = int(rate * secs)
    out = []
    for i in range(n):
        t = i / rate
        tone = 0.45 * math.sin(2 * math.pi * 190 * t) * math.exp(-t / 0.06)
        noise = 0.7 * (rnd.random() * 2 - 1) * math.exp(-t / 0.09)
        out.append(tone + noise)
    return out


def hihat(rate, rnd, secs=0.12):
    n = int(rate * secs)
    out, prev = [], 0.0
    for i in range(n):
        t = i / rate
        x = rnd.random() * 2 - 1
        hp = x - prev          # crude high-pass
        prev = x
        out.append(0.6 * hp * math.exp(-t / 0.03))
    return out


def pluck(rate, freq, rnd, secs=1.2):
    """Karplus-Strong string"""
    n = int(rate * secs)
    period = max(2, int(rate / freq))
    buf = [rnd.random() * 2 - 1 for _ in range(period)]
    out = []
    for i in range(n):
        v = buf[i % period]
        nxt = buf[(i + 1) % period]
        buf[i % period] = 0.996 * 0.5 * (v + nxt)
        out.append(0.9 * v)
    return out


def pad(rate, freq, secs=2.0, detune=0.0, phase0=0.0):
    n = int(rate * secs)
    out = []
    for i in range(n):
        t = i / rate
        a = min(1.0, t / 0.4) * min(1.0, (secs - t) / 0.5)
        v = 0.0
        for k, g in ((1, 0.5), (2, 0.22), (3, 0.12), (5, 0.05)):
            v += g * math.sin(2 * math.pi * freq * k * (1 + detune * k) * t + phase0 * k)
        out.append(a * v)
    return out


def bass(rate, freq, secs=0.8):
    n = int(rate * secs)
    out = []
    for i in range(n):
        t = i / rate
        ph = (freq * t) % 1.0
        saw = 2 * ph - 1
        sub = math.sin(2 * math.pi * freq * t)
        out.append((0.45 * saw + 0.45 * sub) * math.exp(-t / 0.35))
    return out


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    rnd = random.Random(1986)  # fixed seed: reproducible test data

    def p(name):
        return os.path.join(out, name)

    # --- mono, various sample rates -------------------------------------
    write_wav(p("SINE440.wav"), 44100, [sine(44100, 1.0, 440.0)])
    write_wav(p("SINE1K.wav"), 22050, [sine(22050, 0.5, 1000.0)])
    write_wav(p("SWEEP.wav"), 44100, [sweep(44100, 1.5, 40.0, 12000.0)])
    write_wav(p("KICK.wav"), 44100, [kick(44100)])
    write_wav(p("SNARE.wav"), 44100, [snare(44100, rnd)])
    write_wav(p("HIHAT.wav"), 44100, [hihat(44100, rnd)])
    write_wav(p("PLUCK_C3.wav"), 32000, [pluck(32000, 130.81, rnd)])
    write_wav(p("BASS_E1.wav"), 22050, [bass(22050, 41.2)])
    # short low-rate material, typical for 12-bit S900 disks
    write_wav(p("KICK9.wav"), 20000, [kick(20000, 0.35)])
    write_wav(p("SNARE9.wav"), 20000, [snare(20000, rnd, 0.3)])
    write_wav(p("BASS9.wav"), 15000, [bass(15000, 55.0, 0.6)])
    write_wav(p("SINE9.wav"), 30000, [sine(30000, 0.5, 440.0)])
    # --- stereo (imported by akaiutil as "-L" / "-R" sample pair) --------
    left = pad(44100, 220.0, 2.0, detune=0.000)
    right = pad(44100, 220.0, 2.0, detune=0.004, phase0=0.7)
    write_wav(p("PAD_A3.wav"), 44100, [left, right])
    sl = sweep(22050, 1.0, 200.0, 2000.0, 0.7)
    sr = sweep(22050, 1.0, 2000.0, 200.0, 0.7)
    write_wav(p("XSWEEP.wav"), 22050, [sl, sr])
    # --- edge cases -----------------------------------------------------
    write_wav(p("CLICK.wav"), 44100, [[0.0] * 10 + [1.0] + [0.0] * 10])  # 21 samples, odd length
    write_wav(p("FULLSCALE.wav"), 44100,
              [[(-1.0 if (i // 50) % 2 else 1.0) for i in range(4410)]])


if __name__ == "__main__":
    main()
