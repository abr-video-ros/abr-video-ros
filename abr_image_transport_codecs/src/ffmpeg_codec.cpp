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

#include "abr_image_transport_codecs/ffmpeg_codec.hpp"
#include <stdexcept>
#include <rclcpp/logging.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>
#include <ament_index_cpp/get_package_share_directory.hpp>

extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/log.h>
}

namespace abr_image_transport
{

// ============================================================================
// LIFECYCLE MANAGEMENT
// ============================================================================

FfmpegCodec::FfmpegCodec()
{
  // Set reasonable FFmpeg logging level (configurable via YAML)
  av_log_set_level(AV_LOG_WARNING);

  RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"), "FfmpegCodec initialized");
}

FfmpegCodec::~FfmpegCodec()
{
  try {
    // RAII cleanup of all resources
    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"), "FfmpegCodec destroying...");

    closeEncoder();
    closeDecoder();

    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"), "FfmpegCodec destroyed successfully");

  } catch (const std::exception & e) {
    // Use fprintf instead of RCLCPP in destructor to avoid potential issues
    fprintf(stderr, "Exception in FfmpegCodec destructor: %s\n", e.what());
  } catch (...) {
    fprintf(stderr, "Unknown exception in FfmpegCodec destructor\n");
  }
}

// ============================================================================
// CONFIGURATION INTERFACE
// ============================================================================

void FfmpegCodec::configure(const CodecConfig & config)
{
  // Apply basic configuration
  bitrate_ = config.bitrate;
  fps_ = config.fps;

  // Load extended configuration if specified
  if (!config.config_section.empty()) {
    loadConfiguration(config.config_section);
  }

  RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
              "Configured: bitrate=%d, fps=%d", bitrate_, fps_);
}

void FfmpegCodec::loadConfiguration(const std::string & yaml_file_or_section)
{
  try {
    // Resolve configuration file path
    const std::string yaml_file = resolveConfigPath(yaml_file_or_section);

    // Load and validate YAML structure
    YAML::Node cfg = YAML::LoadFile(yaml_file);
    validateConfigStructure(cfg, yaml_file);

    // Extract configuration sections
    YAML::Node ffmpeg_config = cfg["codecs"]["ffmpeg"];

    // Load global settings first
    loadGlobalSettings(cfg);

    // Apply configuration in logical order
    applyCodecConfiguration(ffmpeg_config);

    // Log successful configuration
    logConfigurationSummary(yaml_file);

  } catch (const std::exception & e) {
    throw std::runtime_error("Failed to load FFmpeg configuration: " + std::string(e.what()));
  }
}

std::string FfmpegCodec::resolveConfigPath(const std::string & yaml_file_or_section) const
{
  // Check if it's a file path or section name
  if (yaml_file_or_section.find('/') != std::string::npos ||
    yaml_file_or_section.find('.') != std::string::npos)
  {
    return yaml_file_or_section;  // It's a file path
  }

  // It's a section name, use default config file
  return ament_index_cpp::get_package_share_directory("abr_image_transport") +
         "/params/abr_config.yaml";
}

void FfmpegCodec::validateConfigStructure(
  const YAML::Node & cfg,
  const std::string & yaml_file) const
{
  if (!cfg["codecs"]) {
    throw std::runtime_error("No 'codecs' section found in YAML file: " + yaml_file);
  }
  if (!cfg["codecs"]["ffmpeg"]) {
    throw std::runtime_error("No 'ffmpeg' codec configuration found in YAML file: " + yaml_file);
  }
}

void FfmpegCodec::loadGlobalSettings(const YAML::Node & cfg)
{
  if (cfg["global"] && cfg["global"]["priority_mode"]) {
    priority_mode_ = cfg["global"]["priority_mode"].as<std::string>();
    RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"), "Priority mode: %s", priority_mode_.c_str());
  }
}

void FfmpegCodec::applyCodecConfiguration(const YAML::Node & ffmpeg_config)
{
  // 1. Load codec selection first
  if (ffmpeg_config["codec_name"]) {
    codec_name_ = ffmpeg_config["codec_name"].as<std::string>();
  }

  // 2. Configure logging before other operations
  if (ffmpeg_config["logging"]) {
    loadLoggingConfig(ffmpeg_config["logging"]);
  }

  // 3. Set codec-appropriate defaults
  setCodecDefaults();

  // 4. Apply priority profile (overrides defaults)
  applyPriorityProfile(ffmpeg_config);

  // 5. Load specific configurations (fine-tune settings)
  if (ffmpeg_config["encoder"]) {
    loadEncoderConfig(ffmpeg_config["encoder"]);
  }

  if (ffmpeg_config["codec_configs"] && ffmpeg_config["codec_configs"][codec_name_]) {
    loadCodecSpecificConfig(ffmpeg_config["codec_configs"][codec_name_]);
  }

  if (ffmpeg_config["decoder"]) {
    loadDecoderConfig(ffmpeg_config["decoder"]);
  }
}

void FfmpegCodec::logConfigurationSummary(const std::string & yaml_file) const
{
  RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
              "Configuration loaded from: %s", yaml_file.c_str());
  RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
              "Codec: %s | Preset: %s | Profile: %s | Priority: %s",
              codec_name_.c_str(), preset_.c_str(), profile_.c_str(), priority_mode_.c_str());
  RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
              "Bitrate: %d bps | FPS: %d | GOP: %d | B-frames: %d",
              bitrate_, fps_, keyframe_interval_, b_frames_);
}

// ============================================================================
// CONFIGURATION HELPER IMPLEMENTATIONS
// ============================================================================

void FfmpegCodec::setCodecDefaults()
{
  // Set codec-appropriate default values to avoid conflicts
  if (codec_name_ == "mpeg4") {
    // MPEG-4 defaults (no preset/profile concepts)
    preset_ = "";        // MPEG-4 doesn't use presets
    tune_ = "";          // MPEG-4 doesn't use tune
    profile_ = "";       // MPEG-4 uses different profile system
    level_ = "";         // MPEG-4 uses different level system
  } else if (codec_name_ == "libx264") {
    // H.264 defaults (current defaults are already correct)
    preset_ = "ultrafast";
    tune_ = "zerolatency";
    profile_ = "baseline";
    level_ = "3.1";
  } else if (codec_name_ == "libx265") {
    // H.265 defaults
    preset_ = "ultrafast";
    tune_ = "zerolatency";
    profile_ = "main";
    level_ = "4.0";
  } else {
    // Generic defaults for unknown codecs (minimal settings)
    preset_ = "";
    tune_ = "";
    profile_ = "";
    level_ = "";
  }

  RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"),
               "Set defaults for codec %s: preset='%s', profile='%s'",
               codec_name_.c_str(), preset_.c_str(), profile_.c_str());
}

void FfmpegCodec::applyPriorityProfile(const YAML::Node & config_root)
{
  RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"),
               "Applying priority profile: %s", priority_mode_.c_str());

  try {
    // Get priority profiles section
    if (!config_root["priority_profiles"]) {
      RCLCPP_WARN(rclcpp::get_logger("FfmpegCodec"),
                  "No priority_profiles section found in config, using defaults");
      return;
    }

    const auto priority_profiles = config_root["priority_profiles"];

    // Get the current priority mode profile
    if (!priority_profiles[priority_mode_]) {
      RCLCPP_WARN(rclcpp::get_logger("FfmpegCodec"),
                  "Priority mode '%s' not found in config, using defaults",
                  priority_mode_.c_str());
      return;
    }

    const auto profile = priority_profiles[priority_mode_];

    // Apply codec-specific settings
    if (profile[codec_name_]) {
      const auto codec_profile = profile[codec_name_];

      if (codec_profile["preset"]) {
        preset_ = codec_profile["preset"].as<std::string>();
      }
      if (codec_profile["tune"]) {
        tune_ = codec_profile["tune"].as<std::string>();
      }
      if (codec_profile["profile"]) {
        profile_ = codec_profile["profile"].as<std::string>();
      }
      if (codec_profile["level"]) {
        level_ = codec_profile["level"].as<std::string>();
      }
      if (codec_profile["bitrate"]) {
        bitrate_ = codec_profile["bitrate"].as<int>();
      }
      if (codec_profile["keyframe_interval"]) {
        keyframe_interval_ = codec_profile["keyframe_interval"].as<int>();
      }
      if (codec_profile["b_frames"]) {
        b_frames_ = codec_profile["b_frames"].as<int>();
      }
      if (codec_profile["crf"]) {
        crf_ = codec_profile["crf"].as<int>();
      }
      if (codec_profile["sc_threshold"]) {
        sc_threshold_ = codec_profile["sc_threshold"].as<int>();
      }
      if (codec_profile["max_qp"]) {
        max_qp_ = codec_profile["max_qp"].as<int>();
      }
      if (codec_profile["min_qp"]) {
        min_qp_ = codec_profile["min_qp"].as<int>();
      }
      if (codec_profile["refs"]) {
        refs_ = codec_profile["refs"].as<int>();
      }
      if (codec_profile["cabac"]) {
        cabac_ = codec_profile["cabac"].as<bool>();
      }
      if (codec_profile["deblock"]) {
        deblock_ = codec_profile["deblock"].as<bool>();
      }

      RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                  "Applied %s profile for %s: preset='%s', profile='%s', bitrate=%d",
                  priority_mode_.c_str(), codec_name_.c_str(),
                  preset_.c_str(), profile_.c_str(), bitrate_);
    } else {
      RCLCPP_WARN(rclcpp::get_logger("FfmpegCodec"),
                  "No %s profile found for codec %s in priority mode %s",
                  codec_name_.c_str(), codec_name_.c_str(), priority_mode_.c_str());
    }

  } catch (const YAML::Exception & e) {
    RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"),
                 "Error reading priority profile config: %s", e.what());
  }
}

void FfmpegCodec::loadEncoderConfig(const YAML::Node & encoder_config)
{
  // Load common encoder settings (applicable to all codecs)
  keyframe_interval_ = encoder_config["keyframe_interval"] ? encoder_config["keyframe_interval"].as<int>() : keyframe_interval_;
  b_frames_ = encoder_config["b_frames"] ? encoder_config["b_frames"].as<int>() : b_frames_;

  // Rate control settings
  rate_control_ =
    encoder_config["rate_control"] ? encoder_config["rate_control"].as<std::string>() :
    rate_control_;
  hrd_compliance_ = encoder_config["hrd_compliance"] ? encoder_config["hrd_compliance"].as<bool>() : hrd_compliance_;
  crf_ = encoder_config["crf"] ? encoder_config["crf"].as<int>() : crf_;
  min_qp_ = encoder_config["min_qp"] ? encoder_config["min_qp"].as<int>() : min_qp_;
  max_qp_ = encoder_config["max_qp"] ? encoder_config["max_qp"].as<int>() : max_qp_;
}

void FfmpegCodec::loadCodecSpecificConfig(const YAML::Node & codec_config)
{
  // Load codec-specific settings
  preset_ = codec_config["preset"] ? codec_config["preset"].as<std::string>() : preset_;
  tune_ = codec_config["tune"] ? codec_config["tune"].as<std::string>() : tune_;
  profile_ = codec_config["profile"] ? codec_config["profile"].as<std::string>() : profile_;
  level_ = codec_config["level"] ? codec_config["level"].as<std::string>() : level_;
  refs_ = codec_config["refs"] ? codec_config["refs"].as<int>() : refs_;

  // Advanced settings (mainly for H.264/H.265)
  cabac_ = codec_config["cabac"] ? codec_config["cabac"].as<bool>() : cabac_;
  deblock_ = codec_config["deblock"] ? codec_config["deblock"].as<bool>() : deblock_;

  // MPEG-4 specific settings
  sc_threshold_ =
    codec_config["sc_threshold"] ? codec_config["sc_threshold"].as<int>() : sc_threshold_;
}

void FfmpegCodec::loadDecoderConfig(const YAML::Node & decoder_config)
{
  thread_count_ =
    decoder_config["thread_count"] ? decoder_config["thread_count"].as<int>() : thread_count_;
  thread_type_ =
    decoder_config["thread_type"] ? decoder_config["thread_type"].as<std::string>() : thread_type_;
}


void FfmpegCodec::loadLoggingConfig(const YAML::Node & logging_config)
{
  if (logging_config["level"]) {
    std::string level = logging_config["level"].as<std::string>();

    // Map string levels to FFmpeg log levels
    int av_level = AV_LOG_WARNING;  // Default

    if (level == "quiet") {
      av_level = AV_LOG_QUIET;
    } else if (level == "panic") {
      av_level = AV_LOG_PANIC;
    } else if (level == "fatal") {
      av_level = AV_LOG_FATAL;
    } else if (level == "error") {
      av_level = AV_LOG_ERROR;
    } else if (level == "warning") {
      av_level = AV_LOG_WARNING;
    } else if (level == "info") {
      av_level = AV_LOG_INFO;
    } else if (level == "verbose") {
      av_level = AV_LOG_VERBOSE;
    } else if (level == "debug") {
      av_level = AV_LOG_DEBUG;
    } else if (level == "trace") {
      av_level = AV_LOG_TRACE;
    } else {
      RCLCPP_WARN(rclcpp::get_logger("FfmpegCodec"),
                  "Unknown FFmpeg log level '%s', using 'warning'", level.c_str());
    }

    av_log_set_level(av_level);
    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"),
                 "FFmpeg log level set to: %s (%d)", level.c_str(), av_level);
  }
}

bool FfmpegCodec::updateBitrate(int new_bitrate)
{
  if (new_bitrate <= 0) {
    RCLCPP_WARN(rclcpp::get_logger("FfmpegCodec"), "Invalid bitrate: %d", new_bitrate);
    return false;
  }

  try {
    bitrate_ = new_bitrate;

    // If encoder is initialized, update the context safely
    if (encoder_initialized_ && codec_ctx_) {
      // Check if we can safely update bitrate (not in HRD compliance mode)
      if (hrd_compliance_) {
        RCLCPP_WARN(rclcpp::get_logger("FfmpegCodec"),
                   "Cannot update bitrate with HRD compliance enabled - restart encoder or disable HRD");
        return false;
      }

      // Update bitrate parameters for VBR/ABR modes
      codec_ctx_->bit_rate = bitrate_;
      codec_ctx_->rc_max_rate = static_cast<int64_t>(bitrate_ * 1.2);  // 20% headroom
      codec_ctx_->rc_buffer_size = bitrate_ * 2;  // 2 seconds buffer

      RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"),
                   "Updated encoder bitrate to %d bps (max: %ld bps)", 
                   bitrate_, codec_ctx_->rc_max_rate);
      return true;
    }

    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"),
                 "Bitrate updated, will be applied on next encoder initialization");
    return true;  // Configuration updated, will be applied on next init

  } catch (const std::exception & e) {
    RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"),
                 "Exception updating bitrate: %s", e.what());
    return false;
  }
}

// ============================================================================
// ENCODER IMPLEMENTATION
// ============================================================================

void FfmpegCodec::initEncoder()
{
  if (encoder_initialized_) {
    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"), "Encoder already initialized");
    return;
  }

  // Check if we have frame dimensions stored from previous encode() call
  if (dimensions_known_ && original_width_ > 0 && original_height_ > 0) {
    try {
      // Initialize encoder with stored dimensions
      const AVCodec * codec = avcodec_find_encoder_by_name(codec_name_.c_str());
      if (!codec) {
        throw std::runtime_error("Encoder not found for codec: " + codec_name_ +
            ". Please check if the codec is installed on your system.");
      }

      RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"), "Initializing %s encoder (%s)", 
                  codec_name_.c_str(), codec->long_name);

      codec_ctx_ = avcodec_alloc_context3(codec);
      codec_ctx_->width = original_width_;
      codec_ctx_->height = original_height_;
      codec_ctx_->time_base = {1, fps_};
      codec_ctx_->framerate = {fps_, 1};
      codec_ctx_->gop_size = keyframe_interval_;
      codec_ctx_->max_b_frames = b_frames_;
      codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

      // Set flags for ultra-low latency streaming
      codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;  // Include SPS/PPS in bitstream
      codec_ctx_->flags |= AV_CODEC_FLAG_CLOSED_GOP;     // Ensure closed GOPs for seeking
      codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;      // Enable low delay mode

      // Additional latency optimizations
      codec_ctx_->delay = 0;                             // No encoding delay
      codec_ctx_->thread_count = 1;                      // Single thread for minimal latency
      codec_ctx_->thread_type = FF_THREAD_SLICE;         // Slice-based threading

      // Configure bitrate based on rate control mode
      if (rate_control_ == "crf") {
        // For CRF mode, let the codec handle bitrate automatically
        RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                     "Using CRF mode (CRF %d), bitrate will be determined automatically", crf_);
      } else if (rate_control_ == "cbr") {
        // CBR mode: set exact bitrate
        codec_ctx_->bit_rate = bitrate_;
        codec_ctx_->rc_max_rate = bitrate_;
        codec_ctx_->rc_min_rate = bitrate_;
        RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                   "CBR mode: bit_rate=%ld, rc_max_rate=%ld, rc_min_rate=%ld", 
                   codec_ctx_->bit_rate, codec_ctx_->rc_max_rate, codec_ctx_->rc_min_rate);
      } else {
        // VBR mode: set only average bitrate, let x264 handle the range
        codec_ctx_->bit_rate = bitrate_;
        // Don't set rc_max_rate and rc_min_rate for VBR - let x264 decide
        RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                   "VBR mode: bit_rate=%ld (letting x264 manage rate range)", 
                   codec_ctx_->bit_rate);
      }

      AVDictionary * opts = nullptr;

      // Apply codec options intelligently based on codec capabilities
      applyCodecOptions(&opts, codec);

      // Common options for all codecs - ensure parameter sets are transmitted
      av_dict_set(&opts, "repeat_headers", "1", 0);
      av_dict_set(&opts, "annex_b", "1", 0);        // Use Annex-B format for streaming

      if (avcodec_open2(codec_ctx_, codec, &opts) < 0) {
        throw std::runtime_error("Cannot open codec");
      }
      av_dict_free(&opts);

      // Log codec context information for debugging
      RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                  "Encoder initialized: %s %dx%d, extradata_size: %d",
                  codec_name_.c_str(), codec_ctx_->width, codec_ctx_->height,
                  codec_ctx_->extradata_size);

      // For MPEG-4, verify that we have extradata (sequence headers)
      if (codec_name_ == "mpeg4" && codec_ctx_->extradata_size == 0) {
        RCLCPP_WARN(rclcpp::get_logger("FfmpegCodec"),
                    "No extradata generated for MPEG-4. This may cause decoder issues.");
      }

      frame_ = av_frame_alloc();
      frame_->format = codec_ctx_->pix_fmt;
      frame_->width = codec_ctx_->width;
      frame_->height = codec_ctx_->height;
      av_frame_get_buffer(frame_, 32);

      pkt_ = av_packet_alloc();

      sws_ctx_ = sws_getContext(
        original_width_, original_height_, AV_PIX_FMT_BGR24,
        codec_ctx_->width, codec_ctx_->height, codec_ctx_->pix_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr
      );

      encoder_initialized_ = true;
      RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                  "Encoder initialized successfully: %dx%d", original_width_, original_height_);
    } catch (const std::exception& e) {
      RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"), 
                   "Exception in initEncoder: %s", e.what());
      throw;
    }
  } else {
    // Dimensions not yet known - initialization will happen on first encode()
    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"), 
                 "initEncoder called - dimensions not yet known, deferring until first frame");
  }
}


void FfmpegCodec::closeEncoder()
{
  try {
    // Flush encoder to get any remaining frames
    if (codec_ctx_ && avcodec_is_open(codec_ctx_)) {
      // Send NULL frame to signal end of encoding
      avcodec_send_frame(codec_ctx_, nullptr);

      // Receive any remaining packets
      AVPacket * flush_pkt = av_packet_alloc();
      if (flush_pkt) {
        while (avcodec_receive_packet(codec_ctx_, flush_pkt) == 0) {
          av_packet_unref(flush_pkt);
        }
        av_packet_free(&flush_pkt);
      }
    }

    // Clean up resources in proper order
    if (pkt_) {
      av_packet_unref(pkt_);
      av_packet_free(&pkt_);
      pkt_ = nullptr;
    }

    if (frame_) {
      av_frame_free(&frame_);
      frame_ = nullptr;
    }

    if (sws_ctx_) {
      sws_freeContext(sws_ctx_);
      sws_ctx_ = nullptr;
    }

    if (codec_ctx_) {
      if (avcodec_is_open(codec_ctx_)) {
        avcodec_close(codec_ctx_);
      }
      avcodec_free_context(&codec_ctx_);
      codec_ctx_ = nullptr;
    }

    encoder_initialized_ = false;
    dimensions_known_ = false;  // Reset dimensions when closing encoder
    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"), "Encoder closed successfully");

  } catch (const std::exception & e) {
    RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"), "Exception in closeEncoder: %s", e.what());
    encoder_initialized_ = false;
    dimensions_known_ = false;
  } catch (...) {
    RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"), "Unknown exception in closeEncoder");
    encoder_initialized_ = false;
    dimensions_known_ = false;
  }
}

std::vector<uint8_t> FfmpegCodec::encode(const sensor_msgs::msg::Image & frame_msg)
{
  cv_bridge::CvImagePtr cv_ptr;
  try {
    cv_ptr = cv_bridge::toCvCopy(frame_msg, sensor_msgs::image_encodings::BGR8);
  } catch (...) {
    return {};
  }
  cv::Mat cv_image = cv_ptr->image;

  // Store frame dimensions if not yet known
  if (!dimensions_known_) {
    original_width_ = cv_image.cols;
    original_height_ = cv_image.rows;
    dimensions_known_ = true;
    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"), 
                 "Frame dimensions stored: %dx%d", original_width_, original_height_);
  }

  // Initialize encoder if not already done
  if (!encoder_initialized_) {
    initEncoder();
  }

  uint8_t * src_data[4] = {cv_image.data, nullptr, nullptr, nullptr};
  int src_linesize[4] = {static_cast<int>(cv_image.step), 0, 0, 0};
  sws_scale(sws_ctx_, src_data, src_linesize, 0, cv_image.rows, frame_->data, frame_->linesize);

  frame_->pts = frame_index_++;

  // Force the first frame to be a keyframe to ensure parameter sets are sent
  if (frame_index_ == 1) {
    frame_->flags |= AV_FRAME_FLAG_KEY;
    frame_->pict_type = AV_PICTURE_TYPE_I;
  }

  int ret = avcodec_send_frame(codec_ctx_, frame_);
  if (ret < 0) {
    RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"),
                "Error sending frame to encoder: %d", ret);
    return {};
  }

  ret = avcodec_receive_packet(codec_ctx_, pkt_);
  if (ret == 0) {
    if (pkt_->size > 0) {
      std::vector<uint8_t> data;

      // For MPEG-4 and H.265, manually prepend extradata (sequence headers) to keyframes
      // This ensures the decoder receives necessary parameter information (VPS/SPS/PPS for H.265, sequence headers for MPEG-4)
      if ((codec_name_ == "mpeg4" || codec_name_ == "libx265") &&
        (pkt_->flags & AV_PKT_FLAG_KEY) &&
        codec_ctx_->extradata_size > 0)
      {

        // Prepend extradata to keyframe
        data.reserve(codec_ctx_->extradata_size + pkt_->size);
        data.insert(data.end(), codec_ctx_->extradata,
            codec_ctx_->extradata + codec_ctx_->extradata_size);
        data.insert(data.end(), pkt_->data, pkt_->data + pkt_->size);

        RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"),
                    "%s keyframe with extradata: %d + %d = %zu bytes",
                    codec_name_.c_str(), codec_ctx_->extradata_size, pkt_->size, data.size());
      } else {
        // Normal packet without extradata
        data.assign(pkt_->data, pkt_->data + pkt_->size);
      }

      av_packet_unref(pkt_);
      RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"),
                  "Successfully encoded frame %ld, packet size: %zu bytes",
                  frame_index_ - 1, data.size());
      return data;
    } else {
      RCLCPP_WARN(rclcpp::get_logger("FfmpegCodec"),
                 "Encoder returned empty packet for frame %ld", frame_index_ - 1);
    }
  } else if (ret == AVERROR(EAGAIN)) {
    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"),
                "Encoder needs more frames before producing output");
  } else {
    RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"),
                "Error receiving packet from encoder: %d", ret);
  }
  return {};
}


// ============================================================================
// DECODER IMPLEMENTATION
// ============================================================================
void FfmpegCodec::initDecoder()
{
  if (decoder_initialized_) {
    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"), "Decoder already initialized");
    return;
  }

  try {
    // Find decoder based on encoder codec
    const AVCodec * codec = findMatchingDecoder(codec_name_);

    if (!codec) {
      throw std::runtime_error("Decoder not found for codec: " + codec_name_ +
          ". Please check if the codec is installed on your system.");
    }

    dec_ctx_ = avcodec_alloc_context3(codec);

    // Apply decoder configuration for ultra-low latency
    dec_ctx_->thread_count = 1;                        // Force single thread for minimal latency
    dec_ctx_->thread_type = FF_THREAD_SLICE;           // Slice threading for lower latency
    dec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;        // Enable low delay mode
    dec_ctx_->flags2 |= AV_CODEC_FLAG2_FAST;           // Enable fast decoding

    // Codec-specific decoder optimizations
    if (codec_name_ == "mpeg4") {
      // Be more lenient with MPEG-4 streams - don't skip loop filter
      dec_ctx_->skip_loop_filter = AVDISCARD_NONE;     // Keep loop filter for MPEG-4 quality
      dec_ctx_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;  // Error concealment

      RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                  "MPEG-4 decoder configured with error concealment");
    } else if (codec_name_ == "libx265") {
      // H.265 specific decoder optimizations
      dec_ctx_->skip_loop_filter = AVDISCARD_NONE;     // Keep loop filter for H.265 quality
      dec_ctx_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;  // Error concealment
      dec_ctx_->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;     // Show all frames (don't skip)

      RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                  "H.265 decoder configured with error concealment and parameter set handling");
    } else {
      dec_ctx_->skip_loop_filter = AVDISCARD_ALL;      // Skip loop filter for speed (other codecs)
    }

    if (avcodec_open2(dec_ctx_, codec, nullptr) < 0) {
      throw std::runtime_error("Cannot open decoder");
    }

    dec_frame_ = av_frame_alloc();
    dec_pkt_ = av_packet_alloc();

    decoder_initialized_ = true;
    RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                "Decoder initialized successfully for %s", codec_name_.c_str());
  } catch (const std::exception& e) {
    RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"), 
                 "Exception in initDecoder: %s", e.what());
    throw;
  }
}

void FfmpegCodec::closeDecoder()
{
  try {
    // Clean up decoder resources in proper order
    if (dec_pkt_) {
      av_packet_unref(dec_pkt_);
      av_packet_free(&dec_pkt_);
      dec_pkt_ = nullptr;
    }

    if (dec_frame_) {
      av_frame_free(&dec_frame_);
      dec_frame_ = nullptr;
    }

    if (dec_sws_ctx_) {
      sws_freeContext(dec_sws_ctx_);
      dec_sws_ctx_ = nullptr;
    }

    if (dec_ctx_) {
      if (avcodec_is_open(dec_ctx_)) {
        avcodec_close(dec_ctx_);
      }
      avcodec_free_context(&dec_ctx_);
      dec_ctx_ = nullptr;
    }

    decoder_initialized_ = false;
    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"), "Decoder closed successfully");

  } catch (const std::exception & e) {
    RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"), "Exception in closeDecoder: %s", e.what());
  } catch (...) {
    RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"), "Unknown exception in closeDecoder");
  }
}

sensor_msgs::msg::Image FfmpegCodec::decode(const std::vector<uint8_t> & data)
{
  if (data.empty()) {return sensor_msgs::msg::Image();}

  if (!dec_ctx_) {
    initDecoder();
  }

  av_packet_unref(dec_pkt_);
  dec_pkt_->data = const_cast<uint8_t *>(data.data());
  dec_pkt_->size = static_cast<int>(data.size());

  // For MPEG-4 and H.265, attempt to use parser to extract codec parameters if available
  if ((codec_name_ == "mpeg4" || codec_name_ == "libx265") &&
    dec_ctx_->width == 0 && dec_ctx_->height == 0)
  {

    AVCodecID codec_id = (codec_name_ == "mpeg4") ? AV_CODEC_ID_MPEG4 : AV_CODEC_ID_HEVC;
    AVCodecParserContext * parser_ctx = av_parser_init(codec_id);
    if (parser_ctx) {
      uint8_t * out_data = nullptr;
      int out_size = 0;

      int parsed = av_parser_parse2(parser_ctx, dec_ctx_,
                                    &out_data, &out_size,
                                    data.data(), static_cast<int>(data.size()),
                                    AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

      if (parsed > 0 && dec_ctx_->width > 0 && dec_ctx_->height > 0) {
        RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                    "%s parser extracted dimensions: %dx%d",
                    codec_name_.c_str(), dec_ctx_->width, dec_ctx_->height);
      }

      av_parser_close(parser_ctx);
    }
  }
  int send_ret = avcodec_send_packet(dec_ctx_, dec_pkt_);
  if (send_ret < 0) {
    if (send_ret == AVERROR_INVALIDDATA) {
      RCLCPP_WARN(rclcpp::get_logger("FfmpegCodec"), "Invalid data in packet, skipping frame");
    } else {
      RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"), "Failed to send packet to decoder (error %d)",
          send_ret);
    }
    return sensor_msgs::msg::Image();
  }

  int receive_ret = avcodec_receive_frame(dec_ctx_, dec_frame_);
  if (receive_ret < 0) {
    if (receive_ret == AVERROR(EAGAIN)) {
      RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"), "Decoder needs more data");
    } else if (receive_ret == AVERROR_INVALIDDATA) {
      RCLCPP_WARN(rclcpp::get_logger("FfmpegCodec"),
          "Invalid data in stream, waiting for next keyframe");
    } else {
      RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"),
          "Failed to receive frame from decoder (error %d)", receive_ret);
    }
    return sensor_msgs::msg::Image();
  }

  int width = dec_frame_->width;
  int height = dec_frame_->height;

  // Validate frame dimensions
  if (width <= 0 || height <= 0) {
    if (codec_name_ == "mpeg4") {
      RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"),
                   "Invalid MPEG-4 frame dimensions: %dx%d. Possible causes:\n"
                   "  1. Missing sequence headers in stream\n"
                   "  2. Encoder not generating global headers correctly\n"
                   "  3. Stream corruption or incomplete frames\n"
                   "Consider switching to H.264 (libx264) for better reliability.",
                   width, height);
    } else if (codec_name_ == "libx265") {
      RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"),
                   "Invalid H.265 frame dimensions: %dx%d. Possible causes:\n"
                   "  1. Missing VPS/SPS/PPS parameter sets in stream\n"
                   "  2. Encoder not generating global headers correctly\n"
                   "  3. 'PPS id out of range' errors indicate malformed parameter sets\n"
                   "  4. Stream corruption or incomplete NAL units\n"
                   "Consider switching to H.264 (libx264) for better stability.",
                   width, height);
    } else {
      RCLCPP_ERROR(rclcpp::get_logger("FfmpegCodec"),
                   "Invalid frame dimensions: %dx%d", width, height);
    }
    return sensor_msgs::msg::Image();
  }

  cv::Mat cv_image(height, width, CV_8UC3);
  if (!dec_sws_ctx_) {
    dec_sws_ctx_ = sws_getContext(
      width, height, static_cast<AVPixelFormat>(dec_frame_->format),
      width, height, AV_PIX_FMT_BGR24,
      SWS_BILINEAR, nullptr, nullptr, nullptr
    );
  }

  uint8_t * dst_data[4] = {cv_image.data, nullptr, nullptr, nullptr};
  int dst_linesize[4] = {static_cast<int>(cv_image.step), 0, 0, 0};
  sws_scale(dec_sws_ctx_, dec_frame_->data, dec_frame_->linesize, 0, height, dst_data,
      dst_linesize);

  cv_bridge::CvImage cv_msg;
  cv_msg.encoding = sensor_msgs::image_encodings::BGR8;
  cv_msg.image = cv_image;

  return *cv_msg.toImageMsg();
}

// ================================
// Private Helper Methods
// ================================

void FfmpegCodec::applyCodecOptions(AVDictionary ** opts, const AVCodec * codec)
{
  // Use codec parameter to get codec name for better reliability
  std::string actual_codec_name = codec ? codec->name : codec_name_;

  // Get codec-specific configuration if available
  std::string config_key = codec_name_;
  if (config_key.substr(0, 3) == "lib") {
  // Remove 'lib' prefix for config lookup (e.g., libx264 -> x264)
    config_key = config_key.substr(3);
  }

  RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
               "Applying options for codec: %s (actual: %s, priority: %s)",
               codec_name_.c_str(), actual_codec_name.c_str(), priority_mode_.c_str());

  // Try to apply options, ignore failures (codec may not support them)
  trySetOption(opts, "preset", preset_);
  trySetOption(opts, "tune", tune_);
  trySetOption(opts, "profile", profile_);

  // Auto-detect appropriate level for H.264 if not specified or if current level is too low
  std::string effective_level = level_;
  if (codec_name_ == "libx264" || codec_name_ == "h264") {
    std::string auto_level = detectOptimalH264Level(codec_ctx_->width, codec_ctx_->height);
    if (level_.empty() || compareH264Levels(auto_level, level_) > 0) {
      effective_level = auto_level;
      RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                       "Auto-detected H.264 level: %s (for %dx%d resolution)",
                       auto_level.c_str(), codec_ctx_->width, codec_ctx_->height);
    }

    // H.264-specific options for ultra-low latency streaming
    // Note: intra-refresh is incompatible with refs > 1 and some tune presets like "film"
    bool use_intra_refresh = (refs_ <= 1) && (tune_ != "film");
    std::string intra_refresh_opt = use_intra_refresh ? ":intra-refresh=1" : "";

    // Configure VBV (Video Buffering Verifier) parameters only when needed
    // VBV is mainly for broadcast compliance - for adaptive streaming, we avoid it
    std::string vbv_opts = "";
    if (rate_control_ != "crf" && hrd_compliance_) {
      // Only use VBV when HRD compliance is explicitly required
      int vbv_bufsize = bitrate_ * 2 / 1000;  // 2 seconds of buffer in kbits
      int vbv_maxrate = bitrate_ / 1000;      // Use exact bitrate for strict compliance
      std::string hrd_mode = (rate_control_ == "cbr") ? "cbr" : "vbr";
      
      vbv_opts = ":vbv-bufsize=" + std::to_string(vbv_bufsize) +
        ":vbv-maxrate=" + std::to_string(vbv_maxrate) +  
        ":nal-hrd=" + hrd_mode;
      RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                 "H.264 configured with strict HRD compliance (%s mode, vbv-maxrate=%d kbps)", 
                 hrd_mode.c_str(), vbv_maxrate);
    } else if (rate_control_ != "crf") {
      // For adaptive streaming without HRD: let FFmpeg handle rate control via context parameters
      // No VBV constraints = more flexibility for bitrate adaptation
      RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                 "H.264 configured for flexible rate control (%s mode, no VBV constraints)", 
                 rate_control_.c_str());
    }

    std::string x264_opts = "keyint=" + std::to_string(keyframe_interval_) +
      ":force-cfr=1:repeat-headers=1" + intra_refresh_opt + vbv_opts +
      ":slice-max-size=1500:rc-lookahead=0:sync-lookahead=0:bframes=0:weightp=0";
    trySetOption(opts, "x264opts", x264_opts);

    // Additional ultra-low latency options
    trySetOption(opts, "slices", "1");           // Single slice for minimal delay
    trySetOption(opts, "slice-max-size", "1500"); // MTU-friendly slice size
  }
  trySetOption(opts, "level", effective_level);

  // MPEG-4 specific options
  if (codec_name_ == "mpeg4") {
    // Set scene change threshold to maximum to disable scene change detection
    // This fixes the "closed gop with scene change detection are not supported yet" error
    trySetOption(opts, "sc_threshold", std::to_string(sc_threshold_));

    // Additional MPEG-4 optimizations for stability and proper header inclusion
    trySetOption(opts, "strict", "-2");              // Allow experimental features
    trySetOption(opts, "data_partitioning", "0");    // Disable data partitioning for stability
    trySetOption(opts, "4mv", "0");                  // Disable 4MV for compatibility
    trySetOption(opts, "alternate_scan", "0");       // Use standard scan order

    // Force global headers for MPEG-4 (critical for proper decoding)
    // This ensures sequence headers are included in every keyframe
    av_dict_set(opts, "flags", "+global_header", 0);

    // GOP structure and keyframe settings
    trySetOption(opts, "g", std::to_string(keyframe_interval_));  // GOP size
    trySetOption(opts, "keyint_min", std::to_string(keyframe_interval_ / 4));  // Min keyframe interval

    RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                "MPEG-4 options - sc_threshold: %d, GOP: %d, global_header: enabled",
                sc_threshold_, keyframe_interval_);
  }

  // H.265 (libx265) specific options
  if (codec_name_ == "libx265") {
    // Force global headers for proper parameter set transmission
    av_dict_set(opts, "flags", "+global_header", 0);

    // H.265 specific options for ultra-low latency and stability
    std::string x265_opts = "keyint=" + std::to_string(keyframe_interval_) +
      ":min-keyint=" + std::to_string(keyframe_interval_ / 4) +
      ":repeat-headers=1" +           // Repeat parameter sets
      ":aud=1" +                      // Add access unit delimiters
      ":hrd=1" +                      // Enable HRD compliance
      ":no-scenecut=1" +              // Disable scene cut detection
      ":rc-lookahead=0" +             // Disable lookahead for low latency
      ":bframes=0" +                  // No B-frames for minimal latency
      ":frame-threads=1" +            // Single frame thread
      ":pools=none";                  // Disable thread pools

    // Apply bitrate control for H.265
    if (rate_control_ != "crf") {
      x265_opts += ":bitrate=" + std::to_string(bitrate_ / 1000);  // Convert to kbps
      x265_opts += ":vbv-bufsize=" + std::to_string(bitrate_ * 2 / 1000);  // 2s buffer
      x265_opts += ":vbv-maxrate=" + std::to_string(bitrate_ / 1000);
    }

    trySetOption(opts, "x265-params", x265_opts);

    RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                "H.265 options - GOP: %d, global_header: enabled, low-latency: enabled",
                keyframe_interval_);
  }

  // Advanced options
  trySetOption(opts, "cabac", cabac_ ? "1" : "0");
  trySetOption(opts, "refs", std::to_string(refs_));

  // Rate control options
  if (rate_control_ == "crf") {
    if (!trySetOption(opts, "crf", std::to_string(crf_))) {
      // If CRF not supported, use global quality
      codec_ctx_->global_quality = crf_;
      codec_ctx_->flags |= AV_CODEC_FLAG_QSCALE;
      RCLCPP_INFO(rclcpp::get_logger("FfmpegCodec"),
                       "CRF not supported, using global_quality instead");
    }
  }

  // Quality range (most codecs support these)
  trySetOption(opts, "qmin", std::to_string(min_qp_));
  trySetOption(opts, "qmax", std::to_string(max_qp_));

  // Common options
  av_dict_set(opts, "repeat_headers", "1", 0);
}

bool FfmpegCodec::trySetOption(
  AVDictionary ** opts, const std::string & key,
  const std::string & value)
{
  if (value.empty()) {return false;}

  int result = av_dict_set(opts, key.c_str(), value.c_str(), 0);
  if (result < 0) {
    RCLCPP_DEBUG(rclcpp::get_logger("FfmpegCodec"),
                    "Option '%s' not supported by codec %s",
                    key.c_str(), codec_name_.c_str());
    return false;
  }
  return true;
}

const AVCodec * FfmpegCodec::findMatchingDecoder(const std::string & encoder_name)
{
  // First try to find decoder by the same name
  const AVCodec * decoder = avcodec_find_decoder_by_name(encoder_name.c_str());
  if (decoder) {return decoder;}

  // If not found, try to find by codec ID of the encoder
  const AVCodec * encoder = avcodec_find_encoder_by_name(encoder_name.c_str());
  if (encoder) {
    decoder = avcodec_find_decoder(encoder->id);
    if (decoder) {return decoder;}
  }

  // Try common mappings for encoder->decoder name conversion
  std::string decoder_name = encoder_name;

  // Remove 'lib' prefix if present (e.g., libx264 -> x264)
  if (decoder_name.substr(0, 3) == "lib") {
    decoder_name = decoder_name.substr(3);
    decoder = avcodec_find_decoder_by_name(decoder_name.c_str());
    if (decoder) {return decoder;}
  }

  // Try with 'lib' prefix if not present
  if (encoder_name.substr(0, 3) != "lib") {
    decoder_name = "lib" + encoder_name;
    decoder = avcodec_find_decoder_by_name(decoder_name.c_str());
    if (decoder) {return decoder;}
  }

  RCLCPP_WARN(rclcpp::get_logger("FfmpegCodec"),
               "Could not find matching decoder for encoder: %s",
               encoder_name.c_str());

  return nullptr;
}

std::string FfmpegCodec::detectOptimalH264Level(int width, int height)
{
  // Calculate macroblock count (16x16 pixels per macroblock)
  int mb_width = (width + 15) / 16;
  int mb_height = (height + 15) / 16;
  int total_mbs = mb_width * mb_height;

  // H.264 Level limits (macroblock count, assuming 30fps)
  // Level 3.0: 1620 MBs, 40500 MB/s
  // Level 3.1: 3600 MBs, 108000 MB/s
  // Level 3.2: 5120 MBs, 216000 MB/s
  // Level 4.0: 8192 MBs, 245760 MB/s
  // Level 4.1: 8192 MBs, 245760 MB/s (higher frame rates)
  // Level 4.2: 8704 MBs, 522240 MB/s
  // Level 5.0: 22080 MBs, 589824 MB/s
  // Level 5.1: 36864 MBs, 983040 MB/s

  if (total_mbs <= 1620) {
    return "3.0";
  } else if (total_mbs <= 3600) {
    return "3.1";
  } else if (total_mbs <= 5120) {
    return "3.2";
  } else if (total_mbs <= 8192) {
    return "4.0";
  } else if (total_mbs <= 8704) {
    return "4.2";
  } else if (total_mbs <= 22080) {
    return "5.0";
  } else {
    return "5.1";
  }
}

int FfmpegCodec::compareH264Levels(const std::string & level1, const std::string & level2)
{
  // Convert level strings to comparable numbers
  auto levelToInt = [](const std::string & level) -> int {
      if (level == "3.0") {return 30;}
      if (level == "3.1") {return 31;}
      if (level == "3.2") {return 32;}
      if (level == "4.0") {return 40;}
      if (level == "4.1") {return 41;}
      if (level == "4.2") {return 42;}
      if (level == "5.0") {return 50;}
      if (level == "5.1") {return 51;}
      return 40;  // Default to 4.0
    };

  int l1 = levelToInt(level1);
  int l2 = levelToInt(level2);

  if (l1 > l2) {return 1;}
  if (l1 < l2) {return -1;}
  return 0;
}

}  // namespace abr_image_transport

// Explicit template instantiation for CodecFactory to ensure proper linking
#include "abr_image_transport_codecs/codec_factory.hpp"
template class abr_image_transport::CodecFactory<sensor_msgs::msg::Image>;
