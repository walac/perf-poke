#!/bin/sh
[ -z ${PERF_ARGS+x} ] && { echo "PERF_ARGS not set" >&2; exit 1; }
[ -z ${THRESHOLD+x} ] && { echo "THRESHOLD not set" >&2; exit 1; }
[ -z ${DURATION+x} ] && { echo "DURATION not set" >&2; exit 1; }
[ -z ${CPU+x} ] && { echo "CPU not set" >&2; exit 1; }

# Print useful information
echo "RHEL-86425 perf-poke measurement, running at $(date)"
echo
echo "Kernel version: $(uname -r)"
perf --version
echo "/proc/cmdline:"
cat /proc/cmdline
echo

# Disable taskset if requested. This is useful on OCP
[ "$DISABLE_TASKSET" == "1" ] && echo "Running with taskset disabled" || TASKSET="taskset -c 0"

# Start perf first, perf_poke needs its PID
echo "Starting perf with arguments: $PERF_ARGS"
chrt -f 10 $TASKSET perf $PERF_ARGS -o perf.data -- sleep $DURATION & PERF_PID=$!
echo "perf running, PID $PERF_PID"

# Start perf_poke
echo "Starting perf_poke with perf PID: $PERF_PID and threshold: $THRESHOLD ns"
chrt -f 5 $TASKSET ./perf_poke $PERF_PID $THRESHOLD $CPU & POKE_PID=$!
echo "perf_poke running, PID $POKE_PID"
echo

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
tar -czf perf_data.tar.gz perf.data/

# Wait for data to be collected
echo
echo "perf data is collected as perf_data.tar.gz"
echo "Please collect perf data from container if running inside one"
echo "After the data is collected, this script can be stopped"
exec sleep infinity
