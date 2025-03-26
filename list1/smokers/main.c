#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#define K 5
#define L 3
#define M 2

const int SMOKERS_NUM = K;
const int MORTARS_NUM = L;
const int IGNITORS_NUM = M;

void
init_locks(omp_lock_t* lock, const int number)
{
#pragma omp parallel for num_threads(number)
  for (int i = 0; i < number; i++) {
    omp_init_lock(&lock[i]);
  }
}

void
destroy_locks(omp_lock_t* lock, const int number)
{
#pragma omp parallel for num_threads(number)
  for (int i = 0; i < number; i++) {
    omp_destroy_lock(&lock[i]);
  }
}

omp_lock_t*
aquire(omp_lock_t* lock, const int number)
{

  for (int i = 0; i < number; i++) {
    if (omp_test_lock(&lock[i])) {
      return &lock[i];
    }
  }

  // could not aquire a lock
  return NULL;
}

int
main(int argc, char** argv)
{
  omp_lock_t lmortars[MORTARS_NUM], lignitors[IGNITORS_NUM];
  init_locks(lmortars, MORTARS_NUM);
  init_locks(lignitors, IGNITORS_NUM);
  bool used_mortar = false, ignited = false;

#pragma omp parallel for num_threads(SMOKERS_NUM) private(used_mortar, ignited)
  for (int i = 0; i < 50; i++) {
    int smoker = omp_get_thread_num();
    omp_lock_t* mortar_lock = aquire(lmortars, MORTARS_NUM);
    if (mortar_lock == NULL) {
      printf("%d could not acquire a mortar lock\n", smoker);
      sleep(2);
      continue; // Skip to the next iteration
    }
    printf("%d aquired a mortar and uses it\n", smoker);
    sleep(1); // does thing
    used_mortar = true;
    omp_unset_lock(mortar_lock);
    if (used_mortar) {
      omp_lock_t* ignitor_lock = aquire(lignitors, IGNITORS_NUM);
      if (ignitor_lock == NULL) {
        printf("%d could not acquire an ignitor lock\n", smoker);
        sleep(2);
        continue; // Skip to the next iteration
      }
      printf("%d aquired an ignitor and uses it\n", smoker);
      sleep(1); // ignites
      ignited = true;
      omp_unset_lock(ignitor_lock);
    }
    if (used_mortar && ignited) {
#pragma omp critical
      {
        printf("\t\t%d smokes...\n", smoker);
        sleep(2); // smokes
      }
    }
  }

  destroy_locks(lmortars, MORTARS_NUM);
  destroy_locks(lignitors, IGNITORS_NUM);
  printf("finished\n");
}
