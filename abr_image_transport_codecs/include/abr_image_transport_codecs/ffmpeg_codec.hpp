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

#include "codec_base.hpp"

#include <sensor_msgs/msg/image.hpp>
#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Forward declarations for FFmpeg structures
extern "C" {
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;
struct AVDictionary;
struct AVCodec;
}

namespace abr_image_transport
{

/**
 * @brief High-performance FFmpeg-based video codec for adaptive bitrate streaming
 *
 * This class provides a complete video encoding/decoding solution using FFmpeg libraries.
 * It's designed for real-time ROS 2 applications with focus on low latency streaming
 * and adaptive bitrate control.
 *
 * Key Features:
 * - Multi-codec support (H.264, H.265, MPEG-4)
 * - Priority-based configuration profiles (speed/balanced/quality)
 * - Runtime bitrate adaptation
 * - Optimized for streaming scenarios
 * - YAML-driven configuration
 * - Comprehensive error handling
 *
 * Architecture:
 * - Clean separation between encoder and decoder
 * - Resource management with RAII
 * - Configurable via priority profiles
 * - Thread-safe operations
 */
class FfmpegCodec : public CodecBase<sensor_msgs::msg::Image>
{
public:
  // ============================================================================
  // LIFECYCLE MANAGEMENT
  // ============================================================================

  /**
   * @brief Default constructor
   *
   * Initializes codec with safe defaults. FFmpeg resources are allocated
   * on-demand during first encode/decode operation.
   */
  FfmpegCodec();

  /**
   * @brief Destructor
   *
   * Ensures proper cleanup of all FFmpeg resources using RAII principles.
   * Safe to call even if resources were never allocated.
   */
  ~FfmpegCodec() override;

  // Non-copyable but movable
  FfmpegCodec(const FfmpegCodec &) = delete;
  FfmpegCodec & operator=(const FfmpegCodec &) = delete;
  FfmpegCodec(FfmpegCodec &&) = default;
  FfmpegCodec & operator=(FfmpegCodec &&) = default;

  // ============================================================================
  // CORE CODEC INTERFACE
  // ============================================================================

  /**
   * @brief Configure codec with parameters
   * @param config Basic codec configuration (bitrate, fps, sections)
   */
  void configure(const CodecConfig & config) override;

  /**
   * @brief Load advanced configuration from YAML
   * @param yaml_file_or_section Path to YAML file or section name
   * @throws std::runtime_error on configuration errors
   */
  void loadConfiguration(const std::string & yaml_file_or_section) override;

  /**
   * @brief Get codec identifier
   * @return "ffmpeg" as the codec name
   */
  std::string name() const override {return "ffmpeg";}

  // ============================================================================
  // ENCODER OPERATIONS
  // ============================================================================

  /**
   * @brief Initialize video encoder with current configuration
   * @throws std::runtime_error on initialization failure
   * @note Uses stored frame dimensions from first encode() call
   */
  void initEncoder() override;

  /**
   * @brief Encode ROS image to compressed data
   * @param frame Input image (automatically converts format if needed)
   * @return Compressed video data
   * @throws std::runtime_error on encoding failure
   */
  std::vector<uint8_t> encode(const sensor_msgs::msg::Image & frame) override;

  /**
   * @brief Update encoder bitrate at runtime
   * @param new_bitrate Target bitrate in bits per second
   * @return true if update successful
   */
  bool updateBitrate(int new_bitrate) override;

  /**
   * @brief Clean up encoder resources
   */
  void closeEncoder() override;

  /**
   * @brief Check encoder initialization status
   * @return true if encoder is ready
   */
  bool isEncoderInitialized() const override {return encoder_initialized_;}

  // ============================================================================
  // DECODER OPERATIONS
  // ============================================================================

  /**
   * @brief Initialize video decoder
   * @throws std::runtime_error on initialization failure
   */
  void initDecoder() override;

  /**
   * @brief Decode compressed data to ROS image
   * @param compressed_data Input compressed video data
   * @return Decoded image
   * @throws std::runtime_error on critical decoding errors
   */
  sensor_msgs::msg::Image decode(const std::vector<uint8_t> & compressed_data) override;

  /**
   * @brief Clean up decoder resources
   */
  void closeDecoder() override;

  /**
   * @brief Check decoder initialization status
   * @return true if decoder is ready
   */
  bool isDecoderInitialized() const override {return decoder_initialized_;}

private:
  // ============================================================================
  // CONFIGURATION HELPERS
  // ============================================================================

  /// @brief Resolve YAML file path from parameter (file path or section name)
  std::string resolveConfigPath(const std::string & yaml_file_or_section) const;

  /// @brief Validate YAML configuration structure
  void validateConfigStructure(const YAML::Node & cfg, const std::string & yaml_file) const;

  /// @brief Load global configuration settings
  void loadGlobalSettings(const YAML::Node & cfg);

  /// @brief Apply complete codec configuration from YAML node
  void applyCodecConfiguration(const YAML::Node & ffmpeg_config);

  /// @brief Log configuration summary after loading
  void logConfigurationSummary(const std::string & yaml_file) const;

  /// @brief Set codec-appropriate defaults based on selected codec
  void setCodecDefaults();

  /// @brief Apply priority profile configuration (speed/balanced/quality)
  void applyPriorityProfile(const YAML::Node & ffmpeg_config);

  /// @brief Load encoder-specific YAML configuration
  void loadEncoderConfig(const YAML::Node & encoder_config);

  /// @brief Load codec-specific YAML configuration
  void loadCodecSpecificConfig(const YAML::Node & codec_config);

  /// @brief Load decoder-specific YAML configuration
  void loadDecoderConfig(const YAML::Node & decoder_config);

  /// @brief Load FFmpeg logging configuration
  void loadLoggingConfig(const YAML::Node & logging_config);

  // ============================================================================
  // ENCODING HELPERS
  // ============================================================================

  /// @brief Apply codec-specific encoding options
  void applyCodecOptions(AVDictionary ** opts, const AVCodec * codec);

  /// @brief Safely set FFmpeg option (ignores failures)
  bool trySetOption(AVDictionary ** opts, const std::string & key, const std::string & value);

  /// @brief Find matching decoder for encoder
  const AVCodec * findMatchingDecoder(const std::string & encoder_name);

  /// @brief Detect optimal H.264 level for resolution
  std::string detectOptimalH264Level(int width, int height);

  /// @brief Compare H.264 level strings (-1, 0, 1)
  int compareH264Levels(const std::string & level1, const std::string & level2);

  // ============================================================================
  // MEMBER VARIABLES
  // ============================================================================

  // Image properties (stored from first frame)
  uint32_t original_width_{0};
  uint32_t original_height_{0};
  bool dimensions_known_{false};              ///< Whether frame dimensions are known

  // Encoder resources (RAII managed)
  AVCodecContext * codec_ctx_{nullptr};
  SwsContext * sws_ctx_{nullptr};
  AVFrame * frame_{nullptr};
  AVPacket * pkt_{nullptr};
  int64_t frame_index_{0};
  bool encoder_initialized_{false};

  // Decoder resources (RAII managed)
  AVCodecContext * dec_ctx_{nullptr};
  AVFrame * dec_frame_{nullptr};
  AVPacket * dec_pkt_{nullptr};
  SwsContext * dec_sws_ctx_{nullptr};
  bool decoder_initialized_{false};

  // Configuration parameters (loaded from YAML)

  // Basic parameters
  int bitrate_{2000000};                     ///< Target bitrate (2 Mbps default)
  int fps_{30};                              ///< Target framerate

  // Codec selection & mode
  std::string codec_name_{"libx264"};        ///< FFmpeg codec name
  std::string priority_mode_{"balanced"};    ///< Priority: speed/balanced/quality
  std::string preset_{"medium"};             ///< Encoding preset
  std::string tune_{"zerolatency"};          ///< Tuning preset
  std::string profile_{"main"};              ///< Codec profile
  std::string level_{"4.0"};                 ///< Codec level

  // Encoding structure
  int keyframe_interval_{30};                ///< GOP size
  int b_frames_{0};                          ///< B-frames count
  int refs_{2};                              ///< Reference frames

  // Rate control
  std::string rate_control_{"vbr"};          ///< Rate control mode
  bool hrd_compliance_{false};               ///< HRD compliance (strict VBV)
  int crf_{23};                              ///< Constant rate factor
  int min_qp_{10};                           ///< Minimum quantization
  int max_qp_{45};                           ///< Maximum quantization

  // Advanced features
  bool cabac_{true};                         ///< CABAC entropy coding
  bool deblock_{true};                       ///< Deblocking filter
  int sc_threshold_{1000000000};             ///< Scene change threshold (MPEG-4)

  // Threading
  int thread_count_{0};                      ///< Decoder threads (auto)
  std::string thread_type_{"frame"};         ///< Threading type

  // Performance optimizations
  bool fast_pskip_{true};                    ///< Fast P-skip decision
  bool mixed_refs_{false};                   ///< Mixed reference frames
  int trellis_{0};                           ///< Trellis quantization
};

}  // namespace abr_image_transport
