/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifdef WIN32

#include "spdlog/common.h"
#include "spdlog/sinks/sink.h"
#include "spdlog/details/log_msg.h"

#include <Windows.h>

#include <string>


namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

class windowseventlog_sink : public spdlog::sinks::sink {
 private:
  HANDLE event_source_;

  WORD type_from_level(const spdlog::details::log_msg& msg) const {
    switch (static_cast<int>(msg.level)) {
      case spdlog::level::trace:
      case spdlog::level::debug:
      case spdlog::level::info:
        return EVENTLOG_INFORMATION_TYPE;
      case spdlog::level::warn:
        return EVENTLOG_WARNING_TYPE;
      case spdlog::level::err:
      case spdlog::level::critical:
        return EVENTLOG_ERROR_TYPE;
      default:
        return EVENTLOG_ERROR_TYPE;
    }
  }

 public:
  //
  windowseventlog_sink(const std::string& source_name = "MiNiFi")
  : event_source_(nullptr)
  {
    event_source_ = RegisterEventSourceA(nullptr, source_name.c_str());
    if (event_source_ == nullptr) {
      throw "Failed to create event source"; // TODO
    }
  }

  ~windowseventlog_sink() {
    if (event_source_ != nullptr) {
      DeregisterEventSource(event_source_);
    }
  }

  windowseventlog_sink(const syslog_sink&) = delete;
  windowseventlog_sink& operator=(const syslog_sink &) = delete;
  windowseventlog_sink(syslog_sink&&) = delete;
  windowseventlog_sink& operator=(syslog_sink&&) = delete;

  void log(const spdlog::details::log_msg& msg) override {
    ReportEventA(event_source_,
                 type_from_level(msg) /*wType*/,
                 0U /*wCategory*/,
                 1U /*dwEventID*/,
                 nullptr /* lpUserSid */,
                 1U /*wNumStrings*/,
                 0U /*dwDataSize*/,
                 &msg.raw.str().c_str() /*lpStrings*/,
                 nullptr /*lpRawData*/);
  }

  void flush() override {
  }
};
}
}
}
}
}
}
}

#endif
