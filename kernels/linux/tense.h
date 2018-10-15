#ifndef _DRIVERS_TENSE_H
#define _DRIVERS_TENSE_H

#include <linux/sched/tense.h>

struct tense_module_operations {
        struct tense_operations tense_ops;
        void (*init_module) (void);
        void (*add_current_task) (void);
        void (*remove_current_task)(void);
};

extern tense_module_operations * basic;

#endif