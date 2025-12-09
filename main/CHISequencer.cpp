#include "CHISequencer.hpp"

bool CHISequencer::IsAlive() {
    return state == State::ALIVE;
}

CHISequencer::CHISequencer() {
    // 1. 初始化仿真状态
    this->cycles = 0;
    this->state  = State::ALIVE; // 设为 ALIVE 让 main 中的 while 循环能启动

    // 2. 实例化 FCAgent
    // 这里我们分配 ID 0 到 63 (共64个) 给这个 Agent 使用
    // 你可以根据需要调整这个范围
    this->fcagent = new CCHIAgent::FCAgent(0, 64);

    // 3. [关键] 关联 GlobalBoard
    // 将 Sequencer 自己的 globalBoard 地址传递给 Agent
    if (this->fcagent) {
        this->fcagent->setGlobalBoard(&this->globalBoard);
    }
}

// 析构函数 (别忘了释放内存)
CHISequencer::~CHISequencer() {
    if (this->fcagent) {
        delete this->fcagent;
        this->fcagent = nullptr;
    }
}

void CHISequencer::Tick(uint64_t cycles) {
    if (!IsAlive())
        return;

    this->cycles = cycles;
}

void CHISequencer::Tock() {
    if (!IsAlive())
        return;
    
    fcagent->FIRE();
    fcagent->random_test();
    fcagent->SEND();
}