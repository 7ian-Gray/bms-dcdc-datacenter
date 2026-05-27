#!/bin/bash
set -e

QT_PATH="$HOME/Qt/6.11.0/macos"

cmake --fresh -S app/bms_dcdc_monitor \
  -B build/bms_dcdc_monitor \
  -DCMAKE_PREFIX_PATH="$QT_PATH" \
  -DQt6_DIR="$QT_PATH/lib/cmake/Qt6" \
  -DQt6Charts_DIR="$QT_PATH/lib/cmake/Qt6Charts"

cmake --build build/bms_dcdc_monitor
