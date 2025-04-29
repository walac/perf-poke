#include <linux/bpf.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "GPL";

struct hrtimer;

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1);
} perf_wake_up SEC(".maps");

const volatile unsigned long long threshold = 10000;
unsigned long long entry_time = 0;

SEC("fentry/tick_nohz_handler")
int BPF_PROG(tick_entry, struct hrtimer *timer)
{
    if (bpf_get_smp_processor_id() != 0)
        return 0;

    entry_time = bpf_ktime_get_ns();
    return 0;
}

SEC("fexit/tick_nohz_handler")
int BPF_PROG(tick_exit, struct hrtimer *timer)
{
    unsigned long long exit_time;
    int value = 0;

    if (bpf_get_smp_processor_id() != 0)
        return 0;

    if (!entry_time)
        /* Hit exit before entry */
        return 0;

    exit_time = bpf_ktime_get_ns();
    if (exit_time < entry_time)
        /* Something is wrong */
        goto exit;

    if (exit_time - entry_time > threshold) {
        /* Threshold violated, wake up userspace */
        bpf_ringbuf_output(&perf_wake_up, &value, sizeof(value), 0);
    }

exit:
    entry_time = 0;
    return 0;
}
