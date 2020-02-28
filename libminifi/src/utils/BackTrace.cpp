/* Licensed to the Apache Software Foundation (ASF) under one or more
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
#include "utils/BackTrace.h"
#ifdef HAS_EXECINFO
#include <execinfo.h>
#include <dlfcn.h>
#include <iostream>
#include <utility>
#include <cstring>
#include <cxxabi.h>
#endif

void pull_trace(const uint8_t frames_to_skip) {
#ifdef HAS_EXECINFO
  void* stack_buffer[TRACE_BUFFER_SIZE + 1];

  // retrieve current stack addresses
  int trace_size = backtrace(stack_buffer, TRACE_BUFFER_SIZE);

  /**
   * we can skip the signal handler, call to pull_trace, and the first entry for backtrace_symbols
   */
  for (int i = frames_to_skip; i < trace_size; i++) {
    const char* file_name = "???";
    const char* symbol_name = nullptr;
    uintptr_t symbol_offset = 0;

    Dl_info dl_info{};
    int res = dladdr(stack_buffer[i], &dl_info);
    if (res != 0) {
      if (dl_info.dli_fname != nullptr) {
        const char* last_slash = nullptr;
        if ((last_slash = strrchr(dl_info.dli_fname, '/')) != nullptr) {
          file_name = last_slash + 1;
        } else {
          file_name = dl_info.dli_fname;
        }
      }
      symbol_name = dl_info.dli_sname;
      symbol_offset = reinterpret_cast<uintptr_t>(stack_buffer[i]) - reinterpret_cast<uintptr_t>(dl_info.dli_saddr);
    }
    std::string demangled_symbol_name;
    if (symbol_name != nullptr) {
      int status;
      char* demangled = abi::__cxa_demangle(symbol_name, nullptr, nullptr, &status);
      if (status == 0) {
        demangled_symbol_name = demangled;
        free(demangled);
        symbol_name = demangled_symbol_name.c_str();
      }
    }
    TraceResolver::getResolver().addTraceLine(file_name, symbol_name, symbol_offset);
  }
#endif
}

BackTrace TraceResolver::getBackTrace(std::string thread_name, std::thread::native_handle_type thread_handle) {
  // lock so that we only perform one backtrace at a time.
#ifdef HAS_EXECINFO
  std::lock_guard<std::mutex> lock(mutex_);

  caller_handle_ = pthread_self();
  thread_handle_ = thread_handle;
  trace_ = BackTrace(std::move(thread_name));

  if (0 == thread_handle_ || pthread_equal(caller_handle_, thread_handle)) {
    pull_trace();
  } else {
    if (thread_handle_ == 0) {
      return std::move(trace_);
    }
    emplace_handler();
    if (pthread_kill(thread_handle_, SIGUSR2) != 0) {
      return std::move(trace_);
    }
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGUSR2);
    sigsuspend(&mask);
  }
#else
  // even if tracing is disabled, include thread name into the trace object
  trace_ = BackTrace(std::move(thread_name));
#endif
  return std::move(trace_);
}
#ifdef HAS_EXECINFO
static void handler(int, siginfo_t*, void*) {
  // not the intended thread
  if (!pthread_equal(pthread_self(), TraceResolver::getResolver().getThreadHandle())) {
    return;
  }

  pull_trace();

  pthread_kill(TraceResolver::getResolver().getCallerHandle(), SIGUSR2);
}
#endif

void emplace_handler() {
#ifdef HAS_EXECINFO
  struct sigaction sa{};
  sigfillset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = handler;
  sigaction(SIGUSR2, &sa, nullptr);
#endif
}
