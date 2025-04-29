#!/bin/sh
[ -z ${PERF_ARGS+x} ] && { echo "PERF_ARGS not set" >&2; exit 1; }
[ -z ${THRESHOLD+x} ] && { echo "THRESHOLD not be set" >&2; exit 1; }
[ -z ${DURATION+x} ] && { echo "DURATION not be set" >&2; exit 1; }

chrt -f 10 taskset -c 0 perf $PERF_ARGS -- sleep $DURATION & PERF_PID=$!
chrt -f 10 taskset -c 0 ./perf_poke $PERF_PID $THRESHOLD & POKE_PID=$!

wait $PERF_PID
kill -INT $POKE_PID
wait $POKE_PID
