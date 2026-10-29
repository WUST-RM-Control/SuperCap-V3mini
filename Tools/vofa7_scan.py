# -*- coding: utf-8 -*-
"""数据7 全览：整个测试的过程剖面，找低电容电压/大负载/Trim 饱和段"""
import io
import sys

OUT = sys.stdout
sys.stdout = io.open('Tools/vofa7_scan.txt', 'w', encoding='utf-8', newline='\n')


def P(*a):
    print(*a)


rows = []
for ln in io.open('Doc/测试数据7.csv', encoding='utf-8', errors='ignore'):
    ln = ln.strip()
    if not ln or ln[0].isalpha():
        continue
    try:
        v = [float(x) for x in ln.split(',')[:9]]
    except ValueError:
        continue
    if len(v) == 9 and all(x == x and abs(x) < 1e30 for x in v):
        rows.append(v)

P('样本 %d（%.1f s @1kHz）' % (len(rows), len(rows) / 1000.0))
P('')
P('全记录范围：')
for c, nm, unit in ((0, 'P_Tgt', 'W'), (2, 'P_Ref', 'W'), (3, 'P_Cha', 'W'),
                    (4, 'I_L', 'A'), (5, 'V_B', 'V'), (7, 'Trim', 'W'),
                    (8, 'u', '')):
    col = [r[c] for r in rows]
    P('  %-7s %9.2f ~ %9.2f %s' % (nm, min(col), max(col), unit))

P('')
P('=' * 96)
P('过程剖面（每 1000 点抽 1；标 * 表示该 1000 点内出现 |Trim|>10W 或 V_B<12V）')
P('=' * 96)
P('%7s %10s %8s %8s %9s %8s %8s %8s %8s %s'
  % ('样本', 't(s)', 'P_Tgt', 'V_B', 'P_Cha', 'P_Ref', 'I_L', 'I_L_Ref', 'Trim', ''))
for i in range(0, len(rows), 1000):
    blk = rows[i:i + 1000]
    r = blk[len(blk) // 2]
    flag = ''
    if any(abs(x[7]) > 10.0 for x in blk):
        flag += '*Trim大 '
    if any(x[5] < 12.0 for x in blk):
        flag += '*V_B低 '
    P('%7d %10.2f %8.1f %8.2f %9.1f %8.2f %8.2f %8.2f %8.2f %s'
      % (i, i / 1000.0, r[0], r[5], r[3], r[2], r[4], r[6], r[7], flag))

# 找 V_B 最低、P_Cha 最大的时刻
k_vb = min(range(len(rows)), key=lambda j: rows[j][5])
k_pc = max(range(len(rows)), key=lambda j: rows[j][3])
k_tr = min(range(len(rows)), key=lambda j: rows[j][7])
P('')
P('V_B 最低 @%d (%.2f V)' % (k_vb, rows[k_vb][5]))
P('P_Cha 最大 @%d (%.1f W)' % (k_pc, rows[k_pc][3]))
P('Trim 最小 @%d (%.2f W)' % (k_tr, rows[k_tr][7]))
for k, nm in ((k_vb, 'V_B最低'), (k_pc, 'P_Cha最大'), (k_tr, 'Trim最小')):
    P('')
    P('--- %s 附近 @%d ---' % (nm, k))
    P('%6s %8s %8s %9s %8s %8s %8s %8s'
      % ('n', 'P_Tgt', 'V_B', 'P_Cha', 'P_Ref', 'I_L', 'I_L_Ref', 'Trim'))
    for j in range(max(0, k - 5), min(len(rows), k + 6)):
        r = rows[j]
        P('%6d %8.1f %8.2f %9.1f %8.2f %8.2f %8.2f %8.2f'
          % (j - k, r[0], r[5], r[3], r[2], r[4], r[6], r[7]))

sys.stdout.close()
sys.stdout = OUT
print('done -> Tools/vofa7_scan.txt')
