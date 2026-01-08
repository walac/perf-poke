#include <linux/bpf.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "GPL";

struct hrtimer;
typedef long long ktime_t;

struct trace_event_raw_hrtimer_start {
    struct hrtimer *hrtimer;
    void *function;
    long long expires;
} __attribute__((preserve_access_index));

struct trace_event_raw_hrtimer_expire_entry {
    struct hrtimer *hrtimer;
    void *function;
    long long now;
} __attribute__((preserve_access_index));

typedef void *poke_key_t;
typedef ktime_t poke_value_t;

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1);
} perf_wake_up SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_HASH);
    __uint(max_entries, 10240);
    __uint(key_size, sizeof(poke_key_t));
    __uint(value_size, sizeof(poke_value_t));
} entry_time SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __uint(key_size, sizeof(int));
    __uint(value_size, sizeof(ktime_t));
} now SEC(".maps");

const volatile unsigned long long threshold = 10000;
const volatile int cpu = 0;

static int handle_entry(const poke_key_t key, poke_value_t timestamp)
{
    if (cpu != bpf_get_smp_processor_id())
        return 0;

    if (!timestamp)
        timestamp = bpf_ktime_get_ns();

    return bpf_map_update_elem(&entry_time, &key, &timestamp, 0);
}

static int handle_exit(const poke_key_t key, poke_value_t exit_time)
{
    poke_value_t *pentry_time, timestamp, delta;
    int value = 0;

    if (cpu != bpf_get_smp_processor_id())
        return 0;

    if (!exit_time)
        exit_time = bpf_ktime_get_ns();

    pentry_time = (poke_value_t *) bpf_map_lookup_elem(&entry_time, &key);
    if (!pentry_time)
        return 0;

    timestamp = *pentry_time;
    if (!timestamp)
        return 0;

    if (exit_time < timestamp)
        goto exit;

    delta = exit_time - timestamp;
    if (delta > threshold) {
        /* Threshold violated, wake up userspace */
        bpf_ringbuf_output(&perf_wake_up, &value, sizeof(value), 0);
        bpf_printk("cpu %d %lld", bpf_get_smp_processor_id(), exit_time - timestamp);
    }

exit:
    timestamp = 0;
    bpf_map_update_elem(&entry_time, &key, &timestamp, 0);
    return 0;
}

SEC("tp/timer/hrtimer_start")
int hrtimer_start(struct trace_event_raw_hrtimer_start *args)
{
    return handle_entry(args->hrtimer, args->expires);
}

SEC("tp/timer/hrtimer_expire_entry")
int hrtimer_expire_entry(struct trace_event_raw_hrtimer_expire_entry *args)
{
    int key = 0;

    bpf_map_update_elem(&now, &key, &args->now, 0);
    return 0;
}

SEC("kprobe/timerlat_irq")
int BPF_PROG(timerlat_irq, struct hrtimer *hrtimer)
{
    const int key = 0;
    ktime_t *exit_time;

    exit_time = bpf_map_lookup_elem(&now, &key);
    if (!exit_time || !*exit_time)
        return 0;

    return handle_exit(hrtimer, *exit_time);
}
