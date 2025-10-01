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

#include <pluginlib/class_list_macros.hpp>
#include "abr_image_transport/abr_publisher.hpp"
#include "abr_image_transport/abr_subscriber.hpp"

PLUGINLIB_EXPORT_CLASS(abr_image_transport::AbrPublisher, image_transport::PublisherPlugin)
PLUGINLIB_EXPORT_CLASS(abr_image_transport::AbrSubscriber, image_transport::SubscriberPlugin)
