/*
 * High-Performance Pipelined SHA-256 Assembly Engine for ESP32 (Xtensa LX6)
 *
 * Implements dual-block pipelined SHA-256 hardware acceleration with inline
 * Xtensa assembly for ~650 - 750+ KH/s on ESP32 CYD (ESP32-2432S028).
 *
 * Architecture:
 * - While hardware calculates Block 1, CPU prepares Block 2 (with nonce).
 * - While hardware calculates Block 2, CPU prepares Block 3 (double-hash padding).
 * - Fast 16-bit early rejection entirely inside CPU registers without leaving assembly.
 */

#ifndef SHA256_PIPELINED_H
#define SHA256_PIPELINED_H

#include <stdint.h>
#include <stdbool.h>

#if defined(CONFIG_IDF_TARGET_ESP32)

#ifdef __cplusplus
extern "C" {
#endif

void sha256_pipelined_init(void);

/**
 * Pipelined hardware SHA-256 mining loop in Xtensa assembly.
 *
 * @param sha_base          Base address of SHA peripheral (0x3FF03000)
 * @param header_swapped    Pre-byteswapped 80-byte block header (20 x uint32_t)
 * @param nonce_ptr         Pointer to current swapped nonce (updated in-place)
 * @param nonce_end         Swapped nonce upper bound for this batch
 * @param hash_counter      Pointer to hashes performed counter
 * @param working_job_id    Pointer to volatile job ID (to abort on new stratum job)
 * @param current_job_id    Job ID being mined
 *
 * @return true if 16-bit early reject passed (potential valid share found)
 *         false if completed nonce batch or aborted due to new job
 */
bool sha256_pipelined_mine(
    volatile uint32_t *sha_base,
    const uint32_t *header_swapped,
    uint32_t *nonce_ptr,
    uint32_t nonce_end,
    volatile uint32_t *hash_counter,
    volatile uint8_t *working_job_id,
    uint8_t current_job_id
);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_IDF_TARGET_ESP32
#endif // SHA256_PIPELINED_H
