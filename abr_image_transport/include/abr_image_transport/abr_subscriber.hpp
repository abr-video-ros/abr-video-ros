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
#include <image_transport/simple_subscriber_plugin.hpp>
#include <abr_image_transport_interfaces/msg/abr_image.hpp>
#include "abr_image_transport_codecs/codec_base.hpp"
#include "abr_image_transport_codecs/ffmpeg_codec.hpp"
#include "abr_image_transport_monitor/adaptive_quality_monitor.hpp"

namespace abr_image_transport
{

/**
 * @brief Adaptive bitrate image subscriber plugin
 *
 * This class implements an image_transport subscriber plugin that receives
 * and decodes compressed video streams from ABR publishers. It integrates
 * adaptive quality monitoring to provide feedback for bitrate optimization
 * and ensures efficient video decoding with minimal latency.
 *
 * Key features:
 * - Real-time H.264/H.265 video decompression
 * - Integrated quality monitoring and adaptation
 * - Automatic codec initialization and resource management
 * - Packet size analysis for network adaptation
 * - Seamless integration with image_transport ecosystem
 *
 * The subscriber receives abr_image_transport_interfaces::AbrImage messages
 * and delivers decoded sensor_msgs::Image messages to user callbacks.
 */
class AbrSubscriber
  : public image_transport::SimpleSubscriberPlugin<abr_image_transport_interfaces::msg::AbrImage>
{
public:
  /**
   * @brief Constructor
   *
   * Initializes the subscriber with default codec (FFmpeg) and prepares
   * internal state. Codec and monitor initialization is deferred until
   * subscription setup.
   */
  AbrSubscriber();

  /**
   * @brief Destructor
   *
   * Ensures proper cleanup of codec resources, monitor, and any active
   * subscriptions.
   */
  ~AbrSubscriber() override;

  /**
   * @brief Get the transport plugin name
   *
   * @return std::string The transport name "abr" used by image_transport
   */
  std::string getTransportName() const override {return "abr";}

protected:
  /**
   * @brief Set up subscription and initialize monitoring
   *
   * Called by image_transport when a subscriber is created. Initializes
   * the quality monitor, stores node reference, and sets up the internal
   * subscription to compressed image data.
   *
   * @param node ROS 2 node to use for subscriptions and service calls
   * @param base_topic Base topic name (without transport suffix)
   * @param callback User callback to invoke with decoded images
   * @param custom_qos QoS profile for the subscription
   * @param options Additional subscription options
   */
  void subscribeImpl(
    rclcpp::Node * node,
    const std::string & base_topic,
    const Callback & callback,
    rmw_qos_profile_t custom_qos,
    rclcpp::SubscriptionOptions options) override;

  /**
   * @brief Internal callback for processing compressed images
   *
   * This method is called for each received compressed image. It performs
   * the following operations:
   * 1. Initializes decoder on first use
   * 2. Monitors packet size for quality adaptation
   * 3. Decodes compressed data to RGB image
   * 4. Invokes user callback with decoded image
   *
   * @param message Received compressed image message
   * @param user_cb User callback to invoke with decoded image
   */
  void internalCallback(
    const abr_image_transport_interfaces::msg::AbrImage::ConstSharedPtr & message,
    const Callback & user_cb) override;

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
   * @brief Load subscriber-specific configuration
   *
   * Loads subscriber settings from the configuration file
   * with appropriate fallbacks.
   *
   * @param config_root Root YAML node containing configuration
   */
  void loadSubscriberConfig(const YAML::Node & config_root);

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
   * @brief Setup quality monitor for adaptive streaming
   *
   * Creates and configures the adaptive quality monitor for
   * analyzing streaming performance and providing feedback.
   *
   * @param node ROS 2 node to use for service creation
   * @return bool True if monitor initialized successfully, false otherwise
   */
  bool setupQualityMonitor(rclcpp::Node * node);

  /**
   * @brief Cleanup resources during destruction
   *
   * Handles proper cleanup of codec resources, quality monitor,
   * and service connections to ensure clean shutdown.
   */
  void cleanup();

private:
  /// @brief Video codec instance for decompression
  mutable std::unique_ptr<CodecBase<sensor_msgs::msg::Image>> codec_;

  /// @brief Flag indicating if decoder has been initialized
  mutable bool decoder_initialized_ = false;

  /// @brief Quality monitor for adaptive bitrate control
  std::unique_ptr<AdaptiveQualityMonitor> quality_monitor_;

  /// @brief Shared pointer to ROS 2 node for service calls
  rclcpp::Node::SharedPtr node_ptr_;

  /// @brief Logger instance for this subscriber
  rclcpp::Logger logger_;

  /// @brief Type of codec to use (read from YAML configuration)
  std::string codec_type_ = "ffmpeg";
};

}  // namespace abr_image_transport
