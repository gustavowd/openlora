set(COMPONENT_ADD_INCLUDEDIRS "include" "../backoff/include" "../zlib")
set(COMPONENT_SRCS "openlora.c" "lora_crc.c" "lora.c")
set(COMPONENT_NAME ".")
set(COMPONENT_REQUIRES mbedtls fatfs backoff)

register_component()
