cmake_minimum_required(VERSION 3.8)
project(abr_image_transport)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rcutils REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(abr_image_transport_interfaces REQUIRED)
find_package(abr_image_transport_monitor REQUIRED)
find_package(abr_image_transport_codecs REQUIRED)
find_package(yaml-cpp REQUIRED)

# ===== Find FFmpeg dependencies =====
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)

# ===== Check for VA-API HW acceleration (optional) =====
pkg_check_modules(VAAPI QUIET libva)
if(VAAPI_FOUND)
  message(STATUS "VA-API found: hardware acceleration enabled")
  include_directories(${VAAPI_INCLUDE_DIRS})
  set(EXTRA_LIBS ${VAAPI_LIBRARIES})
else()
  message(WARNING "VA-API not found: falling back to software encoding/decoding")
  set(EXTRA_LIBS "")
endif()

include_directories(include
  ${OpenCV_INCLUDE_DIRS}
  ${AVCODEC_INCLUDE_DIRS}
  ${AVUTIL_INCLUDE_DIRS}
  ${SWSCALE_INCLUDE_DIRS}
)

set(LIBRARY_NAME ${PROJECT_NAME}_component)

add_library(
  ${LIBRARY_NAME}
  SHARED
  src/abr_publisher.cpp
  src/abr_subscriber.cpp
  src/manifest.cpp
)

target_include_directories(${LIBRARY_NAME}
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"
  PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
)

# ===== Link libraries =====
ament_target_dependencies(${LIBRARY_NAME}
  PUBLIC
    rclcpp
    sensor_msgs
    std_msgs
    cv_bridge
    image_transport
    pluginlib
    abr_image_transport_interfaces
    abr_image_transport_monitor
    abr_image_transport_codecs
)

target_link_libraries(${LIBRARY_NAME}
PUBLIC
  ${OpenCV_LIBRARIES}
  ${AVCODEC_LIBRARIES}
  ${AVUTIL_LIBRARIES}
  ${SWSCALE_LIBRARIES}
  ${EXTRA_LIBS}
  yaml-cpp
)

ament_export_dependencies(
  rosidl_default_runtime
  OpenCV
  cv_bridge
  image_transport
  pluginlib
  rclcpp
  rcutils
  sensor_msgs
  std_msgs
  abr_image_transport_interfaces
  abr_image_transport_monitor
  abr_image_transport_codecs
)

# Install the library
install(TARGETS ${LIBRARY_NAME} EXPORT export_${LIBRARY_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# Install include directories
install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

# Export the library for use by other packages
ament_export_targets(export_${LIBRARY_NAME} HAS_LIBRARY_TARGET)

# Export include directories
ament_export_include_directories(include/${PROJECT_NAME})

install(DIRECTORY params/
  DESTINATION share/${PROJECT_NAME}/params)

# Install plugin descriptions
pluginlib_export_plugin_description_file(image_transport abr_plugin.xml)

ament_package()
