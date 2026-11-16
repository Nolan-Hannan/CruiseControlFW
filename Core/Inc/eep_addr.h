/* M95P32 external flash address map. */

#ifndef EEP_ADDR_H
#define EEP_ADDR_H

/* SOC curve banks. */
#define EEP_SOC_BANK_A_ADDR 0x000000UL
#define EEP_SOC_BANK_B_ADDR 0x005000UL
#define EEP_SOC_FILE_SIZE   0x4400UL

/* Bank pointer log, append-only. */
#define EEP_PTR_LOG_ADDR  0x00A000UL
#define EEP_PTR_LOG_SIZE  0x1000UL
#define EEP_PTR_REC_SIZE  16U
#define EEP_PTR_REC_COUNT 256U

/* Pointer record fields. */
#define EEP_PTR_ID_OFF   0x00U
#define EEP_PTR_BANK_OFF 0x04U
#define EEP_PTR_INV_OFF  0x05U
#define EEP_PTR_RSVD_OFF 0x06U
#define EEP_PTR_CRC_OFF  0x0CU

#define EEP_PTR_BANK_A 0x0AU
#define EEP_PTR_BANK_B 0x0BU
#define EEP_PTR_INV_A  0x05U
#define EEP_PTR_INV_B  0x04U

/* SOC file header fields. */
#define EEP_SOC_ID_OFF       0x00U
#define EEP_SOC_VERSION_OFF  0x04U
#define EEP_SOC_DAYS_OFF     0x05U
#define EEP_SOC_SAMPLES_OFF  0x06U
#define EEP_SOC_INTERVAL_OFF 0x08U
#define EEP_SOC_STOP_OFF     0x22U
#define EEP_SOC_HDR_CRC_OFF  0x24U
#define EEP_SOC_RSVD_OFF     0x28U

/* Per-day header block, one per day. */
#define EEP_SOC_DAYHDR_OFF   0x0AU
#define EEP_SOC_DAYHDR_SIZE  8U
#define EEP_SOC_DAY_MONTH    0U
#define EEP_SOC_DAY_DAY      1U
#define EEP_SOC_DAY_YEAR     2U
#define EEP_SOC_DAY_START_H  4U
#define EEP_SOC_DAY_START_M  5U
#define EEP_SOC_DAY_STOP_H   6U
#define EEP_SOC_DAY_STOP_M   7U

/* Day data, then a 2-byte stop character and a 4-byte CRC32. */
#define EEP_SOC_DAYS        3U
#define EEP_SOC_DATA_OFF    0x002CUL
#define EEP_SOC_DATA_SIZE   5760UL
#define EEP_SOC_DAY_STRIDE  0x1686UL
#define EEP_SOC_END_CRC_OFF 0x43BEUL

/* IMU calibration record, a single 32-byte instance. Rewritten whole by the gradcal command. */
#define EEP_IMU_CAL_ADDR      0x00B000UL
#define EEP_IMU_CAL_SIZE      32U

/* IMU calibration record offsets. Floats are IEEE 754, little endian. */
#define EEP_IMU_ID_OFF        0x00U /* "IMUC" */
#define EEP_IMU_VERSION_OFF   0x04U /* 0x01 */
#define EEP_IMU_SAMPLES_OFF   0x05U /* samples averaged */
#define EEP_IMU_RSVD0_OFF     0x06U /* 2 bytes */
#define EEP_IMU_PITCH_OFF     0x08U /* float, level pitch offset, radians */
#define EEP_IMU_GYRO_BIAS_OFF 0x0CU /* float, pitch rate bias at rest, deg/s */
#define EEP_IMU_RSVD1_OFF     0x10U /* 12 bytes */
#define EEP_IMU_CRC_OFF       0x1CU /* CRC32 of bytes 0x00 to 0x1B */

#endif /* EEP_ADDR_H */
