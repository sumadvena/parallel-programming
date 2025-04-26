#include <limits.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define FILENAME "five_d.txt"
#define DIMENSION 5

int best_distance = INT_MAX;
int *best_tour = NULL;
int tsp_data_matrix[DIMENSION][DIMENSION];

bool read_data(size_t rows, size_t cols, int (*a)[cols]) {
  FILE *tsp_data_file;
  tsp_data_file = fopen(FILENAME, "r");
  if (tsp_data_file == NULL)
    return EXIT_FAILURE;

  for (size_t i = 0; i < rows; ++i) {
    for (size_t j = 0; j < cols; ++j)
      fscanf(tsp_data_file, "%d", a[i] + j);
  }

  fclose(tsp_data_file);
  return true;
}

int *create_array() {
  int *arr = malloc(sizeof arr * DIMENSION);
  if (!arr)
    return NULL;
#pragma omp parallel for schedule(runtime)
  for (int i = 0; i < DIMENSION; i++) {
    arr[i] = i;
  }

  return arr;
}

void print_array(int *array) {
  for (int i = 0; i < DIMENSION; i++) {
    printf("%d ", array[i]);
  }
  printf("\n");
}

void swap(int *a, int *b) {
  int temp = *a;
  *a = *b;
  *b = temp;
}

void compute_distance(int tour[], int first_element) {
  int distance = 0;
  // from first to a second city
  distance += tsp_data_matrix[first_element][tour[0]];
#pragma omp parallel for reduction(+ : distance)
  for (int i = 0; i < DIMENSION - 1; i++) {
    distance += tsp_data_matrix[tour[i]][tour[i + 1]];
  }
  // to starting city
  // distance += tsp_data_matrix[tour[DIMENSION - 1]][first_element];
#pragma omp critical
  {
    if (distance < best_distance) {
      best_distance = distance;
      best_tour = tour;
      printf("New best distance: %d\n", best_distance);
      printf("New best tour: ");
      print_array(best_tour);
    }
  }
}

void heaps_permutation(int *array, int size, int first_element) {
  if (size == 1) {
    compute_distance(array, first_element);
    return;
  }

#pragma omp parallel for schedule(runtime)
  for (int i = 0; i < size; i++) {
    heaps_permutation(array, size - 1, first_element);

    if (size % 2 == 0) {
#pragma omp critical
      {
        swap(&array[i], &array[size - 1]);
      }
    } else {
#pragma omp critical
      {
        swap(&array[0], &array[size - 1]);
      }
    }
  }
}

void generate_permutations(int *array) {
  int first_element = array[0]; // first city is not relevant and can stay first
  heaps_permutation(array + 1, DIMENSION - 1, first_element);
}

int main(int argv, char **argc) {
  int *permutation_arr = create_array();

  print_array(permutation_arr);
  read_data(DIMENSION, DIMENSION, tsp_data_matrix);

#pragma omp parallel
  {
#pragma omp single
    {

      printf("%d\n", omp_get_max_threads());
      generate_permutations(permutation_arr);
    }
  }

  printf("Best distance: %d\nTour: ", best_distance);
  print_array(best_tour);
  free(permutation_arr);
  return EXIT_SUCCESS;
}
