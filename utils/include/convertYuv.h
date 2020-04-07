#pragma once
#include "Global.h"
#include "libyuv.h"

// demo : support format translation
// RGB ===> I420
class yuvData{
	private:
		uint8_t     *m_data;
		unsigned int m_width;
		unsigned int m_height;
		unsigned int m_size;
		int          m_pix_fmt;
		bool         m_yuv_valiad;
		FILE        *m_yuv_fd;
		
			
	public:
		yuvData(unsigned int width, unsigned int height, int pix_fmt);

		~yuvData() {
			delete [] m_data;
			closeFile();
		}

		yuvData(const yuvData&) = delete;
		yuvData& operator=(const yuvData&) = delete;

	public:

		void convertToYuv(const uint8_t* src, int src_stride_line, int src_pixel_format);
		uint8_t *getYuvData() {return m_data;}
		unsigned int getYuvWidth() {return m_width;}
		unsigned int getYuvHeight() {return m_height;}
		unsigned int getYuvSize() {return m_size;}
		int getYuvPixFmt() {return m_pix_fmt;}



	private:
		void saveFile();
		void closeFile();
#if 0
	private:
		
	std::mutex m_yuv_mutex;
	bool m_yuv_stop;
	//std::unique_lock<std::mutex> m_yuv_lock;
    std::deque<std::unique_ptr<yuvData> > m_yuv_buffer;

	public:
	// save yuv
	void setYuvData(std::unique_ptr<yuvData> &data);
	uint8_t* getYuvData(bool &bIsStop);
#endif
};
