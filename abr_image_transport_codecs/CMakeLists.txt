cmake_minimum_required(VERSION 3.8)
project(abr_image_transport_codecs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rcutils REQUIRED)
find_package(abr_image_transport_interfaces REQUIRED)

# Find OpenCV
find_package(OpenCV REQUIRED)

# Find FFmpeg libraries using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET libavcodec libavutil libswscale)

# Create the codec library
add_library(${PROJECT_NAME} SHARED
  src/ffmpeg_codec.cpp
)

# Specify include directories
target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${OpenCV_INCLUDE_DIRS}
  PRIVATE
    ${LIBAV_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
  ${OpenCV_LIBRARIES}
  ${LIBAV_LIBRARIES}
)

# Specify dependencies for ament
ament_target_dependencies(${PROJECT_NAME}
  rclcpp
  sensor_msgs
  std_msgs
  cv_bridge
  image_transport
  pluginlib
  rcutils
  abr_image_transport_interfaces
)

# Install the library
install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# Install header files
install(DIRECTORY include/
  DESTINATION include
)

# Export the library for use by other packages
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(
  rclcpp
  sensor_msgs
  std_msgs
  cv_bridge
  image_transport
  pluginlib
  rcutils
  abr_image_transport_interfaces
)

# Export include directories
ament_export_include_directories(include)

ament_package()