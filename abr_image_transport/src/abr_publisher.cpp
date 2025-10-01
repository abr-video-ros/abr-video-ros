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

#include "abr_image_transport/abr_publisher.hpp"
#include "abr_image_transport_codecs/codec_factory.hpp"
#include <yaml-cpp/yaml.h>
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <rclcpp/logging.hpp>

namespace abr_image_transport
{

// ============================================================================
// LIFECYCLE MANAGEMENT
// ============================================================================

AbrPublisher::AbrPublisher()
: logger_(rclcpp::get_logger("AbrPublisher"))
{
  RCLCPP_DEBUG(logger_, "ABR Publisher created");
}

AbrPublisher::~AbrPublisher()
{
  cleanup();
  RCLCPP_DEBUG(logger_, "ABR Publisher destroyed");
}

void AbrPublisher::cleanup()
{
  if (encoder_initialized_ && codec_) {
    codec_->closeEncoder();
    encoder_initialized_ = false;
  }
}

// ============================================================================
// PUBLISHER SETUP
// ============================================================================

void AbrPublisher::advertiseImpl(
  rclcpp::Node * node,
  const std::string & base_topic,
  rmw_qos_profile_t custom_qos,
  rclcpp::PublisherOptions options)
{
  // Update logger with node context
  logger_ = node->get_logger();

  // Load configuration and initialize components
  if (!loadConfiguration()) {
    throw std::runtime_error("Failed to load ABR Publisher configuration");
  }

  if (!initializeCodec()) {
    throw std::runtime_error("Failed to initialize codec");
  }

  // Setup base publisher
  using BaseType = image_transport::SimplePublisherPlugin<abr_image_transport_interfaces::msg::AbrImage>;
  BaseType::advertiseImpl(node, base_topic, custom_qos, options);

  // Setup parameter service
  setupParameterService(node);

  RCLCPP_INFO(logger_, "ABR Publisher advertised on topic: %s", base_topic.c_str());
}

// ============================================================================
// CONFIGURATION MANAGEMENT
// ============================================================================

bool AbrPublisher::loadConfiguration()
{
  try {
    const std::string config_file = getConfigFilePath();
    YAML::Node config = YAML::LoadFile(config_file);

    loadGlobalConfig(config);
    loadPublisherConfig(config);

    RCLCPP_INFO(logger_, "Configuration loaded successfully from: %s", config_file.c_str());
    return true;

  } catch (const std::exception & e) {
    RCLCPP_ERROR(logger_, "Failed to load configuration: %s", e.what());
    return false;
  }
}

std::string AbrPublisher::getConfigFilePath() const
{
  return ament_index_cpp::get_package_share_directory("abr_image_transport") +
         "/params/abr_config.yaml";
}

void AbrPublisher::loadGlobalConfig(const YAML::Node & config)
{
  if (config["global"] && config["global"]["codec_type"]) {
    codec_type_ = config["global"]["codec_type"].as<std::string>();
    RCLCPP_INFO(logger_, "Codec type: %s", codec_type_.c_str());
  } else {
    codec_type_ = "ffmpeg";  // Safe default
    RCLCPP_WARN(logger_, "No codec_type specified, using default: %s", codec_type_.c_str());
  }
}

void AbrPublisher::loadPublisherConfig(const YAML::Node & config)
{
  if (!config["abr_publisher"]) {
    RCLCPP_WARN(logger_, "No abr_publisher configuration found, using defaults");
    return;
  }

  const auto pub_config = config["abr_publisher"];

  // Extract configuration values with defaults
  const int bitrate = pub_config["bitrate"] ? pub_config["bitrate"].as<int>() : 2000000;
  const int fps = pub_config["fps"] ? pub_config["fps"].as<int>() : 30;
  service_name_ =
    pub_config["service_name"] ? pub_config["service_name"].as<std::string>() : "abr_params";

  // Store for codec initialization
  default_bitrate_ = bitrate;
  default_fps_ = fps;

  RCLCPP_INFO(logger_, "Publisher config - Bitrate: %d bps, FPS: %d, Service: %s",
              bitrate, fps, service_name_.c_str());
}

bool AbrPublisher::initializeCodec()
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
      std::string yaml_file =
        ament_index_cpp::get_package_share_directory("abr_image_transport") +
        "/params/abr_config.yaml";
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

void AbrPublisher::setAbrParams(
  const std::shared_ptr<abr_image_transport_interfaces::srv::AbrParams::Request> req,
  std::shared_ptr<abr_image_transport_interfaces::srv::AbrParams::Response> res)
{
  try {
    // Validate bitrate range
    if (req->bitrate < 50000 || req->bitrate > 10000000) {
      res->success = false;
      res->message = "Bitrate out of valid range (50kbps - 10Mbps)";
      RCLCPP_WARN(logger_, "Invalid bitrate requested: %d", req->bitrate);
      return;
    }

    // Update codec parameters using the safe updateBitrate method
    RCLCPP_INFO(logger_, "ABR parameters updated - New bitrate: %d bps", req->bitrate);

    try {
      // Use the safe bitrate update method instead of reinitializing
      if (codec_ && codec_->updateBitrate(req->bitrate)) {
        RCLCPP_INFO(logger_, "Bitrate updated successfully without reinitialization");
      } else {
        RCLCPP_WARN(logger_, "Failed to update bitrate, will apply on next frame");
      }
    } catch (const std::exception & e) {
      RCLCPP_ERROR(logger_, "Exception updating bitrate: %s", e.what());
    }

    res->success = true;
    res->message = "Bitrate successfully updated to " + std::to_string(req->bitrate) + " bps";

  } catch (const std::exception & e) {
    res->success = false;
    res->message = "Failed to update parameters: " + std::string(e.what());
    RCLCPP_ERROR(logger_, "Exception in setAbrParams: %s", e.what());
  }
}

void AbrPublisher::publish(
  const sensor_msgs::msg::Image & message,
  const PublishFn & publish_fn) const
{
  // Initialize codec if not already done
  if (!encoder_initialized_) {
    codec_->initEncoder();
    encoder_initialized_ = true;
  }

  std::vector<uint8_t> encoded_data = codec_->encode(message);

  if (!encoded_data.empty()) {
    abr_image_transport_interfaces::msg::AbrImage abr_msg;
    abr_msg.header = message.header;
    abr_msg.original_width = message.width;
    abr_msg.original_height = message.height;
    abr_msg.encoding = codec_->name();
    abr_msg.compressed_data = std::move(encoded_data);

    publish_fn(abr_msg);
  } else {
    RCLCPP_WARN(logger_, "Empty encoded frame, skipping publish");
  }
}

// ============================================================================
// SERVICE MANAGEMENT
// ============================================================================

void AbrPublisher::setupParameterService(rclcpp::Node * node)
{
  if (!node) {
    RCLCPP_ERROR(logger_, "Cannot setup parameter service with null node");
    return;
  }

  try {
    // Create parameter service
    srv_ = node->create_service<abr_image_transport_interfaces::srv::AbrParams>(
      service_name_,
      [this](const std::shared_ptr<abr_image_transport_interfaces::srv::AbrParams::Request> req,
      std::shared_ptr<abr_image_transport_interfaces::srv::AbrParams::Response> res) {
        this->setAbrParams(req, res);
      });

    RCLCPP_INFO(logger_, "Parameter service '%s' initialized successfully", service_name_.c_str());

  } catch (const std::exception & e) {
    RCLCPP_ERROR(logger_, "Failed to create parameter service '%s': %s",
                 service_name_.c_str(), e.what());
  }
}

}  // namespace abr_image_transport
