#!/usr/bin/env python3
"""接收 Skyline DualLogger 发出的 TCP 日志流。"""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="Switch IP 地址，例如 192.168.1.42")
    parser.add_argument("--port", type=int, default=6969)
    parser.add_argument("--retry-seconds", type=float, default=2.0)
    parser.add_argument("--out", default=None, help="可选的本地日志文件")
    args = parser.parse_args()

    output = None
    if args.out:
        output = Path(args.out)
        output.parent.mkdir(parents=True, exist_ok=True)

    try:
        while True:
            print(f"正在连接 {args.host}:{args.port} 上的 Skyline……", flush=True)
            try:
                with socket.create_connection((args.host, args.port), timeout=10) as connection:
                    connection.settimeout(None)
                    print("连接成功。现在启动或重启游戏；按 Ctrl+C 停止。", flush=True)
                    with output.open("ab") if output else _null_context() as log:
                        while True:
                            data = connection.recv(4096)
                            if not data:
                                print("Skyline 已关闭日志连接；正在重试……", flush=True)
                                break
                            sys.stdout.write(data.decode("utf-8", errors="replace"))
                            sys.stdout.flush()
                            if log:
                                log.write(data)
                                log.flush()
            except OSError as exc:
                print(f"尚未连接（{exc}）；将在 {args.retry_seconds:g} 秒后重试……", flush=True)
            time.sleep(max(args.retry_seconds, 0.1))
    except KeyboardInterrupt:
        print("\n已停止。", flush=True)
    return 0


class _null_context:
    def __enter__(self):
        return None

    def __exit__(self, exc_type, exc, traceback):
        return False


if __name__ == "__main__":
    raise SystemExit(main())
