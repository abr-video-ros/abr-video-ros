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

#include <vector>
#include <cstdint>
#include <string>

namespace abr_image_transport
{

/**
 * @brief Configuration parameters for codec initialization
 *
 * Generic structure to hold codec configuration parameters.
 * Specific codec implementations should extend this or use the
 * YAML configuration system.
 */
struct CodecConfig
{
  int bitrate = 800000;         ///< Target bitrate in bits per second
  int fps = 30;                 ///< Target frames per second
  std::string config_section;   ///< YAML configuration section name
};

/**
 * @brief Abstract base class for video codecs
 *
 * This template class provides a common interface for different video codecs
 * used in the adaptive bitrate image transport system. It defines the basic
 * operations that any codec implementation must support: encoding and decoding
 * of video frames.
 *
 * The interface is designed to be codec-agnostic, allowing for different
 * implementations (FFmpeg, OpenCV, hardware-specific codecs, etc.) while
 * maintaining a consistent API for the transport layer.
 *
 * @tparam T The data type to encode/decode (typically sensor_msgs::msg::Image)
 */
template<typename T>
class CodecBase {
public:
  /**
   * @brief Virtual destructor
   */
  virtual ~CodecBase() = default;

  /**
   * @brief Configure the codec with parameters
   *
   * Sets up codec-specific parameters from configuration. This method
   * should be called before initialization to configure the codec behavior.
   *
   * @param config Generic codec configuration parameters
   * @throws std::runtime_error if configuration is invalid
   */
  virtual void configure(const CodecConfig & config) = 0;

  /**
   * @brief Load codec-specific configuration from YAML
   *
   * Reads codec-specific parameters from the centralized YAML configuration.
   * Each codec implementation should read its own section.
   *
   * @param yaml_section_name Name of the YAML section for this codec
   * @throws std::runtime_error if configuration loading fails
   */
  virtual void loadConfiguration(const std::string & yaml_section_name) = 0;

  /**
   * @brief Initialize the encoder with current parameters
   *
   * This method must be called before any encoding operations.
   * It sets up the internal encoder state based on the current
   * configuration parameters.
   *
   * @throws std::runtime_error if encoder initialization fails
   */
  virtual void initEncoder() = 0;

  /**
   * @brief Clean up and close the encoder
   *
   * Releases all resources associated with the encoder.
   * Should be called when encoding is complete or parameters need to change.
   */
  virtual void closeEncoder() = 0;

  /**
   * @brief Encode a frame to compressed data
   *
   * @param frame The input frame to encode
   * @return std::vector<uint8_t> Compressed data as byte array
   * @throws std::runtime_error if encoding fails
   */
  virtual std::vector<uint8_t> encode(const T & frame) = 0;

  /**
   * @brief Initialize the decoder
   *
   * This method must be called before any decoding operations.
   * Sets up the internal decoder state.
   *
   * @throws std::runtime_error if decoder initialization fails
   */
  virtual void initDecoder() = 0;

  /**
   * @brief Clean up and close the decoder
   *
   * Releases all resources associated with the decoder.
   */
  virtual void closeDecoder() = 0;

  /**
   * @brief Decode compressed data to a frame
   *
   * @param data Compressed data as byte array
   * @return T Decoded frame
   * @throws std::runtime_error if decoding fails
   */
  virtual T decode(const std::vector<uint8_t> & data) = 0;

  /**
   * @brief Get the name of this codec implementation
   *
   * @return std::string Human-readable codec name (e.g., "ffmpeg_h264")
   */
  virtual std::string name() const = 0;

  /**
   * @brief Update encoding bitrate at runtime
   *
   * Allows dynamic adjustment of encoding bitrate without full
   * reinitialization. Not all codecs may support this operation.
   *
   * @param new_bitrate New target bitrate in bits per second
   * @return bool True if update was successful, false otherwise
   */
  virtual bool updateBitrate(int new_bitrate) = 0;

  /**
   * @brief Check if encoder is currently initialized
   *
   * @return bool True if encoder is ready for encoding operations
   */
  virtual bool isEncoderInitialized() const = 0;

  /**
   * @brief Check if decoder is currently initialized
   *
   * @return bool True if decoder is ready for decoding operations
   */
  virtual bool isDecoderInitialized() const = 0;
};

}  // namespace abr_image_transport
