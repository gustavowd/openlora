
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include <string.h>

#include <esp_log.h>
#include "lora.h"

//#define CONFIG_CS_GPIO 18
#define CONFIG_RST_GPIO 23 //14   //TTGO_BEAM usa reset no pino 23 // outros módulos no pino 14
//#define CONFIG_MISO_GPIO 19
//#define CONFIG_MOSI_GPIO 27
//#define CONFIG_SCK_GPIO 5

/*
 * Register definitions
 */
#define REG_FIFO                       0x00
#define REG_OP_MODE                    0x01
#define REG_FRF_MSB                    0x06
#define REG_FRF_MID                    0x07
#define REG_FRF_LSB                    0x08
#define REG_PA_CONFIG                  0x09
#define REG_LNA                        0x0c
#define REG_FIFO_ADDR_PTR              0x0d
#define REG_FIFO_TX_BASE_ADDR          0x0e
#define REG_FIFO_RX_BASE_ADDR          0x0f
#define REG_FIFO_RX_CURRENT_ADDR       0x10
#define REG_IRQ_FLAGS_MASK             0x11
#define REG_IRQ_FLAGS                  0x12
#define REG_RX_NB_BYTES                0x13
#define REG_PKT_SNR_VALUE              0x19
#define REG_PKT_RSSI_VALUE             0x1a
#define REG_MODEM_CONFIG_1             0x1d
#define REG_MODEM_CONFIG_2             0x1e
#define REG_PREAMBLE_MSB               0x20
#define REG_PREAMBLE_LSB               0x21
#define REG_PAYLOAD_LENGTH             0x22
#define REG_MAX_PAYLOAD_LENGHT         0x23
#define REG_MODEM_CONFIG_3             0x26
#define REG_RSSI_WIDEBAND              0x2c
#define REG_LR_IFFREQ1                 0x2F
#define REG_LR_IFFREQ2                 0x30
#define REG_DETECTION_OPTIMIZE         0x31
#define REG_INVERT_IQ                  0x33
#define REG_HIGH_BW_OPTIMIZE_1         0x36
#define REG_DETECTION_THRESHOLD        0x37
#define REG_SYNC_WORD                  0x39
#define REG_HIGH_BW_OPTIMIZE_2         0x3A
#define REG_INVERT_IQ2                 0x3B
#define REG_IMAGECAL                   0x3B
#define REG_DIO_MAPPING_1              0x40
#define REG_DIO_MAPPING_2              0x41
#define REG_VERSION                    0x42
#define REG_PA_DAC                     0x4d

/*
 * Transceiver modes
 */
#define MODE_LONG_RANGE_MODE           0x80
#define MODE_SLEEP                     0x00
#define MODE_STDBY                     0x01
#define MODE_TX                        0x03
#define MODE_RX_CONTINUOUS             0x05
#define MODE_RX_SINGLE                 0x06
#define LORA_MODE_CAD                  0x07

/*
 * PA configuration
 */
#define PA_BOOST                       0x80

/*
 * IRQ masks
 */
#define IRQ_RX_TIMEOUT_MASK            0x80
#define IRQ_RX_DONE_MASK               0x40
#define IRQ_PAYLOAD_CRC_ERROR_MASK     0x20
#define IRQ_TX_DONE_MASK               0x08
#define IRQ_CAD_DONE_MASK              0x04
#define IRQ_CAD_DETECTED_MASK          0x01

#define PA_OUTPUT_RFO_PIN              0
#define PA_OUTPUT_PA_BOOST_PIN         1

#define TIMEOUT_RESET                  100

#define XTAL_FREQ                                   32000000
#define FREQ_STEP                                   61.03515625

/*!
 * RegImageCal
 */
#define RF_IMAGECAL_IMAGECAL_MASK                   0xBF
#define RF_IMAGECAL_IMAGECAL_START                  0x40

#define RF_IMAGECAL_IMAGECAL_RUNNING                0x20
#define RF_IMAGECAL_IMAGECAL_DONE                   0x00  // Default


/*!
 * RegInvertIQ
 */
#define RFLR_INVERTIQ_RX_MASK                       0xBF
#define RFLR_INVERTIQ_RX_OFF                        0x00
#define RFLR_INVERTIQ_RX_ON                         0x40
#define RFLR_INVERTIQ_TX_MASK                       0xFE
#define RFLR_INVERTIQ_TX_OFF                        0x01
#define RFLR_INVERTIQ_TX_ON                         0x00

/*!
 * RegInvertIQ2
 */
#define RFLR_INVERTIQ2_ON                           0x19
#define RFLR_INVERTIQ2_OFF                          0x1D

/*!
 * RegPaConfig
 */
#define RF_PACONFIG_PASELECT_MASK                   0x7F
#define RF_PACONFIG_PASELECT_PABOOST                0x80
#define RF_PACONFIG_PASELECT_RFO                    0x00 // Default

#define RF_PACONFIG_MAX_POWER_MASK                  0x8F

#define RF_PACONFIG_OUTPUTPOWER_MASK                0xF0


/*!
 * RegPaDac
 */
#define RF_PADAC_20DBM_MASK                         0xF8
#define RF_PADAC_20DBM_ON                           0x07
#define RF_PADAC_20DBM_OFF                          0x04  // Default

/*!
 * RegIrqFlags
 */
#define RFLR_IRQFLAGS_RXTIMEOUT                     0x80
#define RFLR_IRQFLAGS_RXDONE                        0x40
#define RFLR_IRQFLAGS_PAYLOADCRCERROR               0x20
#define RFLR_IRQFLAGS_VALIDHEADER                   0x10
#define RFLR_IRQFLAGS_TXDONE                        0x08
#define RFLR_IRQFLAGS_CADDONE                       0x04
#define RFLR_IRQFLAGS_FHSSCHANGEDCHANNEL            0x02
#define RFLR_IRQFLAGS_CADDETECTED                   0x01

/*!
 * RegDioMapping1
 */
#define RFLR_DIOMAPPING1_DIO0_MASK                  0x3F
#define RFLR_DIOMAPPING1_DIO0_00                    0x00  // Default
#define RFLR_DIOMAPPING1_DIO0_01                    0x40
#define RFLR_DIOMAPPING1_DIO0_10                    0x80
#define RFLR_DIOMAPPING1_DIO0_11                    0xC0

static spi_device_handle_t __spi;

static int __implicit;
static long __frequency;

SemaphoreHandle_t lora_sem_irq;
SemaphoreHandle_t lora_sem_tx;
QueueHandle_t     lora_queue_cad;
SemaphoreHandle_t lora_sem_rx;
SemaphoreHandle_t lora_mutex;

static uint8_t lora_datarate = 0xFF;

void lora_enable_irq( void );
void lora_disable_irq( void );

static const char *LORA_TAG = "LoRa";
#define VERBOSE 1

uint8_t get_lora_dr(void) {
   return lora_datarate;
}

/**
 * Write a value to a register.
 * @param reg Register index.
 * @param val Value to write.
 */
void 
lora_write_reg(int reg, int val)
{
   uint8_t out[2] = { 0x80 | reg, val };
   uint8_t in[2];

   spi_transaction_t t = {
      .flags = 0,
      .length = 8 * sizeof(out),
      .tx_buffer = out,
      .rx_buffer = in  
   };
   //ESP_LOGI(LORA_TAG, "addr: 0x%x - data: 0x%x", reg, val);

   if (xSemaphoreTake(lora_mutex, portMAX_DELAY) == pdTRUE) {
      gpio_set_level(CONFIG_CS_GPIO, 0);
      spi_device_transmit(__spi, &t);
      gpio_set_level(CONFIG_CS_GPIO, 1);
      xSemaphoreGive(lora_mutex);
   }
}

/**
 * Read the current value of a register.
 * @param reg Register index.
 * @return Value of the register.
 */
int
lora_read_reg(int reg)
{
   uint8_t out[2] = { reg, 0xff };
   uint8_t in[2];

   spi_transaction_t t = {
      .flags = 0,
      .length = 8 * sizeof(out),
      .tx_buffer = out,
      .rx_buffer = in
   };

   if (xSemaphoreTake(lora_mutex, portMAX_DELAY) == pdTRUE) {
      gpio_set_level(CONFIG_CS_GPIO, 0);
      spi_device_transmit(__spi, &t);
      gpio_set_level(CONFIG_CS_GPIO, 1);
      xSemaphoreGive(lora_mutex);
   }
   return in[1];
}

/**
 * Perform physical reset on the Lora chip
 */
void 
lora_reset(void)
{
   gpio_set_level(CONFIG_RST_GPIO, 0);
   vTaskDelay(pdMS_TO_TICKS(1));
   gpio_set_level(CONFIG_RST_GPIO, 1);
   vTaskDelay(pdMS_TO_TICKS(10));
}

/**
 * Configure explicit header mode.
 * frame size will be included in the frame.
 */
void 
lora_explicit_header_mode(void)
{
   __implicit = 0;
   lora_write_reg(REG_MODEM_CONFIG_1, lora_read_reg(REG_MODEM_CONFIG_1) & 0xfe);
}

/**
 * Configure implicit header mode.
 * All frames will have a predefined size.
 * @param size Size of the frames.
 */
void 
lora_implicit_header_mode(int size)
{
   __implicit = 1;
   lora_write_reg(REG_MODEM_CONFIG_1, lora_read_reg(REG_MODEM_CONFIG_1) | 0x01);
   lora_write_reg(REG_PAYLOAD_LENGTH, size);
}

/**
 * Sets the radio transceiver in idle mode.
 * Must be used to change registers and access the FIFO.
 */
void 
lora_idle(void)
{
   lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

/**
 * Sets the radio transceiver in sleep mode.
 * Low power consumption and FIFO is lost.
 */
void 
lora_sleep(void)
{ 
   lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

/**
 * Sets the radio transceiver in receive mode.
 * Incoming frames will be received.
 */
void 
lora_receive(void)
{
   lora_write_reg( REG_INVERT_IQ, ( ( lora_read_reg( REG_INVERT_IQ ) & RFLR_INVERTIQ_TX_MASK & RFLR_INVERTIQ_RX_MASK ) | RFLR_INVERTIQ_RX_ON | RFLR_INVERTIQ_TX_OFF ) );
   lora_write_reg( REG_INVERT_IQ2, RFLR_INVERTIQ2_ON );
   lora_write_reg( REG_DETECTION_OPTIMIZE, lora_read_reg( REG_DETECTION_OPTIMIZE ) & 0x7F );
   lora_write_reg( REG_LR_IFFREQ2, 0x00 );
   lora_write_reg( REG_LR_IFFREQ1, 0x40 );
   lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0);
   lora_write_reg(REG_FIFO_ADDR_PTR, 0);
   lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);

   lora_write_reg( REG_IRQ_FLAGS_MASK, RFLR_IRQFLAGS_RXTIMEOUT |
                                    //RFLR_IRQFLAGS_RXDONE |
                                    //RFLR_IRQFLAGS_PAYLOADCRCERROR |
                                    RFLR_IRQFLAGS_VALIDHEADER |
                                    RFLR_IRQFLAGS_TXDONE |
                                    RFLR_IRQFLAGS_CADDONE |
                                    RFLR_IRQFLAGS_FHSSCHANGEDCHANNEL |
                                    RFLR_IRQFLAGS_CADDETECTED );

   // DIO0=RxDone
   lora_write_reg( REG_DIO_MAPPING_1, ( lora_read_reg( REG_DIO_MAPPING_1 ) & RFLR_DIOMAPPING1_DIO0_MASK ) | RFLR_DIOMAPPING1_DIO0_00 );
}

#if 0
/**
 * Configure power level for transmission
 * @param level 2-17, from least to most power
 */
void 
lora_set_tx_power(int level)
{
   // RF9x module uses PA_BOOST pin
   if (level < 2) level = 2;
   else if (level > 17) level = 17;
   lora_write_reg(REG_PA_CONFIG, PA_BOOST | (level - 2));
}
#endif

void lora_set_tx_power( int8_t power, bool pa_boost)
{
    uint8_t paConfig = 0;
    uint8_t paDac = 0;

    paConfig = lora_read_reg( REG_PA_CONFIG );
    paDac = lora_read_reg( REG_PA_DAC );

    if (pa_boost == true) {
      paConfig = ( paConfig & RF_PACONFIG_PASELECT_MASK ) | RF_PACONFIG_PASELECT_PABOOST;
    }else {
      paConfig = ( paConfig & RF_PACONFIG_PASELECT_MASK );
    }

    if( ( paConfig & RF_PACONFIG_PASELECT_PABOOST ) == RF_PACONFIG_PASELECT_PABOOST )
    {
        if( power > 17 )
        {
            paDac = ( paDac & RF_PADAC_20DBM_MASK ) | RF_PADAC_20DBM_ON;
        }
        else
        {
            paDac = ( paDac & RF_PADAC_20DBM_MASK ) | RF_PADAC_20DBM_OFF;
        }
        if( ( paDac & RF_PADAC_20DBM_ON ) == RF_PADAC_20DBM_ON )
        {
            if( power > 20 )
            {
                power = 20;
            }
            paConfig = ( paConfig & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 5 ) & 0x0F );
        }
        else
        {
            if( power < 2 )
            {
                power = 2;
            }
            if( power > 17 )
            {
                power = 17;
            }
            paConfig = ( paConfig & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 2 ) & 0x0F );
        }
    }
    else
    {
        if( power > 0 )
        {
            if( power > 15 )
            {
                power = 15;
            }
            paConfig = ( paConfig & RF_PACONFIG_MAX_POWER_MASK & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( 7 << 4 ) | ( power );
        }
        else
        {
            if( power < -4 )
            {
                power = -4;
            }
            paConfig = ( paConfig & RF_PACONFIG_MAX_POWER_MASK & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( 0 << 4 ) | ( power + 4 );
        }
    }
    lora_write_reg( REG_PA_CONFIG, paConfig );
    lora_write_reg( REG_PA_DAC, paDac );
}

void lora_set_tx_power_zero(void) {
   lora_write_reg(REG_PA_CONFIG, 0);
   lora_write_reg( REG_PA_DAC, 0x84 );
}

void lora_max_payload_lenght(uint8_t lenght) {
   lora_write_reg(REG_MAX_PAYLOAD_LENGHT, lenght);
}

/**
 * Set carrier frequency.
 * @param frequency Frequency in Hz
 */
void lora_set_frequency(uint32_t frequency)
{
   __frequency = frequency;

   uint32_t frf = ( uint32_t )( ( double )frequency / ( double )FREQ_STEP );

   lora_write_reg(REG_FRF_MSB, (uint8_t)((frf >> 16) & 0xFF));
   lora_write_reg(REG_FRF_MID, (uint8_t)((frf >> 8) & 0xFF));
   lora_write_reg(REG_FRF_LSB, (uint8_t)((frf >> 0) & 0xFF));
}

/**
 * Set spreading factor.
 * @param sf 6-12, Spreading factor to use.
 */
void 
lora_set_spreading_factor(int sf)
{
   if (sf < 6) sf = 6;
   else if (sf > 12) sf = 12;

   if (sf == 6) {
      lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc5);
      lora_write_reg(REG_DETECTION_THRESHOLD, 0x0c);
   } else {
      lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc3);
      lora_write_reg(REG_DETECTION_THRESHOLD, 0x0a);
   }

   lora_write_reg(REG_MODEM_CONFIG_2, (lora_read_reg(REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
}

/**
 * Set bandwidth (bit rate)
 * @param sbw Bandwidth in Hz (up to 500000)
 */
void 
lora_set_bandwidth(long sbw)
{
   int bw;

   if (sbw <= 7.8E3) bw = 0;
   else if (sbw <= 10.4E3) bw = 1;
   else if (sbw <= 15.6E3) bw = 2;
   else if (sbw <= 20.8E3) bw = 3;
   else if (sbw <= 31.25E3) bw = 4;
   else if (sbw <= 41.7E3) bw = 5;
   else if (sbw <= 62.5E3) bw = 6;
   else if (sbw <= 125E3) bw = 7;
   else if (sbw <= 250E3) bw = 8;
   else bw = 9;
   lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
}

/**
 * Set coding rate 
 * @param denominator 5-8, Denominator for the coding rate 4/x
 */ 
void 
lora_set_coding_rate(int denominator)
{
   if (denominator < 5) denominator = 5;
   else if (denominator > 8) denominator = 8;

   int cr = denominator - 4;
   lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
}

/**
 * Set the size of preamble.
 * @param length Preamble length in symbols.
 */
void 
lora_set_preamble_length(long length)
{
   lora_write_reg(REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
   lora_write_reg(REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}

/**
 * Change radio sync word.
 * @param sw New sync word to use.
 */
void 
lora_set_sync_word(int sw)
{
   lora_write_reg(REG_SYNC_WORD, sw);
}

/**
 * Enable appending/verifying frame CRC.
 */
void 
lora_enable_crc(void)
{
   lora_write_reg(REG_MODEM_CONFIG_2, lora_read_reg(REG_MODEM_CONFIG_2) | 0x04);
}

/**
 * Disable appending/verifying frame CRC.
 */
void 
lora_disable_crc(void)
{
   lora_write_reg(REG_MODEM_CONFIG_2, lora_read_reg(REG_MODEM_CONFIG_2) & 0xfb);
}


static void RxChainCalibration( long frequency )
{
    uint8_t regPaConfigInitVal;
    uint32_t initialFreq;

    // Save context
    regPaConfigInitVal = lora_read_reg( REG_PA_CONFIG );
    initialFreq = ( double )( ( ( uint32_t )lora_read_reg( REG_FRF_MSB ) << 16 ) |
                              ( ( uint32_t )lora_read_reg( REG_FRF_MID ) << 8 ) |
                              ( ( uint32_t )lora_read_reg( REG_FRF_LSB ) ) ) * ( double )FREQ_STEP;

    // Cut the PA just in case, RFO output, power = -1 dBm
    lora_write_reg( REG_PA_CONFIG, 0x00 );

    // Launch Rx chain calibration for LF band
    lora_write_reg( REG_IMAGECAL, ( lora_read_reg( REG_IMAGECAL ) & RF_IMAGECAL_IMAGECAL_MASK ) | RF_IMAGECAL_IMAGECAL_START );
    while( ( lora_read_reg( REG_IMAGECAL ) & RF_IMAGECAL_IMAGECAL_RUNNING ) == RF_IMAGECAL_IMAGECAL_RUNNING )
    {
    }

    // Sets a Frequency in HF band
    lora_set_frequency(frequency);

    // Launch Rx chain calibration for HF band
    lora_write_reg( REG_IMAGECAL, ( lora_read_reg( REG_IMAGECAL ) & RF_IMAGECAL_IMAGECAL_MASK ) | RF_IMAGECAL_IMAGECAL_START );
    while( ( lora_read_reg( REG_IMAGECAL ) & RF_IMAGECAL_IMAGECAL_RUNNING ) == RF_IMAGECAL_IMAGECAL_RUNNING )
    {
    }

    // Restore context
    lora_write_reg( REG_PA_CONFIG, regPaConfigInitVal );
    lora_set_frequency( initialFreq );
}

/**
 * Perform hardware initialization.
 */
int lora_init(uint8_t datarate, int coding_rate, long frequency, int8_t power_level, bool pa_boost, bool enable_crc, bool enable_explicit_header)
{
   esp_err_t ret;

   /*
    * Configure CPU hardware to communicate with the radio chip
    */
   esp_rom_gpio_pad_select_gpio(CONFIG_RST_GPIO);
   gpio_set_direction(CONFIG_RST_GPIO, GPIO_MODE_OUTPUT);
   esp_rom_gpio_pad_select_gpio(CONFIG_CS_GPIO);
   gpio_set_direction(CONFIG_CS_GPIO, GPIO_MODE_OUTPUT);

   spi_bus_config_t bus = {
      .miso_io_num = CONFIG_MISO_GPIO,
      .mosi_io_num = CONFIG_MOSI_GPIO,
      .sclk_io_num = CONFIG_SCK_GPIO,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 0
   };
           
   ret = spi_bus_initialize(VSPI_HOST, &bus, 0);
   assert(ret == ESP_OK);

   spi_device_interface_config_t dev = {
      .clock_speed_hz = 9000000,
      .mode = 0,
      .spics_io_num = -1,
      .queue_size = 1,
      .flags = 0,
      .pre_cb = NULL
   };
   ret = spi_bus_add_device(VSPI_HOST, &dev, &__spi);
   assert(ret == ESP_OK);

   lora_mutex = xSemaphoreCreateMutex();
   /*
    * Perform hardware reset.
    */
   lora_reset();

   /*
    * Check version.
    */
   uint8_t version;
   uint8_t i = 0;
   while(i++ < TIMEOUT_RESET) {
      version = lora_read_reg(REG_VERSION);
      #if VERBOSE != 0
      ESP_LOGI(LORA_TAG, "LoRa Radio version: 0x%x", version);
      #endif
      if(version == 0x12) break;
      vTaskDelay(2);
   }
   //assert(i < TIMEOUT_RESET);
   if (i >= TIMEOUT_RESET) {
      return pdFALSE;
   }

   RxChainCalibration(frequency);

   /*
    * Default configuration.
    */
   lora_sleep();

   // Use all 256 bytes fifo to RX or TX
   lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0);
   lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0);

   lora_write_reg(REG_LNA, lora_read_reg(REG_LNA) | 0x03);
   lora_write_reg(REG_MODEM_CONFIG_3, 0x04);

   lora_set_tx_power(power_level, pa_boost);

   lora_set_coding_rate(coding_rate);
   lora_set_preamble_length(8);

   // DR 0 - SF12 / 125kHz - phy bit/sec = 250 - max mac payload = 51 - tempo de transmissão do payload máximo = 2793,5​ms
   // DR 1 - SF11 / 125kHz - phy bit/sec = 440 - max mac payload = 51 - tempo de transmissão do payload máximo = 1560,6ms
   // DR 2 - SF10 / 125kHz - phy bit/sec = 980 - max mac payload = 51 - tempo de transmissão do payload máximo = 698,4​ms
   // DR 3 - SF9 / 125kHz - phy bit/sec = 1760 - max mac payload = 115 - tempo de transmissão do payload máximo = 676,9​ms
   // DR 4 - SF8 / 125kHz - phy bit/sec = 3125 - max mac payload = 222 - tempo de transmissão do payload máximo = 655,9​ms
   // DR 5 - SF7 / 125kHz - phy bit/sec = 5470 - max mac payload = 222 - tempo de transmissão do payload máximo = 368,9​ms
   // DR 6 - SF8 / 500kHz - phy bit/sec = 12500 - max mac payload = 222 - tempo de transmissão do payload máximo = 164,0​ms
   switch(datarate) {
      case 0:
         #if VERBOSE != 0
         ESP_LOGI(LORA_TAG, "Setting DR: 0");
         #endif
         lora_set_bandwidth(125e3);
         lora_set_spreading_factor(12);
         lora_max_payload_lenght(64);     // max mac payload + 13
         break;
      case 1:
         #if VERBOSE != 0
         ESP_LOGI(LORA_TAG, "Setting DR: 1");
         #endif
         lora_set_bandwidth(125e3);
         lora_set_spreading_factor(11);
         lora_max_payload_lenght(64);     // max mac payload + 13
         break;
      case 2:
         #if VERBOSE != 0
         ESP_LOGI(LORA_TAG, "Setting DR: 2");
         #endif
         lora_set_bandwidth(125e3);
         lora_set_spreading_factor(10);
         lora_max_payload_lenght(64);     // max mac payload + 13
         break;
      case 3:
         #if VERBOSE != 0
         ESP_LOGI(LORA_TAG, "Setting DR: 3");
         #endif
         lora_set_bandwidth(125e3);
         lora_set_spreading_factor(9);
         lora_max_payload_lenght(128);     // max mac payload + 13
         break;
      case 4:
         #if VERBOSE != 0
         ESP_LOGI(LORA_TAG, "Setting DR: 4");
         #endif
         lora_set_bandwidth(125e3);
         lora_set_spreading_factor(8);
         lora_max_payload_lenght(235);     // max mac payload + 13
         break;
      case 5:
         #if VERBOSE != 0
         ESP_LOGI(LORA_TAG, "Setting DR: 5");
         #endif
         lora_set_bandwidth(125e3);
         lora_set_spreading_factor(7);
         lora_max_payload_lenght(235);     // max mac payload + 13
         break;
      case 6:
         #if VERBOSE != 0
         ESP_LOGI(LORA_TAG, "Setting DR: 6");
         #endif
         lora_set_bandwidth(500e3);
         lora_set_spreading_factor(8);
         lora_max_payload_lenght(235);     // max mac payload + 13
         lora_write_reg(REG_HIGH_BW_OPTIMIZE_1, 0x02);
         lora_write_reg(REG_HIGH_BW_OPTIMIZE_2, 0x64);
         break;
      default:
         #if VERBOSE != 0
         ESP_LOGI(LORA_TAG, "Setting DR: Default = 2");
         #endif
         lora_set_bandwidth(125e3);
         lora_set_spreading_factor(10);
         lora_max_payload_lenght(64);     // max mac payload + 13
         break;
   }

   lora_set_frequency(frequency);   // for each channel + 200000 -> last channel (64) = 928000000

   if (enable_crc == true) {
      lora_enable_crc();
   }

   if (enable_explicit_header == true){
      lora_explicit_header_mode();
   }
   lora_enable_irq();
   lora_idle();

   lora_datarate = datarate;
   return pdTRUE;
}


/**
 * LoRa clear channel assessment (cca) 
 */
uint32_t lora_cca(void)
{
   lora_idle();
   lora_write_reg( REG_INVERT_IQ, ( ( lora_read_reg( REG_INVERT_IQ ) & RFLR_INVERTIQ_TX_MASK & RFLR_INVERTIQ_RX_MASK ) | RFLR_INVERTIQ_RX_ON | RFLR_INVERTIQ_TX_OFF ) );
   lora_write_reg( REG_INVERT_IQ2, RFLR_INVERTIQ2_ON );
   lora_write_reg( REG_DETECTION_OPTIMIZE, lora_read_reg( REG_DETECTION_OPTIMIZE ) & 0x7F );
   lora_write_reg( REG_LR_IFFREQ2, 0x00 );
   lora_write_reg( REG_LR_IFFREQ1, 0x40 );
   lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0);
   lora_write_reg(REG_FIFO_ADDR_PTR, 0);
   lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | LORA_MODE_CAD);

   lora_write_reg( REG_IRQ_FLAGS_MASK, RFLR_IRQFLAGS_RXTIMEOUT |
                                    RFLR_IRQFLAGS_RXDONE |
                                    RFLR_IRQFLAGS_PAYLOADCRCERROR |
                                    RFLR_IRQFLAGS_VALIDHEADER |
                                    RFLR_IRQFLAGS_TXDONE |
                                    //RFLR_IRQFLAGS_CADDONE |
                                    RFLR_IRQFLAGS_FHSSCHANGEDCHANNEL// |
                                    //RFLR_IRQFLAGS_CADDETECTED 
                                    );

   // DIO0=CadDone
   lora_write_reg( REG_DIO_MAPPING_1, ( lora_read_reg( REG_DIO_MAPPING_1 ) & RFLR_DIOMAPPING1_DIO0_MASK ) | RFLR_DIOMAPPING1_DIO0_10 );

   // wait until cad completed or timeout of 5ms
   BaseType_t value;
   if (xQueueReceive( lora_queue_cad, &value, 5) == pdTRUE){
      //ESP_LOGI( LORA_TAG, "Received %d\n", value);
      return (uint32_t)value;
   }else {
      //ESP_LOGI( LORA_TAG, "Timeout\n");
      return pdFALSE;
   }
}

/**
 * Send a frame.
 * @param buf Data to be sent
 * @param size Size of data.
 */
uint32_t lora_send_frame(uint8_t *buf, int size, uint32_t timeout)
{
   /*
    * Transfer data to radio.
    */
   lora_idle();
   lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0);
   lora_write_reg(REG_FIFO_ADDR_PTR, 0);

   lora_write_reg( REG_INVERT_IQ, ( ( lora_read_reg( REG_INVERT_IQ ) & RFLR_INVERTIQ_TX_MASK & RFLR_INVERTIQ_RX_MASK ) | RFLR_INVERTIQ_RX_OFF | RFLR_INVERTIQ_TX_ON ) );
   lora_write_reg( REG_INVERT_IQ2, RFLR_INVERTIQ2_ON );

   for(int i=0; i<size; i++) {
      lora_write_reg(REG_FIFO, *buf++);
   }
   
   lora_write_reg(REG_PAYLOAD_LENGTH, size);
   
   /*
    * Start transmission and wait for conclusion.
    */
   lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

   lora_write_reg( REG_IRQ_FLAGS_MASK, RFLR_IRQFLAGS_RXTIMEOUT |
                                    RFLR_IRQFLAGS_RXDONE |
                                    RFLR_IRQFLAGS_PAYLOADCRCERROR |
                                    RFLR_IRQFLAGS_VALIDHEADER |
                                    //RFLR_IRQFLAGS_TXDONE |
                                    RFLR_IRQFLAGS_CADDONE |
                                    RFLR_IRQFLAGS_FHSSCHANGEDCHANNEL |
                                    RFLR_IRQFLAGS_CADDETECTED );

   // DIO0=TxDone
   lora_write_reg( REG_DIO_MAPPING_1, ( lora_read_reg( REG_DIO_MAPPING_1 ) & RFLR_DIOMAPPING1_DIO0_MASK ) | RFLR_DIOMAPPING1_DIO0_01 );

   // wait until transmission completed
   return xSemaphoreTake(lora_sem_tx, timeout);
}

/**
 * Read a received frame.
 * @param buf Buffer for the data.
 * @param size Available size in buffer (bytes).
 * @return Number of bytes received (zero if no frame available).
 */
int lora_read_frame(uint8_t *buf, int size)
{
   int len = 0;

   /*
    * Check interrupts.
    */
   int irq = lora_read_reg(REG_IRQ_FLAGS);
   lora_write_reg(REG_IRQ_FLAGS, irq);
   //if((irq & IRQ_RX_DONE_MASK) == 0) return 0;
   if(irq & IRQ_PAYLOAD_CRC_ERROR_MASK) {
      #if VERBOSE != 0
      ESP_LOGI( LORA_TAG, "Error - IRQ: 0x%02X\n", irq);
      #endif
      return 0;
   } 

   /*
    * Find frame size.
    */
   if (__implicit) len = lora_read_reg(REG_PAYLOAD_LENGTH);
   else len = lora_read_reg(REG_RX_NB_BYTES);

   /*
    * Transfer data from radio.
    */
   lora_idle();   
   lora_write_reg(REG_FIFO_ADDR_PTR, lora_read_reg(REG_FIFO_RX_CURRENT_ADDR));
   if(len > size) len = size;
   for(int i=0; i<len; i++) 
      *buf++ = lora_read_reg(REG_FIFO);

   return len;
}

int lora_read_frame_size(void)
{
   int len = 0;

   /*
    * Check interrupts.
    */
   int irq = lora_read_reg(REG_IRQ_FLAGS);
   lora_write_reg(REG_IRQ_FLAGS, irq);
   //if((irq & IRQ_RX_DONE_MASK) == 0) return 0;
   if(irq & IRQ_PAYLOAD_CRC_ERROR_MASK) {
      #if VERBOSE != 0
      ESP_LOGI( LORA_TAG, "Error - IRQ: 0x%02X\n", irq);
      #endif
      return 0;
   } 

   /*
    * Find frame size.
    */
   if (__implicit) len = lora_read_reg(REG_PAYLOAD_LENGTH);
   else len = lora_read_reg(REG_RX_NB_BYTES);

   return len;
}


/**
 * Returns non-zero if there is data to read (frame received).
 */
int lora_received(uint32_t timeout)
{
   return (int)xSemaphoreTake(lora_sem_rx, timeout);
}

SemaphoreHandle_t lora_get_received_sem(void) {
   return lora_sem_rx;
}

/**
 * Return last frame's RSSI.
 */
int 
lora_frame_rssi(void)
{
   return (lora_read_reg(REG_PKT_RSSI_VALUE) - (__frequency < 868E6 ? 164 : 157));
}

/**
 * Return last frame's SNR (signal to noise ratio).
 */
float 
lora_frame_snr(void)
{
   return ((int8_t)lora_read_reg(REG_PKT_SNR_VALUE)) * 0.25;
}

/**
 * Shutdown hardware.
 */
void 
lora_close(void)
{
   lora_sleep();
//   close(__spi);  FIXME: end hardware features after lora_close
//   close(__cs);
//   close(__rst);
//   __spi = -1;
//   __cs = -1;
//   __rst = -1;
}

void 
lora_dump_registers(void)
{
   int i;
   printf("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
   for(i=0; i<0x40; i++) {
      printf("%02X ", lora_read_reg(i));
      if((i & 0x0f) == 0x0f) printf("\n");
   }
   printf("\n");
}

static void IRAM_ATTR gpio_isr_handler( void * pvParameter )
{
   BaseType_t xHigherPriorityTaskWoken = pdFALSE;
   xSemaphoreGiveFromISR(lora_sem_irq, &xHigherPriorityTaskWoken);
   portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

TaskHandle_t task_lora_irq;
void task_irq(void *p)
{
   while(1){
      xSemaphoreTake(lora_sem_irq, portMAX_DELAY);
      if ((lora_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == IRQ_TX_DONE_MASK) {
         // Clear Flag
         lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
         xSemaphoreGive(lora_sem_tx);
      }

      if ((lora_read_reg(REG_IRQ_FLAGS) & IRQ_RX_DONE_MASK) == IRQ_RX_DONE_MASK) {
         // Clear Flag
         lora_write_reg(REG_IRQ_FLAGS, IRQ_RX_DONE_MASK);
         xSemaphoreGive(lora_sem_rx);
      }

      if ((lora_read_reg(REG_IRQ_FLAGS) & IRQ_CAD_DONE_MASK) == IRQ_CAD_DONE_MASK) {
         BaseType_t value = pdFALSE;
         if ((lora_read_reg(REG_IRQ_FLAGS) & IRQ_CAD_DETECTED_MASK) == IRQ_CAD_DETECTED_MASK) {
            // Clear Flag
            lora_write_reg(REG_IRQ_FLAGS, IRQ_CAD_DONE_MASK | IRQ_CAD_DETECTED_MASK);
         }else {
            // Clear Flag
            lora_write_reg(REG_IRQ_FLAGS, IRQ_CAD_DONE_MASK);
            value = pdTRUE;
         }
         if( xQueueSendToBack( lora_queue_cad, ( void * ) &value, ( TickType_t ) 10 ) != pdPASS )
         {
            // Failed to post the message, even after 10 ticks.
         }
      }
   }
}

void lora_enable_irq( void )
{
   lora_sem_tx = xSemaphoreCreateBinary();
   lora_queue_cad = xQueueCreate(1, sizeof(BaseType_t));
   lora_sem_rx = xSemaphoreCreateBinary();
   lora_sem_irq = xSemaphoreCreateBinary();

   if ((lora_sem_tx == NULL) || (lora_sem_tx == NULL) || (lora_sem_irq == NULL) || (lora_queue_cad == NULL)){
      #if VERBOSE != 0
      ESP_LOGI( LORA_TAG, "Error - No HEAP to allocate LoRa semaphores.\n" );
      #endif
      return;
   }

   if (xTaskCreate(task_irq, "LoRa IRQ Task", 2048, NULL, 30, &task_lora_irq) != pdTRUE) {
      #if VERBOSE != 0
      ESP_LOGI( LORA_TAG, "Error - no HEAP to allocate IRQ Task.\n" );
      #endif
      return;
   } 
  
   gpio_config_t io_conf;   
   io_conf.intr_type = GPIO_INTR_POSEDGE; 
   io_conf.mode = GPIO_MODE_INPUT;  
   io_conf.pin_bit_mask = ((1ULL<<CONFIG_IRQ_GPIO)); 
   io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
   io_conf.pull_up_en = GPIO_PULLUP_ENABLE; 
   gpio_config(&io_conf);  

   gpio_install_isr_service(0);

   gpio_isr_handler_add( CONFIG_IRQ_GPIO, gpio_isr_handler, NULL ); 

}

void lora_disable_irq( void )
{
    gpio_intr_disable( CONFIG_IRQ_GPIO );
    vTaskDelete(task_lora_irq);
    vSemaphoreDelete( lora_sem_tx );
    vSemaphoreDelete( lora_sem_rx );
    vSemaphoreDelete( lora_sem_irq );
}
