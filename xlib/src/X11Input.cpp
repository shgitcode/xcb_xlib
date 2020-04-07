
#include "X11Input.h"

#define OPEN_FILE 1

// Converts a X11 image format to a format that libav/ffmpeg understands.
static AVPixelFormat X11ImageGetPixelFormat(XImage* image) {
	switch(image->bits_per_pixel) {
		case 8: return AV_PIX_FMT_PAL8;
		case 16: {
			if(image->red_mask == 0xf800 && image->green_mask == 0x07e0 && image->blue_mask == 0x001f) return AV_PIX_FMT_RGB565;
			if(image->red_mask == 0x7c00 && image->green_mask == 0x03e0 && image->blue_mask == 0x001f) return AV_PIX_FMT_RGB555;
			break;
		}
		case 24: {
			if(image->red_mask == 0xff0000 && image->green_mask == 0x00ff00 && image->blue_mask == 0x0000ff) return AV_PIX_FMT_BGR24;
			if(image->red_mask == 0x0000ff && image->green_mask == 0x00ff00 && image->blue_mask == 0xff0000) return AV_PIX_FMT_RGB24;
			break;
		}
		case 32: {
			if(image->red_mask == 0xff0000 && image->green_mask == 0x00ff00 && image->blue_mask == 0x0000ff) return AV_PIX_FMT_BGRA;
			if(image->red_mask == 0x0000ff && image->green_mask == 0x00ff00 && image->blue_mask == 0xff0000) return AV_PIX_FMT_RGBA;
			if(image->red_mask == 0xff000000 && image->green_mask == 0x00ff0000 && image->blue_mask == 0x0000ff00) return AV_PIX_FMT_ABGR;
			if(image->red_mask == 0x0000ff00 && image->green_mask == 0x00ff0000 && image->blue_mask == 0xff000000) return AV_PIX_FMT_ARGB;
			break;
		}
	}
	#if 0
	Logger::LogError("[X11ImageGetPixelFormat] " + Logger::tr("Error: Unsupported X11 image pixel format!") + "\n"
					 "    bits_per_pixel = " + QString::number(image->bits_per_pixel) + ", red_mask = 0x" + QString::number(image->red_mask, 16)
					 + ", green_mask = 0x" + QString::number(image->green_mask, 16) + ", blue_mask = 0x" + QString::number(image->blue_mask, 16));
	//throw X11Exception();
	#endif
	return AV_PIX_FMT_NONE;
}

// clears a rectangular area of an image (i.e. sets the memory to zero, which will most likely make the image black)
static void X11ImageClearRectangle(XImage* image, unsigned int x, unsigned int y, unsigned int w, unsigned int h) {

	// check the image format
	if(image->bits_per_pixel % 8 != 0)
		return;
	unsigned int pixel_bytes = image->bits_per_pixel / 8;

	// fill the rectangle with zeros
	for(unsigned int j = 0; j < h; ++j) {
		uint8_t *image_row = (uint8_t*) image->data + image->bytes_per_line * (y + j);
		memset(image_row + pixel_bytes * x, 0, pixel_bytes * w);
	}

}

// Draws the current cursor at the current position on the image. Requires XFixes.
// Note: In the original code from x11grab, the variables for red and blue are swapped
// (which doesn't change the result, but it's confusing).
// Note 2: This function assumes little-endianness.
// Note 3: This function only supports 24-bit and 32-bit images (it does nothing for other bit depths).
static void X11ImageDrawCursor(Display* dpy, XImage* image, int recording_area_x, int recording_area_y) {

	// check the image format
	unsigned int pixel_bytes, r_offset, g_offset, b_offset;
	if(image->bits_per_pixel == 24 && image->red_mask == 0xff0000 && image->green_mask == 0x00ff00 && image->blue_mask == 0x0000ff) {
		pixel_bytes = 3;
		r_offset = 2; g_offset = 1; b_offset = 0;
	} else if(image->bits_per_pixel == 24 && image->red_mask == 0x0000ff && image->green_mask == 0x00ff00 && image->blue_mask == 0xff0000) {
		pixel_bytes = 3;
		r_offset = 0; g_offset = 1; b_offset = 2;
	} else if(image->bits_per_pixel == 32 && image->red_mask == 0xff0000 && image->green_mask == 0x00ff00 && image->blue_mask == 0x0000ff) {
		pixel_bytes = 4;
		r_offset = 2; g_offset = 1; b_offset = 0;
	} else if(image->bits_per_pixel == 32 && image->red_mask == 0x0000ff && image->green_mask == 0x00ff00 && image->blue_mask == 0xff0000) {
		pixel_bytes = 4;
		r_offset = 0; g_offset = 1; b_offset = 2;
	} else if(image->bits_per_pixel == 32 && image->red_mask == 0xff000000 && image->green_mask == 0x00ff0000 && image->blue_mask == 0x0000ff00) {
		pixel_bytes = 4;
		r_offset = 3; g_offset = 2; b_offset = 1;
	} else if(image->bits_per_pixel == 32 && image->red_mask == 0x0000ff00 && image->green_mask == 0x00ff0000 && image->blue_mask == 0xff000000) {
		pixel_bytes = 4;
		r_offset = 1; g_offset = 2; b_offset = 3;
	} else {
		return;
	}

	// get the cursor
	XFixesCursorImage *xcim = XFixesGetCursorImage(dpy);
	if(xcim == NULL)
		return;

	// calculate the position of the cursor
	int x = xcim->x - xcim->xhot - recording_area_x;
	int y = xcim->y - xcim->yhot - recording_area_y;

	// calculate the part of the cursor that's visible
	int cursor_left = std::max(0, -x), cursor_right = std::min((int) xcim->width, image->width - x);
	int cursor_top = std::max(0, -y), cursor_bottom = std::min((int) xcim->height, image->height - y);

	// draw the cursor
	// XFixesCursorImage uses 'long' instead of 'int' to store the cursor images, which is a bit weird since
	// 'long' is 64-bit on 64-bit systems and only 32 bits are actually used. The image uses premultiplied alpha.
	for(int j = cursor_top; j < cursor_bottom; ++j) {
		unsigned long *cursor_row = xcim->pixels + xcim->width * j;
		uint8_t *image_row = (uint8_t*) image->data + image->bytes_per_line * (y + j);
		for(int i = cursor_left; i < cursor_right; ++i) {
			unsigned long cursor_pixel = cursor_row[i];
			uint8_t *image_pixel = image_row + pixel_bytes * (x + i);
			int cursor_a = (uint8_t) (cursor_pixel >> 24);
			int cursor_r = (uint8_t) (cursor_pixel >> 16);
			int cursor_g = (uint8_t) (cursor_pixel >> 8);
			int cursor_b = (uint8_t) (cursor_pixel >> 0);
			if(cursor_a == 255) {
				image_pixel[r_offset] = cursor_r;
				image_pixel[g_offset] = cursor_g;
				image_pixel[b_offset] = cursor_b;
			} else {
				image_pixel[r_offset] = (image_pixel[r_offset] * (255 - cursor_a) + 127) / 255 + cursor_r;
				image_pixel[g_offset] = (image_pixel[g_offset] * (255 - cursor_a) + 127) / 255 + cursor_g;
				image_pixel[b_offset] = (image_pixel[b_offset] * (255 - cursor_a) + 127) / 255 + cursor_b;
			}
		}
	}

	// free the cursor
	XFree(xcim);

}

/*
* shm:
*    no frameRate: 1450/1s
*    30: 30
*    60: 60
*no shm
*    no frameRate: 150/1s
*    30: 15
*    60: 25
*/
X11Input::X11Input(unsigned int x, 
	                   unsigned int y, 
	                   unsigned int width, 
	                   unsigned int height,
	                   unsigned int frame_rate,
	                   bool record_cursor, 
	                   bool follow_cursor, 
	                   bool follow_full_screen) {


	std::cout<<"Enter [X11Input::Construction] !"<<std::endl;

	m_x = x;
	m_y = y;
	m_width = width;
	m_height = height;
	m_record_cursor = record_cursor;
	m_follow_cursor = follow_cursor;
	m_follow_fullscreen = follow_full_screen;

	//add for test
	m_fp = nullptr;
	m_video_frame_rate = frame_rate;
	m_last_timestamp = std::numeric_limits<int64_t>::min();
	m_next_timestamp = hrt_time_micro();

	m_x11_display = NULL;
	m_x11_image = NULL;
	m_x11_shm_info.shmseg = 0;
	m_x11_shm_info.shmid = -1;
	m_x11_shm_info.shmaddr = (char*) -1;
	m_x11_shm_info.readOnly = false;
	m_x11_shm_server_attached = false;

	m_screen_bbox = Rect(m_x, m_y, m_x + m_width, m_y + m_height);

	{
		SharedLock lock(&m_shared_data);
		lock->m_current_width = m_width;
		lock->m_current_height = m_height;
	}

	if(m_width == 0 || m_height == 0) {
		std::cout<<"[X11Input::Construction] Error: Width or height is zero!"<<std::endl;
		throw X11Exception();
	}
	if(m_width > SSR_MAX_IMAGE_SIZE || m_height > SSR_MAX_IMAGE_SIZE) {
		std::cout<<"[X11Input::Construction] Error: Width or height is too large, the maximum width and height is!"<<SSR_MAX_IMAGE_SIZE<<std::endl;
		throw X11Exception();
	}

	std::cout<<"[X11Input::Construction] {x:y}={"<<m_x<<":"<<m_y
		     <<"} {width:height}={"<<m_width<<":"<<m_height<<"}"<<std::endl;

	try {
		Init();
	} catch(...) {
		Free();
		throw;
	}

	if (m_yuv_convert == nullptr) {
	    m_yuv_convert = std::unique_ptr<yuvData>(new yuvData(m_width, m_height, AV_PIX_FMT_YUV420P));
	}
	

	// open file
#if OPEN_FILE
	m_fp = fopen("x11.rgb", "wb");
	if (m_fp == NULL) {
		m_fp = nullptr;
		std::cout<<" open x11.yuv error!"<<std::endl;
	}
#endif
}

X11Input::~X11Input() {

	// tell the thread to stop
	if(m_thread.joinable()) {
		//Logger::LogInfo("[X11Input::~X11Input] " + Logger::tr("Stopping input thread ..."));
		m_should_stop = true;
		m_thread.join();
	}

	// free everything
	Free();

	if (m_fp) {
       fclose(m_fp);
	}

}

void X11Input::GetCurrentSize(unsigned int *width, unsigned int *height) {
	SharedLock lock(&m_shared_data);
	*width = lock->m_current_width;
	*height = lock->m_current_height;
}

double X11Input::GetFPS() {
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

void X11Input::Init() {

	std::cout<<"Enter [X11Input::Init]"<<std::endl;

	m_x11_display = XOpenDisplay(NULL); //QX11Info::display();
	if(m_x11_display == NULL) {
		std::cout<<"[X11Input::Init] Error: Can't open X display!"<<std::endl;
		throw X11Exception();
	}
	
	m_x11_screen = DefaultScreen(m_x11_display); //QX11Info::appScreen();
	m_x11_root = RootWindow(m_x11_display, m_x11_screen); //QX11Info::appRootWindow(m_x11_screen);
	m_x11_visual = DefaultVisual(m_x11_display, m_x11_screen); //(Visual*) QX11Info::appVisual(m_x11_screen);
	m_x11_depth = DefaultDepth(m_x11_display, m_x11_screen); //QX11Info::appDepth(m_x11_screen);

	m_x11_use_shm = XShmQueryExtension(m_x11_display);
	if(m_x11_use_shm) {
		std::cout<<"[X11Input::Init] Using X11 shared memory"<<std::endl;
	} else {
	    std::cout<<"[X11Input::Init] Not Using X11 shared memory"<<std::endl;
	}

	// showing the cursor requires XFixes (which should be supported on any modern X server, but let's check it anyway)
	if(m_record_cursor) {
		int event, error;
		if(!XFixesQueryExtension(m_x11_display, &event, &error)) {
			std::cout<<"[X11Input::Init] Warning: XFixes is not supported by X server, the cursor has been hidden!"<<std::endl;
			m_record_cursor = false;
		}
	}

	// get screen configuration information, so we can replace the unused areas with black rectangles (rather than showing random uninitialized memory)
	// this is also used by the mouse following code to make sure that the rectangle stays on the screen
	UpdateScreenConfiguration();

	// initialize frame counter
	m_frame_counter = 0;
	m_fps_last_timestamp = hrt_time_micro();
	m_fps_last_counter = 0;
	m_fps_current = 0.0;

	// start input thread
	m_should_stop = false;
	m_error_occurred = false;
	m_thread = std::thread(&X11Input::InputThread, this);

}

void X11Input::Free() {
	FreeImage();
	if(m_x11_display != NULL) {
		XCloseDisplay(m_x11_display);
		m_x11_display = NULL;
	}
}

void X11Input::AllocateImage(unsigned int width, unsigned int height) {
	assert(m_x11_use_shm);
	if(m_x11_shm_server_attached && m_x11_image->width == (int) width && m_x11_image->height == (int) height) {
		return; // reuse existing image
	}
	FreeImage();
	m_x11_image = XShmCreateImage(m_x11_display, m_x11_visual, m_x11_depth, ZPixmap, NULL, &m_x11_shm_info, width, height);
	if(m_x11_image == NULL) {
		std::cout<<"[X11Input::AllocateImage] Error: Can't create shared image!"<<std::endl;
		throw X11Exception();
	}
	m_x11_shm_info.shmid = shmget(IPC_PRIVATE, m_x11_image->bytes_per_line * m_x11_image->height, IPC_CREAT | 0700);
	if(m_x11_shm_info.shmid == -1) {
		std::cout<<"[X11Input::AllocateImage] Error: Can't get shared memory!"<<std::endl;
		throw X11Exception();
	}
	m_x11_shm_info.shmaddr = (char*) shmat(m_x11_shm_info.shmid, NULL, SHM_RND);
	if(m_x11_shm_info.shmaddr == (char*) -1) {
		std::cout<<"[X11Input::AllocateImage] Error: Can't attach to shared memory!"<<std::endl;
		throw X11Exception();
	}
	m_x11_image->data = m_x11_shm_info.shmaddr;
	if(!XShmAttach(m_x11_display, &m_x11_shm_info)) {
		std::cout<<"[X11Input::AllocateImage] Error: Can't attach server to shared memory!"<<std::endl;
		throw X11Exception();
	}
	m_x11_shm_server_attached = true;
}

void X11Input::FreeImage() {
	if(m_x11_shm_server_attached) {
		XShmDetach(m_x11_display, &m_x11_shm_info);
		m_x11_shm_server_attached = false;
	}
	if(m_x11_shm_info.shmaddr != (char*) -1) {
		shmdt(m_x11_shm_info.shmaddr);
		m_x11_shm_info.shmaddr = (char*) -1;
	}
	if(m_x11_shm_info.shmid != -1) {
		shmctl(m_x11_shm_info.shmid, IPC_RMID, NULL);
		m_x11_shm_info.shmid = -1;
	}
	if(m_x11_image != NULL) {
		XDestroyImage(m_x11_image);
		m_x11_image = NULL;
	}
}

void X11Input::UpdateScreenConfiguration() {

	// get screen rectangles
	m_screen_rects.clear();
	int event_base, error_base;
	if(XineramaQueryExtension(m_x11_display, &event_base, &error_base)) {
		int num_screens;
		XineramaScreenInfo *screens = XineramaQueryScreens(m_x11_display, &num_screens);
		try {
			for(int i = 0; i < num_screens; ++i) {
				m_screen_rects.emplace_back(screens[i].x_org, screens[i].y_org, screens[i].x_org + screens[i].width, screens[i].y_org + screens[i].height);
			}
		} catch(...) {
			XFree(screens);
			throw;
		}
		XFree(screens);
	} else {
	    std::cout<<"[X11Input::Init] Warning: Xinerama is not supported by X server, multi-monitor support may not work properly."<<std::endl;
		return;
	}

	// make sure that we have at least one monitor
	if(m_screen_rects.size() == 0) {
	    std::cout<<"[X11Input::Init] Warning: No monitors detected, multi-monitor support may not work properly."<<std::endl;
		return;
	}

	std::cout<<"[X11Input::Init] multi-monitor:"<<m_screen_rects.size()<<std::endl;

	// calculate bounding box
	m_screen_bbox = m_screen_rects[0];
	for(size_t i = 1; i < m_screen_rects.size(); ++i) {
		Rect &rect = m_screen_rects[i];
		if(rect.m_x1 < m_screen_bbox.m_x1)
			m_screen_bbox.m_x1 = rect.m_x1;
		if(rect.m_y1 < m_screen_bbox.m_y1)
			m_screen_bbox.m_y1 = rect.m_y1;
		if(rect.m_x2 > m_screen_bbox.m_x2)
			m_screen_bbox.m_x2 = rect.m_x2;
		if(rect.m_y2 > m_screen_bbox.m_y2)
			m_screen_bbox.m_y2 = rect.m_y2;
	}
	if(m_screen_bbox.m_x1 >= m_screen_bbox.m_x2 || m_screen_bbox.m_y1 >= m_screen_bbox.m_y2 ||
	   m_screen_bbox.m_x2 - m_screen_bbox.m_x1 > SSR_MAX_IMAGE_SIZE || m_screen_bbox.m_y2 - m_screen_bbox.m_y1 > SSR_MAX_IMAGE_SIZE) {
       std::cout<<"[X11Input::UpdateScreenConfiguration] "
	            <<"Error: Invalid screen bounding box! "
	            <<"{tx:ty}={"<<m_screen_bbox.m_x1<<":"<<m_screen_bbox.m_y1<<"} "
	            <<"{bx:by}={"<<m_screen_bbox.m_x2<<":"<<m_screen_bbox.m_y2<<"} "
	            <<std::endl;
		throw X11Exception();
	}

	/*qDebug() << "m_screen_rects:";
	for(Rect &rect : m_screen_rects) {
		qDebug() << "    rect" << rect.m_x1 << rect.m_y1 << rect.m_x2 << rect.m_y2;
	}
	qDebug() << "m_screen_bbox:";
	qDebug() << "    rect" << m_screen_bbox.m_x1 << m_screen_bbox.m_y1 << m_screen_bbox.m_x2 << m_screen_bbox.m_y2;*/

	std::cout<<"[X11Input::UpdateScreenConfiguration] box: "
			 <<"{tx:ty}={"<<m_screen_bbox.m_x1<<":"<<m_screen_bbox.m_y1<<"} "
			 <<"{bx:by}={"<<m_screen_bbox.m_x2<<":"<<m_screen_bbox.m_y2<<"} "
			 <<std::endl;

	// calculate dead space
	m_screen_dead_space = {m_screen_bbox};
	for(size_t i = 0; i < m_screen_rects.size(); ++i) {
		/*qDebug() << "PARTIAL m_screen_dead_space:";
		for(Rect &rect : m_screen_dead_space) {
			qDebug() << "    rect" << rect.m_x1 << rect.m_y1 << rect.m_x2 << rect.m_y2;
		}*/
		Rect &subtract = m_screen_rects[i];
		
		std::cout<<"[X11Input::UpdateScreenConfiguration] sub: "
				 <<"{tx:ty}={"<<subtract.m_x1<<":"<<subtract.m_y1<<"} "
				 <<"{bx:by}={"<<subtract.m_x2<<":"<<subtract.m_y2<<"} "
				 <<std::endl;

		std::vector<Rect> result;
		for(Rect &rect : m_screen_dead_space) {
			// rect contain subtract?? == cross?交叉
			// rect包含subtract
			if(rect.m_x1 < subtract.m_x2 && rect.m_y1 < subtract.m_y2 && subtract.m_x1 < rect.m_x2 && subtract.m_y1 < rect.m_y2) {
                // 高度差值 
				unsigned int mid_y1 = std::max(rect.m_y1, subtract.m_y1);
				unsigned int mid_y2 = std::min(rect.m_y2, subtract.m_y2);
                /*
                  *1===============rect
                  * A  *
                  * ***1----sub
                  *     |  |
                  * B
                  *     |  |
                  * *** ----2
                  *     *
                  * ==============2
				*/

				if(rect.m_y1 < subtract.m_y1)// 左上 A
					result.emplace_back(rect.m_x1, rect.m_y1, rect.m_x2, subtract.m_y1);
				if(rect.m_x1 < subtract.m_x1)// B
					result.emplace_back(rect.m_x1, mid_y1, subtract.m_x1, mid_y2);
				if(subtract.m_x2 < rect.m_x2)// C
					result.emplace_back(subtract.m_x2, mid_y1, rect.m_x2, mid_y2);
				if(subtract.m_y2 < rect.m_y2) // D
					result.emplace_back(rect.m_x1, subtract.m_y2, rect.m_x2, rect.m_y2);
			} else {
				result.emplace_back(rect);
			}
		}
		m_screen_dead_space = std::move(result);
	}

	/*qDebug() << "m_screen_dead_space:";
	for(Rect &rect : m_screen_dead_space) {
		qDebug() << "    rect" << rect.m_x1 << rect.m_y1 << rect.m_x2 << rect.m_y2;
	}*/

}

int64_t X11Input::CalculateNextVideoTimestamp(){
	return m_next_timestamp;
}

void X11Input::ReadVideoFrame(int64_t timestamp){
	
	m_next_timestamp = std::max(m_next_timestamp + (int64_t) (1000000 /m_video_frame_rate), timestamp);

    return ;

}

#define FRAME_RATE 1
// ffplay -pixel_format yuv420p -video_size 1280*720  -framerate 25  yuvconvert.yuv 
// ffplay -pixel_format bgra -video_size 1280*720 -framerate 25 x11.rgb

void X11Input::InputThread() {
	try {

		std::cout<<"Enter [X11Input::InputThread]"<<std::endl;

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
			// follow the cursor
			if(m_follow_cursor) {
				int mouse_x, mouse_y, dummy;
				Window dummy_win;
				unsigned int dummy_mask;
				if(XQueryPointer(m_x11_display, m_x11_root, &dummy_win, &dummy_win, &dummy, &dummy, &mouse_x, &mouse_y, &dummy_mask)) {
					if(m_follow_fullscreen) {
						for(Rect &rect : m_screen_rects) {
							if(mouse_x >= (int) rect.m_x1 && mouse_y >= (int) rect.m_y1 && mouse_x < (int) rect.m_x2 && mouse_y < (int) rect.m_y2) {
								grab_x = rect.m_x1;
								grab_y = rect.m_y1;
								grab_width = rect.m_x2 - rect.m_x1;
								grab_height = rect.m_y2 - rect.m_y1;
								break;
							}
						}
					} else {
						int grab_x_target = (mouse_x - (int) grab_width / 2) >> 1;
						int grab_y_target = (mouse_y - (int) grab_height / 2) >> 1;
						int frac = (has_initial_cursor)? lrint(1024.0 * exp(-1e-5 * (double) (timestamp - last_timestamp))) : 0;
						grab_x_target = (grab_x_target + ((int) (grab_x >> 1) - grab_x_target) * frac / 1024) << 1;
						grab_y_target = (grab_y_target + ((int) (grab_y >> 1) - grab_y_target) * frac / 1024) << 1;
						grab_x = clamp(grab_x_target, (int) m_screen_bbox.m_x1, (int) m_screen_bbox.m_x2 - (int) grab_width);
						grab_y = clamp(grab_y_target, (int) m_screen_bbox.m_y1, (int) m_screen_bbox.m_y2 - (int) grab_height);
					}
				}
				has_initial_cursor = true;
			}

			// save current size
			{
				SharedLock lock(&m_shared_data);
				lock->m_current_width = grab_width;
				lock->m_current_height = grab_height;
			}

			// get the image
			if(m_x11_use_shm) {
				AllocateImage(grab_width, grab_height);
				if(!XShmGetImage(m_x11_display, m_x11_root, m_x11_image, grab_x, grab_y, AllPlanes)) {
					std::cout<<"[X11Input::InputThread] Error: Can't get image (using shared memory)!"
						     <<" Usually this means the recording area is not completely inside the screen. Or did you change the screen resolution?"<<std::endl;
					throw X11Exception();
				}
			} else {
				if(m_x11_image != NULL) {
					XDestroyImage(m_x11_image);
					m_x11_image = NULL;
				}
                // for ZPixmap
			    m_x11_image = XGetImage(m_x11_display, m_x11_root, grab_x, grab_y, grab_width, grab_height, AllPlanes, ZPixmap);
				if(m_x11_image == NULL) {
					std::cout<<"[X11Input::InputThread] Error: Can't get image (not using shared memory)!"
						     <<" Usually this means the recording area is not completely inside the screen. Or did you change the screen resolution?"<<std::endl;
					throw X11Exception();
				}
			}

			// clear the dead space
			for(size_t i = 0; i < m_screen_dead_space.size(); ++i) {
				Rect rect = m_screen_dead_space[i];
				if(rect.m_x1 < grab_x)
					rect.m_x1 = grab_x;
				if(rect.m_y1 < grab_y)
					rect.m_y1 = grab_y;
				if(rect.m_x2 > grab_x + grab_width)
					rect.m_x2 = grab_x + grab_width;
				if(rect.m_y2 > grab_y + grab_height)
					rect.m_y2 = grab_y + grab_height;
				if(rect.m_x2 > rect.m_x1 && rect.m_y2 > rect.m_y1)
					X11ImageClearRectangle(m_x11_image, rect.m_x1 - grab_x, rect.m_y1 - grab_y, rect.m_x2 - rect.m_x1, rect.m_y2 - rect.m_y1);
			}

			// draw the cursor
			if(m_record_cursor) {
				X11ImageDrawCursor(m_x11_display, m_x11_image, grab_x, grab_y);
			}

			// increase the frame counter
			++m_frame_counter;

			// push the frame
			uint8_t *image_data = (uint8_t*) m_x11_image->data;
			int image_stride = m_x11_image->bytes_per_line;
			//AV_PIX_FMT_BGRA
			AVPixelFormat x11_image_format = X11ImageGetPixelFormat(m_x11_image);

            // 数据转YUV
			m_yuv_convert->convertToYuv(image_data, image_stride, x11_image_format);

			ReadVideoFrame(timestamp);
			last_timestamp = timestamp;
			if (m_frame_counter == 1) {
				std::cout<<"[X11Input::InputThread] " 
						 <<" counter : "<<m_frame_counter
						 <<" AVPixelFormat: "<<x11_image_format
						 <<" current timestamp : "<<timestamp
						 //<<" next timestamp : "<<next_timestamp
						 <<std::endl;
				
				std::cout<<"[X11Input::InputThread] XImage " 
						 <<" width: "<<m_x11_image->width
						 <<" height: "<<m_x11_image->height
						 <<" format: "<<m_x11_image->format
						 <<" byte_order : "<<m_x11_image->byte_order
						 <<" bitmap_unit : "<<m_x11_image->bitmap_unit
						 <<" bitmap_bit_order : "<<m_x11_image->bitmap_bit_order
						 <<" bitmap_pad : "<<m_x11_image->bitmap_pad
						 <<" depth : "<<m_x11_image->depth
						 <<" bytes_per_line : "<<m_x11_image->bytes_per_line
						 <<" bits_per_pixel : "<<m_x11_image->bits_per_pixel
						 <<" red_mask: "<<m_x11_image->red_mask
						 <<" green_mask : "<<m_x11_image->green_mask
						 <<" blue_mask : "<<m_x11_image->blue_mask
						 <<std::endl;;

			}

			//zpixmap  XImage  width: 1280 height: 720 format: 2 byte_order : 0 bitmap_unit : 32 bitmap_bit_order : 0 bitmap_pad : 32 depth : 24 bytes_per_line : 5120 bits_per_pixel : 32 red_mask: 16711680 green_mask : 65280 blue_mask : 255
            //xyzipmap XImage  width: 1280 height: 720 format: 1 byte_order : 0 bitmap_unit : 32 bitmap_bit_order : 0 bitmap_pad : 32 depth : 24 bytes_per_line : 160  bits_per_pixel : 1  red_mask: 16711680 green_mask : 65280 blue_mask : 255
			// write file
			#if OPEN_FILE
			if (m_fp) {
				if (m_x11_image->format == ZPixmap) {
					fwrite(m_x11_image->data, 1, m_x11_image->width*m_x11_image->height*m_x11_image->bits_per_pixel/8, m_fp);
				} 	    
			}
			#endif
		}

        std::cout<<"[XcbInput::InputThread] "
			     <<" frame counter : "<<m_frame_counter
			     <<" Input thread stopped......"<<std::endl;

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
