#!/usr/bin/env bash
# Produce per-architecture engine integration files on the universal build runner.
set -euo pipefail
variant=$1
editor=OFF
if [[ $variant == navstudio ]]; then editor=ON; fi
build=build/release
out=out/apple
modules() {
  jq -er '.modules | .cgame, .ui, .qagame' "$1/tce-modules-Release.json"
  jq -er '.library' "$2/omnibot-runtime-Release.json"
}
modules "$build/modules" "$build/core" > "$build/macos-files"
for cpu in x86_64 arm64; do
  dest="$out/macos-$cpu"
  mkdir -p "$dest"
  while IFS= read -r module; do
    file="$dest/$(basename "$module")"
    lipo "$module" -thin "$cpu" -output "$file"
    lipo "$file" -verify_arch "$cpu"
  done < "$build/macos-files"
done

for target in iphoneos-arm64 iphonesimulator-arm64 iphonesimulator-x86_64; do
  sdk=${target%-*}
  cpu=${target#*-}
  platform=IOS
  if [[ $sdk == iphonesimulator ]]; then platform=IOSSIMULATOR; fi
  dir="$build/$target"
  dest="$out/$target"
  cmake -S . -B "$dir" -G Ninja -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="$(xcrun --sdk "$sdk" --show-sdk-path)" \
    -DCMAKE_OSX_ARCHITECTURES="$cpu" -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF -DFEATURE_NAVSTUDIO="$editor" -DTCE_BUILD_OMNIBOT_CORE=ON \
    -DOMNIBOT_BOOST_INCLUDEDIR="$PWD/build/dependencies/boost_1_85_0"
  cmake --build "$dir" --target tce_modules omnibot-et --parallel 4
  modules "$dir" "$dir" > "$dir/files"
  mkdir -p "$dest"
  while IFS= read -r module; do
    lipo "$module" -verify_arch "$cpu"
    xcrun vtool -show-build "$module" | grep -E "platform[[:space:]]+$platform$"
    cp "$module" "$dest/"
  done < "$dir/files"
done
