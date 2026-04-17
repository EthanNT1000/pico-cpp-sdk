#ifndef _SUBSYSTASK_H_
#define _SUBSYSTASK_H_

extern "C" {
#include "stdint.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
}

/**
 * @brief The interface class for subsystem tasks
 *
 */
class TaskInterface {
 public:
    enum Priority {
        Low,
        MediumLow,
        Medium,
        MediumHigh,
        High,
        MAX = configMAX_PRIORITIES
    };
    TaskInterface(const char * pcName, const configSTACK_DEPTH_TYPE usStackDepth, Priority uxPriority,
        QueueHandle_t queueRx = nullptr, SemaphoreHandle_t semaRx = nullptr);
    virtual ~TaskInterface();
    static void taskFunctionAdapter(void *args);
    const QueueHandle_t rxQueue;
    const SemaphoreHandle_t rxSema;
    TaskHandle_t taskHandler;
    char name[configMAX_TASK_NAME_LEN];  //NOLINT
    const configSTACK_DEPTH_TYPE stackDepth;
    const UBaseType_t priority;

    uint32_t getStackUsage();
    uint32_t getStackLeft();

    void* operator new(size_t size)     {return pvPortMalloc(size);}
    void* operator new[](size_t size)   {return pvPortMalloc(size);}
    void operator delete(void * ptr)    {vPortFree(ptr);}
    void operator delete[](void * ptr)  {vPortFree(ptr);}

 protected:
    virtual void run() = 0;

 private:
    bool start();  // create task and run
    bool taskStarted;
};
#endif
