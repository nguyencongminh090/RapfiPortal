#!/bin/bash
set -e

echo "========================================="
echo " Building Portal Engine (portal_src)"
echo "========================================="
cd portal_src
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release -j$(nproc)
cd ../..

echo "========================================="
echo " Building UI (gomoku-portal-ui)"
echo "========================================="
cd gomoku-portal-ui
meson setup build --buildtype=release || true
ninja -C build
cd ..

echo "========================================="
echo " Updating Executable folder"
echo "========================================="
cp portal_src/build/pbrain-MINT-P Executable/pbrain-MINT-P
cp gomoku-portal-ui/build/gomoku-portal-ui Executable/PortalGomoku
chmod +x Executable/pbrain-MINT-P
chmod +x Executable/PortalGomoku

echo "========================================="
echo " Packaging distribute_pack.tar.gz"
echo "========================================="
rm -rf distribute_pack
mkdir -p distribute_pack

# Copy Executable folder
cp -r Executable distribute_pack/

# Compress
tar -czvf distribute_pack.tar.gz distribute_pack/

echo "========================================="
echo " Distribution Package Created: distribute_pack.tar.gz"
echo "========================================="
