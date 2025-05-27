#include "error_handler.h"

const char* error_category_name(error_category_t category) {
    switch (category) {
        case ERROR_CAT_HARDWARE:        return "HARDWARE";
        case ERROR_CAT_NETWORK:         return "NETWORK";
        case ERROR_CAT_STORAGE:         return "STORAGE";
        case ERROR_CAT_SENSOR:          return "SENSOR";
        case ERROR_CAT_SYSTEM:          return "SYSTEM";
        case ERROR_CAT_COMMUNICATION:   return "COMM";
        case ERROR_CAT_CONFIG:          return "CONFIG";
        case ERROR_CAT_VALIDATION:      return "VALIDATION";
        default:                        return "UNKNOWN";
    }
}
