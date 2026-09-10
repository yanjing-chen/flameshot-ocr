#!/usr/bin/env bash
set -euo pipefail

OUT="${1:-${PWD}/runtime-build}"
REF="${LLAMA_CPP_REF:-72797e891}"
WORK="${RUNNER_TEMP:-/tmp}/flameshot-ocr-llama"

rm -rf "$WORK"
mkdir -p "$WORK" "$OUT"

echo "Building llama.cpp runtimes from revision: $REF"

git clone --filter=blob:none https://github.com/ggml-org/llama.cpp.git "$WORK/llama.cpp"
git -C "$WORK/llama.cpp" checkout "$REF"

COMMON_FLAGS=(
  -DCMAKE_BUILD_TYPE=Release
  -DGGML_NATIVE=OFF
  -DBUILD_SHARED_LIBS=OFF
  -DLLAMA_CURL=OFF
  -DLLAMA_BUILD_TESTS=OFF
  -DLLAMA_BUILD_EXAMPLES=OFF
)

cmake -S "$WORK/llama.cpp" -B "$WORK/build-vulkan" \
  "${COMMON_FLAGS[@]}" \
  -DGGML_VULKAN=ON

cmake --build "$WORK/build-vulkan" --target llama-server -j"$(nproc)"
cp "$WORK/build-vulkan/bin/llama-server" "$OUT/llama-server-vulkan"

cmake -S "$WORK/llama.cpp" -B "$WORK/build-cpu" \
  "${COMMON_FLAGS[@]}" \
  -DGGML_VULKAN=OFF \
  -DGGML_CUDA=OFF

cmake --build "$WORK/build-cpu" --target llama-server -j"$(nproc)"
cp "$WORK/build-cpu/bin/llama-server" "$OUT/llama-server-cpu"

chmod 0755 "$OUT/llama-server-vulkan" "$OUT/llama-server-cpu"

if command -v strip >/dev/null 2>&1; then
  strip --strip-unneeded "$OUT/llama-server-vulkan" || true
  strip --strip-unneeded "$OUT/llama-server-cpu" || true
fi

echo
echo "Built runtimes:"
ls -lh "$OUT"
