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

#include "abr_image_transport/abr_subscriber.hpp"
#include "abr_image_transport_codecs/codec_factory.hpp"
#include <rclcpp/logging.hpp>
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <yaml-cpp/yaml.h>

namespace abr_image_transport
{

// ============================================================================
// LIFECYCLE MANAGEMENT
// ============================================================================

AbrSubscriber::AbrSubscriber()
: logger_(rclcpp::get_logger("AbrSubscriber"))
{
  RCLCPP_INFO(logger_, "ABR Subscriber initialized");
}

AbrSubscriber::~AbrSubscriber()
{
  cleanup();
  RCLCPP_INFO(logger_, "ABR Subscriber destroyed");
}

void AbrSubscriber::cleanup()
{
  try {
    // Clean up decoder if initialized
    if (decoder_initialized_ && codec_) {
      codec_->closeDecoder();
      decoder_initialized_ = false;
    }

    // Reset resources
    codec_.reset();
    quality_monitor_.reset();
    node_ptr_.reset();

    RCLCPP_DEBUG(logger_, "ABR Subscriber cleanup completed");
  } catch (const std::exception & e) {
    RCLCPP_WARN(logger_, "Exception during cleanup: %s", e.what());
  }
}

// ============================================================================
// SUBSCRIPTION MANAGEMENT
// ============================================================================

void AbrSubscriber::subscribeImpl(
  rclcpp::Node * node,
  const std::string & base_topic,
  const Callback & callback,
  rmw_qos_profile_t custom_qos,
  rclcpp::SubscriptionOptions options)
{
  if (!node) {
    RCLCPP_ERROR(logger_, "Cannot subscribe with null node");
    return;
  }

  // Store node reference for monitor and services
  node_ptr_ = node->shared_from_this();

  // Load configuration and initialize codec
  if (!loadConfiguration()) {
    RCLCPP_ERROR(logger_, "Failed to load configuration, aborting subscription");
    return;
  }

  if (!initializeCodec()) {
    RCLCPP_ERROR(logger_, "Failed to initialize codec, aborting subscription");
    return;
  }

  // Setup quality monitor
  if (!setupQualityMonitor(node)) {
    RCLCPP_WARN(logger_, "Failed to setup quality monitor, continuing without it");
  }

  // Setup base subscription
  using BaseType = image_transport::SimpleSubscriberPlugin<abr_image_transport_interfaces::msg::AbrImage>;
  BaseType::subscribeImpl(node, base_topic, callback, custom_qos, options);

  RCLCPP_INFO(logger_, "ABR Subscriber subscribed to topic: %s", base_topic.c_str());
}

// ============================================================================
// CONFIGURATION MANAGEMENT
// ============================================================================

bool AbrSubscriber::loadConfiguration()
{
  try {
    const std::string config_file = getConfigFilePath();
    YAML::Node config = YAML::LoadFile(config_file);

    loadGlobalConfig(config);
    loadSubscriberConfig(config);

    RCLCPP_INFO(logger_, "Configuration loaded successfully from: %s", config_file.c_str());
    return true;

  } catch (const std::exception & e) {
    RCLCPP_ERROR(logger_, "Failed to load configuration: %s", e.what());
    return false;
  }
}

std::string AbrSubscriber::getConfigFilePath() const
{
  return ament_index_cpp::get_package_share_directory("abr_image_transport") +
         "/params/abr_config.yaml";
}

void AbrSubscriber::loadGlobalConfig(const YAML::Node & config)
{
  if (!config["global"]) {
    RCLCPP_WARN(logger_, "No global configuration found, using defaults");
    return;
  }

  const auto global_config = config["global"];

  // Load codec type
  codec_type_ = global_config["codec_type"] ?
    global_config["codec_type"].as<std::string>() : "ffmpeg";

  RCLCPP_INFO(logger_, "Global config - Codec type: %s", codec_type_.c_str());
}

void AbrSubscriber::loadSubscriberConfig(const YAML::Node & config)
{
  if (!config["abr_subscriber"]) {
    RCLCPP_INFO(logger_, "No subscriber-specific configuration found, using defaults");
    return;
  }

  const auto sub_config = config["abr_subscriber"];

  // Add any subscriber-specific configuration loading here
  // For now, subscriber doesn't have specific configuration parameters
  // but this method is ready for future extensions

  RCLCPP_DEBUG(logger_, "Subscriber configuration loaded");
}

// ============================================================================
// MESSAGE PROCESSING
// ============================================================================

void AbrSubscriber::internalCallback(
  const abr_image_transport_interfaces::msg::AbrImage::ConstSharedPtr & message,
  const Callback & user_cb)
{
  try {
    // Validate inputs
    if (!message) {
      RCLCPP_WARN(logger_, "Received null message, skipping");
      return;
    }

    if (!codec_) {
      RCLCPP_ERROR(logger_, "Codec not initialized, cannot process message");
      return;
    }

    // Initialize decoder on first use
    if (!decoder_initialized_) {
      codec_->initDecoder();
      decoder_initialized_ = true;
      RCLCPP_DEBUG(logger_, "Decoder initialized");
    }

    // Monitor packet size for adaptive quality
    if (quality_monitor_) {
      quality_monitor_->updateAndAdapt(message->compressed_data.size());
    }

    // Decode compressed data
    sensor_msgs::msg::Image decoded_msg = codec_->decode(message->compressed_data);

    if (decoded_msg.data.empty()) {
      RCLCPP_WARN(logger_, "Empty decoded frame, skipping callback");
      return;
    }

    // Invoke user callback with decoded image
    auto decoded_msg_ptr = std::make_shared<sensor_msgs::msg::Image>(std::move(decoded_msg));
    user_cb(decoded_msg_ptr);

  } catch (const std::exception & e) {
    RCLCPP_ERROR(logger_, "Exception in internalCallback: %s", e.what());
  }
}

bool AbrSubscriber::initializeCodec()
{
  try {
    // Create codec instance based on codec_type
    codec_ = abr_image_transport::CodecFactory<sensor_msgs::msg::Image>::create(codec_type_);

    if (!codec_) {
      RCLCPP_ERROR(logger_, "Failed to create codec of type: %s", codec_type_.c_str());

      // Show available codecs
      auto available =
        abr_image_transport::CodecFactory<sensor_msgs::msg::Image>::getAvailableCodecs();
      RCLCPP_INFO(logger_, "Available codec types:");
      for (const auto & codec_name : available) {
        RCLCPP_INFO(logger_, "  - %s", codec_name.c_str());
      }

      // Fallback to default
      codec_type_ = "ffmpeg";
      codec_ = abr_image_transport::CodecFactory<sensor_msgs::msg::Image>::create(codec_type_);
      RCLCPP_WARN(logger_, "Falling back to default codec: %s", codec_type_.c_str());
    }

    if (codec_) {
      RCLCPP_INFO(logger_, "Successfully created codec: %s (name: %s)",
                  codec_type_.c_str(), codec_->name().c_str());

      // Load codec configuration from YAML file
      std::string yaml_file = getConfigFilePath();
      codec_->loadConfiguration(yaml_file);

      return true;
    } else {
      RCLCPP_ERROR(logger_, "Failed to create any codec implementation");
      return false;
    }
  } catch (const std::exception & e) {
    RCLCPP_ERROR(logger_, "Exception during codec initialization: %s", e.what());
    return false;
  }
}

bool AbrSubscriber::setupQualityMonitor(rclcpp::Node * node)
{
  if (!node) {
    RCLCPP_ERROR(logger_, "Cannot setup quality monitor with null node");
    return false;
  }

  try {
    quality_monitor_ = std::make_unique<AdaptiveQualityMonitor>(node_ptr_);
    RCLCPP_INFO(logger_, "Quality monitor initialized successfully");
    return true;

  } catch (const std::exception & e) {
    RCLCPP_ERROR(logger_, "Failed to create quality monitor: %s", e.what());
    return false;
  }
}

}  // namespace abr_image_transport
