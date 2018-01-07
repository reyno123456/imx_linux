#ifndef _HAL_CAN_TEST_H

#define DEV_CAN             "can0"

#define HAL_CAN_SINGLE_FRAME 		0
#define HAL_CAN_MUL_FRAME_START 	0x1
#define HAL_CAN_MUL_FRAME_MIDDLE	0x2
#define HAL_CAN_MUL_FRAME_END		0x3

/********************************************
*with 18 bit externed identification
*0x3FFFF
*0x3 used as type
*after 0xFF used as length
*the last 0xFF used as index
********************************************/
typedef struct can_multi_frame_info
{	
	unsigned char type;
	unsigned char length;
	unsigned char index;
}s_can_multi_frame_info;

typedef struct hal_can_frame
{
	unsigned short int id;
	s_can_multi_frame_info externed_id;

	unsigned char *p_data;
	unsigned char trans_status;
}s_hal_can_frame;

int can_send_multi_frame(int socket, s_hal_can_frame *p_hal_can_frame);
int can_recv_multi_frame(int socket, s_hal_can_frame *p_hal_can_frame);

#define _HAL_CAN_TEST_H
#endif
