#!/usr/bin/env bash

mkdir -p logs
: > logs/summary

for t in \
  "archlinux:latest|pacman -Sy --noconfirm curl" \
  "debian:12|apt-get update -qq && apt-get install -y -qq curl ca-certificates"
do
  img=${t%%|*}
  prep=${t#*|}
  log="logs/$(echo "$img" | tr :/ __).log"

  echo
  echo "========================================"
  echo "== Testing: $img"
  echo "== Log:     $log"
  echo "========================================"

  docker run --rm \
    -e QC_YES=1 \
    -v "$PWD/scripts/install.sh:/install.sh:ro" \
    "$img" \
    sh -c "($prep) >/dev/null 2>&1 &&
           sh /install.sh &&
           . /root/.qc/env &&
           qc --version" 2>&1 | tee "$log"

  if [ "${PIPESTATUS[0]}" -eq 0 ]; then
    echo "PASS $img" | tee -a logs/summary
  else
    echo "FAIL $img" | tee -a logs/summary
  fi
done

echo
echo "========== SUMMARY =========="
cat logs/summary
