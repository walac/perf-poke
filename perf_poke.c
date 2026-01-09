#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "perf_poke.skel.h"

static int loop = 1;

static void sigint(int)
{
    loop = 0;
}

static int handle_rb_event(void *ctx, void *data, size_t data_sz)
{
    return 0;
}

int main(int argc, char **argv)
{
    int perf_pid, threshold, cpu, over = 0, ret, retval = 0;
    void *timerlat_irq;
    struct perf_poke_bpf *bpf;
    struct ring_buffer *rb;

    /* Parse command line arguments */
    if (argc != 5) {
        printf("Usage: perf_poke PERF_PID THRESHOLD CPU TIMERLAT_IRQ_ADDRESS\n");
        return 1;
    }

    perf_pid = atoi(argv[1]);
    threshold = atoi(argv[2]);
    cpu = atoi(argv[3]);
    timerlat_irq = (void *) strtoul(argv[4], NULL, 0);

    /* Open BPF program */
    bpf = perf_poke_bpf__open();
    if (!bpf) {
        fprintf(stderr, "perf_poke: BPF program open failed\n");
        retval = 1;
        goto bpf_destroy;
    }

    /* Set up and load BPF program */
    bpf->rodata->threshold = threshold;
    bpf->rodata->cpu = cpu;
    bpf->bss->timerlat_irq = timerlat_irq;
    ret = perf_poke_bpf__load(bpf);
    if (ret) {
        fprintf(stderr, "perf_poke: BPF program load failed\n");
        retval = 1;
        goto bpf_destroy;
    }

    /* Attach BPF program */
    ret = perf_poke_bpf__attach(bpf);
    if (ret) {
        fprintf(stderr, "perf_poke: BPF program attach failed\n");
        retval = 1;
        goto bpf_destroy;
    }

    /* Set up ring buffer */
    rb = ring_buffer__new(bpf_map__fd(bpf->maps.perf_wake_up),
                          handle_rb_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "perf_poke: ringbuffer setup failed\n");
        retval = 1;
        goto bpf_detach;
    }

    /* Main loop */
    signal(SIGINT, sigint);
    while (loop) {
        /* Wait on poke from BPF side */
        ret = ring_buffer__poll(rb, -1);

        if (ret < 0)
            continue;

        /* Poke perf */
        kill(perf_pid, SIGUSR2);
        ++over;
    }

    printf("perf_poke: threshold exceeded %d times\n", over);

rb_free:
    ring_buffer__free(rb);
bpf_detach:
    perf_poke_bpf__detach(bpf);
bpf_destroy:
    perf_poke_bpf__destroy(bpf);
    return retval;
}
