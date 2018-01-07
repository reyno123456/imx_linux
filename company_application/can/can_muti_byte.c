#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
 
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
 
#include <linux/can.h>
#include <linux/can/raw.h>

#include <stdio.h>
#include <pthread.h>

#include "hal_can.h"
#include "../common/common.h"
 

#define CAN_ID              0x123
#define CAN_DATA_LEN_MAX    8

pthread_mutex_t mutex;
pthread_cond_t cond;

static int can_send(int socket, struct can_frame *pframe)
{
    if (sizeof(struct can_frame) != write(socket, pframe, sizeof(struct can_frame)))
    {
        return RET_FAILED;
    }
    else
    {
        return RET_SUCCESS;
    }
}

static int can_rcv(int socket, struct can_frame *pframe)
{
    if (sizeof(struct can_frame) != read(socket, pframe, sizeof(struct can_frame)))
    {
        return RET_FAILED;
    }
    else
    {
        return RET_SUCCESS;
    }
}

static int can_init(int *psocket, unsigned char *can_dev)
{
    int socked;
	struct sockaddr_can can_addr;
	struct ifreq ifr;
	
	if((socked = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) 
	{
		DEBUG_ERROR("open can socket error.");
		return RET_FAILED;
	}

    memset(&ifr, 0, sizeof(ifr));
/* 	strcpy(ifr.ifr_name, DEV_CAN); */
	strcpy(ifr.ifr_name, can_dev);
	if (RET_SUCCESS != ioctl(socked, SIOCGIFINDEX, &ifr))
	{
		DEBUG_ERROR("ioctl SIOCGIFINDEX failed.");
		close(socked);
		return RET_FAILED;
	}

    memset(&can_addr, 0, sizeof(can_addr));
	can_addr.can_family  = AF_CAN;
	can_addr.can_ifindex = ifr.ifr_ifindex; 
  
	if(bind(socked, (struct sockaddr *)&can_addr, sizeof(can_addr)) < 0) 
	{
		DEBUG_ERROR("bind can socket failed.");
		close(socked);
		return RET_FAILED;
	}

    *psocket = socked;
    
	return RET_SUCCESS;
}

int can_send_multi_frame(int socket, s_hal_can_frame *p_hal_can_frame)
{
	unsigned int i;
	unsigned int length;
	unsigned int frame_index;
	struct can_frame *pframe;
	int ret;

	if (p_hal_can_frame->externed_id.length < 8)
	{
		DEBUG_ERROR("send buffer fail\n");
	}

	pframe = malloc(sizeof(struct can_frame));
	if (pframe == 0)
	{
		DEBUG_ERROR("malloc fail\n");
	}

	pframe->can_id = ((0x123 << 20) | (1 << 31));
	pframe->can_id &= ~(0x3FFFF << 1);
	length = p_hal_can_frame->externed_id.length;
	pframe->can_id |= length << 9;
	pframe->can_dlc = CAN_DATA_LEN_MAX;

	frame_index = 0;
	while(1)
	{
		if (frame_index * CAN_DATA_LEN_MAX >= p_hal_can_frame->externed_id.length)
		{
			break;
		}
		
		for (i = 0; i < p_hal_can_frame->externed_id.length; i++)
		{
			pframe->data[i] = p_hal_can_frame->p_data[i + frame_index * CAN_DATA_LEN_MAX];
		}

		if (frame_index == 0)
		{
			pframe->can_id &= ~(0x3 << 17);							// type clear
			pframe->can_id |= (HAL_CAN_MUL_FRAME_START << 17);		// type set start
		}
		else if ((frame_index+1) * CAN_DATA_LEN_MAX < p_hal_can_frame->externed_id.length)
		{
			pframe->can_id &= ~(0x3 << 17);							// type clear
			pframe->can_id |= (HAL_CAN_MUL_FRAME_MIDDLE << 17);		// type set start
		}
		else
		{
			pframe->can_id &= ~(0x3 << 17);							// type clear
			pframe->can_id |= (HAL_CAN_MUL_FRAME_END << 17);		// type set start			
		}
		
		pframe->can_id &= ~(0xFF << 1);								// index clear
		pframe->can_id |= frame_index << 1;							// index set

		//DEBUG_INFO("canid = 0x%x\n", pframe->can_id);	

		ret = can_send(socket, pframe);
		if (RET_SUCCESS == ret)
		{
/*
			DEBUG_INFO("send can datas: can_id = 0x%x,"
					   "data_len = %d\n", pframe->can_id, pframe->can_dlc);
			for (i = 0; i < pframe->can_dlc; i++)
			{
				DEBUG_INFO("data[%d] = 0x%x\n", i, pframe->data[i]);	
			}
*/
		}
		else
		{
			DEBUG_ERROR("send can datas failed.\n");	
		}

		frame_index++;
	}

	free(pframe);
	return RET_SUCCESS;
}

static int frame_to_multi_frame(struct can_frame *pframe, s_hal_can_frame *p_hal_can_frame)
{
	unsigned char frame_type;
	static unsigned int i;
	unsigned char length;
	unsigned char frame_index;

	frame_type = (pframe->can_id >> 17) & 0x3;
	length = pframe->can_id >> 9;
	frame_index = pframe->can_id >> 1;
	p_hal_can_frame->trans_status = frame_type;
	
	switch (frame_type)
	{
		case HAL_CAN_MUL_FRAME_START:
/*
			for (i = 0; i < CAN_DATA_LEN_MAX; i++)
				p_hal_can_frame->p_data[i] = pframe->data[i];
*/
			memcpy(p_hal_can_frame->p_data, &pframe->data, CAN_DATA_LEN_MAX);
		break;

		case HAL_CAN_MUL_FRAME_MIDDLE:
/*
			for (i = 0; i < CAN_DATA_LEN_MAX; i++)
				p_hal_can_frame->p_data[i + frame_index*CAN_DATA_LEN_MAX] = pframe->data[i];
*/
			memcpy(p_hal_can_frame->p_data + frame_index*CAN_DATA_LEN_MAX, &pframe->data, CAN_DATA_LEN_MAX);
		break;

		case HAL_CAN_MUL_FRAME_END:
/*
			for (i = 0; i < CAN_DATA_LEN_MAX; i++)
				p_hal_can_frame->p_data[i + frame_index*CAN_DATA_LEN_MAX] = pframe->data[i];
*/
			memcpy(p_hal_can_frame->p_data + frame_index*CAN_DATA_LEN_MAX, &pframe->data, CAN_DATA_LEN_MAX);
		break;

		default:break;
	}

	return RET_SUCCESS;	
}

int can_recv_multi_frame(int socket, s_hal_can_frame *p_hal_can_frame)
{
	unsigned int i;
	unsigned int length;
	unsigned int frame_index;
	struct can_frame *pframe;
	int ret;

	pframe = malloc(sizeof(struct can_frame));
	if (pframe == 0)
	{
		DEBUG_ERROR("malloc fail\n");
	}

    ret = can_rcv(socket, pframe);
    if (RET_SUCCESS == ret)
	{
	
	}
	else
	{
		DEBUG_ERROR("read can multi datas failed.\n");
		return ret;
	}

/*
	DEBUG_INFO("recieve can datas: can_id = 0x%x,"
			   "data_len = %d\n", pframe->can_id, pframe->can_dlc);
*/
	
	//p_hal_can_frame->externed_id = (pframe->can_id >> 1) & 0x3FFFF;
	
	//DEBUG_INFO("externed id = 0x%x\n", p_hal_can_frame->externed_id);
	
/*
	for (i = 0; i < pframe->can_dlc; i++)
	{
		DEBUG_INFO("data[%d] = 0x%x\n", i, pframe->data[i]);	
	}
*/
		
	frame_to_multi_frame(pframe, p_hal_can_frame);

	free(pframe);
	return RET_SUCCESS;

}

void *thread_send(void *arg)
{
	s_hal_can_frame hal_can_frame;
	unsigned char length = 64;
	int socket;
	int ret;
	unsigned char i;

	hal_can_frame.p_data = malloc(length*sizeof(unsigned char));
	if (hal_can_frame.p_data == 0)
	{
		DEBUG_ERROR("malloc fail\n");
	}

	for (i = 0; i < length; i++)
	{
		hal_can_frame.p_data[i] = i;
	}

	hal_can_frame.externed_id.length = length;
	
    ret = can_init(&socket, "can0");
    
    if (RET_SUCCESS == ret)
	{

	}
	
    //pthread_cleanup_push(pthread_mutex_unlock,&mutex);
/*
    while(1)
    {
        printf("%s is running\n", __func__);
        can_send_multi_frame(socket, &hal_can_frame);
        sleep(4);
    }
*/
	for (i = 0; i < length; i++)
	{
		DEBUG_INFO("data[%d] = 0x%d\n", i, hal_can_frame.p_data[i]);
	}
	while(1)
	{
		can_send_multi_frame(socket, &hal_can_frame);
		usleep(100000);
	}
    //pthread_cleanup_pop(0);
    free(hal_can_frame.p_data);
    exit((int)thread_send);
}

void *thread_recv(void *arg)
{
	s_hal_can_frame hal_can_frame;
	unsigned char length = 64;
	int socket;
	int ret;
	unsigned char i;
	unsigned int multi_frame_index = 0;

	printf("%s is running data=%s, time=%s\n", __func__, __DATE__, __TIME__);
	hal_can_frame.p_data = calloc(sizeof(unsigned char), length*sizeof(unsigned char));
	if (hal_can_frame.p_data == 0)
	{
		DEBUG_ERROR("malloc fail\n");
	}
	
    ret = can_init(&socket, "can1");
    
    if (RET_SUCCESS == ret)
    {
	    while(1)
	    {
			can_recv_multi_frame(socket, &hal_can_frame);
			if (hal_can_frame.trans_status == HAL_CAN_MUL_FRAME_END)
			{
				for (i = 0; i < length; i++)
				{
					DEBUG_INFO("data[%d] = 0x%d\n", i, hal_can_frame.p_data[i]);
				}
				DEBUG_INFO("multi_frame_index = %d\n", multi_frame_index++);				
			}
	    }
    }
    else
    {
		DEBUG_ERROR("can init receive fail\n");
    }
}


static void print_usage(const char *filename)
{
	DEBUG_INFO("Usage: %s [01]\n", filename);
	DEBUG_INFO(" 0  -- test can recieve.\n"
    	       " 1  -- test can send.\n");
}

int main(int argc, char *argv[])
{
	int index;
    pthread_t thid1,thid2;
    DEBUG_INFO("can multi frame test started\n");

	if ((2 != argc) || ((index = (*argv[1] - '0')) < 0) || (index > 1))
	{
		DEBUG_ERROR("Invalid arguments!\n");
		print_usage(argv[0]);
		return RET_FAILED;
	}

    if (index == 0)
	    pthread_create(&thid2,NULL,(void*)thread_recv,NULL);
	else
    	pthread_create(&thid1,NULL,(void*)thread_send,NULL);

    do{
        pthread_cond_signal(&cond);
    }while(1);
    
    sleep(20);
    pthread_exit(0);
    return 0;
}

