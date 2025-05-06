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

// Non-Blocking Prints FIXME: remove and use regular printfs
static const uint8_t PRINT_QUEUE_LEN = 10;
static const uint16_t PRINT_BUF_SIZE = 128; // Max line length
static QueueHandle_t xPrintQueue;

//struct PrintCommand {
//    char buffer[PRINT_BUF_SIZE];
//    bool is_control_seq; // Not used for windows, but used to denote ANSI control codes
//};

/*
* @brief safe_printf is a non-blocking, thread safe way of printing to the console.
* fill the buffer, put it in the print queue.
*/
//void safe_printf(const char* format, ...) {
//    PrintCommand cmd = { 0 };
//    va_list args; // FIXME: not type safe
//    va_start(args, format);
//    vsnprintf(cmd.buffer, PRINT_BUF_SIZE, format, args);
//    va_end(args);
//    cmd.is_control_seq = false;
//
//    xQueueSend(xPrintQueue, &cmd, pdMS_TO_TICKS(10));
//}

/*
* @brief same idea as safe_printf, but more 'direct' because it handles control codes. 
*/
//void safe_print_control_code(const char* control_code, ...) {
//    PrintCommand cmd = { 0 };
//    strncpy(cmd.buffer, control_code, PRINT_BUF_SIZE);
//    cmd.is_control_seq = true;
//    xQueueSend(xPrintQueue, &cmd, pdMS_TO_TICKS(10));
//}

// Sensors, Queues and dashboard data instances all global for simplicity
static TempSensor temp_sensor;
static LightSensor light_sensor;
static HumiditySensor humidity_sensor;

struct DashboardData {
    float temp;
    float humidity;
    float light;
    uint64_t uptime;
};

static DashboardData dashboard_data;
static SemaphoreHandle_t xDashboardMutex;

/*
* @brief RTOS task for handling printing to console without blocking
*/
//extern "C" void vPrintTask(void* pvParameters) {
//    PrintCommand cmd;
//    while (1) {
//        if (xQueueReceive(xPrintQueue, &cmd, portMAX_DELAY) == pdPASS) {
//            if (cmd.is_control_seq) {
//                fputs(cmd.buffer, stdout); // control sequences should go through, no formatting
//            }
//            else {
//                printf("%s",cmd.buffer);
//            }
//            fflush(stdout); // immediate output
//        }
//    }
//}

/*
* @brief RTOS task for displaying the dashboard, uses semaphores to ensure atomic access to
* global dashboard_data struct.
*/
extern "C" void vDashboardTask(void* pvParameters) {
    const TickType_t xUpdateFrequency = pdMS_TO_TICKS(1000);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    vTaskDelay(pdMS_TO_TICKS(200));

    while (1) {
        if ( xSemaphoreTake(xDashboardMutex, pdMS_TO_TICKS(100)) == pdTRUE ){
            // clear screen
            printf("\033[2J\033[H");
            // Print Dashboard
            printf("=== Potted Plant Environmental Dashboard ===\n");
            printf("Temperature: %.1f C\n", dashboard_data.temp);
            printf("Light Level: %.1f lux\n", dashboard_data.light);
            printf("Humidity:    %.1f %% \n", dashboard_data.humidity);
            printf("Up Time: %llu ms", dashboard_data.uptime); 
            xSemaphoreGive(xDashboardMutex);
        }
        vTaskDelayUntil(&xLastWakeTime, xUpdateFrequency);
    }
}

/*
* @brief RTOS task for polling data from the sensor suite. Round robin access when reading from sensors. 
* Takes in data and places it in the RawDataQueue to be processed.
*/
extern "C" void vSensorTask(void* pvParameters) {
    Sensor* sensors[] = { &temp_sensor, &light_sensor, &humidity_sensor };
    const TickType_t xDelay = pdMS_TO_TICKS(100); // poll every 100 ms
    size_t idx = 0;

    while (1) {
        Sensor::Data data = sensors[idx]->read();
        xQueueSend(xRawDataQueue, &data, portMAX_DELAY);
        idx = (idx + 1) % 3; // alternate sensors
        vTaskDelay(xDelay);
    }
}

/*
* @brief RTOS task that takes in data from the RawDataQueue and applys various filters to the data. 
* Places processed data on the ProcessedDataQueue.
* Currently nothing is utilizing the data in the ProcessedDataQueue, will be utilized for 'live' sensor viewing or other tasks.
*/
extern "C" void vProcessorTask(void* pvParameters) {
    static MovingAverage<float, 5> temperature_filter;
    static MovingAverage<float, 5> humidity_filter;
    static MovingAverage<float, 5> light_filter;
    Sensor::Data data;

    while (1) {
        if (xQueueReceive(xRawDataQueue, &data, portMAX_DELAY) == pdPASS) {            
            if (xSemaphoreTake(xDashboardMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                dashboard_data.uptime = xTaskGetTickCount(); 
                switch (data.type) {
                case Sensor::Type::TEMPERATURE:                 
                    dashboard_data.temp = temperature_filter.addSample(data.value);
                    break;
                case Sensor::Type::LIGHT:                                       
                    dashboard_data.light = light_filter.addSample(data.value);
                    break;
                case Sensor::Type::HUMIDITY:                                      
                    dashboard_data.humidity = humidity_filter.addSample(data.value);
                    break;
                default:
                    break;
                }
                xSemaphoreGive(xDashboardMutex);
            }
            xQueueSend(xProcessedDataQueue, &data, pdMS_TO_TICKS(100));
        }
    }
}

/*
* @brief FreeRTOS setup and entrypoint.
* initilized data queues, creates our semaphore, registers tasks, then starts the scheduler.
*/
void vMain(void) {
    xRawDataQueue = xQueueCreate(5, sizeof(Sensor::Data));
    xProcessedDataQueue = xQueueCreate(5, sizeof(Sensor::Data));

    xDashboardMutex = xSemaphoreCreateMutex();
    xTaskCreate(vDashboardTask, "Dashboard", 1024, NULL, 1, NULL);
    xTaskCreate(vProcessorTask, "Processor", 1024, NULL, 2, NULL);
    xTaskCreate(vSensorTask, "Sensor", 1024, NULL, 3, NULL);

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