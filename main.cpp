extern "C" {
    #include "FreeRTOS.h"
    #include "task.h"
    #include "queue.h"
    #include "semphr.h"
}

#include "sensor.hpp"
#include "temp_sensor.hpp"
#include "light_sensor.hpp"
#include "humidity_sensor.hpp"
#include "moving_average.hpp"
#include <cstdio>
#include <cmath>

static QueueHandle_t xRawDataQueue;
static QueueHandle_t xProcessedDataQueue;

// Non-Blocking Prints
static const uint8_t PRINT_QUEUE_LEN = 10;
static const uint16_t PRINT_BUF_SIZE = 128; // Max line length
static QueueHandle_t xPrintQueue;

struct PrintCommand {
    char buffer[PRINT_BUF_SIZE];
    bool is_control_seq; // Not used for windows, but used to denote ANSI control codes
};

void safe_printf(const char* format, ...) {
    PrintCommand cmd = { 0 };
    va_list args;
    va_start(args, format);
    vsnprintf(cmd.buffer, PRINT_BUF_SIZE, format, args);
    va_end(args);
    cmd.is_control_seq = false;

    xQueueSend(xPrintQueue, &cmd, pdMS_TO_TICKS(10));
}

void safe_print_control_code(const char* control_code, ...) {
    PrintCommand cmd = { 0 };
    strncpy(cmd.buffer, control_code, PRINT_BUF_SIZE);
    cmd.is_control_seq = true;
    xQueueSend(xPrintQueue, &cmd, pdMS_TO_TICKS(10));
}

// Sensors, Queues and dashboard data instances all global for simplicity
static TempSensor temp_sensor;
static LightSensor light_sensor;
static HumiditySensor humidity_sensor;


struct DashboardData {
    float temp;
    float humidity;
    float light;
    uint32_t last_update;
};

static DashboardData dashboard_data;
static SemaphoreHandle_t xDashboardMutex;

extern "C" void vPrintTask(void* pvParameters) {
    PrintCommand cmd;
    while (1) {
        UBaseType_t messages = uxQueueMessagesWaiting(xPrintQueue);
        printf("[PRINT] Queue: %u/%u messages\n", messages, PRINT_QUEUE_LEN);
        if (xQueueReceive(xPrintQueue, &cmd, portMAX_DELAY) == pdPASS) {
            if (cmd.is_control_seq) {
                fputs(cmd.buffer, stdout); // control sequences should go through, no formatting
            }
            else {
                printf("%s",cmd.buffer);
            }
            fflush(stdout); // immediate output
        }
    }
}

extern "C" void vDashboardTask(void* pvParameters) {
    const TickType_t xUpdateFrequency = pdMS_TO_TICKS(1000);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // DEBUG - wait for sensor task to populate data, then start this one.
    vTaskDelay(pdMS_TO_TICKS(200));

    while (1) {
        printf("DASH - Taking mutex....\n");
        if ( xSemaphoreTake(xDashboardMutex, pdMS_TO_TICKS(100)) == pdTRUE ){
            printf("DASH - Mutex get, display code\n");
            // clear screen
            // safe_print_control_code("\033[2J\033[H");
            safe_printf("=== Potted Plant Environmental Dashboard ===\n");
            safe_printf("Temperature: %.1f C\n", dashboard_data.temp);
            safe_printf("Light Level: %.1f lux\n", dashboard_data.light);
            safe_printf("Humidity:    %.1f %\n", dashboard_data.humidity);
            xSemaphoreGive(xDashboardMutex);
            printf("DASH - mutex released\n");
        } else {
            printf("DASH - failed to get mutex\n");
        }
        vTaskDelayUntil(&xLastWakeTime, xUpdateFrequency);
    }
}

extern "C" void vSensorTask(void* pvParameters) {
    Sensor* sensors[] = { &temp_sensor, &light_sensor, &humidity_sensor };
    const TickType_t xDelay = pdMS_TO_TICKS(100);
    size_t idx = 0;

    while (1) {
        Sensor::Data data = sensors[idx]->read();
        xQueueSend(xRawDataQueue, &data, portMAX_DELAY);
        idx = (idx + 1) % 3; // alternate sensors
        vTaskDelay(xDelay);
    }
}

extern "C" void vProcessorTask(void* pvParameters) {
    static MovingAverage<5> temperature_filter;
    static MovingAverage<5> humidity_filter;
    static MovingAverage<5> light_filter;
    Sensor::Data data;
    uint32_t update_count = 0; //dbg

    while (1) {
        if (xQueueReceive(xRawDataQueue, &data, portMAX_DELAY) == pdPASS) {
            bool dash_updated = false; // dbg
            switch (data.type) {
            case Sensor::Type::TEMPERATURE:
                data.value = temperature_filter.process(data.value);
                if (xSemaphoreTake(xDashboardMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    dashboard_data.temp = data.value;
                    dashboard_data.last_update = xTaskGetTickCount();
                    xSemaphoreGive(xDashboardMutex);
                    dash_updated = true; // dbg
                }
                break;
            case Sensor::Type::LIGHT:
                data.value = light_filter.process(data.value);
                if (xSemaphoreTake(xDashboardMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    dashboard_data.light = data.value;
                    dashboard_data.last_update = xTaskGetTickCount();
                    xSemaphoreGive(xDashboardMutex);
                    dash_updated = true; // dbg
                }
                break;
            case Sensor::Type::HUMIDITY:
                data.value = humidity_filter.process(data.value);
                if (xSemaphoreTake(xDashboardMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    dashboard_data.humidity = data.value;
                    dashboard_data.last_update = xTaskGetTickCount();
                    xSemaphoreGive(xDashboardMutex);
                    dash_updated = true; // dbg
                }
                break;
            }

            // Debug output every 10 updates
            if (++update_count % 1 == 0) {
                safe_printf("[PROC] Processed: %s=%.1f (Dashboard %s)\n",
                    data.type == Sensor::Type::TEMPERATURE ? "TEMP" :
                    data.type == Sensor::Type::HUMIDITY ? "HUM" : "LIGHT",
                    data.value,
                    dash_updated ? "UPDATED" : "SKIPPED");
            }
            xQueueSend(xProcessedDataQueue, &data, 0);
        }
    }
}

void vMain(void) {
    xPrintQueue = xQueueCreate(PRINT_QUEUE_LEN, sizeof(PrintCommand));
    xRawDataQueue = xQueueCreate(5, sizeof(Sensor::Data));
    xProcessedDataQueue = xQueueCreate(5, sizeof(Sensor::Data));

    xDashboardMutex = xSemaphoreCreateMutex();
    xTaskCreate(vDashboardTask, "Dashboard", 1024, NULL, 1, NULL);
    xTaskCreate(vSensorTask, "Sensor", 1024, NULL, 3, NULL); 
    xTaskCreate(vProcessorTask, "Processor", 1024, NULL, 2, NULL);
    xTaskCreate(vPrintTask, "Print", 1024, NULL, 4, NULL);

    vTaskStartScheduler();
}

int main( void )
{
    vMain();
    return 0;
}

// Stubbing to stop linker from whining
void vConfigureTimerForRunTimeStats(void) { }
void vAssertCalled(unsigned long ulLine, const char* const pcFileName) {
    printf("Asserted at: %s Line: %lu\n", pcFileName, ulLine);
}