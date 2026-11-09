# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Minimal LZ4 *block* format (no frame), written for the Felucca loader
wrapper: greedy hash-chain-free compressor and a decoder for self-checks.
Spec: https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md
(min match 4, last 5 bytes literals, last match starts >= 12 bytes before the end)."""


def _len_bytes(n):
    out = bytearray()
    while n >= 255:
        out.append(255)
        n -= 255
    out.append(n)
    return out


def compress(src: bytes) -> bytes:
    n, out, table = len(src), bytearray(), {}
    anchor, i = 0, 0
    limit = n - 12                                  # matches must start before this
    while i < limit:
        key = src[i:i + 4]
        cand = table.get(key)
        table[key] = i
        if cand is None or i - cand > 65535:
            i += 1
            continue
        mlen = 4                                    # extend; leave the last 5 bytes as literals
        while i + mlen < n - 5 and src[cand + mlen] == src[i + mlen]:
            mlen += 1
        lit = src[anchor:i]
        tok = (min(len(lit), 15) << 4) | min(mlen - 4, 15)
        out.append(tok)
        if len(lit) >= 15:
            out += _len_bytes(len(lit) - 15)
        out += lit
        off = i - cand
        out += bytes((off & 0xFF, off >> 8))
        if mlen - 4 >= 15:
            out += _len_bytes(mlen - 4 - 15)
        i += mlen
        anchor = i
    lit = src[anchor:]                              # final literal run
    out.append(min(len(lit), 15) << 4)
    if len(lit) >= 15:
        out += _len_bytes(len(lit) - 15)
    out += lit
    return bytes(out)


def decompress(blk: bytes, prefix: bytes = b"") -> bytes:
    out, i = bytearray(prefix), 0
    while i < len(blk):
        tok = blk[i]
        i += 1
        ll = tok >> 4
        if ll == 15:
            while True:
                b = blk[i]
                i += 1
                ll += b
                if b != 255:
                    break
        out += blk[i:i + ll]
        i += ll
        if i >= len(blk):
            break
        off = blk[i] | blk[i + 1] << 8
        i += 2
        ml = tok & 15
        if ml == 15:
            while True:
                b = blk[i]
                i += 1
                ml += b
                if b != 255:
                    break
        ml += 4
        for _ in range(ml):
            out.append(out[-off])
    return bytes(out[len(prefix):])


def compress_small(src: bytes, chain: int = 256) -> bytes:
    """(V10.6z5) the same block format, smaller: an optimal parse (the fewest bytes over every choice of literal runs
    and match lengths, matches from a hash chain of `chain` candidates), for the app image's RAM code and data
    (tools/build.py, firmware/src/boot_lz4.h): ~16 % under the input on the pi32v2's code where compress() takes ~10 %.
    Same constraints as compress(): min match 4, the last 5 bytes literals, no match starting in the last 12."""
    n = len(src)
    limit = n - 12
    head, prev, found = {}, [-1] * n, [()] * n
    for i in range(n - 3):
        key = src[i:i + 4]
        prev[i] = head.get(key, -1)
        head[key] = i
        if i >= limit:
            continue
        c, best, longest, steps = prev[i], [], 3, 0
        while c >= 0 and i - c <= 65535 and steps < chain:
            ln, top = 0, n - 5 - i
            while ln < top and src[c + ln] == src[i + ln]:
                ln += 1
            if ln > longest:
                best.append((ln, i - c))
                longest = ln
            c, steps = prev[c], steps + 1
        found[i] = tuple(best)

    def ext(k):                                     # the bytes a length's 15+ part takes
        return 0 if k < 15 else 1 + (k - 15) // 255
    inf = 1 << 60
    cost, pick = [inf] * (n + 1), [None] * (n + 1)  # cost[i]: src[i:] as sequences starting at i
    mcost, mpick = [inf] * (n + 1), [None] * (n + 1)  # mcost[j]: a match at j and what follows it
    cost[n] = 0
    for i in range(n - 1, -1, -1):
        for ln, off in found[i]:
            for k in (range(4, ln + 1) if ln <= 40 else (*range(4, 20), ln)):
                c = 2 + ext(k - 4) + cost[i + k]
                if c < mcost[i]:
                    mcost[i], mpick[i] = c, (k, off)
        best, at = 1 + ext(n - i) + n - i, None     # all literals to the end
        for j in range(i, min(n, i + 600)):
            if mcost[j] < inf:
                c = 1 + ext(j - i) + j - i + mcost[j]
                if c < best:
                    best, at = c, j
        cost[i], pick[i] = best, at
    out, i = bytearray(), 0
    while i < n or not out:
        j = pick[i] if i < n else None
        lit = src[i:j] if j is not None else src[i:]
        k, off = mpick[j] if j is not None else (4, 0)
        out.append((min(len(lit), 15) << 4) | (min(k - 4, 15) if j is not None else 0))
        if len(lit) >= 15:
            out += _len_bytes(len(lit) - 15)
        out += lit
        if j is None:
            break
        out += bytes((off & 0xFF, off >> 8))
        if k - 4 >= 15:
            out += _len_bytes(k - 4 - 15)
        i = j + k
    return bytes(out)


def boot_unpack(stream: bytes, at: int, size: int):
    """firmware/src/boot_lz4.h's boot_lz4, step for step: the block at stream[at:] unpacked into `size` bytes (the
    section's, from the linker); (the bytes, where the next block starts). An error where boot_lz4 would stop early"""
    out, s = bytearray(), at
    while len(out) < size:
        t = stream[s]
        s += 1
        n = t >> 4
        if n == 15:
            while True:
                b = stream[s]
                s += 1
                n += b
                if b != 255:
                    break
        while n and len(out) < size:
            out.append(stream[s])
            s, n = s + 1, n - 1
        if len(out) >= size:
            break
        off = stream[s] | stream[s + 1] << 8
        s += 2
        n = t & 15
        if n == 15:
            while True:
                b = stream[s]
                s += 1
                n += b
                if b != 255:
                    break
        n += 4
        if not off or off > len(out):
            raise ValueError(f"boot_lz4: a match {off} back at {len(out)}")
        while n and len(out) < size:
            out.append(out[-off])
            n -= 1
    return bytes(out), s
