#include <linux/bpf.h>
#include <bpf/bpf_tracing.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif

char LICENSE[] SEC("license") = "GPL";

struct hrtimer;
typedef long long ktime_t;

struct trace_event_raw_hrtimer_start {
    struct hrtimer *hrtimer;
    void *function;
    ktime_t expires;
} __attribute__((preserve_access_index));

struct trace_event_raw_hrtimer_expire_entry {
    struct hrtimer *hrtimer;
    void *function;
    ktime_t now;
} __attribute__((preserve_access_index));

typedef void *poke_key_t;
typedef ktime_t poke_value_t;

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1);
} perf_wake_up SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_HASH);
    __uint(max_entries, 64);
    __uint(key_size, sizeof(poke_key_t));
    __uint(value_size, sizeof(poke_value_t));
} entry_time SEC(".maps");

const volatile unsigned long long threshold = 10000;
// rtla timerlat uses timerlat_irq, cyclictest uses hrtimer_wakeup
const volatile void *timer_cbs[2];

static int is_latency_timer_cb(const void *p)
{
    unsigned long i;

    for (i = 0; i < ARRAY_SIZE(timer_cbs); ++i) {
        if (p == timer_cbs[i])
            return 1;
    }

    return 0;
}

// volatile to avoid the compiler to optimize it to a single byte value
static volatile int threshold_hit = 0;

static inline int update_time_entry(const poke_key_t key, poke_value_t timestamp)
{
    return bpf_map_update_elem(&entry_time, &key, &timestamp, 0);
}

static int handle_exit(const poke_key_t key, poke_value_t exit_time)
{
    poke_value_t *pentry_time, timestamp, delta;
    int value = 0;

    if (threshold_hit) /* avoid multiple triggers */
        return 0;

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
        threshold_hit = 1;
        bpf_ringbuf_output(&perf_wake_up, &value, sizeof(value), 0);
        bpf_printk("cpu %d %lld", bpf_get_smp_processor_id(), exit_time - timestamp);
    }

exit:
    timestamp = 0;
    update_time_entry(key, 0);
    return 0;
}

SEC("tp/timer/hrtimer_start")
int hrtimer_start(struct trace_event_raw_hrtimer_start *args)
{
    if (!is_latency_timer_cb(args->function))
        return 0;

    return update_time_entry(args->hrtimer, args->expires);
}

SEC("tp/timer/hrtimer_expire_entry")
int hrtimer_expire_entry(struct trace_event_raw_hrtimer_expire_entry *args)
{
    if (!is_latency_timer_cb(args->function))
        return 0;

    return handle_exit(args->hrtimer, args->now);
}

