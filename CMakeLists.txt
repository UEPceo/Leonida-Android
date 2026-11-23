cmake_minimum_required(VERSION 3.22)
project(leonida_mobile_probe C)

set(CMAKE_C_STANDARD 11)
add_executable(vkprobe probe/vkprobe.c)

if(ANDROID)
  # Ejecutable PIE para adb shell; usa el libvulkan.so del sistema.
  target_link_libraries(vkprobe vulkan)
else()
  find_package(Vulkan REQUIRED)
  target_link_libraries(vkprobe Vulkan::Vulkan)
endif()
