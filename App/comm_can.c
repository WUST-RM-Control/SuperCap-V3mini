/**
  ******************************************************************************
  * @file    comm_can.c
  * @brief   FDCAN2 通信
  ******************************************************************************
  */
#include "comm_can.h"
#include "ctrl_core.h"
#include "sample.h"
#include "protect.h"

static uint8_t Can_RxData[8];

/* ---- 总线自愈 ----
   TJA1044 的 VCC 是 5 V。低电压/上电顺序不当的时候 MCU 先跑了，
   总线上没有任何节点 ACK：
     - AutoRetransmission=ENABLE -> 同一帧无限重发 -> 3 格 TX 队列很快堵死；
     - TEC 涨过 256 -> bus-off，M_CAN 停在 INIT，**必须软件清 INIT 才能恢复**。
   所以这里周期性看 PSR / 队列，异常持续 CAN_TRIP_MS 就做一次 Stop+Start 复位。*/
#define CAN_TRIP_MS     100u    /* 连续异常这么久触发一次自愈 */
#define CAN_RETRY_MS    200u    /* 两次自愈的最小间隔，不在坏总线上狂刷 */

static volatile uint8_t Can_Online = 1u;   /* 0 = 总线异常，暂停排队 */
static uint8_t  Can_Bad;
static uint32_t Can_BadSince;
static uint32_t Can_LastTry;
static uint16_t Can_RecoverCnt;            /* 自愈次数（调试用） */

/**
  * @brief 把 FDCAN 从 bus-off / 错误态拉回来
  *        Stop() 置 CCCR.INIT（M_CAN 的 bus-off 恢复就是这个动作），
  *        Start() 再清 INIT；随后取消队列里残留的发送请求。
  */
static void Comm_Can_Restart(void)
{
    (void)HAL_FDCAN_Stop(&hfdcan2);
    hfdcan2.ErrorCode = HAL_FDCAN_ERROR_NONE;
    (void)HAL_FDCAN_Start(&hfdcan2);
    (void)HAL_FDCAN_AbortTxRequest(&hfdcan2,
                                   FDCAN_TX_BUFFER0 | FDCAN_TX_BUFFER1 | FDCAN_TX_BUFFER2);

    if (Can_RecoverCnt < 0xFFFFu) { Can_RecoverCnt++; }
}

void Comm_Can_Monitor(void)
{
    uint32_t psr = hfdcan2.Instance->PSR;
    uint8_t  bad = 0u;

    if ((psr & FDCAN_PSR_BO) != 0u)                        { bad = 1u; }  /* bus-off */
    else if ((psr & FDCAN_PSR_EP) != 0u)                   { bad = 1u; }  /* error passive */
    else if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan2) == 0u) { bad = 1u; }  /* 队列堵死 */

    if (bad)
    {
        if (!Can_Bad) { Can_Bad = 1u; Can_BadSince = HAL_GetTick(); }
        Can_Online = 0u;
    }
    else
    {
        Can_Bad    = 0u;
        Can_Online = 1u;
        hfdcan2.ErrorCode = HAL_FDCAN_ERROR_NONE;   /* 错误位是粘性的，正常时清掉 */
    }

    if (Can_Bad &&
        ((uint32_t)(HAL_GetTick() - Can_BadSince) >= CAN_TRIP_MS) &&
        ((uint32_t)(HAL_GetTick() - Can_LastTry)  >= CAN_RETRY_MS))
    {
        Can_LastTry = HAL_GetTick();
        Can_Bad     = 0u;          /* 下一轮重新评估 */
        Comm_Can_Restart();
    }
}

uint8_t Comm_Can_Is_Online(void)
{
    return Can_Online;
}

void Comm_Can_Init(void)
{
    FDCAN_FilterTypeDef sFilterConfig;

    /* 标准帧 0x000-0x7FF 全部进 FIFO0，远程帧拒收（与旧固件一致） */
    sFilterConfig.IdType       = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex  = 0;
    sFilterConfig.FilterType   = FDCAN_FILTER_RANGE;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1    = 0x000;
    sFilterConfig.FilterID2    = 0x7FF;
    (void)HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilterConfig);

    (void)HAL_FDCAN_ConfigGlobalFilter(&hfdcan2, FDCAN_REJECT, FDCAN_REJECT,
                                       FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);

    (void)HAL_FDCAN_ActivateNotification(&hfdcan2, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    (void)HAL_FDCAN_Start(&hfdcan2);
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    FDCAN_RxHeaderTypeDef hdr;

    (void)RxFifo0ITs;

    if (hfdcan != &hfdcan2) { return; }
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &hdr, Can_RxData) != HAL_OK) { return; }

    if (hdr.Identifier == CAP_CAN_RX_ID)
    {
        Ctrl_Apply_ControlFrame(Can_RxData);
    }
}

void Comm_Can_Send_Feedback(void)
{
    /* 与旧固件逐字节一致：State | V_Cap*100 (LE) | P_Ref*10 | P_Cha*10，共 7 字节 + 1 字节填充 */
    uint8_t  buf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint16_t v, pr, pc;
    FDCAN_TxHeaderTypeDef hdr;

    /* ---- 队列保护 ----
       总线异常（典型：5V 还没上电、没人 ACK）时不要再往队列里塞帧：
       塞满后每笔都是 HAL_ERROR，而且会一直占着队列不放。 */
    if (!Can_Online)  { return; }
    if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan2) == 0u) { return; }

    v  = (uint16_t)(Smp.V_B * 100.0f);
    pr = (Smp.P_Ref > 0.0f) ? (uint16_t)(Smp.P_Ref * 10.0f) : 0u;
    pc = (Smp.P_Cha > 0.0f) ? (uint16_t)(Smp.P_Cha * 10.0f) : 0u;

    buf[0] = Ctrl.Enable_Flag;           /* 与旧固件一致：0/1 表示是否使能 */
    buf[1] = (uint8_t)(v  & 0xFFu);
    buf[2] = (uint8_t)(v  >> 8);
    buf[3] = (uint8_t)(pr & 0xFFu);
    buf[4] = (uint8_t)(pr >> 8);
    buf[5] = (uint8_t)(pc & 0xFFu);
    buf[6] = (uint8_t)(pc >> 8);
    /* buf[7] 保持 0 填充 */

    hdr.Identifier          = CAP_CAN_TX_ID;
    hdr.IdType              = FDCAN_STANDARD_ID;
    hdr.TxFrameType         = FDCAN_DATA_FRAME;
    hdr.DataLength          = FDCAN_DLC_BYTES_8;   /* 与旧固件一致 */
    hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    hdr.BitRateSwitch       = FDCAN_BRS_OFF;
    hdr.FDFormat            = FDCAN_CLASSIC_CAN;
    hdr.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    hdr.MessageMarker       = 0;

    (void)HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &hdr, buf);
}
