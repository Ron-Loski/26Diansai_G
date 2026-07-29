#ifndef __JUSTFLOAT_H_
#define __JUSTFLOAT_H_                  /* JustFloat头文件重复包含保护宏。 */

#include "g_Inc.h"

/**
 * @brief 将浮点数组作为一帧VOFA+ JustFloat数据通过USART3发送。
 * @param[in] Data 待发送的浮点通道数组。
 * @param[in] Channel_Count Data中的通道数量，允许范围为1至22。
 * @return 无；输入无效时不发送数据。
 */
void VOFA_JustFloat_SendArray(const float *Data, uint16_t Channel_Count);

/**
 * @brief 通过USART3发送1通道VOFA+ JustFloat数据帧。
 * @param[in] ch0 第1通道浮点数据。
 * @return 无；数据通过USART3输出。
 */
void VOFA_JustFloat_Send1(float ch0);

/**
 * @brief 通过USART3发送2通道VOFA+ JustFloat数据帧。
 * @param[in] ch0 第1通道浮点数据。
 * @param[in] ch1 第2通道浮点数据。
 * @return 无；数据通过USART3输出。
 */
void VOFA_JustFloat_Send2(float ch0, float ch1);

/**
 * @brief 通过USART3发送3通道VOFA+ JustFloat数据帧。
 * @param[in] ch0 第1通道浮点数据。
 * @param[in] ch1 第2通道浮点数据。
 * @param[in] ch2 第3通道浮点数据。
 * @return 无；数据通过USART3输出。
 */
void VOFA_JustFloat_Send3(float ch0, float ch1, float ch2);

/**
 * @brief 通过USART3发送4通道VOFA+ JustFloat数据帧。
 * @param[in] ch0 第1通道浮点数据。
 * @param[in] ch1 第2通道浮点数据。
 * @param[in] ch2 第3通道浮点数据。
 * @param[in] ch3 第4通道浮点数据。
 * @return 无；数据通过USART3输出。
 */
void VOFA_JustFloat_Send4(float ch0, float ch1, float ch2, float ch3);

/**
 * @brief 通过USART3发送5通道VOFA+ JustFloat数据帧。
 * @param[in] ch0 第1通道浮点数据。
 * @param[in] ch1 第2通道浮点数据。
 * @param[in] ch2 第3通道浮点数据。
 * @param[in] ch3 第4通道浮点数据。
 * @param[in] ch4 第5通道浮点数据。
 * @return 无；数据通过USART3输出。
 */
void VOFA_JustFloat_Send5(float ch0, float ch1, float ch2, float ch3, float ch4);

/**
 * @brief 通过USART3发送6通道VOFA+ JustFloat数据帧。
 * @param[in] ch0 第1通道浮点数据。
 * @param[in] ch1 第2通道浮点数据。
 * @param[in] ch2 第3通道浮点数据。
 * @param[in] ch3 第4通道浮点数据。
 * @param[in] ch4 第5通道浮点数据。
 * @param[in] ch5 第6通道浮点数据。
 * @return 无；数据通过USART3输出。
 */
void VOFA_JustFloat_Send6(float ch0, float ch1, float ch2, float ch3, float ch4, float ch5);

/**
 * @brief 通过USART3发送7通道VOFA+ JustFloat数据帧。
 * @param[in] ch0 第1通道浮点数据。
 * @param[in] ch1 第2通道浮点数据。
 * @param[in] ch2 第3通道浮点数据。
 * @param[in] ch3 第4通道浮点数据。
 * @param[in] ch4 第5通道浮点数据。
 * @param[in] ch5 第6通道浮点数据。
 * @param[in] ch6 第7通道浮点数据。
 * @return 无；数据通过USART3输出。
 */
void VOFA_JustFloat_Send7(float ch0, float ch1, float ch2, float ch3, float ch4, float ch5, float ch6);

/**
 * @brief 通过USART3发送8通道VOFA+ JustFloat数据帧。
 * @param[in] ch0 第1通道浮点数据。
 * @param[in] ch1 第2通道浮点数据。
 * @param[in] ch2 第3通道浮点数据。
 * @param[in] ch3 第4通道浮点数据。
 * @param[in] ch4 第5通道浮点数据。
 * @param[in] ch5 第6通道浮点数据。
 * @param[in] ch6 第7通道浮点数据。
 * @param[in] ch7 第8通道浮点数据。
 * @return 无；数据通过USART3输出。
 */
void VOFA_JustFloat_Send8(float ch0, float ch1, float ch2, float ch3, float ch4, float ch5, float ch6, float ch7);

/**
 * @brief 通过USART3发送9通道VOFA+ JustFloat数据帧。
 * @param[in] ch0 第1通道浮点数据。
 * @param[in] ch1 第2通道浮点数据。
 * @param[in] ch2 第3通道浮点数据。
 * @param[in] ch3 第4通道浮点数据。
 * @param[in] ch4 第5通道浮点数据。
 * @param[in] ch5 第6通道浮点数据。
 * @param[in] ch6 第7通道浮点数据。
 * @param[in] ch7 第8通道浮点数据。
 * @param[in] ch8 第9通道浮点数据。
 * @return 无；数据通过USART3输出。
 */
void VOFA_JustFloat_Send9(float ch0, float ch1, float ch2, float ch3, float ch4, float ch5, float ch6, float ch7, float ch8);

/**
 * @brief 通过USART3发送10通道VOFA+ JustFloat数据帧。
 * @param[in] ch0 第1通道浮点数据。
 * @param[in] ch1 第2通道浮点数据。
 * @param[in] ch2 第3通道浮点数据。
 * @param[in] ch3 第4通道浮点数据。
 * @param[in] ch4 第5通道浮点数据。
 * @param[in] ch5 第6通道浮点数据。
 * @param[in] ch6 第7通道浮点数据。
 * @param[in] ch7 第8通道浮点数据。
 * @param[in] ch8 第9通道浮点数据。
 * @param[in] ch9 第10通道浮点数据。
 * @return 无；数据通过USART3输出。
 */
void VOFA_JustFloat_Send10(float ch0, float ch1, float ch2, float ch3, float ch4, float ch5, float ch6, float ch7, float ch8, float ch9);

#endif
