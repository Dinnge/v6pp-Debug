#[[
    Æô¶¯Òýµ¼¹¹½¨ÎÄ¼þ
    ´´½¨ÓÚ 2024Äê4ÔÂ28ÈÕ ÉÏº£ÊÐ¼Î¶¨Çø°²Í¤Õò

    by 2051565 GTY
]]


set(BOOT_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/../target/objs/boot")

# Ê¹ÓÃ Ninja Ê±£¬Êä³öÂ·¾¶»á³öÏÖÎÊÌâ¡£
set(BOOT_OUTPUT_DIR_NINJA "${CMAKE_SOURCE_DIR}/../target/objs")

file(MAKE_DIRECTORY ${BOOT_OUTPUT_DIR})
set(CMAKE_ASM_NASM_LINK_EXECUTABLE "cp <OBJECTS> ${BOOT_OUTPUT_DIR_NINJA}/<TARGET>.bin")
set(CMAKE_ASM_NASM_OBJECT_FORMAT bin)
if (USE_VESA)
    set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -d USE_VESA")
endif()
enable_language(ASM_NASM)
add_executable(boot boot.asm)


# sector2 Òª×÷ÎªÕû¸öÄÚºË´úÂëµÄÈë¿Ú¡£ÐèÒªµ¥¶À¹¹½¨¡£
# Á´½ÓÊ±£¬Ëü»á±»Ö¸¶¨·Åµ½¿ªÍ·Î»ÖÃ¡£
add_custom_command(
    OUTPUT ${BOOT_OUTPUT_DIR}/sector2.o
    COMMAND nasm -f elf32
            $<$<BOOL:${EARLY_BOOT_GDB}>:-dEARLY_BOOT_GDB>
            ${CMAKE_CURRENT_SOURCE_DIR}/sector2.asm
            -o ${BOOT_OUTPUT_DIR}/sector2.o
)
set(NULL_CPP_FILE_PATH "${CMAKE_BINARY_DIR}/_yros_cmake_hack/null.cpp")
file(WRITE ${NULL_CPP_FILE_PATH} "")
add_library(sector2 ${BOOT_OUTPUT_DIR}/sector2.o ${NULL_CPP_FILE_PATH})
set_target_properties(sector2 PROPERTIES PREFIX "")
set_target_properties(sector2 PROPERTIES SUFFIX ".o")
