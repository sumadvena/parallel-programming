#include <errno.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

const int PHILOSOPHERS = 5;
const int FORKS = PHILOSOPHERS;
typedef enum { HUNGRY, EATING, THINKING } State;

typedef struct {
  omp_lock_t fork_lock;
} Fork;

typedef struct {
  Fork *left;
  Fork *right;
  int last_meal_time;
  State state;
} Philosopher;

int msleep(long tms) {
  struct timespec ts;
  int ret;

  if (tms < 0) {
    errno = EINVAL;
    return -1;
  }

  ts.tv_sec = tms / 1000;
  ts.tv_nsec = (tms % 1000) * 1000000;

  do {
    ret = nanosleep(&ts, &ts);
  } while (ret && errno == EINTR);

  return ret;
}

void init_forks(Fork *fork) {
#pragma omp parallel for num_threads(FORKS)
  for (int i = 0; i < FORKS; i++) {
    omp_init_lock(&fork[i].fork_lock);
    printf("%d fork initialized\n", i);
  }
}

void destroy_forks(Fork *fork) {
#pragma omp parallel for num_threads(FORKS)
  for (int i = 0; i < FORKS; i++) {
    omp_destroy_lock(&fork[i].fork_lock);
    printf("%d fork destroyed\n", i);
  }
}

bool aquire_fork(Fork *fork) {
  if (omp_test_lock(&fork->fork_lock)) {
    printf("\tFork %p acquired successfully\n", (void *)fork);
    return true;
  }

  // desired fork is taken
  return false;
}

void finished_fork(Fork *fork) {
  printf("\tFork %p dropped\n", (void *)fork);
  omp_unset_lock(&fork->fork_lock);
}

void init_philosophers(Philosopher *philosopher, Fork *fork) {
#pragma omp parallel for num_threads(PHILOSOPHERS)
  for (int i = 0; i < PHILOSOPHERS; i++) {
    philosopher[i].last_meal_time = 0;
    philosopher[i].state = HUNGRY;
    philosopher[i].left = &fork[i];
    if (i == PHILOSOPHERS - 1)
      philosopher[i].right = &fork[0];
    else
      philosopher[i].right = &fork[i + 1];
  }
  printf("\n\nInitialized all philosophers...\n");
  for (int i = 0; i < PHILOSOPHERS; i++) {
    printf("\tPhilo %d, is %d %d %p %p \n", i, philosopher[i].last_meal_time,
           philosopher[i].state, (void *)philosopher[i].left,
           (void *)philosopher[i].right);
  }
}

void change_philosopher_state(Philosopher *philosopher, int id) {
  if (philosopher->state == HUNGRY) {
    philosopher->state = EATING;
  } else if (philosopher->state == EATING) {
    philosopher->state = THINKING;
  } else {
    philosopher->state = HUNGRY;
  }

  printf("Philosopher %d is %d\n", id, philosopher->state);
}

void be_a_philosopher(Philosopher *philosopher, int id) {
  msleep(rand() % 1000);
  if (philosopher->state == HUNGRY) {
    if (aquire_fork(philosopher->left)) {
      if (aquire_fork(philosopher->right)) {
        change_philosopher_state(philosopher, id);
        sleep(1);
        finished_fork(philosopher->left);
        finished_fork(philosopher->right);
        change_philosopher_state(philosopher, id);
        sleep(2);
      } else {
        finished_fork(philosopher->left);
        be_a_philosopher(philosopher, id);
      }
    } else {
      be_a_philosopher(philosopher, id);
    }
  } else if (philosopher->state == THINKING) {
    change_philosopher_state(philosopher, id);
    be_a_philosopher(philosopher, id);
  }
}

int main(int argc, char **argv) {
  srand(time(NULL));
  Fork forks[FORKS];
  Philosopher philosophers[PHILOSOPHERS];
  init_forks(forks);
  init_philosophers(philosophers, forks);

#pragma omp parallel for num_threads(PHILOSOPHERS)
  for (int i = 0; i < 100; i++) {
    int id = omp_get_thread_num();
    be_a_philosopher(&philosophers[id], id);
  }

  destroy_forks(forks);
}
