#!/usr/bin/env python3
"""Exercise the real service over HTTP, including graceful shutdown."""

import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request


def main():
    binary = Path(sys.argv[1]).resolve(strict=True)
    # Fail clearly instead of accidentally testing a service already on port 8080.
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 8080))

    # Ignore proxy environment variables for this loopback-only check.
    client = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def get(path):
        with client.open("http://127.0.0.1:8080" + path, timeout=1) as response:
            if response.status != 200:
                raise RuntimeError(f"Unexpected HTTP status: {response.status}")
            return json.load(response)

    with tempfile.TemporaryFile(mode="w+") as log:
        process = subprocess.Popen([str(binary)], stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 15
            while True:
                if process.poll() is not None:
                    raise RuntimeError(f"Service exited during startup: {process.returncode}")
                try:
                    price = get("/api/market/prices/AAPL")
                    break
                except (urllib.error.URLError, TimeoutError):
                    if time.monotonic() >= deadline:
                        raise RuntimeError("Service did not become ready within 15 seconds")
                    time.sleep(0.25)

            if price.get("symbol") != "AAPL" or not all(
                isinstance(price.get(field), (int, float))
                for field in ["bid", "ask", "last", "volume", "timestamp"]
            ):
                raise RuntimeError(f"Invalid price snapshot: {price}")
            book = get("/api/market/orderbook/AAPL")
            if book != {"symbol": "AAPL", "bids": [], "asks": []}:
                raise RuntimeError(f"Unexpected initial order book: {book}")
            if get("/api/market/prices/BTC%2FUSD").get("symbol") != "BTC/USD":
                raise RuntimeError("URL-encoded symbol lookup failed")
            try:
                get("/api/market/prices/DOES_NOT_EXIST")
            except urllib.error.HTTPError as error:
                if error.code != 404:
                    raise
            else:
                raise RuntimeError("Unknown symbol did not return HTTP 404")

            process.terminate()
            if process.wait(timeout=10) != 0:
                raise RuntimeError(f"Service did not shut down cleanly: {process.returncode}")
            print("Smoke test passed: prices, order book, encoded symbols, HTTP 404, shutdown")
        except BaseException:
            log.seek(0)
            print(log.read(), file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()


if __name__ == "__main__":
    main()
