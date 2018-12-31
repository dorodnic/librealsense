// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "../include/librealsense2/hpp/rs_sensor.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"
#include "../include/librealsense2-gl/rs_processing_gl.hpp"

#include "proc/synthetic-stream.h"
#include "upload.h"
#include "option.h"

#define NOMINMAX

#include <glad/glad.h>

#include <iostream>

#include <chrono>
#include <strstream>

#include "synthetic-stream-gl.h"

