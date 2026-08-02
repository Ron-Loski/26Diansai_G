#ifndef __COMMAND_H_
#define __COMMAND_H_                    /* 命令模块头文件重复包含保护宏。 */

#include "g_Inc.h"

typedef enum
{
	Command_None = 0,  /**< 当前没有待执行命令。 */
	Ques1             /**< 执行4096点FFT分析并逐行输出ADC原始采样值。 */
} Commandtypdef;

/**
 * @brief 初始化USART3命令接收状态并启动Receive-to-Idle DMA接收。
 * @note 无输入参数。
 * @return 无；接收缓冲区和命令行状态会被清零。
 */
void USART3_CommandRx_Init(void);

/**
 * @brief 处理一次USART3空闲接收事件，并重新启动DMA接收。
 * @param[in] Size 本次接收到的有效字节数。
 * @return 无；解析结果写入命令模块的内部状态。
 */
void USART3_CommandRx_Event(uint16_t Size);

/**
 * @brief 判断一行文本对应的命令类型。
 * @param[in] Line 以空字符结尾的命令字符串。
 * @return 无；识别结果写入命令模块的内部状态。
 */
void Command_Judege(char *Line);

/**
 * @brief 执行当前待处理命令。
 * @note 无输入参数。
 * @return 无；执行完成后清除内部命令状态。
 */
void Command_Execute(void);

uint8_t Command_TakeWaveDumpRequest(void);

/**
 * @brief 命令学习功能预留接口。
 * @note 无输入参数。
 * @return 无；当前实现不执行操作。
 */
void Command_Study(void);

#endif
