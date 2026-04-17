#include "taskinterface.h"
#include <stdio.h>

/**
 * @brief Construct a new TaskInterface:: TaskInterface object
 *
 * @param pcName Task name
 * @param usStackDepth Task stack size
 * @param uxPriority Task priority
 */
TaskInterface::TaskInterface(const char * const pcName, const configSTACK_DEPTH_TYPE usStackDepth, Priority uxPriority,
    QueueHandle_t queueRx, SemaphoreHandle_t semaRx) : taskStarted(false), stackDepth(usStackDepth)
    , priority(static_cast<UBaseType_t>(uxPriority)), rxQueue(queueRx), rxSema(semaRx) {
    for (uint8_t i = 0; i < configMAX_TASK_NAME_LEN - 1; i++) {
        name[i] = pcName[i];
        if (pcName[i] == 0)
            break;
    }
    name[configMAX_TASK_NAME_LEN - 1] = 0;
    start();
}

/**
 * @brief Destroy the Sub Sys Task:: Sub Sys Task object
 *
 */
TaskInterface::~TaskInterface() {
    vTaskDelete(taskHandler);
    if (rxQueue)
        vQueueDelete(rxQueue);
    if (rxSema)
        vSemaphoreDelete(rxSema);
}

/**
 * @brief taskFunctionAdapter for xTaskCreate usage
 *
 * @param args The pointer of this class
 */
void TaskInterface::taskFunctionAdapter(void *args) {
    TaskInterface *taskInterface = static_cast<TaskInterface *>(args);
    taskInterface->run();
#if (INCLUDE_vTaskDelete == 1)
    vTaskDelete(taskInterface->taskHandler);
#else
    configASSERT(!"Cannot return from a subSysTask->run function "
                    "if INCLUDE_vTaskDelete is not defined.");
#endif
}

/**
 * @brief Get current task stack usage
 *
 * @return uint32_t Stack used size (bytes)
 */
uint32_t TaskInterface::getStackUsage() {
    return stackDepth * sizeof( StackType_t ) - getStackLeft();
}

/**
 * @brief Get stack left size
 *
 * @return uint32_t Stack left size (bytes)
 */
uint32_t TaskInterface::getStackLeft() {
    return uxTaskGetStackHighWaterMark(taskHandler) * sizeof( StackType_t );
}

/**
 * @brief Start the subsystem task function
 *
 * @return true
 * @return false
 */
bool TaskInterface::start() {
    if (taskStarted)
        return false;
    taskStarted = xTaskCreate(taskFunctionAdapter, name, stackDepth, this, priority, &taskHandler);
    return taskStarted;
}
