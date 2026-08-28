#ifndef STATS_H
#define STATS_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Statistics data structure */
typedef struct {
    uint64_t *samples;  /* Pointer to user-provided buffer */
    size_t count;
    size_t capacity;
    int is_sorted;  /* Flag to track if samples are sorted */
} stats_t;

/* Initialize statistics structure with user-provided buffer
 * buffer: Pre-allocated memory for samples (must be at least capacity * sizeof(uint64_t))
 * capacity: Maximum number of samples the buffer can hold
 */
static inline void stats_init(stats_t *stats, uint64_t *buffer, size_t capacity)
{
    stats->samples = buffer;
    stats->count = 0;
    stats->capacity = capacity;
    stats->is_sorted = 0;
}

/* Free statistics structure - now a no-op since memory is managed by caller */
static inline void stats_free(stats_t *stats)
{
    /* Memory is managed by the caller, so we just reset the pointers */
    stats->samples = NULL;
    stats->count = 0;
    stats->capacity = 0;
}

/* Add a single measurement/sample */
static inline int stats_add_sample(stats_t *stats, uint64_t value)
{
    if (stats->count >= stats->capacity) {
        /* Buffer is full - cannot add more samples */
        return -1;
    }
    stats->samples[stats->count++] = value;
    stats->is_sorted = 0;  /* Mark as unsorted after adding data */
    return 0;
}

/* Add multiple measurements at once */
static inline int stats_add_samples(stats_t *stats, uint64_t *values, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (stats_add_sample(stats, values[i]) != 0) {
            return -1;  /* Buffer full */
        }
    }
    return 0;
}

/* Comparison function for qsort */
static int compare_uint64(const void *a, const void *b)
{
    uint64_t val_a = *(const uint64_t *)a;
    uint64_t val_b = *(const uint64_t *)b;
    if (val_a < val_b) return -1;
    if (val_a > val_b) return 1;
    return 0;
}

/* Sort samples in place (required for median and percentile calculations) */
static inline void stats_sort(stats_t *stats)
{
    if (!stats->is_sorted && stats->count > 0) {
        qsort(stats->samples, stats->count, sizeof(uint64_t), compare_uint64);
        stats->is_sorted = 1;
    }
}

/* Ensure samples are sorted (called internally by functions that need sorted data) */
static inline void stats_ensure_sorted(stats_t *stats)
{
    stats_sort(stats);
}

/* Calculate minimum value */
static inline uint64_t stats_min(stats_t *stats)
{
    if (stats->count == 0) return 0;
    
    uint64_t min_val = stats->samples[0];
    for (size_t i = 1; i < stats->count; i++) {
        if (stats->samples[i] < min_val) {
            min_val = stats->samples[i];
        }
    }
    return min_val;
}

/* Calculate maximum value */
static inline uint64_t stats_max(stats_t *stats)
{
    if (stats->count == 0) return 0;
    
    uint64_t max_val = stats->samples[0];
    for (size_t i = 1; i < stats->count; i++) {
        if (stats->samples[i] > max_val) {
            max_val = stats->samples[i];
        }
    }
    return max_val;
}

/* Calculate mean (average) */
static inline double stats_mean(stats_t *stats)
{
    if (stats->count == 0) return 0.0;
    
    uint64_t sum = 0;
    for (size_t i = 0; i < stats->count; i++) {
        sum += stats->samples[i];
    }
    return (double)sum / stats->count;
}

/* Calculate median (requires sorted data) */
static inline double stats_median(stats_t *stats)
{
    if (stats->count == 0) return 0.0;
    
    /* Ensure data is sorted */
    stats_ensure_sorted(stats);
    
    double median;
    if (stats->count % 2 == 0) {
        /* Even number of samples - average of two middle values */
        median = (stats->samples[stats->count / 2 - 1] + stats->samples[stats->count / 2]) / 2.0;
    } else {
        /* Odd number of samples - middle value */
        median = stats->samples[stats->count / 2];
    }
    
    return median;
}

/* Calculate percentile (requires sorted data)
 * percentile: value between 0 and 100 (e.g., 95 for 95th percentile)
 */
static inline double stats_percentile(stats_t *stats, double percentile)
{
    if (stats->count == 0) return 0.0;
    if (percentile < 0.0) percentile = 0.0;
    if (percentile > 100.0) percentile = 100.0;
    
    /* Ensure data is sorted */
    stats_ensure_sorted(stats);
    
    double rank = (percentile / 100.0) * (stats->count - 1);
    size_t lower_idx = (size_t)rank;
    size_t upper_idx = lower_idx + 1;
    
    double result;
    if (upper_idx >= stats->count) {
        result = stats->samples[stats->count - 1];
    } else {
        double fraction = rank - lower_idx;
        result = stats->samples[lower_idx] + fraction * (stats->samples[upper_idx] - stats->samples[lower_idx]);
    }
    
    return result;
}

/* Calculate standard deviation */
static inline double stats_stddev(stats_t *stats)
{
    if (stats->count == 0) return 0.0;
    
    double mean = stats_mean(stats);
    double sum_sq_diff = 0.0;
    
    for (size_t i = 0; i < stats->count; i++) {
        double diff = stats->samples[i] - mean;
        sum_sq_diff += diff * diff;
    }
    
    return sqrt(sum_sq_diff / stats->count);
}

/* Calculate variance */
static inline double stats_variance(stats_t *stats)
{
    if (stats->count == 0) return 0.0;
    
    double mean = stats_mean(stats);
    double sum_sq_diff = 0.0;
    
    for (size_t i = 0; i < stats->count; i++) {
        double diff = stats->samples[i] - mean;
        sum_sq_diff += diff * diff;
    }
    
    return sum_sq_diff / stats->count;
}

/* Print basic statistics summary */
static inline void stats_print_summary(stats_t *stats, const char *label)
{
    if (stats->count == 0) {
        printf("%s: No data\n", label);
        return;
    }
    
    printf("\n=== Statistics Summary: %s ===\n", label);
    printf("Sample count:   %zu\n", stats->count);
    printf("Min:            %lu\n", stats_min(stats));
    printf("Max:            %lu\n", stats_max(stats));
    printf("Mean:           %.2f\n", stats_mean(stats));
    printf("Median:         %.2f\n", stats_median(stats));
    printf("Std Dev:        %.2f\n", stats_stddev(stats));
    printf("Variance:       %.2f\n", stats_variance(stats));
    printf("================================\n\n");
}

/* Print detailed statistics with percentiles */
static inline void stats_print_detailed(stats_t *stats, const char *label)
{
    if (stats->count == 0) {
        printf("%s: No data\n", label);
        return;
    }
    
    printf("\n=== Detailed Statistics: %s ===\n", label);
    printf("Sample count:   %zu\n", stats->count);
    printf("Min:            %lu\n", stats_min(stats));
    printf("Max:            %lu\n", stats_max(stats));
    printf("Mean:           %.2f\n", stats_mean(stats));
    printf("Median (50%%):   %.2f\n", stats_median(stats));
    printf("Std Dev:        %.2f\n", stats_stddev(stats));
    printf("Variance:       %.2f\n", stats_variance(stats));
    printf("\nPercentiles:\n");
    printf("  1st:          %.2f\n", stats_percentile(stats, 1.0));
    printf("  5th:          %.2f\n", stats_percentile(stats, 5.0));
    printf("  25th:         %.2f\n", stats_percentile(stats, 25.0));
    printf("  50th:         %.2f\n", stats_percentile(stats, 50.0));
    printf("  75th:         %.2f\n", stats_percentile(stats, 75.0));
    printf("  95th:         %.2f\n", stats_percentile(stats, 95.0));
    printf("  99th:         %.2f\n", stats_percentile(stats, 99.0));
    printf("====================================\n\n");
}

/* Print all samples */
static inline void stats_print_samples(stats_t *stats, const char *label, int samples_per_line)
{
    if (stats->count == 0) {
        printf("%s: No data\n", label);
        return;
    }
    
    printf("\n=== All Samples: %s ===\n", label);
    for (size_t i = 0; i < stats->count; i++) {
        printf("%lu", stats->samples[i]);
        if (i < stats->count - 1) {
            printf(", ");
        }
        if ((i + 1) % samples_per_line == 0) {
            printf("\n");
        }
    }
    if (stats->count % samples_per_line != 0) {
        printf("\n");
    }
    printf("========================\n\n");
}

#endif /* STATS_H */
