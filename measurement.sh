#!/bin/sh
[ -z ${PERF_ARGS+x} ] && { echo "PERF_ARGS not set" >&2; exit 1; }
[ -z ${THRESHOLD+x} ] && { echo "THRESHOLD not be set" >&2; exit 1; }
[ -z ${DURATION+x} ] && { echo "DURATION not be set" >&2; exit 1; }

echo "RHEL-86425 perf-poke measurement, running at $(date)"
echo
echo "Kernel version: $(uname -r)"
perf --version
echo "/proc/cmdline:"
cat /proc/cmdline
echo

echo "Starting perf with arguments: $PERF_ARGS"
chrt -f 10 taskset -c 0 perf $PERF_ARGS -- sleep $DURATION & PERF_PID=$!
echo "perf running, PID $PERF_PID"
sleep 5
ps -p $PERF_PID || { echo "perf not running, aborting" >&2; exit 1; }

echo "Starting perf_poke with perf PID: $PERF_PID and threshold: $THRESHOLD ns"
chrt -f 5 taskset -c 0 ./perf_poke $PERF_PID $THRESHOLD & POKE_PID=$!
echo "perf_poke running, PID $POKE_PID"
echo

echo "...waiting on measurement to end"
ps -p $PERF_PID > /dev/null && wait $PERF_PID
echo
echo "Measurement ended, stopping perf_poke..."
kill -INT $POKE_PID
ps -p $POKE_PID > /dev/null && wait $POKE_PID
echo "perf_poke stopped"
echo
echo "Please collect perf data from container if running inside one"
