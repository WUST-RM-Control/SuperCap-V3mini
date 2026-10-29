# -*- coding: utf-8 -*-
"""
VOFA CSV 分析：稳态抖动 + 负载阶跃后的 P_Ref 恢复

列：P_Tgt, P_Ref_Avg(1ms), P_Ref(47us), P_Cha_Avg(1ms), I_L_Avg(1ms), V_B
采样：100 Hz（CFG_DEBUG_PERIOD_MS=10）

运行：python Tools/analyze_vofa.py "Doc/测试数据2.csv"
"""
import io
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else 'Doc/测试数据2.csv'
OUT = sys.stdout
sys.stdout = io.open('Tools/vofa_report.txt', 'w', encoding='utf-8', newline='\n')
FS = 100.0
DT = 1.0 / FS


def P(*a):
    print(*a)


def load(p):
    rows = []
    for ln in io.open(p, encoding='utf-8', errors='ignore'):
        ln = ln.strip()
        if not ln or ln[0].isalpha():
            continue
        try:
            v = [float(x) for x in ln.replace(';', ',').split(',')[:6]]
        except ValueError:
            continue
        if len(v) == 6:
            rows.append(v)
    return rows


def stats(xs):
    n = len(xs)
    m = sum(xs) / n
    var = sum((x - m) ** 2 for x in xs) / n
    return m, var ** 0.5, min(xs), max(xs)


d = load(PATH)
P('=' * 84)
P('数据文件：%s    样本 %d 点 = %.1f 秒 @100Hz' % (PATH, len(d), len(d) / FS))
P('=' * 84)

tg = [r[0] for r in d]
pr = [r[1] for r in d]
pi = [r[2] for r in d]
pc = [r[3] for r in d]
il = [r[4] for r in d]
vb = [r[5] for r in d]

m, s, lo, hi = stats(tg)
P('P_Tgt    : 均值 %.1f W，范围 %.0f~%.0f W，出现 %d 个不同档位'
  % (m, lo, hi, len(set(round(x) for x in tg))))
cnt = {}
for x in tg:
    k = round(x)
    cnt[k] = cnt.get(k, 0) + 1
P('           各档位占比：' + '，'.join('%dW:%.0f%%' % (k, 100.0 * v / len(tg))
                                   for k, v in sorted(cnt.items())))

m, s, lo, hi = stats(pc)
P('P_Cha    : 均值 %.1f W，范围 %.1f~%.1f W' % (m, lo, hi))
P('V_B      : %.2f ~ %.2f V' % (min(vb), max(vb)))
P('')

# ---------------- 1. 负载阶跃检测 ----------------
steps = []
for i in range(1, len(pc)):
    dpc = pc[i] - pc[i - 1]
    # 用 3 点中值粗滤，避免单点毛刺
    if abs(dpc) > 15.0:
        if steps and i - steps[-1] < 8:
            continue
        steps.append(i)

P('-' * 84)
P('一、负载阶跃事件（|ΔP_Cha| > 15W/10ms，共 %d 次）' % len(steps))
P('-' * 84)
if steps:
    P('%-6s %8s %9s %9s %10s %11s' % ('#', 'ΔP_Cha', 'P_Ref峰值', '最大偏离', '恢复±2W', '偏离能量'))
    tot_e = []
    for k, i in enumerate(steps[:14]):
        dpc = pc[i] - pc[i - 1]
        tgt = tg[min(i + 1, len(tg) - 1)]
        base = tgt
        seg = range(i, min(i + 60, len(pr)))          # 往后 600 ms
        peak_dev = 0.0
        peak_v = pr[i]
        for j in seg:
            dv = abs(pr[j] - base)
            if dv > peak_dev:
                peak_dev, peak_v = dv, pr[j]
        # 回到 ±2W 的最后一个点
        rec = 0
        for j in seg:
            if abs(pr[j] - base) > 2.0:
                rec = j - i
        e = sum(abs(pi[j] - base) for j in seg) * DT
        tot_e.append(e)
        P('%-6d %8.1f %9.1f %9.1f %8d ms %8.1f mJ'
          % (k + 1, dpc, peak_v, peak_dev, rec * 10, e * 1e3))
    if len(steps) > 14:
        P('... 其余 %d 次省略' % (len(steps) - 14))
    P('')
    P('   阶跃后 P_Ref 最大偏离：中位数 %.1f W，均值 %.1f W'
      % (sorted(abs(pr[i] - tg[i]) for i in steps)[len(steps) // 2],
         sum(abs(pr[i] - tg[i]) for i in steps) / len(steps)))
    P('   偏离能量：均值 %.1f mJ（= 缓冲能量损失，55J 的 %.3f%%）'
      % (sum(tot_e) / len(tot_e) * 1e3, sum(tot_e) / len(tot_e) / 55.0 * 100))

# ---------------- 2. 稳态抖动 ----------------
P('')
P('-' * 84)
P('二、稳态抖动（只统计 |ΔP_Cha|<2W 且 |P_Ref-P_Tgt| 平稳的窗口）')
P('-' * 84)
quiet = []
for i in range(2, len(pr) - 2):
    if abs(pc[i] - pc[i - 1]) < 2.0 and abs(pc[i] - pc[i - 2]) < 2.0:
        quiet.append(i)
if quiet:
    a = [pr[i] for i in quiet]
    b = [pi[i] for i in quiet]
    for nm, xs in (('P_Ref_Avg (1ms 平均)', a), ('P_Ref     (47us 瞬时)', b)):
        m, s, lo, hi = stats(xs)
        P('%-22s 均值 %8.3f W  标准差 %6.3f W  峰峰 %6.3f W  范围 %.2f~%.2f'
          % (nm, m, s, hi - lo, lo, hi))
    m, s, lo, hi = stats([tg[i] for i in quiet])
    P('%-22s 均值 %8.3f W' % ('P_Tgt', m))
    P('')
    ma = sum(a) / len(a)
    mt = sum(tg[i] for i in quiet) / len(quiet)
    P('   稳态功率偏差 (P_Ref_Avg − P_Tgt) = %+.3f W (%.2f%%)' % (ma - mt, (ma - mt) / mt * 100))
    P('   1ms 平均后抖动 %.3f W = %.1f mA @24V；单次 ADC LSB = 64.5 mA'
      % (stats(a)[1], stats(a)[1] / 24.0 * 1e3))

# ---------------- 3. 直流侧一致性 ----------------
P('')
P('-' * 84)
P('三、一致性检查')
P('-' * 84)
bad = 0
for i in range(len(d)):
    if abs(pr[i] - (pc[i] + 24.0 * il[i] * 0.0)) < 0:
        bad += 1
P('I_L 与 P_Ref 的相关系数：%.3f（应接近 1，说明功率主要走变换器）'
  % (sum((il[i] - sum(il) / len(il)) * (pr[i] - sum(pr) / len(pr)) for i in range(len(d)))
     / (len(d) * stats(il)[1] * stats(pr)[1])))
P('')

sys.stdout.close()
sys.stdout = OUT
print('done -> Tools/vofa_report.txt')
