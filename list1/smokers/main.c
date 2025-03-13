#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define Ksmokers 5
#define Lmortars 3
#define Mignitors 2

/*const int SMOKERS = K;*/
/*const int MORTARS = L;*/
/*const int IGNITORS = M;*/

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

int
aquire(omp_lock_t* lock, const int number)
{
    int free_lock = -1;

    #pragma omp parallel for private(free_lock)
    for (int i = 0; i < number; i++) {
        /*printf("searching for a free lock...\n");*/
        if (!omp_test_lock(&lock[i])) {
            /*printf("lock found at %d\n", i);*/
            #pragma omp atomic write
            free_lock = i;
        }
    }

    /*printf("could not aquire any lock\n");*/
    // could not aquire a lock
    return free_lock;
}

int
main(int argc, char** argv)
{
    omp_lock_t lmortars[Lmortars], lignitors[Mignitors];
    init_locks(lmortars, Lmortars);
    init_locks(lignitors, Mignitors);
    bool used_mortar = false, ignited = false;

#pragma omp parallel for num_threads(Ksmokers) private(used_mortar, ignited)
    for (int i = 0; i < 100; i++) {
        int smoker = omp_get_thread_num();
        int free_mortar_lock = aquire(lmortars, Lmortars);
        if (free_mortar_lock != -1) {
            omp_set_lock(&lmortars[free_mortar_lock]);
            printf("%d aquired a %d. mortar and uses it\n",
                   smoker,
                   free_mortar_lock);
            /*sleep(1); // does thing*/
            omp_unset_lock(&lmortars[free_mortar_lock]);
            used_mortar = true;
        }
        int free_ignitor_lock = aquire(lignitors, Mignitors);
        if (free_mortar_lock != -1 && used_mortar) {
            omp_set_lock(&lignitors[free_ignitor_lock]);
            printf("%d aquired a %d. mortar and uses it\n",
                   smoker,
                   free_ignitor_lock);
            /*sleep(1); // ignites*/
            omp_unset_lock(&lignitors[free_ignitor_lock]);
            ignited = true;
        }
        if (used_mortar && ignited) {
#pragma omp critical
            {
                printf("\t\t%d smokes...\n", smoker);
                sleep(1); // smokes
            }
        }
    }

    destroy_locks(lmortars, Lmortars);
    destroy_locks(lignitors, Mignitors);
}
