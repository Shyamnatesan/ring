# Ring

A Bounded Lock-Free Single-Producer, Single-Consumer queue. 

```cpp
shyamnatesan::Ring<int> ring(1024, {});
ring.TryPush(1);
int got = -1;
ring.TryPop(got);
```

## Benchmark

One operation is one completed transfer: a value pushed and then popped. The payload is an `int`, the capacity is 1024, and a run is 10 million transfers, so the ring wraps. Release build, GCC 15.2. The below figure is the median of four process runs.

Machine: AMD RYzen 5 4600H, 6 cores / 12 threads, Linux, producer on core 0, consumer on core 2.

| Setup | Transfers/sec |
|---|---|
| Padded indices, pinned | 156M |
| Padded indices, unpinned | 96M |
| No padding, pinned | 55M |

Top result: **156M transfers/sec**, 6.4 ns per transfer, cores 0 and 2.
