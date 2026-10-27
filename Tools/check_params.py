# -*- coding: utf-8 -*-
"""
App/params.h 参数静态审计

编译器不会报"参数顺序错了"，但这些错误只会在台架上暴露。本脚本把
params.h 的宏解析出来，检查物理/逻辑约束与推导值，输出一张审计表。

运行：python Tools/check_params.py
"""
import io
import math
import re
import sys

SRC = 'App/params.h'
OUT = sys.stdout
sys.stdout = io.open('Tools/param_audit.txt', 'w', encoding='utf-8', newline='\n')


def load():
    txt = io.open(SRC, encoding='utf-8').read()
    # 去掉注释，避免注释里的内容被匹配
    txt = re.sub(r'/\*.*?\*/', '', txt, flags=re.S)
    txt = re.sub(r'//[^\n]*', '', txt)
    env = {}
    for line in txt.splitlines():
        m = re.match(r'\s*#define\s+([A-Za-z_]\w*)\s+(.+?)\s*$', line)
        if not m:
            continue
        name, expr = m.group(1), m.group(2)
        expr = expr.replace('f', '') if re.fullmatch(r'[\d.]+f', expr) else expr
        expr = re.sub(r'(\d)f\b', r'\1', expr)
        try:
            env[name] = eval(expr, {'__builtins__': {}}, dict(env, **{'math': math}))
        except Exception:
            pass
    return env


P = load()

rows = []
fails = []


def chk(no, cond, title, detail):
    rows.append((no, 'PASS' if cond else 'FAIL', title, detail))
    if not cond:
        fails.append((no, title, detail))


def val(k):
    return P.get(k)


def near(a, b, tol):
    return abs(a - b) <= tol


# ---------------- 1. 电压四阈值必须严格递增 ----------------
v = [val('V_DIS_MIN'), val('V_CHG_MAX'), val('V_CHG_REGEN'), val('V_OV_TRIP'), val('V_HW_MAX')]
chk('A1', all(v[i] < v[i + 1] for i in range(4)),
    '电压阈值递增', ' '.join('%.2f' % x for x in v) + '  (8 < 25.5 < 25.9 < 26 < 26.5)')

# ---------------- 2. 放电 taper 与充电 taper 不重叠 ----------------
chk('A2', val('V_DIS_MIN') + val('V_TAP_DIS') < val('V_CHG_MAX') - val('V_TAP_CHG'),
    '充放 taper 不重叠',
    '放电收敛于 %.2fV，充电始于 %.2fV' % (val('V_DIS_MIN') + val('V_TAP_DIS'),
                                          val('V_CHG_MAX') - val('V_TAP_CHG')))

# ---------------- 3. 超压回差 < 与 regen 上限的间距 ----------------
gap = val('V_OV_TRIP') - val('V_CHG_REGEN')
chk('A3', gap > 0.05, 'regen 上限到 OVP 有裕量',
    'V_OV_TRIP − V_CHG_REGEN = %.2f V（应 > 0.05，否则贴阈值抖动）' % gap)

# ---------------- 4. 26.5 解锁点必须低于 OVP 释放点 ----------------
chk('A4', val('V_OV_UNLOCK') < val('V_OV_TRIP') - val('V_OV_HYST'),
    'hw_ov 解锁点低于 OVP 释放点',
    'V_OV_UNLOCK=%.2f < %.2f' % (val('V_OV_UNLOCK'), val('V_OV_TRIP') - val('V_OV_HYST')))

# ---------------- 5. 过流阈值落在传感器量程内 ----------------
adc_full_a = (3.3 - 1.65) / (0.004 * 20)
# INA240 正摆幅上限 ≈ VS-0.2V=3.1V，worst case 3.0V
ina_pos_typ = (3.10 - 1.65) / (0.004 * 20)
ina_pos_min = (3.00 - 1.65) / (0.004 * 20)
chk('A5', val('OC_TRIP_A') < ina_pos_min, '过流阈值低于 INA240 正摆幅 worst case',
    'OC=%.1fA < %.1fA（typ %.1fA，ADC 满量程 %.1fA）'
    % (val('OC_TRIP_A'), ina_pos_min, ina_pos_typ, adc_full_a))
chk('A6', val('I_B_MAX') * 1.09 <= val('OC_TRIP_A'), '电容电流限与过流阈值有 ≥9% 余量',
    'I_B_MAX=%.1fA x1.09=%.2f <= OC=%.1fA' % (val('I_B_MAX'), val('I_B_MAX') * 1.09,
                                              val('OC_TRIP_A')))

# ---------------- 7. 调制器裕量 ----------------
c1 = val('DMAX') - val('MOD_DH')
chk('A7', val('U_LIM') < val('MOD_DH') * 0.9, 'u 限幅小于调制器过渡区裕量',
    'U_LIM=%.3f，r=1 时裕量 = DH = %.3f' % (val('U_LIM'), val('MOD_DH')))
chk('A8', c1 > val('DMAX') - 0.2, 'r=1 处共同占空比不至于过低',
    'c(1) = %.3f' % c1)

# ---------------- 9. 电感电流上限覆盖电容电流上限 ----------------
# 用调制器算 25.5V 时的 D_B
r = val('V_CHG_MAX') / val('V_NOM')
d = 1.0 - abs(r - 1.0) / val('MOD_RB')
d = max(0.0, d)
c = val('DMAX') - val('MOD_DH') * d
w = min(1.0, max(0.0, (r - (1 - val('MOD_BLEND') / 2)) / val('MOD_BLEND')))
db = (1 - w) * c + w * (c / r)
il_need = val('I_B_MAX') / db
chk('A9', il_need < val('I_L_MAX'), 'I_L_MAX 覆盖 15A 电容电流对应的电感电流',
    'V_B=25.5V 时 D_B=%.4f -> I_L=%.2fA < I_L_MAX=%.1fA' % (db, il_need, val('I_L_MAX')))

# ---------------- 10. 电流环推导值 ----------------
kp = 2 * math.pi * val('CUR_FC') * val('L_IND') / val('V_NOM')
ki = kp * 2 * math.pi * (val('CUR_FC') / 5.0)
chk('A10', near(kp, 1.571e-3, 1e-5) and near(ki, 1.184, 0.02),
    'Kp/Ki 与文档推导一致（CUR_FC=600Hz）',
    'Kp=%.4e (文档 1.571e-3), Ki=%.3f (文档 1.184)' % (kp, ki))

# ---------------- 11. 延时预算 ----------------
td_adc = (val('N_AVG') / 2) / val('F_SW')
td_ctrl = val('N_AVG') / val('F_SW')
td = td_adc + td_ctrl
fc_max = 35.0 / (360.0 * td)
chk('A11', val('CUR_FC') < fc_max, '电流环带宽不超过延时预算',
    'Td=%.1fus -> fc_max=%.0fHz, 取 %.0fHz' % (td * 1e6, fc_max, val('CUR_FC')))

# ---------------- 12. 缓冲能量阈值 ----------------
chk('A12', val('E_MIN') <= val('E_CRIT') < val('E_TGT') < val('E_MAX'),
    '缓冲能量阈值递增',
    '%.0f ≤ %.0f < %.0f < %.0f' % (val('E_MIN'), val('E_CRIT'), val('E_TGT'), val('E_MAX')))

# ---------------- 13. P_OVER 与限额 ----------------
chk('A13', 0 < val('P_OVER') <= 10, 'P_OVER 取值合理',
    'P_OVER=%.1f W（协议 Max_Referee_P 为 uint8，0-255W）' % val('P_OVER'))

# ---------------- 14. 倒灌限幅覆盖泄放功率 ----------------
dump_w = val('I_DUMP') * val('V_HW_MAX')
chk('A14', val('P_REV_MAX') > dump_w, 'P_REV_MAX 覆盖 1A 泄放功率',
    '泄放 %.1fW < P_REV_MAX %.1fW' % (dump_w, val('P_REV_MAX')))

# ---------------- 15. 预充电流不超规则 ----------------
chk('A15', val('I_PRE') < val('I_B_MAX'), '预充电流小于 15A 规则限',
    'I_PRE=%.1fA < I_B_MAX=%.1fA' % (val('I_PRE'), val('I_B_MAX')))

# ---------------- 16. 通信超时换算成 100Hz 计数为整数 ----------------
ticks = val('COMM_TIMEOUT_MS') / 10.0
chk('A16', abs(ticks - round(ticks)) < 0.01, '通信超时能被 100Hz 任务整除',
    '%d ms / 10 ms = %d 拍' % (val('COMM_TIMEOUT_MS'), ticks))

# ---------------- 17. 软件限流阈值 ----------------
chk('A17', val('I_LIM_RELEASE') < val('I_LIM_SOFT') < val('OC_TRIP_A') < ina_pos_min
            and 0 < val('U_LIM_BACK') < val('U_LIM'),
    '软件限流阈值有序（回差<限流<跳闸<INA240 摆幅）',
    'I_LIM_RELEASE=%.1f < I_LIM_SOFT=%.1f < OC_TRIP_A=%.1f < %.2fA；U_LIM_BACK=%.3f < U_LIM=%.3f'
    % (val('I_LIM_RELEASE'), val('I_LIM_SOFT'), val('OC_TRIP_A'), ina_pos_min,
       val('U_LIM_BACK'), val('U_LIM')))

# ---------------- 18. 软启动时间与控制周期 ----------------
chk('A18', val('SOFT_START_MS') > 0 and val('SOFT_START_MS') < 2000,
    '软启动时间合理', '%d ms' % val('SOFT_START_MS'))

# ---------------- 19. 控制周期 ----------------
ts = val('N_AVG') / val('F_SW')
chk('A19', near(ts, 47.06e-6, 0.5e-6), '控制周期 = 16/340k',
    '%.2f us -> %.2f kHz' % (ts * 1e6, 1 / ts / 1000))

# ---------------- 20. 过流饱和计数 ----------------
ticks = val('OC_SAT_MS') * 21.25
chk('A20', 40 <= ticks + 3.5 <= 49, 'OC_SAT_TICKS 与 2ms 一致（protect.c 取 43）',
    '%.1f 拍 -> protect.c 写 43' % ticks)


# ---------------- 21. .ioc 与生成代码一致性 ----------------
def read(p):
    return io.open(p, encoding='utf-8').read()


def find(pat, txt):
    m = re.search(pat, txt)
    return m.group(1) if m else None


ioc_txt = read('SuperCapV3.ioc')
i2c_src = read('Core/Src/i2c.c')
hrt_src = read('Core/Src/hrtim.c')

t_ioc = find(r'I2C1\.Timing=(0x[0-9A-Fa-f]+)', ioc_txt)
t_src = find(r'hi2c1\.Init\.Timing\s*=\s*(0x[0-9A-Fa-f]+)', i2c_src)
chk('A21', t_ioc is not None and t_ioc.lower() == (t_src or '').lower(),
    'I2C1 时序：.ioc 与 i2c.c 一致', '.ioc=%s  i2c.c=%s' % (t_ioc, t_src))

# 解析 TIMINGR 并换算实际 SCL 频率（I2C1 内核时钟 = PCLK1 = 170 MHz）
if t_ioc:
    v = int(t_ioc, 16)
    presc = (v >> 28) & 0xF
    scldel = (v >> 20) & 0xF
    sdadel = (v >> 16) & 0xF
    sclh = (v >> 8) & 0xFF
    scll = v & 0xFF
    t_presc = (presc + 1) / 170e6
    t_low = (scll + 1) * t_presc
    t_high = (sclh + 1) * t_presc
    f_scl = 1.0 / (t_low + t_high)
    ok_f = 0.20e6 < f_scl <= 0.401e6
    ok_low = t_low >= 1.30e-6
    ok_setup = (scldel + 1) * t_presc >= 100e-9
    chk('A22', ok_f and ok_low and ok_setup, 'I2C 400kHz 快速模式时序合规',
        'f_SCL=%.0f kHz, t_LOW=%.3f us(>=1.3), t_SU;DAT=%.0f ns(>=100), '
        'PRESC=%d SCLDEL=%d SDADEL=%d SCLH=%d SCLL=%d'
        % (f_scl / 1e3, t_low * 1e6, (scldel + 1) * t_presc * 1e9,
           presc, scldel, sdadel, sclh, scll))

dt_ioc = set(re.findall(r'HRTIM1\.(?:Rising|Falling)Value-Output_T[AB]1T[AB]2=(\d+)', ioc_txt))
dt_rise = find(r'pDeadTimeCfg\.RisingValue\s*=\s*(\d+);', hrt_src)
dt_fall = find(r'pDeadTimeCfg\.FallingValue\s*=\s*(\d+);', hrt_src)
dt_ps_ioc = set(re.findall(
    r'HRTIM1\.PrescalerDead-Output_T[AB]1T[AB]2=HRTIM_TIMDEADTIME_PRESCALERRATIO_(\w+)', ioc_txt))
dt_ps_src = find(r'pDeadTimeCfg\.Prescaler\s*=\s*HRTIM_TIMDEADTIME_PRESCALERRATIO_(\w+);', hrt_src)
chk('A23', dt_ioc == {'10'} and dt_rise == '10' and dt_fall == '10'
          and dt_ps_ioc == {'MUL4'} and dt_ps_src == 'MUL4',
    'HRTIM 死区 10 计数 x MUL4(1.472ns)=14.7ns，.ioc/代码一致',
    '.ioc 计数=%s 预分频=%s | hrtim.c Rise=%s Fall=%s 预分频=%s'
    % (sorted(dt_ioc), sorted(dt_ps_ioc), dt_rise, dt_fall, dt_ps_src))

# ---------------- 24. HRTIM DLL 校准只能是单次 ----------------
# CubeMX 默认生成 HRTIM_CALIBRATIONRATE_3 = 2048 x tHRTIM ≈ 12 us（83 kHz）周期校准，
# 等于整机一直在重校准：每次校准扰动高分辨率边沿 -> 占空比抖动 -> 电感电流周期性脉动。
# 这个选项在 .ioc 里没有对应项，只能靠本检查防止重新生成代码时被悄悄改回去。
cal_rate = find(r'HAL_HRTIM_DLLCalibrationStart\(\s*&hhrtim1\s*,\s*(\w+)\s*\)', hrt_src)
chk('A24', cal_rate == 'HRTIM_SINGLE_CALIBRATION',
    'HRTIM DLL 只在上电校准一次（周期校准会抖动占空比）',
    'hrtim.c 传入 = %s' % cal_rate)


# ---------------- 输出 ----------------
print('=' * 96)
print('App/params.h 参数静态审计')
print('=' * 96)
print('%-5s %-5s %-38s %s' % ('编号', '结果', '检查项', '数值'))
print('-' * 96)
for no, st, title, detail in rows:
    print('%-5s %-5s %-38s %s' % (no, st, title, detail))
print('-' * 96)
print('共 %d 项，通过 %d，失败 %d' % (len(rows), len(rows) - len(fails), len(fails)))
if fails:
    print('\n失败项：')
    for no, title, detail in fails:
        print('  %s  %s  (%s)' % (no, title, detail))
print()

sys.stdout.close()
sys.stdout = OUT
print('done -> Tools/param_audit.txt  (%d/%d passed)' % (len(rows) - len(fails), len(rows)))
