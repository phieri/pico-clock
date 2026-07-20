#include "runtime.h"

int main(void) {
    runtime_state_t runtime;
    runtime_state_init(&runtime);
    runtime_run(&runtime);
    return 0;
}
