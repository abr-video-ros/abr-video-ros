# ABR Image Transport

![ROS 2](https://img.shields.io/badge/ROS-2-blue)
![Build](https://img.shields.io/github/actions/workflow/status/abr-video-ros/abr-video-ros/ci.yml?branch=jazzy)
![License](https://img.shields.io/badge/license-GPLv3-green)
![Jazzy](https://img.shields.io/badge/ROS_2-Jazzy-brightgreen)

**Adaptive Bitrate Video Streaming for ROS 2**

ABR Image Transport is a high-performance ROS 2 `image_transport` plugin that provides intelligent adaptive video compression using industry-standard codecs (H.264, H.265, MPEG-4). Designed for real-time robotics applications, it automatically adjusts video quality based on network conditions while maintaining ultra-low latency.

## ✨ Key Features

- 🚀 **Ultra-Low Latency**: Optimized for robotics with <50ms encoding latency
- 🧠 **Intelligent Adaptation**: Real-time bitrate adjustment based on network performance  
- 🎯 **Production Ready**: Stable H.264 implementation with comprehensive error handling
- ⚙️ **Highly Configurable**: YAML-based configuration with priority profiles
- 📊 **Performance Monitoring**: Built-in quality metrics and network analysis
- 🔌 **Plug & Play**: Seamless integration with existing ROS 2 image pipelines

## 🚀 Quick Start

```bash
# Install dependencies
sudo apt update
sudo apt install ros-jazzy-desktop libavcodec-dev libavutil-dev libswscale-dev libx264-dev

# Clone and build
cd ~/ros2_ws/src
git clone https://github.com/abr-video-ros/abr-video-ros.git
cd ~/ros2_ws && colcon build --symlink-install && source install/setup.bash

# Test with camera
ros2 run v4l2_camera v4l2_camera_node &
ros2 run rqt_image_view rqt_image_view /image_raw/abr
```

---

## 🏗️ Architecture

```
┌─────────────────┐    ┌─────────────────┐            ┌─────────────────┐    ┌─────────────────┐
│   Image Source  │    │  ABR Publisher  │  Network   │ ABR Subscriber  │    │  Image Viewer   │
│                 │───▶│                 │───────────▶│                 │───▶│                 │
│  (Camera Node)  │    │ • Video Encode  │            │ • Video Decode  │    │                 │
│                 │    │                 │◀-----      │                 │    │                 │
└─────────────────┘    └─────────────────┘      \     └─────────────────┘    └─────────────────┘
                                │                \             │
                                ▼           (srv) \            ▼
                       ┌───────────────┐           \  ┌─────────────────┐
                       │ Codec Factory │            \ │ Quality Monitor │
                       │               │             \│                 │
                       │ • H.264 ✅    │              │ • Bitrate Track │
                       │ • H.265 ⚠️    │              │ • Auto Adapt    │
                       │ • MPEG-4 ⚠️   │              │ • Network Stats │
                       └───────────────┘              └─────────────────┘
```

**Key Components:**
- **ABR Publisher**: Encodes video and adapts bitrate dynamically  
- **ABR Subscriber**: Decodes video on the receiving end  
- **Quality Monitor**: Monitors network and performance to adjust quality  
- **Codec Factory**: Manages available codecs (H.264/H.265/MPEG-4)  

---

## 📦 Advanced Installation

<details>
<summary>🔧 System Requirements & Dependencies</summary>

**Requirements:**
- Ubuntu 24.04+ with ROS 2 Jazzy
- 4GB+ RAM (8GB recommended)
- Multi-core CPU (encoding is intensive)

**Full Setup:**
```bash
# Complete dependency installation
sudo apt install ros-jazzy-desktop python3-colcon-common-extensions
sudo apt install libavcodec-dev libavutil-dev libswscale-dev libx264-dev libx265-dev
sudo apt install ros-jazzy-cv-bridge ros-jazzy-image-transport ros-jazzy-rqt-image-view

# Build with optimizations
cd ~/ros2_ws
rosdep update && rosdep install --from-paths src --ignore-src -r -y
colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release

# Verify installation
ros2 pkg list | grep abr_image_transport
ros2 run image_transport list_transports  # Should show 'abr'
```
</details>

---

## ⚙️ Configuration

Configuration file: `src/abr_image_transport/params/abr_config.yaml`

```yaml
global:
  codec_type: "ffmpeg"

codecs:
  ffmpeg:
    codec_name: "libx264"  # Recommended for production
    
    priority_profiles:
      balanced:
        libx264:
          preset: "medium"
          bitrate: 2000000
          
abr_monitor:
  enabled: true
  min_bitrate: 500000
  max_bitrate: 8000000
```

---

## 🔧 Codec Support

| Codec | Status | Recommendation |
|-------|--------|----------------|
| **H.264 (libx264)** | ✅ Stable | Recommended for production |
| **H.265 (libx265)** | ⚠️ Experimental | For testing only |
| **MPEG-4** | ⚠️ Legacy | Not recommended |

### Recommended H.264 Configuration

```yaml
codecs:
  ffmpeg:
    codec_name: "libx264"
    codec_configs:
      libx264:
        preset: "medium"      # Options: ultrafast, fast, medium, slow
        tune: "zerolatency"   # For low-latency streaming
        profile: "main"
        bitrate: 2000000
        gop_size: 30
```

---

## 🎮 Usage

### Basic Commands

```bash
# Runtime bitrate control
ros2 service call /abr_params abr_image_transport_interfaces/srv/AbrParams "{bitrate: 3000000}"
```

### Code Integration

```cpp
// C++ - Use standard image_transport API
#include <image_transport/image_transport.hpp>

image_transport::ImageTransport it(node);
auto pub = it.advertise("/camera/image", 1);  // ABR used if configured
```

```python
# Python - Standard image_transport usage
import image_transport
it = image_transport.create(node)
pub = it.advertise('/camera/image', 1, 'abr')  # Explicit ABR transport
```


### Quick Configuration Examples

#### For Robotics (Low Latency)
```yaml
codecs:
  ffmpeg:
    codec_name: "libx264"
    priority_profiles:
      speed:
        libx264:
          preset: "ultrafast"
          tune: "zerolatency"
          bitrate: 1500000
          gop_size: 10        # Frequent keyframes
```

#### For Streaming (High Quality)
```yaml
codecs:
  ffmpeg:
    codec_name: "libx264"
    priority_profiles:
      quality:
        libx264:
          preset: "slow"
          tune: "film"
          bitrate: 5000000
          gop_size: 30
```

#### For Bandwidth-Limited Networks
```yaml
abr_monitor:
  enabled: true
  min_bitrate: 300000        # 300 kbps minimum
  max_bitrate: 2000000       # 2 Mbps maximum
  adaptation_interval: 0.5   # Fast adaptation
```

---

## 🔍 Performance & Benchmarks

### Typical Performance Metrics

| Resolution | Codec | Bitrate | CPU Usage* | Latency |
|------------|-------|---------|------------|---------|
| 640x480    | H.264 | 1 Mbps  | ~15%       | <30ms   |
| 1280x720   | H.264 | 2 Mbps  | ~25%       | <40ms   |
| 1920x1080  | H.264 | 4 Mbps  | ~45%       | <50ms   |

*CPU usage on Intel i5-8265U @ 1.60GHz

### Network Adaptation Examples

```bash
# Force specific quality level
ros2 service call /abr_params abr_image_transport_interfaces/srv/AbrParams \
  "{bitrate: 1500000}"
```

## ⚠️ Troubleshooting

### Common Issues

**Error: `Codec 'libx264' not found`**
```bash
# Install missing codec libraries
sudo apt install libx264-dev libavcodec-dev libavutil-dev

# Verify installation
ffmpeg -codecs | grep x264
```

**High CPU usage**
```yaml
codecs:
  ffmpeg:
    codec_configs:
      libx264:
        preset: "ultrafast"    # Fastest encoding
        tune: "zerolatency"    # Minimize latency
        thread_count: 2        # Limit threads
```

**Poor video quality**
```yaml
codecs:
  ffmpeg:
    codec_configs:
      libx264:
        preset: "medium"       # Better quality
        bitrate: 3000000       # Higher bitrate
        crf: 23               # Lower CRF = higher quality
```

**Decoder errors with H.265/MPEG-4**
```yaml
global:
  codec_type: "ffmpeg"
codecs:
  ffmpeg:
    codec_name: "libx264"     # Switch to stable H.264
```

**Network adaptation not working**
```yaml
abr_monitor:
  enabled: true
  adaptation_interval: 1.0    # Slower adaptation
  stable_window: 10          # More samples before changing
```

### FAQ

**Q: Which codec should I use for production?**  
A: Use H.264 (libx264) - it's the most stable and compatible option.

**Q: Can I use this with different camera types?**  
A: Yes, any node that publishes `sensor_msgs/Image` messages is compatible.

**Q: How do I reduce latency for teleoperation?**  
A: Use the "speed" priority profile with ultrafast preset and zerolatency tune.

**Q: Is GPU acceleration supported?**  
A: Currently CPU-only. GPU support (NVENC/VAAPI) is planned for future releases.

**Q: Can I use this with rosbag?**  
A: Yes, but compressed topics may need special handling during playback.

## 📄 License

GNU General Public License v3.0 — see [LICENSE](LICENSE)  

---

## 📚 Documentation

- **📖 API Documentation**: [GitHub Pages](https://abr-video-ros.github.io/abr-video-ros/)
- **📝 Technical Paper**: *Coming Soon*

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details.

## 📈 Roadmap

- [ ] **GPU Acceleration** (NVENC, VAAPI, QSV)
- [ ] **AV1 Codec Support** for next-generation compression
- [ ] **Hardware-specific Optimizations** (Jetson, RPi)
- [ ] **Multi-stream Support** for multiple cameras
- [ ] **Quality Metrics Integration** (PSNR, SSIM)

## Support

- **🐛 Bug Reports**: [GitHub Issues](https://github.com/abr-video-ros/abr-video-ros/issues)
- **💬 Discussions**: [GitHub Discussions](https://github.com/abr-video-ros/abr-video-ros/discussions)
- **📧 Email**: josemiguel.guerrero@urjc.es
- **🏫 Institution**: Universidad Rey Juan Carlos

## Citation

If you use this work in research, please cite:

```bibtex
@software{abr_image_transport_2025,
  title={ABR Image Transport: Adaptive Bitrate Video Streaming for ROS 2},
  author={Guerrero Hernandez, Jose Miguel},
  year={2025},
  publisher={Universidad Rey Juan Carlos},
  url={https://github.com/abr-video-ros/abr-video-ros},
  version={1.0.0}
}
```
