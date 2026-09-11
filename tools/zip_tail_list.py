"""List a ZIP's central directory from an HTTP-range tail download.

Why this exists: the CI artifacts here are ~120 MB, and this machine's link to
GitHub runs at a few tens of KB/s, so downloading a whole artifact to answer
"what is actually inside?" is not practical. A ZIP keeps its central directory
at the very end of the file, so the last couple of hundred KB are enough to
enumerate every entry with its uncompressed/compressed size.

Usage:
    python zip_tail_list.py <file-with-http-headers-and-tail> [total_size]

The input file is expected to be the raw output of:
    curl -s -i -H "Range: bytes=<total-200000>-<total-1>" <blob-url>
"""

import struct
import sys


def strip_http_headers(raw: bytes) -> bytes:
    if raw.startswith(b"HTTP/"):
        end = raw.find(b"\r\n\r\n")
        if end != -1:
            return raw[end + 4:]
    return raw


def find_eocd(buf: bytes) -> int:
    # EOCD is in the last 64 KiB + 22 bytes at worst (no zip comment here).
    start = max(0, len(buf) - 65558)
    idx = buf.rfind(b"PK\x05\x06", start)
    return idx


def parse(buf: bytes, base_offset: int):
    eocd = find_eocd(buf)
    if eocd == -1:
        raise SystemExit("EOCD not found in tail; grab a bigger tail")
    (_, _, _, _, total_entries, cd_size, cd_offset, _comment_len) = struct.unpack_from("<4s4H2LH", buf, eocd)

    # cd_offset is relative to the start of the archive; the tail starts at
    # base_offset, so translate into a local index.
    local_cd = cd_offset - base_offset
    if local_cd < 0:
        raise SystemExit(
            f"central directory starts {cd_offset} but tail starts at {base_offset}; "
            "grab a bigger tail"
        )

    entries = []
    p = local_cd
    for _ in range(total_entries):
        if buf[p:p + 4] != b"PK\x01\x02":
            break
        (_, _, _, _, _, _, _, crc, csize, usize, nlen, elen, clen, _, _, _, _) = struct.unpack_from(
            "<4s6H3L5H2L", buf, p
        )
        name = buf[p + 46:p + 46 + nlen].decode("utf-8", "replace")
        entries.append((name, usize, csize))
        p += 46 + nlen + elen + clen
    return entries, cd_offset, cd_size


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    raw = open(sys.argv[1], "rb").read()
    body = strip_http_headers(raw)
    # Where does this chunk sit in the whole file? Recover it from the HTTP
    # Content-Range header when present, else from an explicit argument.
    base = 0
    if raw.startswith(b"HTTP/"):
        head = raw.split(b"\r\n\r\n", 1)[0].decode("latin-1")
        for line in head.splitlines():
            if line.lower().startswith("content-range:"):
                # Content-Range: bytes 127800000-128031463/128031464
                rng = line.split(":", 1)[1].strip()
                rng = rng.split(" ", 1)[1] if " " in rng else rng
                base = int(rng.split("-")[0])
    elif len(sys.argv) > 2 and sys.argv[2] != "0":
        # Tail downloaded without headers: caller passes total size and we
        # assume the chunk is the final len(body) bytes.
        base = int(sys.argv[2]) - len(body)

    entries, cd_offset, cd_size = parse(body, base)
    entries.sort(key=lambda e: e[1], reverse=True)

    total_u = sum(e[1] for e in entries)
    print(f"entries: {len(entries)}   central dir @ {cd_offset} ({cd_size} B)")
    print(f"uncompressed total: {total_u/1048576:.1f} MB")
    print()
    for name, usize, csize in entries:
        print(f"{usize/1048576:9.2f} MB  (zip {csize/1048576:8.2f} MB)  {name}")


if __name__ == "__main__":
    main()
