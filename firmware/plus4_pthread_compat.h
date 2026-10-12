
#pragma once

#include <pthread.h>

#include "plus4_hot_code.h"

#ifndef pthread_mutex_lock
#define pthread_mutex_lock(mutex_) (0)
#endif
#ifndef pthread_mutex_unlock
#define pthread_mutex_unlock(mutex_) (0)
#endif
