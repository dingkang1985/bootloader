#include "lwip_comm.h" 
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/init.h"
#include "ethernetif.h" 
#include "lwip/timers.h"
#include "lwip/tcp_impl.h"
#include "lwip/ip_frag.h"
#include "lwip/tcpip.h" 
#include "malloc.h"
#include "delay.h"
#include "usart.h"  
#include <stdio.h>
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//lwip通用驱动 代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2014/8/15
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
//*******************************************************************************
//修改信息
//无
////////////////////////////////////////////////////////////////////////////////// 	   
   
__lwip_dev lwipdev;						//lwip控制结构体 
struct netif lwip_netif;				//定义一个全局的网络接口

extern u32 memp_get_memorysize(void);	//在memp.c里面定义
extern u8_t *memp_memory;				//在memp.c里面定义.
extern u8_t *ram_heap;					//在mem.c里面定义.

u32 TCPTimer=0;			//TCP查询计时器
u32 ARPTimer=0;			//ARP查询计时器
u32 lwip_localtime;		//lwip本地时间计数器,单位:ms



//lwip中mem和memp的内存申请
//返回值:0,成功;
//    其他,失败
u8 lwip_comm_mem_malloc(void)
{
	u32 mempsize;
	u32 ramheapsize; 
	mempsize=memp_get_memorysize();			//得到memp_memory数组大小
	memp_memory=mymalloc(SRAMIN,mempsize);	//为memp_memory申请内存
	ramheapsize=LWIP_MEM_ALIGN_SIZE(MEM_SIZE)+2*LWIP_MEM_ALIGN_SIZE(4*3)+MEM_ALIGNMENT;//得到ram heap大小
	ram_heap=mymalloc(SRAMIN,ramheapsize);	//为ram_heap申请内存 
	if(!memp_memory||!ram_heap)//有申请失败的
	{
		lwip_comm_mem_free();
		return 1;
	}
	return 0;	
}
//lwip中mem和memp内存释放
void lwip_comm_mem_free(void)
{ 	
	myfree(SRAMIN,memp_memory);
	myfree(SRAMIN,ram_heap);
}
//lwip 默认IP设置
//lwipx:lwip控制结构体指针
void lwip_comm_default_ip_set(__lwip_dev *lwipx)
{
	u32 sn0;
	sn0=*(vu32*)(0x1FFF7A10);//获取STM32的唯一ID的前24位作为MAC地址后三字节
	//MAC地址设置(高三字节固定为:2.0.0,低三字节用STM32唯一ID)
	lwipx->mac[0]=2;//高三字节(IEEE称之为组织唯一ID,OUI)地址固定为:2.0.0
	lwipx->mac[1]=0;
	lwipx->mac[2]=0;
	lwipx->mac[3]=(sn0>>16)&0XFF;//低三字节用STM32的唯一ID
	lwipx->mac[4]=(sn0>>8)&0XFFF;;
	lwipx->mac[5]=sn0&0XFF; 
	//默认本地IP为:192.168.1.30
	lwipx->ip[0]=192;	
	lwipx->ip[1]=168;
	lwipx->ip[2]=1;
	lwipx->ip[3]=30;
	//默认子网掩码:255.255.255.0
	lwipx->netmask[0]=255;	
	lwipx->netmask[1]=255;
	lwipx->netmask[2]=255;
	lwipx->netmask[3]=0;
	//默认网关:192.168.1.1
	lwipx->gateway[0]=192;	
	lwipx->gateway[1]=168;
	lwipx->gateway[2]=1;
	lwipx->gateway[3]=1;	
} 

//LWIP初始化(LWIP启动的时候使用)
//返回值:0,成功
//      1,内存错误
//      2,LAN8720初始化失败
//      3,网卡添加失败.
u8 lwip_comm_init(void)
{
	struct netif *Netif_Init_Flag;		//调用netif_add()函数时的返回值,用于判断网络初始化是否成功
	struct ip_addr ipaddr;  			//ip地址
	struct ip_addr netmask; 			//子网掩码
	struct ip_addr gw;      			//默认网关 
	if(ETH_Mem_Malloc())return 1;		//内存申请失败
	if(lwip_comm_mem_malloc())return 1;	//内存申请失败
	if(LAN8720_Init())return 2;			//初始化LAN8720失败 
	lwip_init();						//初始化LWIP内核
	lwip_comm_default_ip_set(&lwipdev);	//设置默认IP等信息

			//使用静态IP
	IP4_ADDR(&ipaddr,lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);
	IP4_ADDR(&netmask,lwipdev.netmask[0],lwipdev.netmask[1] ,lwipdev.netmask[2],lwipdev.netmask[3]);
	IP4_ADDR(&gw,lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);
	
	#if 0
	printf("网卡en的MAC地址为:................%d.%d.%d.%d.%d.%d\r\n",lwipdev.mac[0],lwipdev.mac[1],lwipdev.mac[2],lwipdev.mac[3],lwipdev.mac[4],lwipdev.mac[5]);
	printf("静态IP地址........................%d.%d.%d.%d\r\n",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);
	printf("子网掩码..........................%d.%d.%d.%d\r\n",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);
	printf("默认网关..........................%d.%d.%d.%d\r\n",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);
    #endif
	
	
	Netif_Init_Flag=netif_add(&lwip_netif,&ipaddr,&netmask,&gw,NULL,&ethernetif_init,&ethernet_input);//向网卡列表中添加一个网口
	

	
	if(Netif_Init_Flag==NULL)return 3;//网卡添加失败 
	else//网口添加成功后,设置netif为默认值,并且打开netif网口
	{
		netif_set_default(&lwip_netif); //设置netif为默认网口
		netif_set_up(&lwip_netif);		//打开netif网口
	}
	return 0;//操作OK.
}   

//当接收到数据后调用 
void lwip_pkt_handle(void)
{
  //从网络缓冲区中读取接收到的数据包并将其发送给LWIP处理 
 ethernetif_input(&lwip_netif);
}

//LWIP轮询任务
void lwip_periodic_handle()
{

  //ARP每5s周期性调用一次
  if ((lwip_localtime - ARPTimer) >= ARP_TMR_INTERVAL)
  {
    ARPTimer =  lwip_localtime;
    etharp_tmr();
  }


}






























