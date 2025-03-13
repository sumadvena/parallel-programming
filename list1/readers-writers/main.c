#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define PAGES 3
#define THREADS 8

void
writer(int id, int* array, int* read_counter)
{
    int index = rand() % PAGES;
    int value = rand() % 100;
#pragma omp atomic write
    array[index] = value;
    fprintf(stdout, "\t\t Write %i - %i - p. %i\n", id, value, index);
#pragma omp atomic write
    (*read_counter) = 0;
    sleep(1);
}

void
reader(int id, int* array, int* read_counter)
{
    int index = rand() % PAGES;
    int value = 0;
    value = array[index];
    fprintf(stdout, "Read %i - %i - p. %i\n", id, value, index);
#pragma omp atomic update
    (*read_counter)++;
    sleep(1);
}

int
main(int argv, char** argc)
{
    srand(time(NULL));
    int read_counter = 0;
    bool first_write = true;
    int array[PAGES];

#pragma omp parallel num_threads(THREADS)
    {

        /*while (true) {*/
            #pragma omp parallel for
            for (int i = 0; i < THREADS; i++) {
                if (i < 3) {
                    if (first_write || read_counter >= 3) {
                        writer(i, array, &read_counter);
                    #pragma omp critical
                        printf("%i\n", i);
                        first_write = false;
                        continue;
                    }
                } else {
                    if (!first_write) {
                        reader(i, array, &read_counter);
                    #pragma omp critical
                        printf("%i\n", i);
                        continue;
                    }
                }
            }
        /*}*/
    }

    return 0;
}
