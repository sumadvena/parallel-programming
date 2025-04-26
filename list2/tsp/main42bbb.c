#include <limits.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define FILENAME "dantzig42_d.txt"
#define DIMENSION 42
#define TASK_CUTOFF 8  // Control task granularity

// Global variables
int best_distance = INT_MAX;
int *best_tour = NULL;
int tsp_data_matrix[DIMENSION][DIMENSION];
omp_lock_t best_tour_lock;

// Read distance matrix from file
bool read_data(size_t rows, size_t cols, int (*a)[cols]) {
  FILE *tsp_data_file;
  tsp_data_file = fopen(FILENAME, "r");
  if (tsp_data_file == NULL)
    return false;

  for (size_t i = 0; i < rows; ++i) {
    for (size_t j = 0; j < cols; ++j)
      fscanf(tsp_data_file, "%d", a[i] + j);
  }

  fclose(tsp_data_file);
  return true;
}

// Create an array with values 0 to DIMENSION-1
int *create_array() {
  int *arr = malloc(sizeof(int) * DIMENSION);
  if (!arr)
    return NULL;

  for (int i = 0; i < DIMENSION; i++) {
    arr[i] = i;
  }
  return arr;
}

// Print an array of size DIMENSION
void print_array(int *array) {
  for (int i = 0; i < DIMENSION; i++) {
    printf("%d ", array[i]);
  }
  printf("\n");
}

// Swap two integers
void swap(int *a, int *b) {
  int temp = *a;
  *a = *b;
  *b = temp;
}

// Compute tour distance and update best_tour if better
void compute_distance(int *tour, int first_element) {
  int distance = 0;

  // Distance from first city to second city
  distance += tsp_data_matrix[first_element][tour[0]];

  // Distances between cities in the tour
  for (int i = 0; i < DIMENSION - 1; i++) {
    distance += tsp_data_matrix[tour[i]][tour[i + 1]];
  }

  // Return distance to first city
  distance += tsp_data_matrix[tour[DIMENSION - 1]][first_element];

  // Update global best if this tour is better
  if (distance < best_distance) {
    omp_set_lock(&best_tour_lock);
    if (distance < best_distance) { // Check again inside lock
      best_distance = distance;

      // Create deep copy of the best tour
      if (best_tour == NULL) {
        best_tour = malloc(sizeof(int) * DIMENSION);
      }
      memcpy(best_tour, tour, sizeof(int) * DIMENSION);

      printf("Thread %d found new best distance: %d\n", omp_get_thread_num(),
             best_distance);
    }
    omp_unset_lock(&best_tour_lock);
  }
}

void heaps_permutations(int *array, const int size, int first_element) {
  int *counter_array = calloc(size, sizeof counter_array);

  // default order 123...
  compute_distance(array, first_element);

  int i = 1;
  while (i < size) {
    if (counter_array[i] < i) {
      if (i % 2 == 0) {
        swap(&array[0], &array[i]);
      } else {
        swap(&array[counter_array[i]], &array[i]);
      }
#pragma omp task
      {
        compute_distance(array, first_element);
      }
      counter_array[i] += 1;
      i = 1;
    } else {
      counter_array[i] = 0;
      i++;
    }
  }

  free(counter_array);
}

void generate_permutations(int *array) {
  int first_element = array[0]; // First city stays fixed

#pragma omp parallel
  {
#pragma omp single
    {
      heaps_permutations(array + 1, DIMENSION - 1, first_element);
    }
  }
}

int main(int argc, char **argv) {
  // Set OpenMP configuration
  omp_set_dynamic(0); // Disable dynamic adjustment of threads
  omp_set_nested(1);  // Enable nested parallelism
  omp_set_num_threads(omp_get_num_procs()); // Use all available processors
  omp_init_lock(&best_tour_lock);

  printf("Running with %d threads\n", omp_get_num_procs());

  int *permutation_arr = create_array();
  if (!permutation_arr) {
    fprintf(stderr, "Memory allocation failed\n");
    return EXIT_FAILURE;
  }

  printf("Initial tour: ");
  print_array(permutation_arr);

  if (!read_data(DIMENSION, DIMENSION, tsp_data_matrix)) {
    fprintf(stderr, "Failed to read data file\n");
    free(permutation_arr);
    return EXIT_FAILURE;
  }

  double start_time = omp_get_wtime();
  generate_permutations(permutation_arr);
  double end_time = omp_get_wtime();

  printf("\nComputation completed in %.2f seconds\n", end_time - start_time);
  printf("Best distance: %d\n", best_distance);
  printf("Best tour: ");
  print_array(best_tour);

  // Cleanup
  free(permutation_arr);
  free(best_tour);
  omp_destroy_lock(&best_tour_lock);

  return EXIT_SUCCESS;
}
