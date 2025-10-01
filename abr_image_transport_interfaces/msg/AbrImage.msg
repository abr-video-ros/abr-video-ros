# Standard ROS header
std_msgs/Header header

# Original image resolution
uint32 original_width
uint32 original_height

# Codec used for compression (e.g. "h264", "hevc")
string encoding

# Compressed image data (AVPacket)
uint8[] compressed_data