/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Per-track rational step clock. SPAN counts DIV units. Keep the remainder in the indexed
 * boundaries rather than rounding every step: the sum over LEN is exactly period * SPAN.
 * No 64-bit arithmetic, float, per-sample work or additional audio buffers. */
static uint8_t fit_valid(uint32_t value) { return (value & 127u) <= 64u ? (uint8_t)value : 0u; }
static uint32_t fit_value_span(uint32_t value)
{
    uint32_t span = fit_valid(value) & 127u;
    return span ? span : 16u;
}
/* Exact fractional duration without overflowing the intermediate product for very slow fitted loops. */
static __attribute__((always_inline)) inline uint32_t fit_scale_samples(uint32_t duration, uint32_t amount, uint32_t shift)
{
    return (duration >> shift) * amount + (((duration & ((1u << shift) - 1u)) * amount) >> shift);
}
static uint32_t fit_span(const track_t *t) { return fit_value_span(t->seq_fit); }
static __attribute__((noinline)) uint32_t fit_step_samples(const track_t *t, uint32_t period, uint32_t idx)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    uint32_t total = period * fit_span(t), base = total / len, rem = total % len;
    uint32_t sw = len > 1u ? base * (uint32_t)track_swing(t) / 250u : 0u, duration;
    if (idx >= len) idx %= len;
    duration = base + (((idx * rem) % len) + rem >= len);
    /* Pairwise swing conserves the loop, including odd LEN: the last unpaired step is straight. */
    if (idx & 1u) duration -= sw;
    else if (idx + 1u < len) duration += sw;
    return duration ? duration : 1u;
}
