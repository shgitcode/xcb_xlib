#pragma once
#include "Global.h"
#include "MutexDataPair.h"
#include "convertYuv.h"

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

class XcbInput  {

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

    // 采集帧数
	std::atomic<uint32_t> m_frame_counter;
	// 帧率统计
	int64_t m_fps_last_timestamp;
	uint32_t m_fps_last_counter;
	double m_fps_current;

    // xcb属性
    xcb_connection_t *m_xcb_conn;
	xcb_screen_t *m_xcb_screen;
	int m_xcb_screen_num;
	xcb_window_t m_xcb_window;	
#ifdef CONFIG_LIBXCB_SHM
	xcb_shm_seg_t m_xcb_segment;
#endif

    // 采用共享内存时，数据存储内存
	uint8_t *m_xcb_buffer; // for shm
	// 非共享内存的采集数据存储
	uint8_t *m_xcb_frame;
	
	// 采集每个像素比特数
	unsigned int m_bits_per_pixel;
	// 采集长度
	unsigned int m_frame_size;
	// 采集格式
	int m_pix_fmt;
	// 每行长度
	int m_stride_line;

	bool m_xcb_use_shm;

	Rect m_screen_bbox;

	std::thread m_thread;
	MutexDataPair<SharedData> m_shared_data;
	std::atomic<bool> m_should_stop, m_error_occurred;

	// add
	unsigned int m_video_frame_rate;
	int64_t m_last_timestamp; // the timestamp of the last received video frame (for gap detection)
	int64_t m_next_timestamp; // the preferred timestamp of the next frame (for rate control)

	// 采集数据存储FD
	FILE* m_fp;
   
	// 存储yuv数据
	std::unique_ptr<yuvData> m_yuv_convert;

public:
	XcbInput(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int frame_rate, bool record_cursor, bool follow_cursor, bool follow_fullscreen);
	~XcbInput();

	// Reads the current size of the stream.
	// This function is thread-safe.
	void GetCurrentSize(unsigned int* width, unsigned int* height);

	// Returns the total number of captured frames.
	// This function is thread-safe.
	double GetFPS();

	uint32_t GetFrameCounter(){return m_frame_counter;}
	
	// Returns whether an error has occurred in the input thread.
	// This function is thread-safe.
	inline bool HasErrorOccurred() { return m_error_occurred; }

	// stop
	void setStop(){m_should_stop = true;}

public:
	// save yuv
	void setYuvData(std::unique_ptr<yuvData> &data);
	uint8_t* getYuvData(bool &bIsStop);

	
private:
	void Init();
	int64_t CalculateNextVideoTimestamp();
	void ReadVideoFrame(int64_t timestamp);

private:
	xcb_screen_t * get_screen(const xcb_setup_t *setup, int screen_num);
	int check_shm(xcb_connection_t *conn);
	int check_position(int *pix_fmt);
	int check_xfixes(xcb_connection_t *conn);
	int xcbgrab_reposition(xcb_query_pointer_reply_t *p,
									xcb_get_geometry_reply_t *geo);
	int xcbgrab_frame();
	int allocate_shm();
	int xcbgrab_frame_shm();
	void xcbgrab_draw_mouse(xcb_query_pointer_reply_t *p,
							 xcb_get_geometry_reply_t *geo);
	int xcbgrab_read_header();
	int xcbgrab_read_packet();
	int xcbgrab_read_close();

private:
	void InputThread();

};
