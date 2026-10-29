# -*- coding: utf-8 -*-
"""数据11（DMAX=0.92）验收：高占空比工况的突发有没有消失"""
import io
import sys

OUT = sys.stdout
sys.stdout = io.open('Tools/vofa11.txt', 'w', encoding='utf-8', newline='\n')


def P(*a):
    print(*a)


def load(path):
    rows = []
    for ln in io.open(path, encoding='utf-8', errors='ignore'):
        ln = ln.strip()
        if not ln or ln[0].isalpha():
            continue
        try:
            v = [float(x) for x in ln.split(',')[:14]]
        except ValueError:
            continue
        if len(v) == 14 and all(x == x and abs(x) < 1e30 for x in v):
            rows.append(v)
    return rows


def analyse(path, tag):
    d = load(path)
    en = [r for r in d if r[0] > 5.0 and abs(r[6]) > 0.3]
    ev = []
    for k in range(60, len(en)):
        if abs(en[k][3] - en[k - 30][3]) > 40.0 and (not ev or k - ev[-1] > 300):
            ev.append(k)

    wins, i, W = [], 0, 300
    while i < len(en) - W:
        seg = [en[j][3] for j in range(i, i + W)]
        far = all(not (k - 400 <= i <= k + W + 100) for k in ev)
        mr = sum(en[j][2] for j in range(i, i + W)) / W
        mt = sum(en[j][0] for j in range(i, i + W)) / W
        if max(seg) - min(seg) < 2.0 and far and abs(mr - mt) < 5.0:
            wins.append((i, i + W))
            i += W
        else:
            i += 15

    out = []
    for a, b in wins:
        mv = sum(en[j][5] for j in range(a, b)) / (b - a)
        ma = sum(en[j][13] for j in range(a, b)) / (b - a)
        mc = sum(en[j][3] for j in range(a, b)) / (b - a)
        md = sum(en[j][11] for j in range(a, b)) / (b - a)     # D_A 该窗均值
        mr = sum(en[j][2] for j in range(a, b)) / (b - a)
        dev = sorted(abs(en[j][2] - mr) for j in range(a, b))
        out.append((a, ma, mv, mv / ma, mc, md, dev[len(dev) // 2],
                    dev[int(0.99 * (len(dev) - 1))]))
    return out


P('=' * 100)
P('高占空比工况（D_A 均值 > 0.80 且负载 > 90W）的噪底对比')
P('=' * 100)
P('%-22s %5s %8s %8s %8s %8s %8s %8s'
  % ('数据', '窗口数', 'V_B', 'D_A', 'P_Cha', 'P50', 'P99', 'P99max'))
P('-' * 100)
for path, tag in (('Doc/测试数据9.csv', '数据9  DMAX=0.95 fc600'),
                  ('Doc/测试数据10.csv', '数据10 DMAX=0.95 fc600+DT'),
                  ('Doc/测试数据11.csv', '数据11 DMAX=0.92 fc600'),
                  ('Doc/测试数据12.csv', '数据12 DMAX=0.92 fc750'),
                  ('Doc/测试数据13.csv', '数据13 DMAX=0.92 fc900')):
    try:
        w = analyse(path, tag)
    except FileNotFoundError:
        P('%-22s 文件不存在' % tag)
        continue
    hi = [x for x in w if x[5] > 0.80 and x[4] > 90.0]
    if hi:
        P('%-22s %5d %8.2f %8.3f %8.1f %8.3f %8.3f %8.3f'
          % (tag, len(hi),
             sum(x[2] for x in hi) / len(hi), sum(x[5] for x in hi) / len(hi),
             sum(x[4] for x in hi) / len(hi),
             sum(x[6] for x in hi) / len(hi),
             sum(x[7] for x in hi) / len(hi),
             max(x[7] for x in hi)))
    else:
        P('%-22s 无高占空比窗口' % tag)

P('')
P('=' * 100)
P('数据13 逐窗口明细')
P('=' * 100)
w = analyse('Doc/测试数据13.csv', 'd13')
P('%7s %7s %7s %7s %7s %7s %7s %7s'
  % ('样本', 'V_A', 'V_B', 'r', 'D_A', 'P_Cha', 'P50', 'P99'))
for x in w:
    mark = '  <<' if x[7] > 2.0 else ''
    P('%7d %7.2f %7.2f %7.3f %7.3f %7.1f %7.3f %7.3f%s'
      % (x[0], x[1], x[2], x[3], x[5], x[4], x[6], x[7], mark))
bad = [x for x in w if x[7] > 2.0]
P('')
P('  P99 > 2W 的窗口：%d / %d' % (len(bad), len(w)))
if bad:
    P('  它们的位置：%s' % ', '.join('%d' % x[0] for x in bad))
    P('  它们的 V_B ：%s' % ', '.join('%.2f' % x[2] for x in bad))
    P('  它们的 D_A ：%s' % ', '.join('%.3f' % x[5] for x in bad))
    P('  它们的负载：%s' % ', '.join('%.0f' % x[4] for x in bad))

sys.stdout.close()
sys.stdout = OUT
print('done -> Tools/vofa11.txt')
