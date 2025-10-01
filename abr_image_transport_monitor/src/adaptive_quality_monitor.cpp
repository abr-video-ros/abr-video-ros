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

#include "abr_image_transport_monitor/adaptive_quality_monitor.hpp"
#include <cmath>

namespace abr_image_transport
{

AdaptiveQualityMonitor::AdaptiveQualityMonitor(rclcpp::Node::SharedPtr node)
: node_(std::move(node))
{
  if (!node_) {
    throw std::invalid_argument("AdaptiveQualityMonitor: node cannot be null");
  }

  try {
    // Create service client for updating encoder parameters
    RCLCPP_INFO(node_->get_logger(), "AdaptiveQualityMonitor initializing...");

    client_ = node_->create_client<abr_image_transport_interfaces::srv::AbrParams>("abr_params");
    if (!client_) {
      RCLCPP_WARN(node_->get_logger(), "Failed to create ABR params service client");
    }

    loadDefaultYaml();

    RCLCPP_INFO(node_->get_logger(), "AdaptiveQualityMonitor initialized successfully");
  } catch (const std::exception & e) {
    RCLCPP_ERROR(node_->get_logger(), "Failed to initialize AdaptiveQualityMonitor: %s", e.what());
    throw;
  }
}

void AdaptiveQualityMonitor::loadDefaultYaml()
{
  // Load configuration file from package share
  const std::string yaml_file =
    ament_index_cpp::get_package_share_directory("abr_image_transport") +
    "/params/abr_config.yaml";

  try {
    YAML::Node cfg = YAML::LoadFile(yaml_file);
    if (cfg["abr_monitor"]) {
      auto y = cfg["abr_monitor"];
      min_bitrate_safe_ =
        y["min_bitrate_safe"] ? y["min_bitrate_safe"].as<int>() : min_bitrate_safe_;
      max_bitrate_safe_ =
        y["max_bitrate_safe"] ? y["max_bitrate_safe"].as<int>() : max_bitrate_safe_;
      fps_variance_threshold_ =
        y["fps_variance_threshold"] ? y["fps_variance_threshold"].as<double>() :
        fps_variance_threshold_;
      increase_factor_ =
        y["increase_factor"] ? y["increase_factor"].as<double>() : increase_factor_;
      decrease_factor_ =
        y["decrease_factor"] ? y["decrease_factor"].as<double>() : decrease_factor_;
      stable_window_ = y["stable_window"] ? y["stable_window"].as<size_t>() : stable_window_;

      // Load new parameters from centralized config
      if (y["window_size"]) {
        stable_window_ = y["window_size"].as<size_t>();
      }
      if (y["adaptation_enabled"]) {
        bool adaptation_enabled = y["adaptation_enabled"].as<bool>();
        RCLCPP_INFO(node_->get_logger(), "Adaptation %s",
                   adaptation_enabled ? "enabled" : "disabled");
      }
    }
  } catch (const std::exception & e) {
    RCLCPP_WARN(node_->get_logger(), "Could not load YAML config: %s", e.what());
  }

  RCLCPP_INFO(node_->get_logger(), "ABR monitor config loaded: %s", yaml_file.c_str());
}

void AdaptiveQualityMonitor::updateAndAdapt(size_t packet_size)
{
  // Early return if adaptation is disabled or node is invalid
  if (!adaptation_enabled_ || !node_) {
    return;
  }

  // Validate packet size
  if (packet_size == 0) {
    RCLCPP_WARN_THROTTLE(node_->get_logger(), *node_->get_clock(), 5000,
                         "Received zero-size packet, skipping adaptation");
    return;
  }

  try {
    using namespace std::chrono;
    auto now = steady_clock::now();

    // Store history of timestamps for stability analysis
    timestamps_.push_back(now);
    size_history_.push_back(packet_size);

    // Maintain sliding window size with bounds checking
    const size_t max_window = std::max(stable_window_, static_cast<size_t>(10));
    if (timestamps_.size() > max_window) {
      timestamps_.pop_front();
    }
    if (size_history_.size() > max_window) {
      size_history_.pop_front();
    }

    // Need at least 2 timestamps to calculate FPS
    if (timestamps_.size() < 2) {
      return;
    }

    // Calculate FPS values based on inter-frame time differences
    std::vector<double> fps_window;
    fps_window.reserve(timestamps_.size() - 1);

    for (size_t i = 1; i < timestamps_.size(); ++i) {
      double dt = duration_cast<milliseconds>(timestamps_[i] - timestamps_[i - 1]).count() / 1000.0;
      if (dt > 0.001 && dt < 10.0) {  // Reasonable time range (1ms to 10s)
        fps_window.push_back(1.0 / dt);
      }
    }

    // Need sufficient data for reliable statistics
    if (fps_window.size() < 3) {
      return;
    }

    // Compute average FPS
    double fps_avg = 0.0;
    for (double f : fps_window) {
      fps_avg += f;
    }
    fps_avg /= fps_window.size();

    // Validate FPS is reasonable
    if (fps_avg <= 0.0 || fps_avg > 1000.0) {
      RCLCPP_WARN_THROTTLE(node_->get_logger(), *node_->get_clock(), 5000,
                           "Invalid FPS calculated: %.2f, skipping adaptation", fps_avg);
      return;
    }

    // Calculate variance with numerical stability
    double variance = 0.0;
    for (double f : fps_window) {
      double diff = f - fps_avg;
      variance += diff * diff;
    }
    variance /= fps_window.size();
    double stddev = std::sqrt(variance);

    // Relative variance (instability metric) with safe division
    double fps_var_rel = (fps_avg > 1e-6) ? (stddev / fps_avg) : 0.0;

    // Decide bitrate adjustment with bounds checking
    double desired_bitrate = static_cast<double>(current_bitrate_);

    if (fps_var_rel > fps_variance_threshold_) {
      // Stream unstable, decrease bitrate
      desired_bitrate = std::max(desired_bitrate * decrease_factor_,
                                static_cast<double>(min_bitrate_safe_));
      stable_count_ = 0;
    } else {
      // Stream stable, gradually increase bitrate
      stable_count_++;
      if (stable_count_ >= stable_window_) {
        desired_bitrate = std::min(desired_bitrate * increase_factor_,
                                  static_cast<double>(max_bitrate_safe_));
        stable_count_ = 0;
      }
    }

    // Validate desired bitrate is within bounds
    desired_bitrate = std::clamp(desired_bitrate,
                                static_cast<double>(min_bitrate_safe_),
                                static_cast<double>(max_bitrate_safe_));

    // Apply changes if bitrate changed significantly (avoid micro-adjustments)
    int new_bitrate = static_cast<int>(desired_bitrate);
    if (std::abs(new_bitrate - current_bitrate_) > (current_bitrate_ * 0.05)) {  // 5% threshold
      RCLCPP_DEBUG(node_->get_logger(),
        "ABR monitor: pkt=%zu fps_avg=%.2f fps_var=%.3f bitrate=%d->%d",
        packet_size, fps_avg, fps_var_rel, current_bitrate_, new_bitrate);

      current_bitrate_ = new_bitrate;
      sendRequest();
    }

  } catch (const std::exception & e) {
    RCLCPP_ERROR(node_->get_logger(), "Exception in updateAndAdapt: %s", e.what());
  } catch (...) {
    RCLCPP_ERROR(node_->get_logger(), "Unknown exception in updateAndAdapt");
  }
}

void AdaptiveQualityMonitor::sendRequest()
{
  // Validate node and client before proceeding
  if (!node_ || !client_) {
    RCLCPP_WARN(rclcpp::get_logger("abr_monitor"), "Node or client is null, cannot send request");
    return;
  }

  // Wait for service to be ready
  if (!client_->service_is_ready()) {
    RCLCPP_DEBUG(node_->get_logger(), "ABR service not ready, skipping request");
    return;
  }

  try {
    // Send request with error handling
    auto req = std::make_shared<abr_image_transport_interfaces::srv::AbrParams::Request>();
    req->bitrate = current_bitrate_;

    // Use callback to handle response/error
    auto future = client_->async_send_request(req,
        [this](rclcpp::Client<abr_image_transport_interfaces::srv::AbrParams>::SharedFuture future)
        {
          try {
            auto response = future.get();
            RCLCPP_DEBUG(node_->get_logger(), "ABR bitrate update successful: %d bps",
            current_bitrate_);
          } catch (const std::exception & e) {
            RCLCPP_WARN(node_->get_logger(), "ABR service call failed: %s", e.what());
          }
      });

    RCLCPP_INFO(node_->get_logger(), "ABR monitor: requested bitrate=%d", current_bitrate_);
  } catch (const std::exception & e) {
    RCLCPP_ERROR(node_->get_logger(), "Exception in sendRequest: %s", e.what());
  }
}

}  // namespace
