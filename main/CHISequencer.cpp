#include "CHISequencer.hpp"

bool CHISequencer::IsAlive() {
    return state == State::ALIVE;
}

void CHISequencer::Tick(uint64_t cycles) {
    if (!IsAlive())
        return;

    this->cycles = cycles;
}

void CHISequencer::Tock() {
    if (!IsAlive())
        return;
    
    fcagent->handle_channel();
    fcagent->random_test();
    fcagent->update_signal();
}