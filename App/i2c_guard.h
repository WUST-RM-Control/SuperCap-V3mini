/**
  ******************************************************************************
  * @file    i2c_guard.h
  * @brief   I2C1 总线守卫：让 OLED 热插拔不影响程序运行
  *
  *  背景：板上 SCL/SDA 没有上拉电阻（上拉在 OLED 模块上），屏一拔总线就悬空，
  *        I2C 外设看到 SCL 被拉低 -> BUSY 标志永远不清 -> 后续每次传输都白等超时。
  *
  *  三层防护：
  *    1) 打开 MCU 内部上拉：模块不在时总线保持空闲，而不是悬空
  *    2) 发送走 **DMA**（整页 129 字节 ≈2.9 ms，不阻塞主循环），
  *       每笔发送都有超时 + 连续失败计数，超限自动进入"离线"状态
  *    3) 离线后周期性探测，探测前先做 9 时钟总线恢复 + 外设复位，插回去能自动恢复
  ******************************************************************************
  */
#ifndef __I2C_GUARD_H
#define __I2C_GUARD_H

#include "main.h"
#include "params.h"

void    I2C_Guard_Init(void);                 /* 内部上拉 + DMA/I2C 中断注册，清统计 */
uint8_t I2C_Guard_Send(uint16_t addr, const uint8_t *buf, uint16_t len);  /* DMA 异步；0 = 已发起 */
uint8_t I2C_Guard_Is_Busy(void);              /* 1 = 上一笔还在传 */
void    I2C_Guard_Tick(void);                 /* 主循环反复调用：DMA 卡死时超时收尾 */
uint8_t I2C_Guard_Tx(uint16_t addr, const uint8_t *buf, uint16_t len);    /* 阻塞小命令；0 = OK */
uint8_t I2C_Guard_Probe(uint16_t addr);       /* 离线时尝试恢复；0 = OK */
uint8_t I2C_Guard_Is_Offline(void);
uint16_t I2C_Guard_FailCount(void);

#endif /* __I2C_GUARD_H */
