# -*- coding: utf-8 -*-
"""
VOFA 数据对比（两个文件）
用法：python Tools/vofa_cmp.py "Doc/测试数据4.csv" "Doc/测试数据5.csv"

指标：
  稳态 —— 功率偏差、Trim 均值、窗口内抖动 std
  阶跃 —— 过冲/凹陷深度、凹陷持续、恢复±2W、偏离能量、Trim 摆幅、ΔI_L
"""
import io
import sys

A_PATH = sys.argv[1] if len(sys.argv) > 1 else 'Doc/测试数据4.csv'
B_PATH = sys.argv[2] if len(sys.argv) > 2 else 'Doc/测试数据5.csv'

OUT = sys.stdout
sys.stdout = io.open('Tools/vofa_compare.txt', 'w', encoding='utf-8', newline='\n')
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
    # 必须同时满足：已使能(P_Tgt>20) 且 变换器确实在工作(I_L_Ref 非零)。
    # 否则会把"已使能但软启动尚未完成"的段当成稳态——数据5 里就存在
    # P_Tgt=30 但 I_L_Ref=0、Trim=0、u=0、P_Ref=0.48W 的段，
    # 混进去会把均值拉低、std 放大，凭空造出一个 -18% 的"回归"。
    return [r for r in rows if r[0] > 20.0 and abs(r[6]) > 0.3]


def stats(d):
    o = {'n': len(d)}

    wins, i, W = [], 0, 300
    while i < len(d) - W:
        seg = [d[j][3] for j in range(i, i + W)]
        if max(seg) - min(seg) < 2.0:
            wins.append((i, i + W))
            i += W
        else:
            i += 15

    n = syy = 0
    devs, trims = [], []
    for a, b in wins:
        mr = sum(d[j][2] for j in range(a, b)) / (b - a)
        mt = sum(d[j][0] for j in range(a, b)) / (b - a)
        devs.append(mr - mt)
        trims.append(sum(d[j][7] for j in range(a, b)) / (b - a))
        for j in range(a, b):
            syy += (d[j][2] - mr) ** 2
            n += 1
    o['wins'] = len(wins)
    o['dev'] = sum(devs) / len(devs) if devs else 0.0
    o['devpct'] = o['dev'] / 30.0 * 100
    o['trim'] = sum(trims) / len(trims) if trims else 0.0
    o['rip'] = (syy / n) ** 0.5 if n else 0.0

    ev = []
    for k in range(60, len(d)):
        if abs(d[k][3] - d[k - 30][3]) > 40.0 and (not ev or k - ev[-1] > 400):
            ev.append(k)
    evs = []
    for k in ev:
        tgt = d[k][0]
        seg = list(range(k, min(k + 300, len(d))))
        if len(seg) < 50:
            continue
        pk = max(d[j][2] for j in seg)
        mn = min(d[j][2] for j in seg)
        below = [j for j in seg if d[j][2] < tgt - 2.0]
        dur = (below[-1] - below[0] + 1) / FS * 1e3 if below else 0.0
        rec = 0
        for j in seg:
            if abs(d[j][2] - tgt) > 2.0:
                rec = j - k
        e = sum(abs(d[j][2] - tgt) for j in seg) / FS * 1e3
        tr = [d[j][7] for j in seg]
        evs.append({'k': k, 'dp': d[k][3] - d[k - 30][3], 'over': pk - tgt,
                    'dip': mn - tgt, 'durdip': dur, 'rec': rec / FS * 1e3,
                    'e': e, 'trsw': max(tr) - min(tr),
                    'dil': d[k + 20][4] - d[k - 20][4]})
    o['ev'] = evs
    return o


def show(tag, o):
    P('')
    P('-' * 98)
    P('%s   使能 %d 点，静窗 %d 段' % (tag, o['n'], o['wins']))
    P('-' * 98)
    P('  稳态：偏差 %+.3f W (%+.2f%%)   Trim %+.2f W   抖动 std %.3f W'
      % (o['dev'], o['devpct'], o['trim'], o['rip']))
    if not o['ev']:
        P('  （无阶跃事件）')
        return
    P('  %-8s %9s %9s %10s %10s %8s %9s %9s %9s'
      % ('样本', 'ΔP_Cha', '过冲', '凹陷最深', '凹陷持续', 'ΔI_L', '恢复±2W', '能量', 'Trim摆幅'))
    for x in o['ev']:
        P('  %-8d %9.1f %9.2f %10.2f %8.1fms %7.2fA %7.1fms %7.1fmJ %8.2fW'
          % (x['k'], x['dp'], x['over'], x['dip'], x['durdip'], x['dil'],
             x['rec'], x['e'], x['trsw']))
    n = len(o['ev'])
    P('  %-8s %9.1f %9.2f %10.2f %8.1fms %7.2fA %7.1fms %7.1fmJ %8.2fW'
      % ('均值', sum(x['dp'] for x in o['ev']) / n, sum(x['over'] for x in o['ev']) / n,
         sum(x['dip'] for x in o['ev']) / n, sum(x['durdip'] for x in o['ev']) / n,
         sum(x['dil'] for x in o['ev']) / n, sum(x['rec'] for x in o['ev']) / n,
         sum(x['e'] for x in o['ev']) / n, sum(x['trsw'] for x in o['ev']) / n))


A = stats(load(A_PATH))
B = stats(load(B_PATH))
P('=' * 98)
P('对比：%s   vs   %s' % (A_PATH, B_PATH))
P('=' * 98)
show(A_PATH, A)
show(B_PATH, B)

P('')
P('=' * 98)
P('差异（均值）')
P('=' * 98)
if A['ev'] and B['ev']:
    n, m = len(A['ev']), len(B['ev'])
    for key, nm, unit in (('over', '过冲', 'W'), ('dip', '凹陷深度', 'W'),
                          ('durdip', '凹陷持续', 'ms'), ('rec', '恢复±2W', 'ms'),
                          ('e', '偏离能量', 'mJ'), ('trsw', 'Trim摆幅', 'W'),
                          ('dil', 'ΔI_L', 'A')):
        va = sum(x[key] for x in A['ev']) / n
        vb = sum(x[key] for x in B['ev']) / m
        P('  %-10s %9.2f %-3s -> %9.2f %-3s  (%+.0f%%)'
          % (nm, va, unit, vb, unit, (vb / va - 1) * 100 if va else 0))
P('  稳态抖动   %9.3f W   -> %9.3f W  (%+.0f%%)'
  % (A['rip'], B['rip'], (B['rip'] / A['rip'] - 1) * 100 if A['rip'] else 0))

sys.stdout.close()
sys.stdout = OUT
print('done -> Tools/vofa_compare.txt')
