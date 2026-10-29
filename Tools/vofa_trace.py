# -*- coding: utf-8 -*-
"""看原始时序：找真正安静的段 + 打印阶跃附近逐点"""
import io
import sys

PATH = 'Doc/测试数据2.csv'
OUT = sys.stdout
sys.stdout = io.open('Tools/vofa_trace.txt', 'w', encoding='utf-8', newline='\n')


def P(*a):
    print(*a)


d = []
for ln in io.open(PATH, encoding='utf-8', errors='ignore'):
    ln = ln.strip()
    if not ln or ln[0].isalpha():
        continue
    try:
        v = [float(x) for x in ln.split(',')[:6]]
    except ValueError:
        continue
    if len(v) == 6:
        d.append(v)

# 真正安静的段：窗口 50 点（0.5s）内 P_Cha 峰峰 < 3W
W = 50
quiet = []
i = 0
while i < len(d) - W:
    seg = [d[j][3] for j in range(i, i + W)]
    if max(seg) - min(seg) < 3.0:
        quiet.append((i, i + W))
        i += W
    else:
        i += 5

P('安静窗口（0.5s 内 P_Cha 峰峰<3W）共 %d 段' % len(quiet))
if quiet:
    ref, ins, tgs = [], [], []
    for a, b in quiet:
        for j in range(a, b):
            ref.append(d[j][1])
            ins.append(d[j][2])
            tgs.append(d[j][0])
    n = len(ref)
    mar = sum(ref) / n
    mav = sum(tgs) / n
    s = (sum((x - mar) ** 2 for x in ref) / n) ** 0.5
    si = (sum((x - sum(ins) / n) ** 2 for x in ins) / n) ** 0.5
    P('  样本 %d 点（%.1f s）' % (n, n / 100.0))
    P('  P_Tgt        均值 %.3f W' % mav)
    P('  P_Ref_Avg    均值 %.3f W  std %.3f W  峰峰 %.2f W'
      % (mar, s, max(ref) - min(ref)))
    P('  P_Ref 瞬时   均值 %.3f W  std %.3f W  峰峰 %.2f W'
      % (sum(ins) / n, si, max(ins) - min(ins)))
    P('  稳态偏差     %+.3f W (%+.2f%%)' % (mar - mav, (mar - mav) / mav * 100))

P('')
P('=' * 78)
P('阶跃事件附近逐点（t 从事件前 5 点起，单位 10ms）')
P('=' * 78)
ev = []
for k in range(1, len(d)):
    if abs(d[k][3] - d[k - 1][3]) > 15.0:
        if ev and k - ev[-1] < 8:
            continue
        ev.append(k)

for n, k in enumerate(ev[:4]):
    P('')
    P('--- 事件 %d @ 样本 %d ---' % (n + 1, k))
    P('%6s %8s %10s %10s %10s %8s' % ('t(ms)', 'P_Cha', 'P_Ref_Avg', 'P_Ref', 'I_L', 'V_B'))
    for j in range(max(0, k - 3), min(len(d), k + 22)):
        P('%6d %8.1f %10.2f %10.2f %10.2f %8.2f'
          % ((j - k) * 10, d[j][3], d[j][1], d[j][2], d[j][4], d[j][5]))

sys.stdout.close()
sys.stdout = OUT
print('done -> Tools/vofa_trace.txt')
