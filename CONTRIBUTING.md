# Contributing to ABR Image Transport

Thank you for your interest in contributing to ABR Image Transport! This document provides guidelines and information for contributors.

## 🎯 Project Overview

ABR Image Transport is a high-performance ROS 2 image_transport plugin that provides adaptive bitrate video streaming using industry-standard codecs. We welcome contributions that help improve performance, stability, documentation, and expand codec support.

## 🚀 Quick Start for Contributors

### Development Environment Setup

1. **Fork and Clone**
   ```bash
   # Fork the repository on GitHub first
   git clone https://github.com/YOUR_USERNAME/abr-video-ros.git
   cd abr-video-ros
   
   # Add upstream remote
   git remote add upstream https://github.com/abr-video-ros/abr-video-ros.git
   ```

2. **Install Dependencies**
   ```bash
   # System dependencies
   sudo apt update
   sudo apt install ros-jazzy-desktop python3-colcon-common-extensions
   sudo apt install libavcodec-dev libavutil-dev libswscale-dev libx264-dev
   
   # ROS 2 dependencies
   cd ~/ros2_ws
   rosdep update
   rosdep install --from-paths src --ignore-src -r -y
   ```

3. **Development Tools**
   ```bash
   # Install pre-commit for code quality
   pip3 install pre-commit
   pre-commit install
   
   # Install additional development tools
   sudo apt install cppcheck clang-format clang-tidy
   pip3 install cpplint
   ```

4. **Build for Development**
   ```bash
   # Debug build for development
   colcon build --cmake-args -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
   
   # Enable all compiler warnings
   colcon build --cmake-args -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic"
   ```

## 📋 Types of Contributions

We welcome various types of contributions:

### 🐛 Bug Reports
- Use the [Bug Report Template](https://github.com/abr-video-ros/abr-video-ros/issues/new?template=bug_report.md)
- Include system information, ROS 2 version, and codec details
- Provide minimal reproduction steps
- Include relevant log output

### ✨ Feature Requests
- Use the [Feature Request Template](https://github.com/abr-video-ros/abr-video-ros/issues/new?template=feature_request.md)
- Describe the use case and expected behavior
- Consider performance implications
- Discuss compatibility with existing systems

### 🔧 Code Contributions
- **Codec Support**: New codec implementations or improvements
- **Performance**: Optimization and profiling improvements
- **Documentation**: Code comments, API docs, tutorials
- **Testing**: Unit tests, integration tests, benchmarks
- **Bug Fixes**: Address reported issues

### 📚 Documentation
- README improvements
- API documentation
- Tutorials and examples
- Configuration guides
- Performance benchmarks

## 🛠️ Development Workflow

### Branch Strategy
We use a simplified Git workflow:

```bash
# Create feature branch
git checkout -b feature/your-feature-name

# Keep your branch updated
git fetch upstream
git rebase upstream/main

# Push your branch
git push origin feature/your-feature-name
```

### Commit Guidelines
Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `perf`: Performance improvements
- `test`: Adding or updating tests
- `chore`: Maintenance tasks

**Examples:**
```
feat(codec): add AV1 codec support
fix(encoder): resolve memory leak in H.264 encoder
docs(readme): update installation instructions
perf(decoder): optimize frame buffer allocation
```

### Code Quality Standards

#### C++ Guidelines
- Follow [ROS 2 C++ Style Guide](https://docs.ros.org/en/rolling/The-ROS2-Project/Contributing/Code-Style-Language-Versions.html)
- Use modern C++17 features appropriately
- Prefer RAII for resource management
- Use smart pointers for dynamic allocation
- Follow const correctness principles

#### Code Formatting
```bash
# Format code with clang-format
find . -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Check code style
cpplint --recursive src/
```

#### Static Analysis
```bash
# Run clang-tidy
clang-tidy src/**/*.cpp -- -I src/

# Run cppcheck
cppcheck --enable=all --inconclusive src/
```

## 🧪 Testing

### Running Tests
```bash
# Build and run all tests
colcon build
colcon test

# Run specific package tests
colcon test --packages-select abr_image_transport_codecs

# View test results
colcon test-result --verbose
```

### Writing Tests
- Add unit tests for new functionality
- Use Google Test framework
- Test edge cases and error conditions
- Include performance regression tests for codec changes

#### Example Test Structure
```cpp
#include <gtest/gtest.h>
#include "abr_image_transport_codecs/ffmpeg_codec.hpp"

class FfmpegCodecTest : public ::testing::Test {
protected:
    void SetUp() override {
        codec_ = std::make_unique<abr_image_transport::FfmpegCodec>();
    }
    
    std::unique_ptr<abr_image_transport::FfmpegCodec> codec_;
};

TEST_F(FfmpegCodecTest, InitializeEncoder) {
    // Test encoder initialization
    EXPECT_NO_THROW(codec_->initEncoder());
    EXPECT_TRUE(codec_->isEncoderInitialized());
}
```

## 📊 Performance Considerations

### Benchmarking
- Use `ros2 run` with timing for performance tests
- Profile CPU usage with `htop` or `perf`
- Monitor memory usage with `valgrind`
- Test with various resolutions and bitrates

### Optimization Guidelines
- Minimize memory allocations in hot paths
- Use appropriate FFmpeg threading settings
- Consider SIMD optimizations for pixel format conversions
- Profile before and after optimization changes

## 🚦 Pull Request Process

### Before Submitting
1. **Test Thoroughly**
   ```bash
   # Build and test
   colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release
   colcon test
   
   # Run integration tests
   ros2 launch abr_image_transport test_integration.launch.py
   ```

2. **Code Quality Checks**
   ```bash
   # Run pre-commit checks
   pre-commit run --all-files
   
   # Check for memory leaks (if applicable)
   valgrind --tool=memcheck your_test_program
   ```

3. **Documentation Updates**
   - Update README if adding new features
   - Add inline documentation for new functions
   - Update configuration examples if needed

### PR Template
When creating a pull request, include:

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation update
- [ ] Performance improvement
- [ ] Code refactoring

## Testing
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Manual testing completed

## Performance Impact
Describe any performance implications

## Breaking Changes
List any breaking changes

## Checklist
- [ ] Code follows style guidelines
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] Tests added/updated
```

### Review Process
1. **Automated Checks**: CI must pass
2. **Code Review**: At least one maintainer approval
3. **Testing**: Manual testing by reviewer if needed
4. **Documentation**: Check for adequate documentation

## 🎯 Areas for Contribution

### High Priority
- **GPU Acceleration**: NVENC, VAAPI, QSV support
- **Additional Codecs**: AV1, VP9 implementation
- **Performance Optimization**: Encoder/decoder improvements
- **Error Handling**: Robust error recovery mechanisms

### Medium Priority
- **Configuration UI**: Dynamic parameter adjustment tools
- **Monitoring Dashboard**: Real-time performance metrics
- **Multi-stream Support**: Multiple camera handling
- **Platform Support**: ARM64, Jetson optimizations

### Documentation
- **Video Tutorials**: Setup and usage guides
- **API Documentation**: Comprehensive code documentation
- **Performance Guides**: Optimization best practices
- **Troubleshooting**: Common issues and solutions

## 📞 Getting Help

### Communication Channels
- **GitHub Discussions**: General questions and feature discussions
- **GitHub Issues**: Bug reports and specific problems
- **Email**: josemiguel.guerrero@urjc.es for direct contact

### Development Questions
- Check existing issues and discussions first
- Provide context and system information
- Include relevant code snippets or logs
- Be specific about your development environment

## 📜 Code of Conduct

### Our Standards
- **Respectful Communication**: Be kind and professional
- **Constructive Feedback**: Focus on code, not people
- **Inclusive Environment**: Welcome contributors of all backgrounds
- **Collaborative Spirit**: Help others learn and grow

### Unacceptable Behavior
- Harassment or discriminatory language
- Personal attacks or trolling
- Publishing private information
- Inappropriate sexual content

### Enforcement
Project maintainers will address any violations of the code of conduct. Serious violations may result in temporary or permanent bans from the project.

## 🏆 Recognition

### Contributors
All contributors are recognized in:
- GitHub contributors list
- Release notes acknowledgments
- Annual project reports

### Significant Contributions
Major contributors may be:
- Added to the maintainers team
- Invited to join steering committee
- Recognized in academic publications

## 🔄 Release Process

### Versioning
We follow [Semantic Versioning](https://semver.org/):
- **MAJOR**: Breaking changes
- **MINOR**: New features (backward compatible)
- **PATCH**: Bug fixes

### Release Timeline
- **Minor releases**: Every 3-4 months
- **Patch releases**: As needed for critical fixes
- **Major releases**: Annually or for significant changes

## 📚 Additional Resources

### Learning Resources
- [ROS 2 Documentation](https://docs.ros.org/en/jazzy/)
- [FFmpeg Documentation](https://ffmpeg.org/documentation.html)
- [Image Transport Tutorials](https://github.com/ros-perception/image_transport_tutorials)

### Development Tools
- [VS Code ROS Extension](https://marketplace.visualstudio.com/items?itemName=ms-iot.vscode-ros)
- [clangd Language Server](https://clangd.llvm.org/)
- [ROS 2 Development Setup](https://docs.ros.org/en/jazzy/Installation.html)

---

Thank you for contributing to ABR Image Transport! Your efforts help make video streaming in ROS 2 more efficient and accessible for the robotics community. 🚀

For questions about this contributing guide, please [open an issue](https://github.com/abr-video-ros/abr-video-ros/issues) or contact the maintainers directly.