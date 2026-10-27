#include <drogon/drogon.h>
#include <thread>
#include "OutboxWorker.h"

int main() {
    drogon::app().loadConfigFile("../config.json");
    
    std::thread outbox_thread([]() {
        OutboxWorker::run();
    });
    outbox_thread.detach();

    drogon::app().run();
    return 0;
}
