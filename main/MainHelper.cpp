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

#include "MainHelper.h"

#ifdef WIN32
FILE* __cdecl _imp____iob_func()
{
  struct _iobuf_VS2012 { // ...\Microsoft Visual Studio 11.0\VC\include\stdio.h #56
    char *_ptr;
    int   _cnt;
    char *_base;
    int   _flag;
    int   _file;
    int   _charbuf;
    int   _bufsiz;
    char *_tmpfname;
  };
  // VS2015 has FILE = struct {void* _Placeholder}

  static struct _iobuf_VS2012 bufs[3];
  static char initialized = 0;

  if (!initialized) {
    bufs[0]._ptr = (char*)stdin->_Placeholder;
    bufs[1]._ptr = (char*)stdout->_Placeholder;
    bufs[2]._ptr = (char*)stderr->_Placeholder;
    initialized = 1;
  }

  return (FILE*)&bufs;
}

FILE* __cdecl __imp___iob_func()
{
  struct _iobuf_VS2012 { // ...\Microsoft Visual Studio 11.0\VC\include\stdio.h #56
    char *_ptr;
    int   _cnt;
    char *_base;
    int   _flag;
    int   _file;
    int   _charbuf;
    int   _bufsiz;
    char *_tmpfname;
};
  // VS2015 has FILE = struct {void* _Placeholder}

  static struct _iobuf_VS2012 bufs[3];
  static char initialized = 0;

  if (!initialized) {
    bufs[0]._ptr = (char*)stdin->_Placeholder;
    bufs[1]._ptr = (char*)stdout->_Placeholder;
    bufs[2]._ptr = (char*)stderr->_Placeholder;
    initialized = 1;
  }

  return (FILE*)&bufs;
}

#endif

/**
 * Validates a MINIFI_HOME value.
 * @param home_path
 * @return true if home_path represents a valid MINIFI_HOME
 */
bool validHome(const std::string &home_path) {
  struct stat stat_result { };
  std::string sep;
  sep += FILE_SEPARATOR;
#ifdef WIN32
  sep = "";
#endif
  auto properties_file_path = home_path + sep + DEFAULT_NIFI_PROPERTIES_FILE;
  return (stat(properties_file_path.c_str(), &stat_result) == 0);
}

/**
 * Configures the logger to log everything to syslog/Windows Event Log, and for the minimum log level to INFO
 */
void setSyslogLogger() {
  std::shared_ptr<logging::LoggerProperties> service_logger = std::make_shared<logging::LoggerProperties>();
  service_logger->set("appender.syslog", "syslog");
  service_logger->set("logger.root", "INFO,syslog");
  logging::LoggerConfiguration::getConfiguration().initialize(service_logger);
}
