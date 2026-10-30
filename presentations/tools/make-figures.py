"""由實際遙測 CSV 與韌體參數產生簡報圖表（presentations/assets/figures/）。

    python presentations/tools/make-figures.py

資料：docs/data/run-*.csv（原始遙測 CSV 未修改的副本）。
頻譜做法與展示網站 viewer.js 相同：依序號重建 6 ms 等間隔序列（重複序號去除、缺口線性內插），
去平均、加 Hann 窗後做 FFT，於 2–10 Hz 取峰值。
"""
from pathlib import Path
import csv
import math

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "docs" / "data"
OUT = ROOT / "presentations" / "assets" / "figures"
OUT.mkdir(parents=True, exist_ok=True)

DT = 0.006  # FLIGHT_TASK_PERIOD_MS = 6
BG, SURFACE, INK, MUTED, GRID = "#0B1220", "#111A2B", "#E6EDF6", "#9AA8BA", "#26344A"
BLUE, AMBER, TEAL, ROSE = "#4EA1FF", "#F5A524", "#2DD4BF", "#F472B6"

plt.rcParams.update({
    "font.family": ["Microsoft JhengHei", "Noto Sans TC", "sans-serif"],
    "axes.unicode_minus": False,  # 中文字型缺 U+2212
    "figure.facecolor": BG, "axes.facecolor": BG, "savefig.facecolor": BG,
    "axes.edgecolor": GRID, "axes.labelcolor": MUTED, "text.color": INK,
    "xtick.color": MUTED, "ytick.color": MUTED, "grid.color": GRID,
    "axes.grid": True, "grid.linewidth": 0.8, "font.size": 15,
    "axes.spines.top": False, "axes.spines.right": False, "lines.linewidth": 2.0,
    "legend.frameon": False, "legend.fontsize": 14,
})


def load(name):
    with open(DATA / name, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    seen, out = set(), []
    for r in rows:
        s = int(r["sequence"])
        if s in seen:
            continue
        seen.add(s)
        out.append(r)
    out.sort(key=lambda r: int(r["sequence"]))
    return out


def uniform(rows, col):
    """依序號重建 6 ms 等間隔序列，缺口線性內插。"""
    seq = np.array([int(r["sequence"]) for r in rows], dtype=float)
    val = np.array([float(r[col]) for r in rows])
    grid = np.arange(seq[0], seq[-1] + 1)
    return (grid - seq[0]) * DT, np.interp(grid, seq, val)


def spectrum(rows, col="gyro_x_deg_s"):
    _, x = uniform(rows, col)
    x = x - x.mean()
    w = np.hanning(len(x))
    n = 1 << (len(x) - 1).bit_length()  # 與 viewer.js 相同：補零至 2 的次方
    mag = np.abs(np.fft.rfft(x * w, n)) * 2 / w.sum()
    f = np.fft.rfftfreq(n, DT)
    band = (f >= 2) & (f <= 10)
    peak = f[band][np.argmax(mag[band])]
    return f, mag, peak


# ---------- 1. 實測：p_2.3 在 11.8 s 擾動後的衰減振盪 ----------
rows = load("run-p-2.3.csv")
t = np.array([float(r["flight_elapsed_s"]) for r in rows])
sel = (t >= 10.5) & (t <= 15.5)
fig, (a1, a2) = plt.subplots(2, 1, figsize=(12, 6.2), sharex=True, gridspec_kw={"height_ratios": [1, 1.4]})
a1.plot(t[sel], [float(r["roll_angle_deg"]) for r, s in zip(rows, sel) if s], color=TEAL)
a1.set_ylabel("Roll 角 (°)")
a2.plot(t[sel], [float(r["gyro_x_target_deg_s"]) for r, s in zip(rows, sel) if s], color=BLUE, label="目標角速度（外環輸出）")
a2.plot(t[sel], [float(r["gyro_x_deg_s"]) for r, s in zip(rows, sel) if s], color=AMBER, label="量測角速度（濾波後）")
a2.set_ylabel("角速度 (°/s)")
a2.set_xlabel("飛控時間 flight_elapsed_s (s)")
a2.legend(loc="upper right", ncol=2)
fig.tight_layout()
fig.savefig(OUT / "event-p2.3.png", dpi=160)
plt.close(fig)

# ---------- 2. 實測：三份記錄的角速度頻譜 ----------
runs = [("run-p-2.3.csv", "telemetry_p_2.3", BLUE), ("run-d-0.13.csv", "telemetry_D_0.13", TEAL),
        ("run-new.csv", "telemetry_new", AMBER)]
fig, ax = plt.subplots(figsize=(12, 5.6))
peaks = {}
for name, label, color in runs:
    f, mag, peak = spectrum(load(name))
    peaks[label] = peak
    m = (f >= 0.5) & (f <= 15)
    ax.plot(f[m], mag[m] / mag[(f >= 2) & (f <= 10)].max(), color=color, label=f"{label}　峰值 {peak:.2f} Hz")
fc = 1 / (2 * math.pi * DT) * -math.log(0.85)  # y[k]=0.15x+0.85y[k-1] 的 -3 dB 近似
ax.axvline(fc, color=ROSE, linestyle="--", linewidth=1.6)
ax.annotate(f"陀螺低通截止 ≈ {fc:.1f} Hz（推算）", xy=(fc, 0.5), xytext=(6.4, 0.5), color=ROSE, fontsize=14,
            va="center", arrowprops={"arrowstyle": "->", "color": ROSE})
ax.axvspan(2, 10, color=SURFACE, zorder=0)
ax.set_xlabel("頻率 (Hz)")
ax.set_ylabel("正規化振幅（2–10 Hz 峰值 = 1）")
ax.set_xlim(0.5, 15)
ax.set_ylim(0, 1.08)
ax.legend(loc="upper right")
fig.tight_layout()
fig.savefig(OUT / "spectrum.png", dpi=160)
plt.close(fig)

# ---------- 3. 推算：陀螺一階低通 α=0.15 @ 6 ms 的頻率響應 ----------
alpha = 0.15
f = np.linspace(0.1, 40, 2000)
z = np.exp(1j * 2 * np.pi * f * DT)
H = alpha / (1 - (1 - alpha) / z)
fig, (a1, a2) = plt.subplots(2, 1, figsize=(8.5, 6.2), sharex=True)
a1.semilogx(f, 20 * np.log10(np.abs(H)), color=BLUE)
a1.axhline(-3, color=MUTED, linestyle=":", linewidth=1.2)
a1.set_ylabel("增益 (dB)")
a2.semilogx(f, np.degrees(np.angle(H)), color=AMBER)
a2.set_ylabel("相位 (°)")
a2.set_xlabel("頻率 (Hz)")
p4 = np.degrees(np.angle(alpha / (1 - (1 - alpha) / np.exp(1j * 2 * np.pi * 4 * DT))))
for ax in (a1, a2):
    ax.axvline(4, color=ROSE, linestyle="--", linewidth=1.4)
a2.annotate(f"4 Hz：{p4:.0f}°", xy=(4, p4), xytext=(6.5, p4 + 18), color=ROSE, fontsize=15,
            arrowprops={"arrowstyle": "->", "color": ROSE})
ticks = [0.1, 0.4, 1, 4, 10, 40]
a2.set_xticks(ticks, [f"{v:g}" for v in ticks])  # 中文字型缺上標負號，改用一般數字刻度
a2.xaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
fig.tight_layout()
fig.savefig(OUT / "gyro-lpf-response.png", dpi=160)
plt.close(fig)

print("peaks:", {k: round(v, 2) for k, v in peaks.items()}, "fc:", round(fc, 2), "phase@4Hz:", round(p4, 1))
