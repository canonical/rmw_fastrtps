// Copyright 2016-2018 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <string>
#include <vector>

#include "rcutils/logging_macros.h"
#include "rcutils/types.h"

#include "rmw_fastrtps_shared_cpp/find_and_replace.hpp"
#include "rmw_fastrtps_shared_cpp/namespace_prefix.hpp"

/// Return the demangle ROS topic or the original if not a ROS topic.
std::string
_demangle_if_ros_topic(const std::string & topic_name)
{
  return _strip_ros_prefix_if_exists(topic_name);
}

/// Return the demangled ROS type or the original if not a ROS type.
std::string
_demangle_if_ros_type(const std::string & dds_type_string)
{
  if (dds_type_string[dds_type_string.size() - 1] != '_') {
    // not a ROS type
    return dds_type_string;
  }

  std::string substring = "dds_::";
  size_t substring_position = dds_type_string.find(substring);
  if (substring_position == std::string::npos) {
    // not a ROS type
    return dds_type_string;
  }

  std::string type_namespace = dds_type_string.substr(0, substring_position);
  type_namespace = find_and_replace(type_namespace, "::", "/");
  size_t start = substring_position + substring.size();
  std::string type_name = dds_type_string.substr(start, dds_type_string.length() - 1 - start);
  return type_namespace + type_name;
}

/// Return the service name for a given topic if it is part of one, else "".
std::string
_demangle_service_from_topic(const std::string & topic_name)
{
  std::string prefix = _get_ros_prefix_if_exists(topic_name);
  if (prefix.empty()) {
    // not a ROS topic or service
    return "";
  }
  std::vector<std::string> prefixes = {
    ros_service_response_prefix,
    ros_service_requester_prefix,
  };
  if (
    std::none_of(
      prefixes.cbegin(), prefixes.cend(),
      [&prefix](auto x) {
        return prefix == x;
      }))
  {
    // not a ROS service topic
    return "";
  }
  std::vector<std::string> suffixes = {
    "Reply",
    "Request",
  };
  std::string found_suffix;
  size_t suffix_position = std::string::npos;
  for (auto suffix : suffixes) {
    suffix_position = topic_name.rfind(suffix);
    if (suffix_position != std::string::npos) {
      if (topic_name.length() - suffix_position - suffix.length() != 0) {
        RCUTILS_LOG_WARN_NAMED("rmw_fastrtps_shared_cpp",
          "service topic has service prefix and a suffix, but not at the end"
          ", report this: '%s'", topic_name.c_str());
        continue;
      }
      found_suffix = suffix;
      break;
    }
  }
  if (std::string::npos == suffix_position) {
    RCUTILS_LOG_WARN_NAMED("rmw_fastrtps_shared_cpp",
      "service topic has prefix but no suffix"
      ", report this: '%s'", topic_name.c_str());
    return "";
  }
  // strip off the suffix first
  std::string service_name = topic_name.substr(0, suffix_position + 1);
  // then the prefix
  size_t start = prefix.length();  // explicitly leave / after prefix
  return service_name.substr(start, service_name.length() - 1 - start);
}

/// Return the demangled service type if it is a ROS srv type, else "".
std::string
_demangle_service_type_only(const std::string & dds_type_name)
{
  std::string ns_substring = "dds_::";
  size_t ns_substring_position = dds_type_name.find(ns_substring);
  if (std::string::npos == ns_substring_position) {
    // not a ROS service type
    return "";
  }
  auto suffixes = {
    std::string("_Response_"),
    std::string("_Request_"),
  };
  std::string found_suffix = "";
  size_t suffix_position = 0;
  for (auto suffix : suffixes) {
    suffix_position = dds_type_name.rfind(suffix);
    if (suffix_position != std::string::npos) {
      if (dds_type_name.length() - suffix_position - suffix.length() != 0) {
        RCUTILS_LOG_WARN_NAMED("rmw_fastrtps_shared_cpp",
          "service type contains 'dds_::' and a suffix, but not at the end"
          ", report this: '%s'", dds_type_name.c_str());
        continue;
      }
      found_suffix = suffix;
      break;
    }
  }
  if (std::string::npos == suffix_position) {
    RCUTILS_LOG_WARN_NAMED("rmw_fastrtps_shared_cpp",
      "service type contains 'dds_::' but does not have a suffix"
      ", report this: '%s'", dds_type_name.c_str());
    return "";
  }
  // everything checks out, reformat it from '[type_namespace::]dds_::<type><suffix>'
  // to '[type_namespace/]<type>'
  std::string type_namespace = dds_type_name.substr(0, ns_substring_position);
  type_namespace = find_and_replace(type_namespace, "::", "/");
  size_t start = ns_substring_position + ns_substring.length();
  std::string type_name = dds_type_name.substr(start, suffix_position - start);
  return type_namespace + type_name;
}
