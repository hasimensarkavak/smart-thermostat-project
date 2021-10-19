#include <homekit/homekit.h>
#include <homekit/characteristics.h>

void thermostat_identify(homekit_value_t _value) {
    printf("Thermostat identify\n");
}

void on_update_target_temp(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    printf("Thermostat Update Target Temp\n");
}

void on_update_mode(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    printf("Thermostat Update Mode\n");
}


homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_(
    CURRENT_TEMPERATURE, 0
);
homekit_characteristic_t target_temperature  = HOMEKIT_CHARACTERISTIC_(
    TARGET_TEMPERATURE, 22, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update_target_temp)
);

homekit_characteristic_t units = HOMEKIT_CHARACTERISTIC_(TEMPERATURE_DISPLAY_UNITS, 0);

homekit_characteristic_t current_state = HOMEKIT_CHARACTERISTIC_(
    CURRENT_HEATING_COOLING_STATE, 1, .valid_values = {.count = 3, .values = (uint8_t[]) { 0, 1, 2}});

homekit_characteristic_t target_state = HOMEKIT_CHARACTERISTIC_(
    TARGET_HEATING_COOLING_STATE, 1, .valid_values = {.count = 3, .values = (uint8_t[]) { 0, 1, 2}}, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update_mode)
);

homekit_characteristic_t current_humidity = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);



homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Thermostat"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Kristal"),  ////Bu Kısmıda Kontrol Et!!!!
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "041"),
            HOMEKIT_CHARACTERISTIC(MODEL, "SmartThermostat ST0"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, thermostat_identify),
            NULL
        }),
        HOMEKIT_SERVICE(THERMOSTAT, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Thermostat"),
            &current_temperature,
            &target_temperature,
            &current_state,
            &target_state,
            &units,
            &current_humidity,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-141" ////Değiştirmeyi Unutma!!!!!!!!!!!!!!!
};
