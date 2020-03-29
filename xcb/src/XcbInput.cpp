/*
Copyright (c) 2012-2020 Maarten Baert <maarten-baert@hotmail.com>

This file contains code from x11grab.c (part of ffmpeg/libav). The copyright information for x11grab.c is:
>> FFmpeg/Libav integration:
>> Copyright (C) 2006 Clemens Fruhwirth <clemens@endorphin.org>
>>                    Edouard Gomez <ed.gomez@free.fr>
>>
>> This file contains code from grab.c:
>> Copyright (c) 2000-2001 Fabrice Bellard
>>
>> This file contains code from the xvidcap project:
>> Copyright (C) 1997-1998 Rasca, Berlin
>>               2003-2004 Karl H. Beckers, Frankfurt

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

#include <time.h>

#include "XcbInput.h"

#ifdef WITH_XCB_SHM
#define TEST_SYN "have xcb_shm"
#else
#define TEST_SYN "not found xcb_shm"
#endif

#ifdef CONFIG_LIBXCB_SHM
#define USE_TEST_SYN "configure have xcb_shm"
#else
#define USE_TEST_SYN "configure not found xcb_shm"
#endif


#define FOLLOW_CENTER -1
#define FRAME_RATE 1
#define OPEN_FILE 0

XcbInput::XcbInput(unsigned int x, 
	                   unsigned int y, 
	                   unsigned int width, 
	                   unsigned int height, 
	                   unsigned int frame_rate, 
	                   bool record_cursor, 
	                   bool follow_cursor, 
	                   bool follow_full_screen) {

	std::cout<<"Enter [XcbInput::construction]"<<std::endl;

	m_x = x;
	m_y = y;
	m_width = width;
	m_height = height;
	m_record_cursor = record_cursor;
	m_follow_cursor = follow_cursor;
	m_follow_fullscreen = follow_full_screen;

    //
    std::cout<<TEST_SYN<<std::endl;
	std::cout<<"============="<<std::endl;
	std::cout<<USE_TEST_SYN<<std::endl;
	//add
	m_fp = nullptr;
	m_video_frame_rate = frame_rate;
	m_last_timestamp = std::numeric_limits<int64_t>::min();
	m_next_timestamp = hrt_time_micro();

	m_xcb_conn = NULL;
	m_xcb_screen = NULL;
	m_xcb_buffer = nullptr;
	m_xcb_img = nullptr;

	m_screen_bbox = Rect(m_x, m_y, m_x + m_width, m_y + m_height);

	{
		SharedLock lock(&m_shared_data);
		lock->m_current_width = m_width;
		lock->m_current_height = m_height;
	}

	if(m_width == 0 || m_height == 0) {
		std::cout<<"[XcbInput::Init] Error: "
			     <<"Width or height is zero!"
			     <<std::endl;
		throw X11Exception();
	}
	
	if(m_width > SSR_MAX_IMAGE_SIZE || m_height > SSR_MAX_IMAGE_SIZE) {
		std::cout<<"[XcbInput::Init] Error: "
			     <<"Width or height is too large, "
			     <<"the maximum width and height is "
			     <<SSR_MAX_IMAGE_SIZE
			     <<std::endl;
		throw X11Exception();
	}

	std::cout<<"[XcbInput::Init] {x:y}={"<<m_x<<":"<<m_y
		     <<"} {width:height}={"<<m_width<<":"<<m_height<<"}"
		     <<std::endl;

	try {
		Init();
	} catch(...) {
		xcbgrab_read_close();
		throw;
	}

	// open file
	#if OPEN_FILE
	m_fp = fopen("xcbCap.yuv", "wb");
	if (m_fp == NULL) {
		m_fp = nullptr;
		std::cout<<" open cap.yuv error!"<<std::endl;
	}
	#endif

}

XcbInput::~XcbInput() {

	// tell the thread to stop
	if(m_thread.joinable()) {

		std::cout<<"[XcbInput::~XcbInput] "
			     <<"Stopping input thread ..."
			     <<std::endl;

		m_should_stop = true;
		m_thread.join();
	}

	// free everything
	xcbgrab_read_close();

	if (m_fp) {
       fclose(m_fp);
	}

}

void XcbInput::GetCurrentSize(unsigned int *width, 
                                     unsigned int *height) {
	SharedLock lock(&m_shared_data);
	*width = lock->m_current_width;
	*height = lock->m_current_height;
}

double XcbInput::GetFPS() {
	int64_t timestamp = hrt_time_micro();
	uint32_t frame_counter = m_frame_counter;
	unsigned int time = timestamp - m_fps_last_timestamp;
	if(time > 500000) {
		unsigned int frames = frame_counter - m_fps_last_counter;
		m_fps_last_timestamp = timestamp;
		m_fps_last_counter = frame_counter;
		m_fps_current = (double) frames / ((double) time * 1.0e-6);
	}
	return m_fps_current;
}

void XcbInput::Init() {

	std::cout<<"Enter [XcbInput::Init]"<<std::endl;

    xcbgrab_read_header();
	
	// initialize frame counter
	m_frame_counter = 0;
	m_fps_last_timestamp = hrt_time_micro();
	m_fps_last_counter = 0;
	m_fps_current = 0.0;

	// start input thread
	m_should_stop = false;
	m_error_occurred = false;
	m_thread = std::thread(&XcbInput::InputThread, this);

}

int64_t XcbInput::CalculateNextVideoTimestamp(){
	return m_next_timestamp;
}

void XcbInput::ReadVideoFrame(int64_t timestamp){
	m_next_timestamp = std::max(m_next_timestamp + (int64_t) (1000000 /m_video_frame_rate), timestamp);

    return ;

}

void XcbInput::InputThread() {
	try {

		std::cout<<"Enter [XcbInput::InputThread]"<<std::endl;

		unsigned int grab_x = m_x, grab_y = m_y, grab_width = m_width, grab_height = m_height;
		bool has_initial_cursor = false;
		int64_t last_timestamp = hrt_time_micro();
		int64_t timestamp = hrt_time_micro();

		m_next_timestamp = timestamp;

		while(!m_should_stop) {
			
			timestamp = hrt_time_micro();
#if FRAME_RATE
			// sleep
			int64_t next_timestamp = CalculateNextVideoTimestamp();
			int64_t wait = next_timestamp - timestamp;
			
			if(wait > 0) {
				usleep(wait);
				timestamp = hrt_time_micro();	
			}
#endif
            xcbgrab_read_packet();
			
			// increase the frame counter
			++m_frame_counter;

#if OPEN_FILE
			// push the frame
			//uint8_t *image_data = (uint8_t*) m_x11_image->data;
			//int image_stride = m_x11_image->bytes_per_line;
			// write file
			//fwrite(m_x11_image->data, 1, m_x11_image->width*m_x11_image->height*m_x11_image->bits_per_pixel/8, m_fp);
#endif
			ReadVideoFrame(timestamp);
			last_timestamp = timestamp;
			//int64_t timestamp0 = hrt_time_micro();

			//std::cout<<"total time: "<<(timestamp0-timestamp)<<std::endl;
#if 0
			std::cout<<"[XcbInput::InputThread]" <<" counter :"<<m_frame_counter
				     <<" current timestamp : "<<timestamp
				     <<" next timestamp : "<<next_timestamp<<std::endl;
#endif
		}

        std::cout<<"[XcbInput::InputThread] "
			     <<"Input thread stopped..count: "
			     <<m_frame_counter<<std::endl;
	} catch(const std::exception& e) {
		m_error_occurred = true;
		
        std::cout<<"[XcbInput::InputThread] "
			     <<"Exception "<<e.what()
			     <<" in input thread."<<std::endl;
	} catch(...) {
		m_error_occurred = true;
		
        std::cout<<"[XcbInput::InputThread] "
			     <<"Unknown exception in input thread."<<std::endl;
	}
}


xcb_screen_t * XcbInput::get_screen(const xcb_setup_t *setup, int screen_num)
{
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    xcb_screen_t *screen     = NULL;

    for (; it.rem > 0; xcb_screen_next (&it)) {
        if (!screen_num) {
            screen = it.data;
            break;
        }

        screen_num--;
    }

    return screen;
}

#ifdef CONFIG_LIBXCB_SHM
int XcbInput::check_shm(xcb_connection_t *conn)
{
    xcb_shm_query_version_cookie_t cookie = xcb_shm_query_version(conn);
    xcb_shm_query_version_reply_t *reply;

    reply = xcb_shm_query_version_reply(conn, cookie, NULL);
    if (reply) {
        free(reply);
        return 1;
    }

    return 0;
}
int XcbInput::allocate_shm()
{
    int size = m_frame_size + 64;
    uint8_t *data;
    int id;

    if (m_xcb_buffer){
        //std::cout<<"shm have been created!"<<std::endl;
		return 0;
	}
       

    id = shmget(IPC_PRIVATE, size, IPC_CREAT | 0777);
    if (id == -1) {
        char errbuf[1024];
        std::cout<<"Cannot get : "<<size 
                 <<" bytes of shared memory"<<std::endl;
        return -1;
    }
	
	// attach
    xcb_shm_attach(m_xcb_conn, m_xcb_segment, id, 0);
    data = (uint8_t*)shmat(id, NULL, 0);
    shmctl(id, IPC_RMID, 0);
    if ((intptr_t)data == -1 || !data) {
        std::cout<<"shmctl failed!"<<std::endl;
        return -1;
    }
	
    m_xcb_buffer = data;
	
    return 0;
}

int XcbInput::xcbgrab_frame_shm()
{
    xcb_shm_get_image_cookie_t iq;
    xcb_shm_get_image_reply_t *img;
    xcb_drawable_t drawable = m_xcb_screen->root;
    xcb_generic_error_t *e = NULL;
    int ret;

    ret = allocate_shm();
    if (ret < 0) {
        std::cout<<"allocate_shm failed!"<<std::endl;
        return ret;
	}

    // ´ÓÖ¸¶¨ÇøÓò×¥È¡Í¼Ïñ
    iq = xcb_shm_get_image(m_xcb_conn, drawable,
                           m_x, m_y, m_width, m_height, ~0,
                           XCB_IMAGE_FORMAT_Z_PIXMAP, m_xcb_segment, 0);
    img = xcb_shm_get_image_reply(m_xcb_conn, iq, &e);

    xcb_flush(m_xcb_conn);

    if (e) {
		std::cout<<"shm Cannot get the image data "
		         <<"event_error: response_type:"<<e->response_type
		         <<" error_code:"<<e->error_code
		         <<" sequence:"<<e->sequence
		         <<" resource_id:"<<e->resource_id
		         <<" minor_code:"<<e->minor_code
		         <<" major_code:"<<e->major_code
		         <<std::endl;		

        return -1;
    }

    free(img);

    return 0;
}

#endif
int XcbInput::check_position(int *pix_fmt)
{
    int ret = -1;
    xcb_get_geometry_cookie_t gc;
    xcb_get_geometry_reply_t *geo;// ÆÁÄ»ScreenµÄ¿í¸ß
    
    const xcb_setup_t *xcb_setup = xcb_get_setup(m_xcb_conn);;
	// ÆÁÄ»ScreenµÄ¸ñÊ½
    const xcb_format_t *fmt  = xcb_setup_pixmap_formats(xcb_setup);
    int length               = xcb_setup_pixmap_formats_length(xcb_setup);

    // µÃµ½ÆÁÄ»ScreenµÄ¿í¸ß
    gc  = xcb_get_geometry(m_xcb_conn, m_xcb_screen->root);
    geo = xcb_get_geometry_reply(m_xcb_conn, gc, NULL);

    if (m_x + m_width > geo->width ||
        m_y + m_height > geo->height) {
		std::cout<< "Capture area: "<<m_width<<"x"<<m_height
				 << " at position: "<<m_x<<"."<<m_y
				 << " outside the screen size: "<<geo->width<<"x"<<geo->height
				 <<std::endl;
        return -1;
    }

	*pix_fmt = 0;

    // ±¾»úÆ÷ÉÏdepth:24
    while (length--) {
        if (fmt->depth == geo->depth) {
            switch (geo->depth) {
            case 32:
                if (fmt->bits_per_pixel == 32)
                    *pix_fmt = AV_PIX_FMT_0RGB;
                break;
            case 24:// bpp:32
                if (fmt->bits_per_pixel == 32)
                    *pix_fmt = AV_PIX_FMT_0RGB32;
                else if (fmt->bits_per_pixel == 24)
                    *pix_fmt = AV_PIX_FMT_RGB24;
                break;
            case 16:
                if (fmt->bits_per_pixel == 16)
                    *pix_fmt = AV_PIX_FMT_RGB565;
                break;
            case 15:
                if (fmt->bits_per_pixel == 16)
                    *pix_fmt = AV_PIX_FMT_RGB555;
                break;
            case 8:
                if (fmt->bits_per_pixel == 8)
                    *pix_fmt = AV_PIX_FMT_RGB8;
                break;
            }
        }

        if (*pix_fmt) {
			
			m_bits_per_pixel = fmt->bits_per_pixel;
			m_frame_size = m_width * m_height * fmt->bits_per_pixel / 8;

			std::cout<< "Capture Input legth: "<< length
					 << " fmt depth: "<<fmt->depth
					 << " bits_per_pixel: "<<m_bits_per_pixel
					 << " frame size: "<<m_frame_size<<std::endl;
            ret = 0;
			break;
        }
		
        fmt++;
    }

    free(geo);

    return ret;
}

#ifdef CONFIG_LIBXCB_XFIXES
int XcbInput::check_xfixes(xcb_connection_t *conn)
{
    xcb_xfixes_query_version_cookie_t cookie;
    xcb_xfixes_query_version_reply_t *reply;

    cookie = xcb_xfixes_query_version(conn, XCB_XFIXES_MAJOR_VERSION,
                                      XCB_XFIXES_MINOR_VERSION);
    reply  = xcb_xfixes_query_version_reply(conn, cookie, NULL);

    if (reply) {
        free(reply);
        return 1;
    }
    return 0;
}

#define BLEND(target, source, alpha) \
    (target) + ((source) * (255 - (alpha)) + 255 / 2) / 255

void XcbInput::xcbgrab_draw_mouse(xcb_query_pointer_reply_t *p,
                               xcb_get_geometry_reply_t *geo)
{
    uint32_t *cursor;
    uint8_t *image = m_xcb_buffer;
    int stride     = m_bits_per_pixel/ 8;// ×Ö½ÚÊý
    xcb_xfixes_get_cursor_image_cookie_t cc;
    xcb_xfixes_get_cursor_image_reply_t *ci;
    int cx, cy, x, y, w, h, c_off, i_off;

    cc = xcb_xfixes_get_cursor_image(m_xcb_conn);
    ci = xcb_xfixes_get_cursor_image_reply(m_xcb_conn, cc, NULL);// Êó±êÐÅÏ¢£¿
    if (!ci)
        return;

    cursor = xcb_xfixes_get_cursor_image_cursor_image(ci);
    if (!cursor)
        return;

    // Êó±êÓÒÉÏ½ÇÎ»ÖÃ
    cx = ci->x - ci->xhot;
    cy = ci->y - ci->yhot;

    // ×îÖÕ£¬È·¶¨Î»ÖÃ
    x = FFMAX(cx, m_x);
    y = FFMAX(cy, m_y);

    // ÓÒ±ßÎ»ÖÃ
    w = FFMIN(cx + ci->width,  m_x + m_width)  - x;
	// ÏÂ±ßÎ»ÖÃ
    h = FFMIN(cy + ci->height, m_y + m_height) - y;

    c_off = x - cx;
    i_off = x - m_x;

    cursor += (y - cy) * ci->width;
    image  += (y - m_y) * m_width * stride;// µÃµ½Êó±êÔÙÍ¼ÏñÖÐµÄÎ»ÖÃ

    for (y = 0; y < h; y++) {
        cursor += c_off;
        image  += i_off * stride;
        for (x = 0; x < w; x++, cursor++, image += stride) {
            int r, g, b, a;

            r =  *cursor        & 0xff;
            g = (*cursor >>  8) & 0xff;
            b = (*cursor >> 16) & 0xff;
            a = (*cursor >> 24) & 0xff;

            if (!a)
                continue;

            if (a == 255) {
                image[0] = r;
                image[1] = g;
                image[2] = b;
            } else {
                image[0] = BLEND(r, image[0], a);
                image[1] = BLEND(g, image[1], a);
                image[2] = BLEND(b, image[2], a);
            }

        }
        cursor +=  ci->width - w - c_off;
        image  += (m_width - w - i_off) * stride;
    }

    free(ci);
}

#endif
int XcbInput::xcbgrab_reposition(xcb_query_pointer_reply_t *p,
                              xcb_get_geometry_reply_t *geo)
{
    int x = m_x, y = m_y;
	// ×¥ÆÁµÄ¿í¸ß
    int w = m_width, h = m_height, f = m_follow_cursor;
    int p_x, p_y;

    if (!p || !geo)
        return -1;

    // Êó±êÒÆ¶¯µÄÎ»ÖÃå
    p_x = p->win_x;
    p_y = p->win_y;

    if (f == FOLLOW_CENTER) {
        x = p_x - w / 2;
        y = p_y - h / 2;
    } else {
        // ×¥È¡ÇøÓòµÄ×óÓÒ
        int left   = x + f;
        int right  = x + w - f;
		// ÉÏÏÂ
        int top    = y + f;
        int bottom = y + h + f;
		
        if (p_x > right) {// Êó±êÒÆ³öÁË×¥È¡ÇøÓòµÄÓÒ±ß
            x += p_x - right;
        } else if (p_x < left) {
            x -= left - p_x;
        }
        if (p_y > bottom) { // Êó±êÒÆ³öÁË×¥È¡ÇøÓòµÄÏÂ±ß
            y += p_y - bottom;
        } else if (p_y < top) {
            y -= top - p_y;
        }
    }

    // ÐÞ¸Ä×¥È¡ÇøÓòµÄ¿ªÊ¼Î»ÖÃ£¨È·±£²»Òª³¬³öÆÁÄ»£©
    m_x = FFMIN(FFMAX(0, x), geo->width  - w);
    m_y = FFMIN(FFMAX(0, y), geo->height - h);

    return 0;
}

int XcbInput::xcbgrab_frame()
{
    xcb_get_image_cookie_t iq;
    xcb_get_image_reply_t *img;
    xcb_drawable_t drawable = m_xcb_screen->root;
    xcb_generic_error_t *e = NULL;
    uint8_t *data;

	iq	= xcb_get_image(m_xcb_conn, XCB_IMAGE_FORMAT_Z_PIXMAP, drawable,
						m_x, m_y, m_width, m_height, ~0);
		
	img = xcb_get_image_reply(m_xcb_conn, iq, &e);
		
	if (e) {
		std::cout<<"Cannot get the image data "
		         <<"event_error: response_type:"<<e->response_type
		         <<" error_code:"<<e->error_code
		         <<" sequence:"<<e->sequence
		         <<" resource_id:"<<e->resource_id
		         <<" minor_code:"<<e->minor_code
		         <<" major_code:"<<e->major_code
		         <<std::endl;		
		return -1;
	}
		
	if (!img)
		return -1;
		
	//data   = xcb_get_image_data(img);
	//m_xcb_frame_size = xcb_get_image_data_length(img);
#if 0
	if (m_xcb_frame==nullptr){
		m_xcb_frame = new uint8_t[m_xcb_frame_size]; 
	}

	memcpy(m_xcb_frame, data, m_xcb_frame_size);
#endif		
	free(img);
		
	return 0;
}



int XcbInput::xcbgrab_read_header()
{
    int ret = 0;
    char *display_name = nullptr;// firstly set nullptr
	const xcb_setup_t *xcb_setup;

	std::cout<<"Enter xcbgrab_read_header."<<std::endl;

    // 1. Á´½ÓX Server
    m_xcb_conn = xcb_connect(NULL, &m_xcb_screen_num);
    if ((ret = xcb_connection_has_error(m_xcb_conn))) {
		std::cout<<"Cannot open display default, error: "
			     <<ret<<std::endl;
        return -1;
    }
	
    // 2. »ñÈ¡ÆÁÄ»Screen
    xcb_setup = xcb_get_setup(m_xcb_conn);
    // »ñÈ¡×îºóÒ»¸öÆÁÄ»Screen
    m_xcb_screen = get_screen(xcb_setup, m_xcb_screen_num);
    if (!m_xcb_screen) {
		std::cout<<"The screen  "<<m_xcb_screen_num 
			     <<" does not exist."<<std::endl;
        xcbgrab_read_close();
        return -1;
    }

	std::cout<<"The screen number: "
		     <<m_xcb_screen_num<<std::endl;

    // 3. ¼ì²â·¶Î§
    ret = check_position(&m_pix_fmt);
    if (ret < 0) {
        xcbgrab_read_close();
        return ret;
    }

#ifdef CONFIG_LIBXCB_SHM
    if ((m_xcb_use_shm = check_shm(m_xcb_conn))){
        m_xcb_segment = xcb_generate_id(m_xcb_conn);
    }
#endif

#ifdef CONFIG_LIBXCB_XFIXES
    if (m_record_cursor) {
        if (!(m_record_cursor = check_xfixes(m_xcb_conn))) {
			std::cout<<"XFixes not available, cannot draw the mouse."<<std::endl;
        }
		
        if (m_bits_per_pixel < 24) {	
			std::cout<<m_bits_per_pixel<<" bits per pixel screen."<<std::endl;
            m_record_cursor = 0;
        }
    }
#endif

	std::cout<<"Xcb use shm : "<<m_xcb_use_shm
		     <<" Xcb record cursor : "<<m_record_cursor
		     <<std::endl;


    return 0;
}

int XcbInput::xcbgrab_read_packet()
{
    xcb_query_pointer_cookie_t pc;
    xcb_get_geometry_cookie_t gc;
    xcb_query_pointer_reply_t *p  = NULL;// ¼ÇÂ¼Êó±êÒÆ¶¯Î»ÖÃ£¿
    xcb_get_geometry_reply_t *geo = NULL;
    int ret = 0;

	//int64_t timestamp = hrt_time_micro();

    if (m_follow_cursor || m_record_cursor) {
        pc  = xcb_query_pointer(m_xcb_conn, m_xcb_screen->root);
        gc  = xcb_get_geometry(m_xcb_conn, m_xcb_screen->root);
        p   = xcb_query_pointer_reply(m_xcb_conn, pc, NULL);
        geo = xcb_get_geometry_reply(m_xcb_conn, gc, NULL);
    }

	//int64_t timestamp0 = hrt_time_micro();

    //std::cout<<"time: "<<(timestamp0-timestamp)<<std::endl;

	if (m_follow_cursor && p->same_screen)
        xcbgrab_reposition(p, geo);

    // ¹²ÏíÄÚ´æ·½Ê½--¸ßÐ§ 
#ifdef CONFIG_LIBXCB_SHM
    if (m_xcb_use_shm && xcbgrab_frame_shm() < 0)
        m_xcb_use_shm = 0;
#endif
	//int64_t timestamp01 = hrt_time_micro();
	//std::cout<<"shm0 time: "<<(timestamp01-timestamp0)<<":"<<m_xcb_use_shm<<std::endl;

    if (!m_xcb_use_shm)// ¿½±´·½Ê½
        ret = xcbgrab_frame();

	//int64_t timestamp1 = hrt_time_micro();

	//std::cout<<"shm time: "<<(timestamp1-timestamp01)<<std::endl;

#ifdef CONFIG_LIBXCB_XFIXES
    if (ret >= 0 && m_record_cursor && p->same_screen)
        xcbgrab_draw_mouse(p, geo);
#endif

	//int64_t timestamp2 = hrt_time_micro();

	//std::cout<<"fixes time: "<<(timestamp2-timestamp1)<<std::endl;

    free(p);
    free(geo);

    return ret;
}

int XcbInput::xcbgrab_read_close()
{

    if (m_xcb_buffer) {
        shmdt(m_xcb_buffer);
    }

    xcb_disconnect(m_xcb_conn);

    return 0;
}

