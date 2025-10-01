/*********************************************************************
* Copyright 2025 José Miguel Guerrero Hernández
*
* This file is part of abr-video-ros (Adaptative Bitrate Video for ROS 2).
*
* abr-video-ros is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* abr-video-ros is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with program.  If not, see <https://www.gnu.org/licenses/>.
*********************************************************************/

#pragma once

#include <memory>
#include <string>
#include <sensor_msgs/msg/image.hpp>
#include <image_transport/simple_publisher_plugin.hpp>
#include <abr_image_transport_interfaces/msg/abr_image.hpp>
#include <abr_image_transport_interfaces/srv/abr_params.hpp>
#include "abr_image_transport_codecs/codec_base.hpp"
#include "abr_image_transport_codecs/ffmpeg_codec.hpp"

namespace abr_image_transport
{

/**
 * @brief Adaptive bitrate image publisher plugin
 *
 * This class implements an image_transport publisher plugin that provides
 * adaptive bitrate video streaming capabilities. It compresses images using
 * configurable video codecs (primarily H.264 via FFmpeg) and allows runtime
 * adjustment of encoding parameters such as bitrate, frame rate, and quality.
 *
 * Key features:
 * - Real-time video compression with H.264/H.265
 * - Runtime parameter adjustment via ROS 2 services
 * - YAML-based configuration loading
 * - Multiple encoding presets for different use cases
 * - Integration with adaptive quality monitoring
 *
 * The publisher subscribes to standard sensor_msgs::Image topics and publishes
 * compressed data as abr_image_transport_interfaces::AbrImage messages.
 */
class AbrPublisher
  : public image_transport::SimplePublisherPlugin<
    abr_image_transport_interfaces::msg::AbrImage>
{
public:
  /**
   * @brief Constructor
   *
   * Initializes the publisher with default codec (FFmpeg) and sets up
   * internal state. The actual codec initialization is deferred until
   * the first image is published.
   */
  AbrPublisher();

  /**
   * @brief Destructor
   *
   * Ensures proper cleanup of codec resources and service connections.
   */
  ~AbrPublisher() override;

  /**
   * @brief Get the transport plugin name
   *
   * @return std::string The transport name "abr" used by image_transport
   */
  std::string getTransportName() const override {return "abr";}

protected:
  /**
   * @brief Initialize the publisher and set up services
   *
   * Called by image_transport when a publisher is created. Sets up the
   * parameter service, loads configuration from YAML, and prepares the
   * codec for operation.
   *
   * @param node ROS 2 node to use for service creation and logging
   * @param base_topic Base topic name (without transport suffix)
   * @param custom_qos QoS profile for the publisher
   * @param options Additional publisher options
   */
  void advertiseImpl(
    rclcpp::Node * node,
    const std::string & base_topic,
    rmw_qos_profile_t custom_qos,
    rclcpp::PublisherOptions options) override;

  /**
   * @brief Encode and publish an image
   *
   * Takes a ROS Image message, encodes it using the configured codec,
   * and publishes the compressed data. Handles codec initialization
   * on first use and manages encoder state.
   *
   * @param message Input image to compress and publish
   * @param publish_fn Function to call with the compressed message
   */
  void publish(
    const sensor_msgs::msg::Image & message,
    const PublishFn & publish_fn) const override;

  /**
   * @brief Service callback for parameter updates
   *
   * Handles incoming requests to change encoding parameters such as
   * bitrate. Validates parameters and updates the codec configuration.
   * May trigger encoder reinitialization if necessary.
   *
   * @param req Service request containing new parameters
   * @param res Service response indicating success/failure
   */
  void setAbrParams(
    const std::shared_ptr<abr_image_transport_interfaces::srv::AbrParams::Request> req,
    std::shared_ptr<abr_image_transport_interfaces::srv::AbrParams::Response> res);

  /**
   * @brief Load configuration from YAML file
   *
   * Reads the centralized abr_config.yaml file and applies publisher
   * configuration including codec settings, bitrate, frame rate, and
   * service name.
   */
  void loadDefaultYaml();

  /**
   * @brief Initialize codec based on codec_type configuration
   *
   * Creates the appropriate codec instance using the factory pattern
   * based on the codec_type specified in the YAML configuration.
   *
   * @return bool True if codec initialized successfully, false otherwise
   */
  bool initializeCodec();

  /**
   * @brief Load configuration from YAML file and initialize codec
   *
   * Main configuration loading method that orchestrates the entire
   * configuration process including path resolution and codec setup.
   *
   * @return bool True if configuration loaded successfully, false otherwise
   */
  bool loadConfiguration();

  /**
   * @brief Get the path to the configuration file
   *
   * Resolves the configuration file path, checking both package-relative
   * and absolute paths with proper error handling.
   *
   * @return std::string Path to the configuration file
   */
  std::string getConfigFilePath() const;

  /**
   * @brief Load global codec configuration
   *
   * Loads the codec_type setting from the global configuration section
   * and validates it against supported codec types.
   *
   * @param config_root Root YAML node containing configuration
   */
  void loadGlobalConfig(const YAML::Node & config_root);

  /**
   * @brief Load publisher-specific configuration
   *
   * Loads publisher settings including service name from the
   * configuration file with appropriate fallbacks.
   *
   * @param config_root Root YAML node containing configuration
   */
  void loadPublisherConfig(const YAML::Node & config_root);

  /**
   * @brief Setup parameter service for runtime configuration changes
   *
   * Creates and configures the ROS 2 service for handling parameter
   * update requests from clients.
   *
   * @param node ROS 2 node to use for service creation
   */
  void setupParameterService(rclcpp::Node * node);

  /**
   * @brief Cleanup resources during destruction
   *
   * Handles proper cleanup of codec resources and service connections
   * to ensure clean shutdown.
   */
  void cleanup();

private:
  /// @brief Video codec instance for compression
  mutable std::unique_ptr<CodecBase<sensor_msgs::msg::Image>> codec_;

  /// @brief Flag indicating if encoder has been initialized
  mutable bool encoder_initialized_ = false;

  /// @brief Logger instance for this publisher
  rclcpp::Logger logger_;

  /// @brief ROS 2 service for parameter updates
  rclcpp::Service<abr_image_transport_interfaces::srv::AbrParams>::SharedPtr srv_;

  /// @brief Name of the parameter service (configurable)
  std::string service_name_ = "abr_params";

  /// @brief Type of codec to use (read from YAML configuration)
  std::string codec_type_ = "ffmpeg";

  /// @brief Default bitrate value loaded from configuration
  int default_bitrate_ = 1000000;

  /// @brief Default frame rate value loaded from configuration
  int default_fps_ = 30;
};

}  // namespace abr_image_transport
