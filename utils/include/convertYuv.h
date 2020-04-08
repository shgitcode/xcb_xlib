#pragma once
#include "Global.h"
#include "libyuv.h"

// demo : support format translation
// RGB ===> I420
// 转换为I420，并写入文件
class yuvData{
    private:
		// source
	    struct src_data{
			uint8_t *m_data;
			int      m_stride_line;
			int      m_pix_fmt;

			src_data(uint8_t* src, int src_stride_line, int src_pixel_format)
				:m_data(std::move(src)),m_stride_line(src_stride_line),m_pix_fmt(src_pixel_format) {
			}
		};
	private:
		// dst
		uint8_t     *m_data;
		unsigned int m_width;
		unsigned int m_height;
		unsigned int m_size;
		int          m_pix_fmt;
		
		// 存储
		bool         m_yuv_valiad;
		FILE        *m_yuv_fd;

        // 线程
		std::thread m_thread;
		std::deque<src_data> m_data_queue;
		unsigned int          m_queue_size;

        std::mutex     m_queue_mutex; 
		std::condition_variable m_queue_condition; 

		std::atomic<bool> m_should_stop;
			
	public:
		yuvData(unsigned int width, unsigned int height, int pix_fmt);

		~yuvData();

		yuvData(const yuvData&) = delete;
		yuvData& operator=(const yuvData&) = delete;

	public:

		uint8_t *getYuvData() {return m_data;}
		unsigned int getYuvWidth() {return m_width;}
		unsigned int getYuvHeight() {return m_height;}
		unsigned int getYuvSize() {return m_size;}
		int getYuvPixFmt() {return m_pix_fmt;}

    public:
		void setStop();
		// src_stride_line:一行跨度:rgba = 4*width
		void putData(uint8_t* src, int src_stride_line, int src_pixel_format);

	private:
		void saveFile();
		void closeFile();

	private:
		void convertToYuv(const uint8_t* src, int src_stride_line, int src_pixel_format);
		void convertThread();
};
