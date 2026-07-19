#include <pthread.h>

static int shared_counter;

static void *
increment_shared_counter(void *unused)
{
    (void)unused;
    for (int i = 0; i < 1000; i++)
        shared_counter++;
    return NULL;
}

int
main(void)
{
    pthread_t first;
    pthread_t second;

    if (pthread_create(&first, NULL, increment_shared_counter, NULL) != 0)
        return 2;
    if (pthread_create(&second, NULL, increment_shared_counter, NULL) != 0)
        return 3;
    if (pthread_join(first, NULL) != 0)
        return 4;
    if (pthread_join(second, NULL) != 0)
        return 5;

    return shared_counter < 0;
}
