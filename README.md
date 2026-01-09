# perf-poke

A BPF-based tool that monitors timer latency and signals `perf` when a threshold is exceeded.
This enables capturing Intel PT traces precisely when latency issues occur, making it useful for
debugging real-time latency problems in conjunction with `rtla timerlat`.

## How it Works

perf-poke attaches a BPF program to monitor `hrtimer_expire_entry` events. When the latency
exceeds a configured threshold on a specific CPU, it sends `SIGUSR2` to a running `perf record`
process, triggering a snapshot capture.

## Requirements

- Fedora/RHEL-based system (or adapt package names for your distro)
- Root privileges
- Kernel with BPF and Intel PT support
- [Intel XED](https://github.com/intelxed/xed) (X86 Encoder Decoder) for decoding Intel PT traces

## Building

### Install Intel XED

Follow the instructions at https://github.com/intelxed/xed to build and install Intel XED.

### Build perf-poke

```bash
git clone https://github.com/walac/perf-poke
cd perf-poke
dnf install -y libbpf-devel llvm-devel bpftool clang
make
```

## Usage

The measurement requires two terminals: one for perf-poke and one for rtla timerlat.

### Terminal 1: Run perf-poke

This example assumes you want to monitor CPU 41.

Configure environment variables and start the measurement:

```bash
export CPU=41
export THRESHOLD=30000
export DURATION=3h
export PERF_ARGS="record -m 4G --cpu $CPU --no-switch-events --kcore -e intel_pt/cyc,noretcomp/k -S"

./measurement.sh
```

| Variable    | Description                                         |
|-------------|-----------------------------------------------------|
| `CPU`       | CPU core to monitor                                 |
| `THRESHOLD` | Latency threshold in nanoseconds                    |
| `DURATION`  | Measurement duration (e.g., `3h`, `30m`)            |
| `PERF_ARGS` | Arguments passed to `perf record`                   |

### Terminal 2: Run rtla timerlat

In a separate terminal, set up and run rtla timerlat:

```bash
export TIMERLAT_IRQ=$(awk '$3 == "timerlat_irq" { printf("0x%s", $1); }' /proc/kallsyms)
export CPU=41
export THRESHOLD=30
export DURATION=3h

# Set up perf probes
perf probe hrtimer_interrupt
perf probe __sysvec_apic_timer_interrupt

# Run timerlat
rtla timerlat top --dma-latency 0 --aa-only $THRESHOLD -T 500 -i $THRESHOLD -p 200 -t \
    -c $CPU -H 0-2,36-38 -d $DURATION -P f:95 --deepest-idle-state 0 \
    -e timer:hrtimer_expire_entry --filter "function == $TIMERLAT_IRQ" \
    -e timer:hrtimer_start --filter "function == $TIMERLAT_IRQ" \
    -e probe
```

Note: The `THRESHOLD` for rtla timerlat is in microseconds, while perf-poke uses nanoseconds.

## Collecting Traces

Once rtla timerlat reports a latency exceeding the threshold, interrupt the `measurement.sh`
script with `Ctrl-C`. Then use the following command to decode and view the Intel PT traces:

```bash
perf script --deltatime --call-ret-trace -F -cpu,+flags,+callindent --insn-trace --xed
```

This requires Intel XED to be installed for instruction decoding.

## Output

After the measurement completes, perf data is archived to `perf_data.tar.gz` in the current
directory.
