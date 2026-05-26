#!/bin/bash
set -e

QT_PATH="$HOME/Qt/6.11.0/macos"

cmake -S app/bms_dcdc_monitor \
  -B build/bms_dcdc_monitor \
  -DCMAKE_PREFIX_PATH="$QT_PATH"

cmake --build build/bms_dcdc_monitor
