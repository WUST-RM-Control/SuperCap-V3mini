# -*- coding: utf-8 -*-
"""数据7 验收：稳态 P99 + 瞬态 + Trim 是否还会撞限幅"""
import io
import sys

OUT = sys.stdout
sys.stdout = io.open('Tools/vofa7.txt', 'w', encoding='utf-8', newline='\n')


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
        # 丢掉含 nan/inf 的行（首帧抓包错位会产生 10^38 / nan）
        if len(v) == 9 and all(x == x and abs(x) < 1e30 for x in v):
            rows.append(v)
    return rows


def pct(xs, q):
    s = sorted(xs)
    return s[int(q / 100.0 * (len(s) - 1))]


FILES = [('Doc/测试数据4.csv', '数据4 600Hz 无抗饱和(基线)'),
         ('Doc/测试数据7.csv', '数据7 600Hz +抗饱和')]

P('=' * 92)
P('一、稳态噪底（剔除阶跃后 0.4s；判据 P99 < 1W）')
P('=' * 92)
P('%-30s %7s %7s %7s %7s %7s %6s' % ('', 'P50', 'P90', 'P99', 'P99.9', 'max', '窗口'))
P('-' * 92)
for path, tag in FILES:
    raw = load(path)
    d = [r for r in raw if r[0] > 20.0 and abs(r[6]) > 0.3]
    ev = []
    for k in range(60, len(d)):
        if abs(d[k][3] - d[k - 30][3]) > 40.0 and (not ev or k - ev[-1] > 300):
            ev.append(k)
    wins, i, W = [], 0, 300
    while i < len(d) - W:
        seg = [d[j][3] for j in range(i, i + W)]
        far = all(not (k - 400 <= i <= k + W + 100) for k in ev)
        # 窗口均值必须贴近 P_Tgt：排除软启动段（Soft<1 时 I_L_Ref 被斜坡乘过，
        # P_Ref 还在 0→30 爬升，混进来会把 P99 抬高好几倍——数据7 就这样被误判）
        mr = sum(d[j][2] for j in range(i, i + W)) / W
        mt = sum(d[j][0] for j in range(i, i + W)) / W
        near = abs(mr - mt) < 5.0
        if max(seg) - min(seg) < 2.0 and far and near:
            wins.append((i, i + W))
            i += W
        else:
            i += 15
    x = []
    for a, b in wins:
        m = sum(d[j][2] for j in range(a, b)) / (b - a)
        x.extend(abs(d[j][2] - m) for j in range(a, b))
    if x:
        P('%-30s %7.3f %7.3f %7.3f %7.3f %7.2f %6d'
          % (tag, pct(x, 50), pct(x, 90), pct(x, 99), pct(x, 99.9), max(x), len(wins)))
    else:
        P('%-30s 无静窗' % tag)

P('')
P('=' * 92)
P('二、Trim 抗饱和验证（撞限幅是 TRIM_MAX=15W）')
P('=' * 92)
P('%-30s %9s %9s %10s %12s' % ('', 'Trim最小', 'Trim最大', '|Trim|≥14.9', 'V_B 范围'))
P('-' * 92)
for path, tag in FILES:
    d = [r for r in load(path) if r[0] > 20.0]
    tr = [r[7] for r in d]
    vb = [r[5] for r in d]
    sat = [i for i, v in enumerate(tr) if abs(v) >= 14.9]
    P('%-30s %9.2f %9.2f %7d点 %7.2f~%.2fV'
      % (tag, min(tr), max(tr), len(sat), min(vb), max(vb)))

P('')
P('=' * 92)
P('三、负载阶跃')
P('=' * 92)
for path, tag in FILES:
    d = [r for r in load(path) if r[0] > 20.0 and abs(r[6]) > 0.3]
    ev = []
    for k in range(60, len(d)):
        if abs(d[k][3] - d[k - 30][3]) > 40.0 and (not ev or k - ev[-1] > 400):
            ev.append(k)
    rows = []
    for k in ev:
        tgt = d[k][0]
        seg = list(range(k, min(k + 300, len(d))))
        if len(seg) < 50:
            continue
        pk = max(d[j][2] for j in seg)
        mn = min(d[j][2] for j in seg)
        below = [j for j in seg if d[j][2] < tgt - 2.0]
        dur = (below[-1] - below[0] + 1) / 1000.0 * 1e3 if below else 0.0
        rec = 0
        for j in seg:
            if abs(d[j][2] - tgt) > 2.0:
                rec = j - k
        e = sum(abs(d[j][2] - tgt) for j in seg) / 1000.0 * 1e3
        trs = [d[j][7] for j in seg]
        rows.append((d[k][3] - d[k - 30][3], pk - tgt, mn - tgt, dur,
                     rec, e, max(trs) - min(trs)))
    P('')
    P('%s：%d 次事件' % (tag, len(rows)))
    P('  %-9s %8s %9s %9s %9s %9s %9s'
      % ('ΔP_Cha', '过冲', '凹陷', '凹陷持续', '恢复±2W', '能量', 'Trim摆幅'))
    for r in rows:
        P('  %-9.1f %8.2f %9.2f %8.1fms %8.1fms %7.1fmJ %8.2fW'
          % (r[0], r[1], r[2], r[3], r[4], r[5], r[6]))
    if rows:
        n = len(rows)
        P('  %-9s %8.2f %9.2f %8.1fms %8.1fms %7.1fmJ %8.2fW'
          % ('均值', sum(x[1] for x in rows) / n, sum(x[2] for x in rows) / n,
             sum(x[3] for x in rows) / n, sum(x[4] for x in rows) / n,
             sum(x[5] for x in rows) / n, sum(x[6] for x in rows) / n))

sys.stdout.close()
sys.stdout = OUT
print('done -> Tools/vofa7.txt')
