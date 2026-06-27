FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ make && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY Makefile .
COPY src/ src/

RUN make

FROM debian:bookworm-slim

WORKDIR /app
COPY --from=builder /app/build/redis_clone .

EXPOSE 6380

CMD ["./redis_clone"]
