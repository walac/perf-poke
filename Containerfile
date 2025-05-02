FROM fedora:41
ARG BUILDDEPS="make gcc libbpf-devel bpftool clang"
ARG DEPS="libbpf perf ps libcgroup-tools rsync"
ARG FUNC="tick_nohz_handler"

RUN dnf install -y $DEPS
RUN mkdir -p /perf_poke
COPY perf_poke.bpf.c /perf_poke
COPY perf_poke.c /perf_poke
COPY Makefile /perf_poke
RUN sed -i "s/tick_nohz_handler/$FUNC/g" /perf_poke/perf_poke.bpf.c
RUN dnf install -y $BUILDDEPS && make -C /perf_poke && dnf remove -y $BUILDDEPS

COPY measurement.sh /perf_poke
WORKDIR /perf_poke
ENTRYPOINT ["/perf_poke/measurement.sh"]
