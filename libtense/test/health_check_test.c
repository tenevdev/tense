#include "../tense.h"

int main(void) {
    if (tense_init() != -1)
        tense_health_check();
    tense_destroy();
}
