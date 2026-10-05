/*
 * High-Performance Pipelined SHA-256 Assembly Engine for ESP32 (Xtensa LX6)
 *
 * Implements dual-block pipelined SHA-256 hardware acceleration with inline
 * Xtensa assembly for ~650 - 750+ KH/s on ESP32 CYD (ESP32-2432S028).
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
 * Runs until:
 * - mining_flag becomes false, OR
 * - 16-bit early reject passes (potential valid share found)
 *
 * @param sha_base          SHA_TEXT_BASE register address (0x3FF03000)
 * @param header_swapped    Pre-byteswapped 80-byte header (20 x uint32_t)
 * @param nonce_ptr         Pointer to swapped nonce value (updated in-place)
 * @param hash_count_ptr    Pointer to 64-bit hash counter (incremented per hash)
 * @param mining_flag       Pointer to mining active flag (exits when false)
 *
 * @return true if potential share found (16-bit early reject passed)
 *         false if stopped due to mining_flag becoming false
 */
bool sha256_pipelined_mine(
    volatile uint32_t *sha_base,
    const uint32_t *header_swapped,
    uint32_t *nonce_ptr,
    volatile uint64_t *hash_count_ptr,
    volatile bool *mining_flag
);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_IDF_TARGET_ESP32
#endif // SHA256_PIPELINED_H
