#!/usr/bin/env bash
set -euo pipefail

binary=${1:?Usage: package-release.sh BINARY vMAJOR.MINOR.PATCH}
version=${2:?Usage: package-release.sh BINARY vMAJOR.MINOR.PATCH}
if [[ ! "$version" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?(\+[0-9A-Za-z.-]+)?$ ]]; then
  echo "Expected a version such as v1.0.0 or v1.0.0-rc.1" >&2
  exit 1
fi
if [[ ! -x "$binary" ]]; then
  echo "Executable not found: $binary" >&2
  exit 1
fi

package="exchange-engine-$version-linux-x86_64"
mkdir -p "dist/$package"
install -m 755 "$binary" "dist/$package/market_data_service"
strip --strip-unneeded "dist/$package/market_data_service"
cp README.md "dist/$package/README.md"
ldd "dist/$package/market_data_service" > "dist/$package/RUNTIME-DEPENDENCIES.txt"
cat > "dist/$package/RUNNING.md" <<'EOF'
# Run the Exchange Engine

This is a Linux x86-64 binary, built on Ubuntu 24.04 with GCC 13.
It uses the system's glibc and C++ runtime; it is not a static binary.
Ubuntu 24.04 users can install the runtime with:

    sudo apt-get install libc6 libgcc-s1 libstdc++6

Run it from this directory:

    ./market_data_service

It serves simulated market data on port 8080. In another terminal:

    curl http://127.0.0.1:8080/api/market/prices/AAPL
    curl http://127.0.0.1:8080/api/market/orderbook/AAPL

Press Ctrl+C to stop. This package runs locally and includes no cloud deployment.
EOF
tar -C dist -czf "dist/$package.tar.gz" "$package"
(
  cd dist
  sha256sum "$package.tar.gz" > SHA256SUMS
)
echo "Created dist/$package.tar.gz and dist/SHA256SUMS"
