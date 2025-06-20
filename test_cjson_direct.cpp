#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "cJSON_TEST";

extern "C" void app_main() {
    ESP_LOGI(TAG, "Starting cJSON direct test");
    
    // Test 1: Basic string creation
    ESP_LOGI(TAG, "=== Test 1: Basic cJSON_CreateString ===");
    cJSON* test_string = cJSON_CreateString("hello world");
    if (test_string) {
        char* json_str = cJSON_Print(test_string);
        ESP_LOGI(TAG, "Created string: %s", json_str ? json_str : "NULL");
        if (json_str) free(json_str);
        cJSON_Delete(test_string);
    } else {
        ESP_LOGE(TAG, "Failed to create string!");
    }
    
    // Test 2: Object with cJSON_AddStringToObject
    ESP_LOGI(TAG, "=== Test 2: cJSON_AddStringToObject ===");
    cJSON* obj1 = cJSON_CreateObject();
    if (obj1) {
        cJSON_AddStringToObject(obj1, "jsonrpc", "2.0");
        cJSON_AddStringToObject(obj1, "method", "test");
        
        char* json_str = cJSON_Print(obj1);
        ESP_LOGI(TAG, "Object with AddStringToObject: %s", json_str ? json_str : "NULL");
        if (json_str) free(json_str);
        cJSON_Delete(obj1);
    }
    
    // Test 3: Object with cJSON_CreateString + cJSON_AddItemToObject
    ESP_LOGI(TAG, "=== Test 3: cJSON_CreateString + cJSON_AddItemToObject ===");
    cJSON* obj2 = cJSON_CreateObject();
    if (obj2) {
        cJSON* jsonrpc_str = cJSON_CreateString("2.0");
        if (jsonrpc_str) {
            cJSON_AddItemToObject(obj2, "jsonrpc", jsonrpc_str);
        }
        
        cJSON* method_str = cJSON_CreateString("test");
        if (method_str) {
            cJSON_AddItemToObject(obj2, "method", method_str);
        }
        
        char* json_str = cJSON_Print(obj2);
        ESP_LOGI(TAG, "Object with CreateString+AddItem: %s", json_str ? json_str : "NULL");
        if (json_str) free(json_str);
        cJSON_Delete(obj2);
    }
    
    // Test 4: Compare serialized output
    ESP_LOGI(TAG, "=== Test 4: Memory and pointer analysis ===");
    cJSON* test_obj = cJSON_CreateObject();
    if (test_obj) {
        const char* test_value = "test_string";
        ESP_LOGI(TAG, "Original string: '%s' at address %p", test_value, test_value);
        
        // Method 1: AddStringToObject
        cJSON* obj_method1 = cJSON_CreateObject();
        cJSON_AddStringToObject(obj_method1, "test", test_value);
        
        // Method 2: CreateString + AddItem
        cJSON* obj_method2 = cJSON_CreateObject();
        cJSON* str_item = cJSON_CreateString(test_value);
        if (str_item) {
            ESP_LOGI(TAG, "Created string item: type=%d, valuestring='%s'", 
                     str_item->type, str_item->valuestring ? str_item->valuestring : "NULL");
            cJSON_AddItemToObject(obj_method2, "test", str_item);
        }
        
        // Compare outputs
        char* json1 = cJSON_PrintUnformatted(obj_method1);
        char* json2 = cJSON_PrintUnformatted(obj_method2);
        
        ESP_LOGI(TAG, "Method 1 (AddStringToObject): %s", json1 ? json1 : "NULL");
        ESP_LOGI(TAG, "Method 2 (CreateString+AddItem): %s", json2 ? json2 : "NULL");
        
        if (json1) free(json1);
        if (json2) free(json2);
        cJSON_Delete(obj_method1);
        cJSON_Delete(obj_method2);
    }
    
    // Test 5: Check cJSON version and build info
    ESP_LOGI(TAG, "=== Test 5: cJSON Library Info ===");
    ESP_LOGI(TAG, "cJSON version: %s", cJSON_Version());
    
    // Test 6: Memory stress test
    ESP_LOGI(TAG, "=== Test 6: Memory stress test ===");
    for (int i = 0; i < 10; i++) {
        cJSON* temp_obj = cJSON_CreateObject();
        if (temp_obj) {
            cJSON_AddStringToObject(temp_obj, "iteration", "value");
            char* temp_json = cJSON_PrintUnformatted(temp_obj);
            ESP_LOGI(TAG, "Iteration %d: %s", i, temp_json ? temp_json : "NULL");
            if (temp_json) free(temp_json);
            cJSON_Delete(temp_obj);
        }
    }
    
    ESP_LOGI(TAG, "cJSON direct test completed");
}