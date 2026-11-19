idf_component_register(SRCS "log.c"
                       INCLUDE_DIRS "include"
                       LDFRAGMENTS "linker.lf")

# ESPurna: esp_log_write() by ets_printf, without asprintf / vasprintf (log.c)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ESPURNA_NO_SDK_LOG)
