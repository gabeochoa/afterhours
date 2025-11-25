# Afterhours ECS Performance Benchmark

This example provides comprehensive performance benchmarking for the afterhours ECS library. Use it to establish performance baselines before SOA migration and measure improvements after implementation.

## Usage

### Run All Benchmarks
```bash
cd vendor/afterhours/example/performance_benchmark
make
```

### Run Specific Benchmark Categories
```bash
./performance_benchmark.exe --creation   # Entity creation benchmarks
./performance_benchmark.exe --access     # Component access benchmarks
./performance_benchmark.exe --queries    # Query performance benchmarks
./performance_benchmark.exe --systems   # System iteration benchmarks
./performance_benchmark.exe --memory    # Memory usage benchmarks
./performance_benchmark.exe --scaling    # Query scaling benchmarks
```

## What It Measures

### Entity Creation
- Time to create 10,000 entities with random component combinations
- Measures: creation overhead, merge performance

### Component Access
- Direct component access patterns
- Multiple component access patterns
- Measures: component lookup performance, cache behavior

### Query Performance
- Single component queries
- Multiple component queries (2, 3 components)
- Queries with tags
- `gen_first()` performance
- `gen_count()` performance
- Measures: query filtering speed, fingerprint checking

### System Iteration
- System loop performance with multiple systems
- Measures: iteration overhead, system dispatch cost

### Memory Usage
- Memory footprint at different entity counts (1K, 10K, 100K)
- Measures: memory scaling, overhead per entity

### Query Scaling
- Query performance across entity counts (100 to 50,000)
- Measures: O(n) scaling behavior, performance degradation

## Output Format

Each benchmark reports:
- **Min**: Fastest execution time
- **Max**: Slowest execution time
- **Mean**: Average execution time
- **Median**: Median execution time
- **Iterations**: Number of benchmark runs

Times are reported in microseconds (us).

## Configuration

Edit `main.cpp` to adjust:
- `NUM_ENTITIES`: Default entity count (default: 10,000)
- `QUERY_ITERATIONS`: Number of query benchmark iterations (default: 1000)
- Component types and distributions
- Entity creation patterns

## Use Cases

1. **Before SOA Migration**: Run benchmarks to establish baseline performance
2. **After SOA Migration**: Run benchmarks to measure improvements
3. **Performance Debugging**: Identify slow queries or operations
4. **Optimization Validation**: Verify optimizations actually improve performance
5. **Regression Testing**: Ensure performance doesn't degrade over time

## Example Output

```
Afterhours ECS Performance Benchmark
=====================================

=== Query Performance Benchmark ===

Query: Transform + Velocity:
  Iterations: 100
  Min:        140.42 us
  Max:        298.17 us
  Mean:       179.90 us
  Median:     177.33 us
```

## Notes

- Benchmarks use fixed seed (42) for reproducibility
- Uses `-O2 -march=native` for realistic performance
- Includes warmup iterations to avoid cold start effects
- Measures wall-clock time, not CPU time

