/**
  ******************************************************************************
  * @file    params.h
  * @brief   全部可调参数的唯一来源。数值均由硬件参数推导，见
  *          Doc/开发文档-控制软件重写.md §9，验证脚本 Tools/sim_ctrl.py
  ******************************************************************************
  */
#ifndef __PARAMS_H
#define __PARAMS_H

/* ============================ 硬件常量 ============================ */
#define L_IND           10e-6f      /* L2 XAL1510-103MED 10uH */
#define R_DS_ON         6.6e-3f     /* BSC066N06NS @Vgs=10V,25C；热 8.9m */
#define DCR_IND         5.0e-3f     /* XAL1510 典型 DCR */
#define R_BRIDGE        (2.0f*R_DS_ON + DCR_IND)   /* 桥内串联 18.2 mΩ */
#define R_SHUNT         4e-3f       /* R6/R77/R79 */
#define C_BUS           500e-6f     /* C26 470u + 3x10u */
#define V_NOM           24.0f
#define F_SW            340000.0f

/* ============================ 采样 ============================ */
#define N_AVG           16                       /* ADC 块平均点数 */
#define TS_CTRL         ((float)N_AVG / F_SW)    /* 47.06 us -> 21.25 kHz */
#define ADC_V_GAIN      11.0f                    /* (100k+10k)/10k 分压 */
#define ADC_VREF        3.3f
#define ADC_FULL        4096.0f
#define I_SENS_GAIN     12.5f                    /* 1/(20*4mΩ) = 12.5 A/V */
#define I_SENS_ZERO     1.65f                    /* REF3033/2 */

/* ADC 通道索引（与 DMA 缓冲顺序一致） */
#define CH_VA           0
#define CH_IA           1
#define CH_IREF         2
#define CH_IB           3
#define CH_VB           4
#define CH_NUM          5

/* ============================ 调制器 ============================ */
#define DMAX            0.95f       /* 自举刷新窗口 147ns */
#define DMIN            0.05f
#define DMIN_ABS        0.012f      /* D_A 的**绝对**下限（192 计数 ≈ 564ns 导通）。
                                       两个约束同时决定它：
                                       1) 深放电时前馈 c*r→0，若用 DMIN=0.05 钳位，
                                          V_L 会被顶成 +0.05*V_A ≈ 1.3V 且 u 再也调不回来
                                          （钳位后 V_L=u*V_A 不成立）——实测直接冲过流；
                                       2) ADC 触发点 CMP4 = CMP3/2 必须 ≥ 96 计数
                                          （HRTIM 硬性要求，越界则比较事件不产生，
                                          ADC 静默停转、控制节拍消失），
                                          故 CMP3 ≥ 192 -> D_A ≥ 192/16000 = 0.012。
                                       注意：代价是 V_B≈0 时电感电压最小 ≈0.012*V_A。 */
#define MOD_DH          0.10f       /* 过渡区共同占空比下压量 */
#define MOD_RB          0.08f
#define MOD_BLEND       0.10f

/* ---- OLED 显示低通（只影响显示，不参与控制）---- */
#define UI_LP_HZ         10.0f      /* Hz：屏上所有数值的一阶低通截止频率。
                                       瞬时值随开关动作/环路抖动很大（实测 I_Ref
                                       同一秒内在 0.18~2.0A 来回跳，而电源读数稳定
                                       15W），直接显示原始值跳得看不清 */
#define KP_UI            (6.2831853f * UI_LP_HZ / 1000.0f)  /* 在 1 kHz 任务里算，
                                       而非 21.25 kHz 中断：12 个变量的代价降到 1/21，
                                       对显示精度完全够用 */
#define UI_PBAR_MAX      60.0f      /* W：功率条的满量程（±对称，中点=0） */

/* ============================ 电流环 ============================ */
#define CUR_FC          600.0f      /* Hz。设计值本来是 1200（sim 算 PM=62°），但实测
                                       电流环在 ~1 kHz 处极限振荡：示波器量 RC 后的
                                       REF_I，1.65V→1.874V、周期 1ms 来回摆（≈±2A）。
                                       原因：Smp.I_L 是“固定相位采样 + 除占空比”重构的，
                                       有效增益随 D 变化，实际环路增益比模型高。
                                       设计带宽砍半留余量 -> 实际穿越仍约 1.2kHz。 */
#define KP_CUR          (6.2831853f * CUR_FC * L_IND / V_NOM)
#define KI_CUR          (KP_CUR * 6.2831853f * (CUR_FC / 5.0f))
#define U_LIM           0.08f       /* 占空比增量限幅 */
#define I_L_MAX         18.0f

/* ============================ 功率环 ============================ */
#define LPF_CHA_HZ      2000.0f
#define LPF_REF_HZ      150.0f      /* 误差通道。**不要为了滤抖动而调低**：
                                       sim_ctrl.py 实测 20Hz 会把“恒功率段能量误差”
                                       从 0.6% 恶化到 1.9%、功率偏差 0.4%→4.5%，
                                       因为修正积分需要这个带宽。
                                       P_Ref 的抖动应该从采样侧（F18 的 CMP4）解决。 */
#define TRIM_FC         5.0f        /* Hz：修正积分带宽（sim_ctrl.py 扫描最优） */
#define KI_PWR          (6.2831853f * TRIM_FC)
#define TRIM_MAX        15.0f       /* W：修正权限（sim 扫描最优；实测平均残差 ~0.5W） */
#define P_REV_MAX       40.0f       /* 允许倒灌电管的最大功率 */

/* ============================ 缓冲环 ============================ */
#define E_TGT           55.0f       /* 期望缓冲能量 J */
#define E_CRIT          10.0f       /* 低于此值强制最小取电 */
#define E_MIN           0.0f
#define E_MAX           60.0f
#define KP_E            1.0f        /* W/J -> τ=1s */
#define P_OVER          5.0f        /* 允许超限额，换"取满" */
#define LPF_E_HZ        0.5f

/* ============================ 电容电压/电流 ============================ */
#define I_B_MAX         15.0f       /* RM 规则，充放对称 */
#define V_CHG_MAX       25.5f       /* 正常充电调节点 */
#define V_TAP_CHG       1.5f
#define V_CHG_REGEN     25.9f       /* 动能回收放宽点 */
#define V_TAP_REGEN     0.4f
#define V_DIS_MIN       8.0f
#define V_TAP_DIS       1.5f
#define V_OV_TRIP       26.0f
#define V_OV_HYST       0.2f
#define V_HW_MAX        26.5f
#define V_HW_CUTOFF     26.8f       /* 最后防线：到这里还降不下来就关断 PWM（正常 26.5 只锁存+泄放） */
#define V_OV_UNLOCK     25.5f       /* 26.5 故障泄放到此电压自动解锁 */
#define I_DUMP          1.0f        /* 主动泄放（反冲电管/底盘） */
#define I_PRE           8.0f        /* 深放电恒流预充 */

/* ============================ 保护 ============================ */
#define SAFE_V_REF_MAX  32.0f
#define SAFE_V_REF_CLR  31.0f
#define OV_DEB_MS       10          /* 母线过压防抖：V_A/V_B 越限必须连续这么久才认。
                                       不加会怎样（实测）：上电头一秒 V_B 通道会读到
                                       26.8V 以上三次（间隔约 300ms），而母线电容 100uF
                                       物理上不可能在 1 拍(47us)内跳 24V —— 那是采样
                                       毛刺；四次毛刺就够把 HardLock(FLT_LOCK) 打上，
                                       整块板永久停在 F80、半桥不开。 */
#define OV_FAST_TICKS    43u        /* 2ms @21.25kHz：L1 快速判据的防抖拍数 */
#define CHASSIS_OFF_V    18.0f
#define CHASSIS_ON_V     19.0f
#define OC_TRIP_A        16.5f      /* 过流动作点。受 INA240 正向摆幅限制：
                                       Vout_max ≈ VS-0.2 = 3.1V -> (3.1-1.65)/0.08 = 18.1A
                                       取 16.5A(=2.97V) 才有 330mV 可读裕量，
                                       同时对 15A 运行限留 10% 不误动 */
#define OC_ADC_SAT       3700       /* ≈16.9A：接近 INA240 正摆幅上限(=3.0V 码值)。
                                       软件限流的后备判据，需持续 2ms */
#define OC_SAT_MS        2

/* ---- 软件限流（取代 ADC 模拟看门狗，21.25 kHz 每拍判一次） ---- */
#define I_LIM_SOFT       15.0f      /* A：|I_A|/|I_B|/|I_L| 任一越限即进入压制。
                                       对应 RM 的 15A 电容限流，留 1.5A 到跳闸 */
#define I_LIM_RELEASE    14.0f      /* A：退出压制的回差，防止在阈值上抖动 */
#define U_LIM_BACK       0.02f      /* 限流时每拍强制回退的 u 步长（425/s） */
#define OC_TRIP_TICKS    4          /* 连续 N 拍(4x47us=188us)越 OC_TRIP_A 才判过流 */

/* ---- 控制节拍自愈（ADC1/DMA1_Channel1 停摆）---- */
#define TICK_RECOVER_MS  20         /* 节拍丢失后每隔 N ms 重新武装一次 ADC1，
                                       并把 ADC 触发点（CMP4）重写一遍 */

#define COMM_TIMEOUT_MS  500
#define SOFT_START_MS    300
#define FAULT_RETRY_WIN_MS 10000
#define FAULT_RETRY_MAX  3          /* 窗口内跳闸 N 次 -> 硬锁，只能断电复位 */
#define FAULT_COOL_MS    200        /* 故障消失后至少静默 N ms 才允许重进 SOFT_START。
                                       否则一次误判就是“跳闸-重启-跳闸”无限循环，
                                       每次重启都往功率级灌一次冲击（实测会持续
                                       反复重启）。 */

/* ============================ 模式 ============================ */
#define CFG_RESET_ON_COMM_LOSS  0   /* 1 = 还原旧版"禁用 1s 自复位" */
#define CFG_IWDG_ENABLE         1   /* 1 = 独立看门狗（在 1 kHz 中断里喂，主循环卡住不复位） */
#define CFG_DEBUG_UART          0   /* 1 = 10 Hz 串口打印工程量（P1 验收/调试用）；上车/正常使用保持 0 */
#define CFG_DEBUG_PERIOD_MS     100 /* 打印周期 */

/* ============================ OLED / I2C 守卫 ============================ */
#define OLED_I2C_ADDR    0x78       /* 7bit 0x3C 左移一位 */
#define I2C_TIMEOUT_MS       2u     /* 单次传输的**基础**超时；实际按字节数追加（见 i2c_guard.c） */
#define I2C_BYTE_US          40u    /* 每个字节的传输预算 @400kHz（9bit/400k = 22.5us，留余量） */
#define I2C_FAIL_MAX         3u     /* 连续失败多少次判离线 */
#define OLED_PROBE_MS    2000u      /* 离线后多久试一次恢复 */

/* ============================ 协议 ============================ */
#define CAP_CAN_RX_ID    0x30
#define CAP_CAN_TX_ID    0x31

/* ============================ 工具宏 ============================ */
#define CLAMP(x, lo, hi)  (((x) < (lo)) ? (lo) : (((x) > (hi)) ? (hi) : (x)))

#endif /* __PARAMS_H */
