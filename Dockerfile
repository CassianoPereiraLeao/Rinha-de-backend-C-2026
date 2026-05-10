FROM gcc:14-bookworm AS builder
WORKDIR /app

RUN apt-get update && apt-get install -y zlib1g-dev && rm -rf /var/lib/apt/lists/*
 
COPY . .
 
RUN gcc -O2 -o prepare_refs tools/prepare_refs.c -lz
 
RUN ./prepare_refs resources/references.json.gz resources/references.bin resources/labels.bin
 
RUN gcc src/*.c \
    -O3 \
    -march=x86-64-v3 \
    -flto \
    -fomit-frame-pointer \
    -pipe \
    -o backend \
    -lm
 
FROM debian:bookworm-slim
WORKDIR /app
 
RUN mkdir -p /sockets && chmod 777 /sockets

COPY --from=builder /app/backend .
COPY --from=builder /app/resources/references.bin resources/
COPY --from=builder /app/resources/labels.bin      resources/
 
RUN chmod +x ./backend
 
CMD ["./backend"]