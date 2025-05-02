#!/bin/sh
[ -z ${PERF_ARGS+x} ] && { echo "PERF_ARGS not set" >&2; exit 1; }
[ -z ${THRESHOLD+x} ] && { echo "THRESHOLD not be set" >&2; exit 1; }
[ -z ${DURATION+x} ] && { echo "DURATION not be set" >&2; exit 1; }

# Print useful information
echo "RHEL-86425 perf-poke measurement, running at $(date)"
echo
echo "Kernel version: $(uname -r)"
perf --version
echo "/proc/cmdline:"
cat /proc/cmdline
echo

# Start perf first, perf_poke needs its PID
echo "Starting perf with arguments: $PERF_ARGS"
chrt -f 10 taskset -c 0 perf $PERF_ARGS -o perf.data.2 -- sleep $DURATION & PERF_PID=$!
echo "perf running, PID $PERF_PID"

# Wait for five seconds for perf to initialize
sleep 5
ps -p $PERF_PID > /dev/null || { echo "perf not running, aborting" >&2; exit 1; }

# Start perf_poke
echo "Starting perf_poke with perf PID: $PERF_PID and threshold: $THRESHOLD ns"
chrt -f 5 taskset -c 0 ./perf_poke $PERF_PID $THRESHOLD & POKE_PID=$!
echo "perf_poke running, PID $POKE_PID"
echo

# Collect kcore after perf_poke's BPF program is JITed
sleep 5
echo "Collecting kcore..."
perf record --no-samples --kcore true || { echo "kcore collect failed" >&2; kill -INT $POKE_PID; kill -INT $PERF_PID; exit 1; }

# Wait on measurement to end
echo "...waiting on measurement to end"
ps -p $PERF_PID > /dev/null && wait $PERF_PID
chmod -R a+r perf.data*
echo
echo "Measurement ended, stopping perf_poke..."
kill -INT $POKE_PID
ps -p $POKE_PID > /dev/null && wait $POKE_PID

# Collect data
echo "perf_poke stopped, creating an archive from perf data"
mv perf.data.2 perf.data/data
tar -czf perf_data.tar.gz perf.data/

# Wait for data to be collected
echo
echo "perf data is collected as perf_data.tar.gz"
echo "Please collect perf data from container if running inside one"
echo "After the data is collected, this script can be stopped"
exec sleep infinity
