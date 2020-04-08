/*
 // ARGBToI420 : https://blog.csdn.net/XIAIBIANCHENG/article/details/73065646?utm_source=blogxgwz8
*/

#include <time.h>

#include "convertYuv.h"

yuvData::yuvData(unsigned int width, unsigned int height, int pix_fmt) {
    m_width   = width;
	m_height  = height;
	m_pix_fmt = pix_fmt;

	if (m_pix_fmt == AV_PIX_FMT_YUV420P) {
        m_size = m_width * m_height*3/2;
		m_data = new uint8_t[m_size];
	}

	m_yuv_fd = nullptr;

	m_thread = std::thread(&yuvData::convertThread,this);

}

yuvData::~yuvData() {
	if(m_thread.joinable()) {
		std::cout<<"[yuvData::~yuvData] "
			     <<"Stopping input thread ..."
			     <<std::endl;
		m_should_stop = true;
		m_thread.join();
	}
	
	delete [] m_data;

	closeFile();
}

void yuvData::convertToYuv(const uint8_t* src, int src_stride_line, int src_pixel_format){
	uint8_t* dst_y;
    int dst_stride_y;
    uint8_t* dst_u;
    int dst_stride_u;
    uint8_t* dst_v;
    int dst_stride_v;

	m_yuv_valiad = false;

	switch(m_pix_fmt) {
	
	    case AV_PIX_FMT_YUV420P:
			dst_y = m_data;
			dst_stride_y = m_width;
			dst_u = m_data + dst_stride_y*m_height;
			dst_stride_u = m_width>>1;
			dst_v = m_data + dst_stride_y*m_height+dst_stride_u*m_height/2;
			dst_stride_v = m_width>>1;
				 
			break;
				
		default:
			break;
	
		}

#if 0
    switch(src_pixel_format) {

		case AV_PIX_FMT_RGB24:

			if (m_pix_fmt == AV_PIX_FMT_YUV420P) {
				libyuv::RGB24ToI420(src,src_stride_line,
                            dst_y,dst_stride_y,
                            dst_u,dst_stride_u,
                            dst_v,dst_stride_v,
                            m_width,m_height);
				m_yuv_valiad = true;
			}
			 
           break;

		case AV_PIX_FMT_BGRA:	
		case AV_PIX_FMT_0RGB32:
		case AV_PIX_FMT_ARGB:

			if (m_pix_fmt == AV_PIX_FMT_YUV420P) {		
				libyuv::ARGBToI420(src,src_stride_line,
							dst_y,dst_stride_y,
							dst_u,dst_stride_u,
							dst_v,dst_stride_v,
							m_width,m_height);
				m_yuv_valiad = true;
			}

			break;
			
		default:
			break;

	}
#else
	uint32_t fourcc = 0;
    int ret = 0;

	switch(src_pixel_format) {
	
		   case AV_PIX_FMT_RGB24:
	
			  fourcc = libyuv::FOURCC_24BG;
				
			  break;
	
		   case AV_PIX_FMT_BGRA:
	
			   //fourcc = libyuv::FOURCC_BGRA;
			   // ÄÚ´æË³ÐòÊÇBGRA,ËùÒÓÃ·½·¨µÃ·´¹ýÀ´ARGB
			   fourcc = libyuv::FOURCC_ARGB;

			   break;
			   
		   case AV_PIX_FMT_RGBA:
			   	fourcc = libyuv::FOURCC_RGBA;
				
			   break;
				   
		   case AV_PIX_FMT_0RGB32:
		   case AV_PIX_FMT_ARGB:
	
			   fourcc = libyuv::FOURCC_ARGB;
	
			   break;
			   
		   default:
			   break;
	
	   }


	ret = libyuv::ConvertToI420(src,src_stride_line,
						  dst_y,dst_stride_y,
						  dst_u,dst_stride_u,
						  dst_v,dst_stride_v,
						  0,0,
						  m_width,m_height,
						  m_width,m_height,
						  libyuv::kRotate0,
						  fourcc);

	if (ret == 0) {
		m_yuv_valiad = true;
	}

#endif
	saveFile();

}



void yuvData::saveFile(){
    if (!m_yuv_valiad) {
		return;
	}

    if (m_yuv_fd == nullptr) {
		m_yuv_fd = fopen("yuvconvert.yuv", "wb");
		if (m_yuv_fd == nullptr) {
			std::cout<<" open yuvconvert.yuv error!"<<std::endl;
			return;
		}
	}

	
	fwrite(m_data, 1, m_size, m_yuv_fd);
}

void yuvData::closeFile(){
    if (m_yuv_fd != nullptr) {
		fclose(m_yuv_fd);
		m_yuv_fd = nullptr;
	}

}

void yuvData::putData(uint8_t* src, int src_stride_line, int src_pixel_format){
    if (m_should_stop) {
        return;
	}

    if (m_data_queue.size() > m_queue_size) {
	    std::unique_lock<std::mutex> lock(m_queue_mutex);
		m_queue_condition.wait(lock);

		if (m_should_stop) {
            return;
	    }
	}

	m_data_queue.emplace_back(src, src_stride_line, src_pixel_format);

	std::unique_lock<std::mutex> lock(m_queue_mutex);
	m_queue_condition.notify_one();
}


void yuvData::convertThread(){
    m_should_stop = false;

    while(!m_should_stop) {

        if (m_data_queue.empty()) {
			std::unique_lock<std::mutex> lock(m_queue_mutex);
			m_queue_condition.wait(lock);
			if (m_should_stop) {
                return;
	        }
		}

        src_data data = m_data_queue.front();

        // delete front
		m_data_queue.pop_front();
		std::unique_lock<std::mutex> lock(m_queue_mutex);
		m_queue_condition.notify_one();

		convertToYuv(data.m_data, data.m_stride_line, data.m_pix_fmt);

	}
}


void yuvData::setStop(){
	std::unique_lock<std::mutex> lock(m_queue_mutex);
	m_queue_condition.notify_all();

	m_should_stop = true;
}
