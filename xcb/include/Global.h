/*
Copyright (c) 2012-2020 Maarten Baert <maarten-baert@hotmail.com>

This file is part of SimpleScreenRecorder.

SimpleScreenRecorder is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

SimpleScreenRecorder is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with SimpleScreenRecorder.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef GLOBAL_H
#define GLOBAL_H

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <stdint.h>
#include <string.h>

#include <pwd.h>
#include <random>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>

#include <iostream>
#include <algorithm>
#include <array>
#include <atomic>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#include <xcb/xcb.h>

#ifdef CONFIG_LIBXCB_XFIXES
#include <xcb/xfixes.h>
#endif

#ifdef CONFIG_LIBXCB_SHM
#include <sys/shm.h>
#include <xcb/shm.h>
#endif

#ifdef CONFIG_LIBXCB_SHAPE
#include <xcb/shape.h>
#endif


// undefine problematic Xlib macros
extern "C"
{
#include "libavformat/avformat.h"
};

#if 0

#define AVPixelFormat PixelFormat
#define AV_PIX_FMT_NONE PIX_FMT_NONE
#define AV_PIX_FMT_PAL8 PIX_FMT_PAL8
#define AV_PIX_FMT_RGB565 PIX_FMT_RGB565
#define AV_PIX_FMT_RGB555 PIX_FMT_RGB555
#define AV_PIX_FMT_BGR24 PIX_FMT_BGR24
#define AV_PIX_FMT_RGB24 PIX_FMT_RGB24
#define AV_PIX_FMT_BGRA PIX_FMT_BGRA
#define AV_PIX_FMT_RGBA PIX_FMT_RGBA
#define AV_PIX_FMT_ABGR PIX_FMT_ABGR
#define AV_PIX_FMT_ARGB PIX_FMT_ARGB
#define AV_PIX_FMT_YUV420P PIX_FMT_YUV420P
#define AV_PIX_FMT_YUV422P PIX_FMT_YUV422P
#define AV_PIX_FMT_YUV444P PIX_FMT_YUV444P
#define AV_PIX_FMT_NV12 PIX_FMT_NV12
#endif


// Maximum allowed image size (to avoid 32-bit integer overflow)
#define SSR_MAX_IMAGE_SIZE 20000

#define SINK_TIMESTAMP_NONE  ((int64_t) 0x8000000000000000ull)  // the sink doesn't want any new frames at the moment
#define SINK_TIMESTAMP_ASAP  ((int64_t) 0x8000000000000001ull)  // the sink wants a new frame as soon as possible



class X11Exception : public std::exception {
public:
	inline virtual const char* what() const throw() override {
		return "X11Exception";
	}
};

// high resolution timer
// when compile, need to add <time.h>
// and link -lrt , because librt contain clock_gettime() function.
inline int64_t hrt_time_micro() {
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t) ts.tv_sec * (uint64_t) 1000000 + (uint64_t) (ts.tv_nsec / 1000);
}

template<typename T>
inline T clamp(T v, T lo, T hi) {
	assert(lo <= hi);
	if(v < lo)
		return lo;
	if(v > hi)
		return hi;
	return v;
}

template<> inline float clamp<float>(float v, float lo, float hi) {
	assert(lo <= hi);
	return fmin(fmax(v, lo), hi);
}
template<> inline double clamp<double>(double v, double lo, double hi) {
	assert(lo <= hi);
	return fmin(fmax(v, lo), hi);
}


#endif // GLOBAL_H
