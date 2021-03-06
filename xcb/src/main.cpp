

#include "XcbInput.h"


int main(int argc, char* argv[]) 
{

    if (argc < 2) {
        std::cout<<"please use : xcbcap frameRate"<<std::endl;
		return 0;
	}

    std::unique_ptr<XcbInput> m_xcb_input = nullptr;
	int m_sleep = 5;

    unsigned int m_video_x = 0;
	unsigned int m_video_y = 0;
	unsigned int m_video_in_width = 0;
	unsigned int m_video_in_height = 0;
	unsigned int m_video_frame_rate = atoi(argv[1]);;
	bool m_video_record_cursor = true;
	bool m_video_follow_cursor = false;
	bool m_video_follow_full_screen = false;
	
	
    m_xcb_input.reset(new XcbInput(m_video_x, m_video_y,
		                           m_video_in_width, 
		                           m_video_in_height,
		                           m_video_frame_rate,
		                           m_video_record_cursor,
                                   m_video_follow_cursor, 
                                   m_video_follow_full_screen));
	

	std::this_thread::sleep_for(std::chrono::seconds(m_sleep));

    std::cout<<"xcb sleep: "<<m_sleep<<std::endl;
	
	m_xcb_input->setStop();
	
	std::this_thread::sleep_for(std::chrono::seconds(1));

	std::cout<<"xcb capture frame: "<<m_xcb_input->GetFrameCounter()/m_sleep<<std::endl;

    //std::cout<<"xcb capture end!"<<0x8000000000000000ull<<":"<<0x8000000000000001ull<<std::endl;
}
