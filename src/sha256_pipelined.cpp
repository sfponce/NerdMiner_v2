/*
 * SparkMiner & BitsyMiner Pipelined SHA-256 Xtensa Assembly Engine
 * Ported to NerdMiner_v2 for ESP32 CYD (ESP32-2432S028)
 *
 * Provides ~650 - 750+ KH/s on classic ESP32 (Xtensa LX6).
 */

#include <Arduino.h>
#include "sha256_pipelined.h"

#if defined(CONFIG_IDF_TARGET_ESP32)

#include <soc/dport_reg.h>
#include <soc/hwcrypto_reg.h>

void sha256_pipelined_init(void) {
    // Enable SHA peripheral clock and clear reset
    DPORT_REG_SET_BIT(DPORT_PERI_CLK_EN_REG, DPORT_PERI_EN_SHA);
    DPORT_REG_CLR_BIT(DPORT_PERI_RST_EN_REG, DPORT_PERI_EN_SHA | DPORT_PERI_EN_SECUREBOOT);
}

bool IRAM_ATTR sha256_pipelined_mine(
    volatile uint32_t *sha_base,
    const uint32_t *header_swapped,
    uint32_t *nonce_ptr,
    uint32_t nonce_end,
    volatile uint32_t *hash_counter,
    volatile uint8_t *working_job_id,
    uint8_t current_job_id
) {
    const uint32_t shaPad = 0x80000000u;
    const uint32_t firstShaBitLen = 0x00000280u;  // 640 bits (80 bytes)
    const uint32_t secondShaBitLen = 0x00000100u; // 256 bits (32 bytes)
    uint32_t candidate_found = 0;

    // Ensure SHA hardware clock is enabled
    if (!(DPORT_REG_READ(DPORT_PERI_CLK_EN_REG) & DPORT_PERI_EN_SHA)) {
        DPORT_REG_SET_BIT(DPORT_PERI_CLK_EN_REG, DPORT_PERI_EN_SHA);
        DPORT_REG_CLR_BIT(DPORT_PERI_RST_EN_REG, DPORT_PERI_EN_SHA | DPORT_PERI_EN_SECUREBOOT);
    }

    __asm__ __volatile__(
        // Setup registers:
        // a2: current swapped nonce
        // a5: control base (sha_base + 0x90)
        "l32i.n   a2,  %[nonce], 0        \n"
        "addi     a5,  %[sb], 0x90        \n"

    "pipe_start: \n"

        // ===== BLOCK 1: Load 64 bytes of header to SHA_TEXT_BASE =====
        "l32i.n    a3,  %[IN],  0         \n"
        "s32i.n    a3,  %[sb],  0         \n"
        "l32i.n    a3,  %[IN],  4         \n"
        "s32i.n    a3,  %[sb],  4         \n"
        "l32i.n    a3,  %[IN],  8         \n"
        "s32i.n    a3,  %[sb],  8         \n"
        "l32i.n    a3,  %[IN],  12        \n"
        "s32i.n    a3,  %[sb],  12        \n"
        "l32i.n    a3,  %[IN],  16        \n"
        "s32i.n    a3,  %[sb],  16        \n"
        "l32i.n    a3,  %[IN],  20        \n"
        "s32i.n    a3,  %[sb],  20        \n"
        "l32i.n    a3,  %[IN],  24        \n"
        "s32i.n    a3,  %[sb],  24        \n"
        "l32i.n    a3,  %[IN],  28        \n"
        "s32i.n    a3,  %[sb],  28        \n"
        "l32i.n    a3,  %[IN],  32        \n"
        "s32i.n    a3,  %[sb],  32        \n"
        "l32i.n    a3,  %[IN],  36        \n"
        "s32i.n    a3,  %[sb],  36        \n"
        "l32i.n    a3,  %[IN],  40        \n"
        "s32i.n    a3,  %[sb],  40        \n"
        "l32i.n    a3,  %[IN],  44        \n"
        "s32i.n    a3,  %[sb],  44        \n"
        "l32i.n    a3,  %[IN],  48        \n"
        "s32i.n    a3,  %[sb],  48        \n"
        "l32i.n    a3,  %[IN],  52        \n"
        "s32i.n    a3,  %[sb],  52        \n"
        "l32i.n    a3,  %[IN],  56        \n"
        "s32i.n    a3,  %[sb],  56        \n"
        "l32i.n    a3,  %[IN],  60        \n"
        "s32i.n    a3,  %[sb],  60        \n"

        // ===== START SHA on block 1 =====
        "movi.n    a3, 1                  \n"
        "s32i.n    a3, a5, 0              \n" // offset 0x90: SHA_START
        "memw                             \n"

        // ===== PIPELINE: Prepare block 2 while hardware hashes block 1 =====
        "l32i      a3,  %[IN], 64         \n"
        "s32i.n    a3,  %[sb],  0         \n"
        "l32i      a3,  %[IN], 68         \n"
        "s32i.n    a3,  %[sb],  4         \n"
        "l32i      a3,  %[IN], 72         \n"
        "s32i.n    a3,  %[sb],  8         \n"

        // Store current nonce
        "s32i.n    a2,  %[sb], 12         \n" // offset 12: nonce position

        // Store block 2 padding and length
        "s32i.n    %[pad],  %[sb], 16     \n" // 0x80000000 at word 4
        "s32i.n    %[len1], %[sb], 60     \n" // 640 bits at word 15

        // Zero words 5 to 14 (offsets 20 to 56)
        "movi.n    a4, 0                  \n"
        "addi      a8, %[sb], 20          \n"
        "movi.n    a3, 10                 \n"
        "loop      a3, 1f                 \n"
        "s32i.n    a4, a8, 0              \n"
        "addi.n    a8, a8, 4              \n"
    "1: \n"

        // ===== WAIT for block 1 =====
    "wait_b1: \n"
        "l32i.n    a3, a5, 12             \n" // offset 0x9C: SHA_BUSY
        "bnez.n    a3, wait_b1            \n"

        // ===== CONTINUE with block 2 =====
        "movi.n    a3, 1                  \n"
        "s32i.n    a3, a5, 4              \n" // offset 0x94: SHA_CONTINUE
        "memw                             \n"

        // ===== WAIT for block 2 =====
    "wait_b2: \n"
        "l32i.n    a4, a5, 12             \n"
        "bnez.n    a4, wait_b2            \n"

        // ===== LOAD intermediate hash result =====
        "movi.n    a4, 1                  \n"
        "s32i.n    a4, a5, 8              \n" // offset 0x98: SHA_LOAD
        "memw                             \n"

        // Increment nonce now
        "addi.n    a2, a2, 1              \n"

        // ===== WAIT for load =====
    "wait_load1: \n"
        "l32i.n    a4, a5, 12             \n"
        "bnez.n    a4, wait_load1         \n"

        // ===== PREPARE double hash (second SHA) =====
        // Words 0..7 already have 32-byte digest, words 9..14 already zeroed
        "s32i.n    %[pad],  %[sb], 32     \n" // 0x80000000 at word 8 (offset 32)
        "s32i.n    %[len2], %[sb], 60     \n" // 256 bits at word 15 (offset 60)

        // ===== START second SHA =====
        "movi.n    a4, 1                  \n"
        "s32i.n    a4, a5, 0              \n" // SHA_START
        "memw                             \n"

        // ===== INCREMENT hash counter =====
        "l32i.n    a3, %[ih], 0           \n"
        "addi.n    a3, a3, 1              \n"
        "s32i.n    a3, %[ih], 0           \n"

        // ===== WAIT for second SHA =====
    "wait_b3: \n"
        "l32i.n    a4, a5, 12             \n"
        "bnez.n    a4, wait_b3            \n"

        // ===== LOAD final hash result =====
        "movi.n    a3, 1                  \n"
        "s32i.n    a3, a5, 8              \n" // SHA_LOAD
        "memw                             \n"

        // ===== WAIT for final load =====
    "wait_load2: \n"
        "l32i.n    a4, a5, 12             \n"
        "bnez.n    a4, wait_load2         \n"

        // ===== CHECK if stratum job changed =====
        "l8ui      a3, %[w_job], 0        \n"
        "bne       a3, %[c_job], pipe_end \n"

        // ===== EARLY REJECT: check upper 16 bits of H0 =====
        "l16ui     a3, %[sb], 28          \n"
        "beqz.n    a3, pipe_cand          \n" // Potential share found!

        // Check if batch finished
        "bgeu      a2, %[n_end], pipe_end \n"

        // Loop to next hash
        "j         pipe_start             \n"

    "pipe_cand: \n"
        "movi.n    a3, 1                  \n"
        "s32i.n    a3, %[c_found], 0      \n"

    "pipe_end: \n"
        // Store updated nonce
        "s32i.n    a2, %[nonce], 0        \n"

        :
        : [sb]      "r" (sha_base),
          [IN]      "r" (header_swapped),
          [ih]      "r" (hash_counter),
          [nonce]   "r" (nonce_ptr),
          [n_end]   "r" (nonce_end),
          [w_job]   "r" (working_job_id),
          [c_job]   "r" (current_job_id),
          [c_found] "r" (&candidate_found),
          [pad]     "r" (shaPad),
          [len1]    "r" (firstShaBitLen),
          [len2]    "r" (secondShaBitLen)
        : "a2", "a3", "a4", "a5", "a8", "memory"
    );

    return (candidate_found != 0);
}

#endif // CONFIG_IDF_TARGET_ESP32
