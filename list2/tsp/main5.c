#include <limits.h>
#include <omp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILENAME "five_d.txt"
#define DIMENSION 5
#define TASK_CUTOFF 8 // Control task granularity

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

void compute_distance(int *tour) {
  int distance = 0;

  // Distances between cities in the tour
#pragma omp parallel for reduction(+ : distance)
  for (int i = 0; i < DIMENSION; i++) {
    distance += tsp_data_matrix[tour[i]][tour[i + 1]];
  }

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

void greedy(int *array) {
  int *tour = calloc(DIMENSION + 1, sizeof tour);
  int *visited = calloc(DIMENSION, sizeof tour);
  visited[0] = 1; // flag first city as visited
  tour[0] = array[0];
  tour[DIMENSION] = array[0]; // last element is the first city
  for (int i = 1; i < DIMENSION; i++) {
    int minimum = INT_MAX;
    int choosen_city;
    for (int j = 1; j < DIMENSION; j++) {
      if (visited[j] == 1)
        continue;

      if (tsp_data_matrix[tour[i - 1]][j] < minimum) {
        minimum = tsp_data_matrix[tour[i - 1]][j];
        choosen_city = j;
      }
    }
    tour[i] = choosen_city;
    visited[choosen_city] = 1;
    printf("Tour: ");
    print_array(tour);
    printf("Visited: ");
    print_array(visited);
  }

  free(visited);
  compute_distance(tour);
  free(tour);
}

void generate_permutations(int *array) { greedy(array); }

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
