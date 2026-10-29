# -*- coding: utf-8 -*-
"""
稳态噪底分位数（抗离群），判断电流环稳不稳。

用法：python Tools/vofa_pct.py [file1 file2 ...]
     不给参数时默认跑 测试数据4/5/6。
判据：P99 < 1W 干净；P99 > 5W 说明贴到稳定边界、有间歇突发。
"""
import io
import sys

ARGS = sys.argv[1:]
if not ARGS:
    ARGS = ['Doc/测试数据4.csv', 'Doc/测试数据5.csv', 'Doc/测试数据6.csv']

OUT = sys.stdout
sys.stdout = io.open('Tools/vofa_pct.txt', 'w', encoding='utf-8', newline='\n')


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


def pct(xs, q):
    s = sorted(xs)
    return s[int(q / 100.0 * (len(s) - 1))]


P('%-14s %7s %7s %7s %7s %7s %7s %8s' % ('', 'P50', 'P90', 'P99', 'P99.9', 'max', 'std', '窗口数'))
P('-' * 84)
for path in ARGS:
    d = load(path)
    tag = path.split('/')[-1].replace('测试数据', '数据').replace('.csv', '')
    # 阶跃位置
    ev = []
    for k in range(60, len(d)):
        if abs(d[k][3] - d[k - 30][3]) > 40.0 and (not ev or k - ev[-1] > 300):
            ev.append(k)
    wins, i, W = [], 0, 300
    while i < len(d) - W:
        seg = [d[j][3] for j in range(i, i + W)]
        # 静窗 + 距任何阶跃 > 400 点（0.4s）
        far = all(not (k - 400 <= i <= k + W + 100) for k in ev)
        if max(seg) - min(seg) < 2.0 and far:
            wins.append((i, i + W))
            i += W
        else:
            i += 15
    x = []
    for a, b in wins:
        m = sum(d[j][2] for j in range(a, b)) / (b - a)
        x.extend(abs(d[j][2] - m) for j in range(a, b))
    if not x:
        P('%-14s 无可用窗口' % tag)
        continue
    n = len(x)
    mm = sum(x) / n
    sd = (sum((v - mm) ** 2 for v in x) / n) ** 0.5
    P('%-14s %7.3f %7.3f %7.3f %7.3f %7.2f %7.3f %8d'
      % (tag, pct(x, 50), pct(x, 90), pct(x, 99), pct(x, 99.9), max(x), sd, len(wins)))
P('')
P('（已剔除阶跃后 0.4s 内的静窗，单位 W）')
sys.stdout.close()
sys.stdout = OUT
print('done -> Tools/vofa_pct.txt')
