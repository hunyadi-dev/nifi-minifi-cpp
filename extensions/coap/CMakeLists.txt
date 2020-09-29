#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#


include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
include_directories(protocols nanofi controllerservice server)
include_directories(../http-curl/)

if (ENABLE_NANOFI)
file(GLOB CSOURCES "nanofi/*.c")
endif()
file(GLOB SOURCES "*.cpp" "protocols/*.cpp" "processors/*.cpp" "controllerservice/*.cpp" "server/*.cpp" )

if (ENABLE_NANOFI)
add_library(nanofi-coap-c STATIC ${CSOURCES})
endif()
add_library(minifi-coap STATIC ${SOURCES})
set_property(TARGET minifi-coap PROPERTY POSITION_INDEPENDENT_CODE ON)

if (ENABLE_NANOFI)
target_link_libraries(nanofi-coap-c COAP::libcoap Threads::Threads)
endif()
target_link_libraries(minifi-coap ${LIBMINIFI})
if (ENABLE_NANOFI)
target_link_libraries(minifi-coap nanofi-coap-c Threads::Threads COAP::libcoap minifi-http-curl)
else()
target_link_libraries(minifi-coap Threads::Threads COAP::libcoap minifi-http-curl)
endif()

SET (COAP-EXTENSION minifi-coap PARENT_SCOPE)
register_extension(minifi-coap)

