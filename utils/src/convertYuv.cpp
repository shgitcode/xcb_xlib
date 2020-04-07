/*
 // ARGBToI420 : https://blog.csdn.net/XIAIBIANCHENG/article/details/73065646?utm_source=blogxgwz8
*/

#include <time.h>

#include "convertYuv.h"

yuvData::yuvData(unsigned int width, unsigned int height, int pix_fmt) {
    m_width = width;
	m_height = height;
	m_pix_fmt = pix_fmt;

	if (m_pix_fmt == AV_PIX_FMT_YUV420P) {
        m_size = m_width * m_height*3/2;
		m_data = new uint8_t[m_size];
	}

	m_yuv_fd = nullptr;

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


#if 0
void yuvData::setYuvData(std::unique_ptr<yuvData> &data) {
	    std::unique_lock<std::mutex> yuv_lock(m_yuv_mutex); 

		yuv_lock.lock();
	    m_yuv_buffer.push_back(std::move(data));
	}


uint8_t* yuvData::getYuvData(bool &bIsStop) {
	std::unique_lock<std::mutex> yuv_lock(m_yuv_mutex); 
		
	yuv_lock.lock();

	if (m_yuv_buffer.empty()) {
        bIsStop = m_yuv_stop;
		return nullptr;
	}

	std::unique_ptr<yuvData> data = std::move(m_yuv_buffer.front());
	m_yuv_buffer.pop_front();

	return data->getData();
}
#endif

