#!/usr/bin/env python3
"""Send the assignment's calculator queries over a single persistent TCP socket."""

from __future__ import annotations

import argparse
import socket
import sys
from typing import List, Tuple


def read_response(sock: socket.socket) -> Tuple[int, bytes]:
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = sock.recv(4096)
        if not chunk:
            raise RuntimeError("connection closed while reading headers")
        buf += chunk

    header, rest = buf.split(b"\r\n\r\n", 1)
    status = int(header.split(b" ", 2)[1])
    length = 0
    for line in header.split(b"\r\n"):
        if line.lower().startswith(b"content-length:"):
            length = int(line.split(b":", 1)[1].strip())

    body = rest
    while len(body) < length:
        chunk = sock.recv(length - len(body))
        if not chunk:
            raise RuntimeError("connection closed while reading body")
        body += chunk

    leftover = body[length:]
    if leftover:
        raise RuntimeError("unexpected leftover bytes after one response")
    return status, body[:length]


def send_request(sock: socket.socket, request: str) -> Tuple[int, bytes]:
    sock.sendall(request.encode("ascii"))
    return read_response(sock)


CASES: List[Tuple[str, str, int, bytes]] = [
    (
        "add",
        "GET /add?a=2&b=3 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n",
        200,
        b"5",
    ),
    (
        "sub",
        "GET /sub?a=10&b=4 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n",
        200,
        b"6",
    ),
    (
        "mul",
        "GET /mul?a=6&b=7 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n",
        200,
        b"42",
    ),
    (
        "div",
        "GET /div?a=9&b=3 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n",
        200,
        b"3",
    ),
    (
        "div-zero",
        "GET /div?a=1&b=0 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n",
        400,
        None,  # type: ignore[arg-type]
    ),
    (
        "invalid-operand",
        "GET /add?a=x&b=3 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n",
        400,
        None,  # type: ignore[arg-type]
    ),
    (
        "unknown-path",
        "GET /pow?a=2&b=8 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n",
        404,
        None,  # type: ignore[arg-type]
    ),
    (
        "method-not-allowed",
        "POST /add HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 0\r\n\r\n",
        405,
        None,  # type: ignore[arg-type]
    ),
    (
        "missing-host",
        "GET /add?a=1&b=2 HTTP/1.1\r\n\r\n",
        400,
        None,  # type: ignore[arg-type]
    ),
]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()

    sock = socket.create_connection((args.host, args.port), timeout=5)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    failed = 0
    try:
        for name, raw, expected_status, expected_body in CASES:
            status, body = send_request(sock, raw)
            ok = status == expected_status and (
                expected_body is None or body == expected_body
            )
            mark = "ok" if ok else "FAIL"
            extra = f" body={body!r}" if expected_body is not None else f" body_len={len(body)}"
            print(f"[{mark}] {name}: status={status} expected={expected_status}{extra}")
            if not ok:
                failed += 1
        print("persistent connection stayed open for all requests")
    finally:
        sock.close()

    if failed:
        print(f"{failed} case(s) failed", file=sys.stderr)
        return 1
    print("all persistent-connection cases passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
