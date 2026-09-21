#!/usr/bin/env python3
"""Open many concurrent Keep-Alive connections and fire calculator requests."""

from __future__ import annotations

import argparse
import asyncio
import random
import sys
import time
from typing import List, Tuple


Request = Tuple[bytes, int, bytes]


def build_cases() -> List[Request]:
    return [
        (b"GET /add?a=2&b=3 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", 200, b"5"),
        (b"GET /sub?a=10&b=4 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", 200, b"6"),
        (b"GET /mul?a=6&b=7 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", 200, b"42"),
        (b"GET /div?a=9&b=3 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", 200, b"3"),
        (b"GET /div?a=1&b=0 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", 400, b""),
        (b"GET /add?a=x&b=3 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", 400, b""),
    ]


async def read_response(reader: asyncio.StreamReader) -> Tuple[int, bytes]:
    header = await reader.readuntil(b"\r\n\r\n")
    status = int(header.split(b" ", 2)[1])
    length = 0
    header_block, _ = header.split(b"\r\n\r\n", 1)
    for line in header_block.split(b"\r\n"):
        if line.lower().startswith(b"content-length:"):
            length = int(line.split(b":", 1)[1].strip())
    body = b""
    if length:
        body = await reader.readexactly(length)
    return status, body


async def worker(
    host: str,
    port: int,
    requests_per_conn: int,
    cases: List[Request],
    errors: List[str],
) -> int:
    ok = 0
    try:
        reader, writer = await asyncio.open_connection(host, port)
    except Exception as exc:  # noqa: BLE001
        errors.append(f"connect failed: {exc}")
        return 0

    try:
        rng = random.Random()
        for _ in range(requests_per_conn):
            raw, expected_status, expected_body = rng.choice(cases)
            writer.write(raw)
            await writer.drain()
            status, body = await read_response(reader)
            if status != expected_status:
                errors.append(f"status {status} != {expected_status}")
                continue
            if expected_status == 200 and body != expected_body:
                errors.append(f"body {body!r} != {expected_body!r}")
                continue
            ok += 1
    except Exception as exc:  # noqa: BLE001
        errors.append(str(exc))
    finally:
        writer.close()
        try:
            await writer.wait_closed()
        except Exception:  # noqa: BLE001
            pass
    return ok


async def run(host: str, port: int, connections: int, requests_per_conn: int) -> int:
    cases = build_cases()
    errors: List[str] = []
    started = time.perf_counter()
    results = await asyncio.gather(
        *[
            worker(host, port, requests_per_conn, cases, errors)
            for _ in range(connections)
        ]
    )
    elapsed = time.perf_counter() - started
    total_ok = sum(results)
    total = connections * requests_per_conn
    rps = total_ok / elapsed if elapsed > 0 else 0.0
    print(
        f"connections={connections} requests_per_conn={requests_per_conn} "
        f"ok={total_ok}/{total} errors={len(errors)} "
        f"elapsed_s={elapsed:.3f} rps={rps:.1f}"
    )
    if errors:
        for sample in errors[:10]:
            print(f"  sample error: {sample}", file=sys.stderr)
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--connections", type=int, default=200)
    parser.add_argument("--requests", type=int, default=50, help="requests per connection")
    args = parser.parse_args()
    return asyncio.run(run(args.host, args.port, args.connections, args.requests))


if __name__ == "__main__":
    raise SystemExit(main())
