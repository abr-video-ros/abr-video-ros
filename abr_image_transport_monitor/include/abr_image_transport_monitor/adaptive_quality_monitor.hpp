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

#include <deque>
#include <chrono>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <abr_image_transport_interfaces/srv/abr_params.hpp>
#include <yaml-cpp/yaml.h>
#include <ament_index_cpp/get_package_share_directory.hpp>

namespace abr_image_transport
{

/**
 * @brief Adaptive quality monitor for real-time bitrate optimization
 *
 * This class implements an intelligent monitoring system that analyzes video
 * stream characteristics in real-time to optimize encoding parameters. It
 * tracks packet sizes, frame rates, and network conditions to automatically
 * adjust bitrate for optimal quality-bandwidth trade-offs.
 *
 * The monitor employs several strategies:
 * - **Stability Analysis**: Monitors frame rate variance to detect network issues
 * - **Packet Size Tracking**: Analyzes compressed data sizes to infer quality
 * - **Conservative Adaptation**: Uses hysteresis to prevent oscillation
 * - **Service Integration**: Communicates with publishers via ROS 2 services
 *
 * Algorithm Overview:
 * 1. Collect packet size and timing data in sliding windows
 * 2. Calculate frame rate stability metrics
 * 3. Apply adaptation rules based on current conditions
 * 4. Send parameter updates to publishers when needed
 *
 * @note This class is designed to be used by ABR subscribers to provide
 *       feedback to publishers for optimal streaming quality.
 */
class AdaptiveQualityMonitor
{
public:
  /**
   * @brief Constructor
   *
   * Initializes the monitor with the given ROS 2 node, loads configuration
   * from YAML, and sets up service client for parameter updates.
   *
   * @param node Shared pointer to ROS 2 node for logging and service calls
   * @throws std::runtime_error if configuration loading fails
   */
  explicit AdaptiveQualityMonitor(rclcpp::Node::SharedPtr node);

  /**
   * @brief Destructor
   *
   * Cleans up resources and ensures proper service client shutdown.
   */
  ~AdaptiveQualityMonitor() = default;

  /**
   * @brief Process new packet and adapt bitrate if necessary
   *
   * This is the main entry point for the monitoring algorithm. It:
   * 1. Records the packet size and current timestamp
   * 2. Updates internal sliding window buffers
   * 3. Analyzes frame rate stability and quality metrics
   * 4. Triggers bitrate adaptation if conditions are met
   * 5. Sends service requests to update publisher parameters
   *
   * @param packet_size Size of the received compressed packet in bytes
   *
   * @note This method should be called for every received video packet
   *       to maintain accurate monitoring statistics.
   */
  void updateAndAdapt(size_t packet_size);

  /**
   * @brief Get current bitrate setting
   *
   * @return int Current target bitrate in bits per second
   */
  int getCurrentBitrate() const {return current_bitrate_;}

  /**
   * @brief Check if adaptation is currently enabled
   *
   * @return bool True if automatic adaptation is active
   */
  bool isAdaptationEnabled() const {return adaptation_enabled_;}

  /**
   * @brief Enable or disable automatic adaptation
   *
   * @param enabled True to enable adaptation, false to disable
   */
  void setAdaptationEnabled(bool enabled) {adaptation_enabled_ = enabled;}

private:
  /**
   * @brief Load configuration parameters from YAML file
   *
   * Reads the centralized abr_config.yaml file and applies monitor
   * configuration including thresholds, adaptation factors, and limits.
   * Falls back to default values if parameters are missing.
   *
   * @throws std::runtime_error if YAML file cannot be read
   */
  void loadDefaultYaml();

  /**
   * @brief Send bitrate update request to publisher
   *
   * Creates and sends an asynchronous service request to update the
   * publisher's encoding bitrate. Handles service availability and
   * error conditions gracefully.
   *
   * @note Uses async service calls to avoid blocking the monitoring loop
   */
  void sendRequest();

  /**
   * @brief Calculate frame rate statistics from timestamp history
   *
   * Analyzes recent timestamps to compute average frame rate and
   * variance for stability assessment.
   *
   * @return std::pair<double, double> Average FPS and variance
   */
  std::pair<double, double> calculateFpsStats() const;

  /**
   * @brief Determine if current conditions are stable enough for upscaling
   *
   * @return bool True if conditions allow bitrate increase
   */
  bool isStableForIncrease() const;

  /**
   * @brief Determine if current conditions require bitrate reduction
   *
   * @return bool True if bitrate should be decreased
   */
  bool shouldDecrease() const;

  /// @name ROS 2 Integration
  /// @{
  rclcpp::Node::SharedPtr node_;  ///< ROS 2 node for logging and services
  rclcpp::Client<abr_image_transport_interfaces::srv::AbrParams>::SharedPtr client_;  ///< Service client for parameter updates
  /// @}

  /// @name Monitoring Data
  /// @{
  std::deque<size_t> size_history_;  ///< Sliding window of packet sizes (bytes)
  std::deque<std::chrono::steady_clock::time_point> timestamps_;  ///< Sliding window of packet timestamps
  /// @}

  /// @name Adaptation Parameters
  /// @{
  int current_bitrate_ = 400000;    ///< Current target bitrate (bps)
  int min_bitrate_safe_ = 100000;   ///< Minimum allowed bitrate (bps)
  int max_bitrate_safe_ = 2000000;  ///< Maximum allowed bitrate (bps)
  double increase_factor_ = 1.5;    ///< Multiplier for bitrate increases
  double decrease_factor_ = 0.7;    ///< Multiplier for bitrate decreases
  /// @}

  /// @name Stability Tracking
  /// @{
  size_t stable_count_ = 0;                 ///< Counter for consecutive stable frames
  size_t stable_window_ = 10;               ///< Required stable frames before increase
  double fps_variance_threshold_ = 2.0;     ///< Maximum FPS variance for stability
  /// @}

  /// @name Control Flags
  /// @{
  bool adaptation_enabled_ = true;  ///< Enable/disable automatic adaptation
  /// @}
};

}  // namespace abr_image_transport
