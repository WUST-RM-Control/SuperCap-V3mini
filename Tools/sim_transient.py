# -*- coding: utf-8 -*-
"""瞬态度量修正版：只看快速段 + 偏离能量，并分离"延时"与"增益"两个因素"""
import io
import sys

OUT = sys.stdout
sys.stdout = io.open('Tools/transient_report.txt', 'w', encoding='utf-8', newline='\n')


def P(*a):
    print(*a)


L_IND, F_SW = 10e-6, 340e3
V_A, V_B = 24.0, 12.0
DMAX, DMIN = 0.95, 0.05
MOD_DH, MOD_RB, MOD_BLEND = 0.10, 0.08, 0.10
U_LIM = 0.08
R_EFF = 9e-3          # 实测"环路增益高于模型"的等效值


def clamp(x, lo, hi):
    return lo if x < lo else (hi if x > hi else x)


def modulator(u):
    r = V_B / V_A
    d = max(0.0, 1.0 - abs(r - 1.0) / MOD_RB)
    c = DMAX - MOD_DH * d
    w = clamp((r - (1.0 - MOD_BLEND * 0.5)) / MOD_BLEND, 0.0, 1.0)
    D_A = (1 - w) * (c * r + u) + w * c
    D_B = (1 - w) * c + w * ((c - u) / r)
    return clamp(D_A, 0.012, DMAX), clamp(D_B, DMIN, DMAX)


class Cfg:
    def __init__(self, name, cur_fc, n_avg, lpf_cha=2000.0, vel_ff=False,
                 kp_p=0.0, trim_fc=5.0):
        self.name, self.cur_fc, self.n_avg = name, cur_fc, n_avg
        self.lpf_cha, self.vel_ff = lpf_cha, vel_ff
        self.kp_p, self.trim_fc = kp_p, trim_fc
        self.Ts = n_avg / F_SW
        kp = 6.2831853 * cur_fc * L_IND / 24.0
        self.KP, self.KI = kp, kp * 6.2831853 * (cur_fc / 5.0)

    def a_cha(self):
        return min(1.0, 6.2831853 * self.lpf_cha * self.Ts) if self.lpf_cha > 0 else 1.0


def run(cfg, p_pre=30.0, p_post=150.0, t_step=5e-3, t_end=350e-3, p_tgt=80.0):
    Ts = cfg.Ts
    dt = Ts / 20.0
    a_cha = cfg.a_cha()

    D_A, D_B = modulator(0.0)
    I_L = (p_tgt - p_pre) / (V_A * D_A)
    u = e_last = P_Cha_f = P_Ref_f = Trim = 0.0
    I_L_Ref_prev = 0.0
    h_PCha = h_PRef = 0.0
    h_I_L = I_L
    I_L_Ref = I_L

    t = 0.0
    rec = []
    while t < t_end:
        P_Cha = p_post if t >= t_step else p_pre
        P_A = V_A * D_A * I_L
        P_Ref = P_Cha + P_A

        P_Cha_f += a_cha * (h_PCha - P_Cha_f)
        P_Ref_f += min(1.0, 6.2831853 * 150.0 * Ts) * (h_PRef - P_Ref_f)
        P_A_ref = (p_tgt - P_Cha_f) + Trim + cfg.kp_p * (p_tgt - P_Ref_f)
        Trim = clamp(Trim + 6.2831853 * cfg.trim_fc * (p_tgt - P_Ref_f) * Ts, -15.0, 15.0)

        i_b_ref = P_A_ref / V_B
        lim = 15.0 * clamp((25.5 - V_B) / 1.5, 0.0, 1.0)
        I_L_Ref = clamp(clamp(i_b_ref, -lim, lim) / D_B, -18.0, 18.0)

        e = (I_L_Ref - h_PRef * 0.0 - 0.0)  # placeholder
        e = (I_L_Ref - h_I_L) * (24.0 / V_A)
        du = cfg.KP * (e - e_last) + cfg.KI * e * Ts
        e_last = e
        u = clamp(u + du, -U_LIM, U_LIM)

        u_cmd = u
        if cfg.vel_ff:
            u_cmd = clamp(u + L_IND * (I_L_Ref - I_L_Ref_prev) / Ts / V_A, -U_LIM, U_LIM)
        I_L_Ref_prev = I_L_Ref

        D_A, D_B = modulator(u_cmd)
        h_PCha, h_PRef, h_I_L = P_Cha, P_Ref, I_L

        for _ in range(20):
            I_L += (D_A * V_A - D_B * V_B - I_L * R_EFF) / L_IND * dt
            t += dt
        rec.append((t, P_Ref))
    return rec


def metrics(rec, p_step=5e-3, p_tgt=80.0):
    dP = abs(150.0 - 30.0)
    # 偏离能量（J）与首次回到 10% 的时间
    e = 0.0
    t10 = None
    prev_t = rec[0][0]
    for t, p in rec:
        if t > p_step:
            e += abs(p - p_tgt) * (t - prev_t)
            if t10 is None and abs(p - p_tgt) < 0.10 * dP:
                t10 = t - p_step
        prev_t = t
    # 等效跌落时长：三角近似 E = 0.5*dP*T -> T = 2E/dP
    return e, 2.0*e/dP


P('=' * 92)
P('P_Cha 阶跃 30W -> 150W (Δ=120W)，P_Tgt=80W，V_A=24V，V_B=12V(r=0.5)')
P('=' * 92)
P('%-34s %8s %8s %12s %12s' % ('配置', 'fc', 'Ts(us)', '偏离能量', '等效跌落时长'))
P('-' * 92)

cfgs = [
    Cfg('A 现状 (kp_p=0, Trim 5Hz)',        600.0, 16),
    Cfg('B kp_p=0.3',                       600.0, 16, kp_p=0.3),
    Cfg('C kp_p=0.7',                       600.0, 16, kp_p=0.7),
    Cfg('D kp_p=1.2',                       600.0, 16, kp_p=1.2),
    Cfg('E kp_p=2.0',                       600.0, 16, kp_p=2.0),
    Cfg('F kp_p=1.2 + 去前馈LPF',            600.0, 16, kp_p=1.2, lpf_cha=0.0),
    Cfg('G kp_p=1.2 + Trim 1.5Hz',          600.0, 16, kp_p=1.2, trim_fc=1.5),
]
base = None
for c in cfgs:
    e, t10 = metrics(run(c))
    if base is None:
        base = e
    P('%-34s %8.0f %8.1f %10.1f mJ %9.0f us  (x%.2f)'
      % (c.name, c.cur_fc, c.Ts * 1e6, e * 1e3, t10 * 1e6, e / base))

P('-' * 92)
P('说明：')
P('  * "偏离能量" = ∫|P_Ref - P_Tgt|dt，直接换算成缓冲能量损失，是最该看的指标')
P('  * G 行只是验证"光提增益"这条路：实机在 fc=1200 已经出 1kHz 极限环（周期 1ms、±2A）')
P('')
sys.stdout.close()
sys.stdout = OUT
print('done -> Tools/transient_report.txt')
