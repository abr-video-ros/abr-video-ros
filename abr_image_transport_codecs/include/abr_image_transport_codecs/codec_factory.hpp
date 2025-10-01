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
#include "ffmpeg_codec.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

namespace abr_image_transport
{

/**
 * @brief Factory class for creating codec instances
 *
 * This factory allows dynamic creation of codec instances based on string names.
 * It supports registration of new codec types at runtime, making the system
 * easily extensible for future codec implementations.
 *
 * Usage:
 * @code
 * auto codec = CodecFactory<sensor_msgs::msg::Image>::create("ffmpeg");
 * @endcode
 */
template<typename T>
class CodecFactory 
{
public:
  using CodecPtr = std::unique_ptr<CodecBase<T>>;
  using CreateFunction = std::function<CodecPtr()>;

  /**
   * @brief Create a codec instance by name
   *
   * @param codec_type Name of the codec type to create
   * @return std::unique_ptr<CodecBase<T>> Codec instance or nullptr if not found
   */
  static CodecPtr create(const std::string & codec_type)
  {
    initialize();

    auto it = creators_.find(codec_type);
    if (it != creators_.end()) {
      return it->second();
    }

    return nullptr;
  }

  /**
   * @brief Register a new codec type
   *
   * @param codec_type Name of the codec type
   * @param creator Function that creates instances of this codec
   */
  static void registerCodec(const std::string & codec_type, CreateFunction creator)
  {
    creators_[codec_type] = creator;
  }

  /**
   * @brief Get list of available codec types
   *
   * @return std::vector<std::string> List of registered codec type names
   */
  static std::vector<std::string> getAvailableCodecs()
  {
    initialize();

    std::vector<std::string> codecs;
    for (const auto & pair : creators_) {
      codecs.push_back(pair.first);
    }
    return codecs;
  }

  /**
   * @brief Check if a codec type is available
   *
   * @param codec_type Name of the codec type to check
   * @return bool True if codec type is registered
   */
  static bool isAvailable(const std::string & codec_type)
  {
    initialize();
    return creators_.find(codec_type) != creators_.end();
  }

private:
  /// @brief Map of codec type names to creation functions
  static std::unordered_map<std::string, CreateFunction> creators_;

  /// @brief Flag to indicate if the factory has been initialized
  static bool initialized_;

  /// @brief Initialize the factory with default codecs
  static void initialize()
  {
    if (!initialized_) {
      // Register built-in codecs
      registerCodec("ffmpeg", []() -> CodecPtr {
          return std::make_unique<FfmpegCodec>();
            });

      // Future codecs can be registered here:
      // registerCodec("opencv", []() -> CodecPtr {
      //     return std::make_unique<OpenCVCodec>();
      // });
      //
      // registerCodec("hardware", []() -> CodecPtr {
      //     return std::make_unique<HardwareCodec>();
      // });

      initialized_ = true;
    }
  }
};

// Static member definitions
template<typename T>
std::unordered_map<std::string,
  typename CodecFactory<T>::CreateFunction> CodecFactory<T>::creators_;

template<typename T>
bool CodecFactory<T>::initialized_ = false;

}  // namespace abr_image_transport
