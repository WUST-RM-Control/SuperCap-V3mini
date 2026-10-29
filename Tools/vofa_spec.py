# -*- coding: utf-8 -*-
"""数据4/5 稳态抖动频谱对比：是单频振荡还是宽带噪声"""
import io
import math
import sys

OUT = sys.stdout
sys.stdout = io.open('Tools/vofa_spec.txt', 'w', encoding='utf-8', newline='\n')
FS = 1000.0


def P(*a):
    print(*a)


def load(path):
    rows = []
    for ln in io.open(path, encoding='utf-8', errors='ignore'):
        ln = ln.strip()
        if not ln or ln[0].isalpha():
            continue
        try:
            v = [float(x) for x in ln.split(',')[:9]]
        except ValueError:
            continue
        if len(v) == 9:
            rows.append(v)
    return [r for r in rows if r[0] > 20.0 and abs(r[6]) > 0.3]


def runs(d):
    wins, i, W = [], 0, 300
    while i < len(d) - W:
        seg = [d[j][3] for j in range(i, i + W)]
        if max(seg) - min(seg) < 2.0:
            wins.append((i, i + W))
            i += W
        else:
            i += 15
    return wins


for path, tag in (('Doc/测试数据4.csv', 'CUR_FC=600'),
                  ('Doc/测试数据5.csv', 'CUR_FC=900')):
    d = load(path)
    wins = runs(d)
    P('=' * 78)
    P('%s   %s   静窗 %d 段' % (path, tag, len(wins)))
    P('=' * 78)
    if not wins:
        P('  无静窗')
        continue

    x = []
    for a, b in wins:
        m = sum(d[j][2] for j in range(a, b)) / (b - a)
        x.extend(d[j][2] - m for j in range(a, b))
    n = len(x)
    sd = (sum(v * v for v in x) / n) ** 0.5
    zc = sum(1 for k in range(1, n) if x[k - 1] * x[k] < 0)
    P('  样本 %d   std %.3f W   峰峰 %.2f W   过零折算 %.0f Hz'
      % (n, sd, max(x) - min(x), zc / 2.0 / (n / FS)))

    # 2 抽点 -> 500 Hz 等效采样，扫 10..240 Hz 找主峰
    xs = x[::2]
    fs2 = FS / 2.0
    best_f, best_a = 0.0, 0.0
    for f in range(10, 241, 5):
        w = 2 * math.pi * f / fs2
        re = sum(v * math.cos(w * k) for k, v in enumerate(xs))
        im = sum(v * math.sin(w * k) for k, v in enumerate(xs))
        amp = 2.0 * (re * re + im * im) ** 0.5 / len(xs)
        if amp > best_a:
            best_a, best_f = amp, f
    P('  主峰：%.0f Hz，等效正弦振幅 %.3f W' % (best_f, best_a))
    # 单频判断：正弦振幅 / std ≈ 1.414
    ratio = best_a / sd if sd else 0.0
    P('  主峰/std = %.2f  -> %s'
      % (ratio, '以单频振荡为主，环路接近不稳定' if ratio > 1.0
         else '以宽带噪声为主，是环路把测量噪声放大了'))
    P('')

sys.stdout.close()
sys.stdout = OUT
print('done -> Tools/vofa_spec.txt')
