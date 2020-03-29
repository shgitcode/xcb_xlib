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

#pragma once
#include "Global.h"
#include "MutexDataPair.h"

class X11Input  {

private:
	struct Rect {
		unsigned int m_x1, m_y1, m_x2, m_y2;
		inline Rect() {}
		inline Rect(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2) : m_x1(x1), m_y1(y1), m_x2(x2), m_y2(y2) {}
	};
	struct SharedData {
		unsigned int m_current_width, m_current_height;
	};
	typedef MutexDataPair<SharedData>::Lock SharedLock;

private:
	unsigned int m_x, m_y, m_width, m_height;
	bool m_record_cursor, m_follow_cursor, m_follow_fullscreen;

	std::atomic<uint32_t> m_frame_counter;
	int64_t m_fps_last_timestamp;
	uint32_t m_fps_last_counter;
	double m_fps_current;

	Display *m_x11_display;
	int m_x11_screen;
	Window m_x11_root;
	Visual *m_x11_visual;
	int m_x11_depth;
	bool m_x11_use_shm;
	XImage *m_x11_image;
	XShmSegmentInfo m_x11_shm_info;
	bool m_x11_shm_server_attached;

	Rect m_screen_bbox;
	std::vector<Rect> m_screen_rects;
	std::vector<Rect> m_screen_dead_space;

	std::thread m_thread;
	MutexDataPair<SharedData> m_shared_data;
	std::atomic<bool> m_should_stop, m_error_occurred;

	// add
	unsigned int m_video_frame_rate;
	int64_t m_last_timestamp; // the timestamp of the last received video frame (for gap detection)
	int64_t m_next_timestamp; // the preferred timestamp of the next frame (for rate control)
	FILE* m_fp;

public:
	X11Input(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int frame_rate, bool record_cursor, bool follow_cursor, bool follow_fullscreen);
	~X11Input();

	// Reads the current size of the stream.
	// This function is thread-safe.
	void GetCurrentSize(unsigned int* width, unsigned int* height);

	// Returns the total number of captured frames.
	// This function is thread-safe.
	double GetFPS();

	// Returns whether an error has occurred in the input thread.
	// This function is thread-safe.
	inline bool HasErrorOccurred() { return m_error_occurred; }

	// stop
	void setStop(){m_should_stop = true;}

private:
	void Init();
	void Free();
	int64_t CalculateNextVideoTimestamp();
	void ReadVideoFrame(int64_t timestamp);

private:
	void AllocateImage(unsigned int width, unsigned int height);
	void FreeImage();
	void UpdateScreenConfiguration();

private:
	void InputThread();

};
