#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

const int WRITERS = 3;
const int READERS = 5;

int
main(int argv, char** argc)
{
    srand(time(NULL));

    omp_lock_t isWriting, read;
    omp_init_lock(&isWriting);
    omp_init_lock(&read);

    int readCounter = 3;
    int operationCounter = 0;
    bool firstWrite = true;
    int* array = calloc(READERS, sizeof *array);

    if (!array) {
        printf("Could not allocate the memory. Exiting.\n");
        return 0;
    }

#pragma omp parallel num_threads(WRITERS + READERS)
    while (true) {
        int threadNum = omp_get_thread_num();
        if (threadNum < WRITERS) {
            omp_set_lock(&isWriting);
            if (readCounter >= 3) {
                int writingSpot = rand() % READERS;
                int writeValue = rand() % 100 + 100;
                firstWrite = false;
                readCounter = 0;
                operationCounter++;
                array[writingSpot] = writeValue;
                printf("\t\t %d. %d wrote %d to %d\n",
                       operationCounter,
                       threadNum,
                       writeValue,
                       writingSpot);
            }
            /*if (!omp_test_lock(&isWriting)) {printf("set");}*/
            omp_unset_lock(&isWriting);
        } else {
            if (!firstWrite) {
                // offset needed to use only readers but still be inside the
                // array
                int rindex = threadNum - WRITERS;
                int readValue = array[rindex];
                omp_set_lock(&read);
                readCounter++;
                operationCounter++;
                printf("%d. %d read %d from %d\n",
                       operationCounter,
                       threadNum,
                       readValue,
                       rindex);
                omp_unset_lock(&read);
            }
        }

        if (operationCounter >= 40) {
            break;
        }
    }

    omp_destroy_lock(&read);
    omp_destroy_lock(&isWriting);
    free(array);
}
