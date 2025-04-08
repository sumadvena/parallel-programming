#include <math.h>
#include <omp.h>
#include <stdio.h>

int main(int argc, char **argv) {
  int N = 0;
  double_t total_sum = 0.0;
  fprintf(stdout, "n: ");
  scanf("%d", &N);

#pragma omp parallel for reduction(+ : total_sum)
  for (int i = 1; i <= N; i++) {
    total_sum += 1.0 / i;
  }

  total_sum -= log((double_t)N);

  fprintf(stdout, "\ng_n = %lf\n", total_sum);
}
