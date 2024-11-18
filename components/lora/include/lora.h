
#ifndef __LORA_H__
#define __LORA_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define CHANNEL_0   915200000
#define CHANNEL_1   915400000
#define CHANNEL_2   915600000
#define CHANNEL_3   915800000
#define CHANNEL_4   916000000
#define CHANNEL_5   916200000
#define CHANNEL_6   916400000
#define CHANNEL_7   916600000
#define CHANNEL_8   916800000
#define CHANNEL_9   917000000
#define CHANNEL_10   917200000
#define CHANNEL_11   917400000
#define CHANNEL_12   917600000
#define CHANNEL_13   917800000
#define CHANNEL_14   918000000
#define CHANNEL_15   918200000
#define CHANNEL_16   918400000

void lora_reset(void);
void lora_explicit_header_mode(void);
void lora_implicit_header_mode(int size);
void lora_idle(void);
void lora_sleep(void); 
void lora_receive(void);
void lora_set_tx_power( int8_t power, bool pa_boost);
void lora_set_frequency(uint32_t frequency);
void lora_set_spreading_factor(int sf);
void lora_set_bandwidth(long sbw);
void lora_set_coding_rate(int denominator);
void lora_set_preamble_length(long length);
void lora_set_sync_word(int sw);
void lora_enable_crc(void);
void lora_disable_crc(void);
int lora_init(uint8_t datarate, int coding_rate, long frequency, int8_t power_level, bool pa_boost, bool enable_crc, bool enable_explicit_header);
uint32_t lora_cca(void);
uint32_t lora_send_frame(uint8_t *buf, int size, uint32_t timeout);
int lora_read_frame(uint8_t *buf, int size);
int lora_read_frame_size(void);
int lora_received(uint32_t timeout);
SemaphoreHandle_t lora_get_received_sem(void);
int lora_frame_rssi(void);
float lora_frame_snr(void);
void lora_close(void);
int lora_initialized(void);
void lora_dump_registers(void);
void lora_enable_irq( void );
void lora_disable_irq( void );
uint8_t get_lora_dr(void);

#endif
