#include <vita2d.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <psp2/ctrl.h>
#include <psp2/types.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/touch.h>
#include <psp2/motion.h>
#include <psp2/kernel/threadmgr.h>

#define GAMEPAD_PORT  5000
#define NET_INIT_SIZE 1*1024*1024

#define PAD_PACKET_MODE     0
#define EXT_PAD_PACKET_MODE 1

// PadPacket struct
typedef struct {
	uint32_t buttons;
	uint8_t lx;
	uint8_t ly;
	uint8_t rx;
	uint8_t ry;
	uint16_t tx;
	uint16_t ty;
	float gx;
	float gy;
	float gz;
	float ax;
	float ay;
	float az;
	uint8_t click;
} PadPacket;

// ExtPadPacket struct
typedef struct {
	SceCtrlData pad;
	SceTouchData front;
	SceTouchData retro;
	SceMotionState motion_state;
} ExtPadPacket;

// Values for click value
#define NO_INPUT 0x00
#define MOUSE_MOV 0x01
#define LEFT_CLICK 0x08
#define RIGHT_CLICK 0x10

static uint8_t mode = PAD_PACKET_MODE;

// Server thread
volatile int connected = 0;


static int server_thread(unsigned int args, void* argp){
	
	// Initializing a PadPacke 
	PadPacket pkg;
    ExtPadPacket ext_pkg;

	// Initializing a socket
	int fd = sceNetSocket("VitaPad", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	SceNetSockaddrIn serveraddr;
	serveraddr.sin_family = SCE_NET_AF_INET;
	serveraddr.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);
	serveraddr.sin_port = sceNetHtons(GAMEPAD_PORT);
	sceNetBind(fd, (SceNetSockaddr *)&serveraddr, sizeof(serveraddr));


	sceNetListen(fd, 128);
	sceMotionGetAngleThreshold();
	sceMotionSetDeadband(0);
	sceMotionSetTiltCorrection(0);

	for (;;){
		SceNetSockaddrIn clientaddr;
		unsigned int addrlen = sizeof(clientaddr);
		clientaddr.sin_addr.s_addr = inet_addr("192.168.0.38");
		clientaddr.sin_family = SCE_NET_AF_INET;
		clientaddr.sin_port = sceNetHtons(5000);

		// int client = sceNetAccept(fd, (SceNetSockaddr *)&clientaddr, &addrlen);
		int err = sceNetConnect(fd, (SceNetSockaddr *)&clientaddr, sizeof(SceNetSockaddr));
		// if (client >= 0) {
			connected = 1;
			// char unused[8];
			for (;;){
				// sceNetRecv(client,unused,256,0);
				sceCtrlPeekBufferPositive(0, &ext_pkg.pad, 1);
				sceTouchPeek(SCE_TOUCH_PORT_FRONT, &ext_pkg.front, 1);
				sceTouchPeek(SCE_TOUCH_PORT_BACK, &ext_pkg.retro, 1);
				sceMotionGetState(&ext_pkg.motion_state);
				switch (mode) {
				case PAD_PACKET_MODE:
					memcpy(&pkg, &ext_pkg.pad.buttons, 8); // Buttons + analogs state
					memcpy(&pkg.tx, &ext_pkg.front.report[0].x, 4); // Touch state
					memcpy(&pkg.gx, &ext_pkg.motion_state.angularVelocity.x, 4); // Gyro state
					memcpy(&pkg.gy, &ext_pkg.motion_state.angularVelocity.y, 4); // Gyro state
					memcpy(&pkg.gz, &ext_pkg.motion_state.angularVelocity.z, 4); // Gyro state
					memcpy(&pkg.ax, &ext_pkg.motion_state.acceleration.x, 12); // Accel state
					// printf("Ang.Velocity <x:%+1.3f y:%+1.3f z:%+1.3f>     \n",state.angularVelocity.x, state.angularVelocity.y, state.angularVelocity.z);
					uint8_t flags = NO_INPUT;
					if (ext_pkg.front.reportNum > 0) flags += MOUSE_MOV;
					if (ext_pkg.retro.reportNum > 0){
						if (ext_pkg.retro.report[0].x > 960) flags += RIGHT_CLICK;
						else flags += LEFT_CLICK;
					}
					pkg.click = flags;
					// sceNetSendto(fd, &pkg, sizeof(PadPacket), SCE_NET_MSG_DONTWAIT, (SceNetSockaddr *)&clientaddr, sizeof(SceNetSockaddr)); // Sending PadPacket
					sceNetSend(fd, &pkg, sizeof(PadPacket), SCE_NET_MSG_DONTWAIT);
					break;
				case EXT_PAD_PACKET_MODE:
					sceNetSendto(fd, &ext_pkg, sizeof(ExtPadPacket), SCE_NET_MSG_DONTWAIT, (SceNetSockaddr *)&clientaddr, sizeof(SceNetSockaddr)); // Sending ExtPadPacket
					break;
				default:
					break;
				}
			}
		// }
	}
	
	return 0;
}

vita2d_pgf* debug_font;
uint32_t text_color;

int main(){
	
	// Enabling analog, touch and motion support
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, 1);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, 1);
	sceMotionStartSampling();
	
	// Initializing graphics stuffs
	vita2d_init();
	vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
	debug_font = vita2d_load_default_pgf();
	uint32_t text_color = RGBA8(0xFF, 0xFF, 0xFF, 0xFF);
	
	// Initializing network stuffs
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	char vita_ip[32];
	int ret = sceNetShowNetstat();
	if (ret == SCE_NET_ERROR_ENOTINIT) {
		SceNetInitParam initparam;
		initparam.memory = malloc(NET_INIT_SIZE);
		initparam.size = NET_INIT_SIZE;
		initparam.flags = 0;
		ret=sceNetInit(&initparam);
	}
	ret = sceNetCtlInit();
	SceNetCtlInfo info;
	ret=sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info);
	sprintf(vita_ip,"%s",info.ip_address);
	SceNetInAddr vita_addr;
	sceNetInetPton(SCE_NET_AF_INET, info.ip_address, &vita_addr);
	
	// Starting server thread
	SceUID thread = sceKernelCreateThread("VitaPad Thread",&server_thread, 0x10000100, 0x10000, 0, 0, NULL);
	sceKernelStartThread(thread, 0, NULL);
	
	uint32_t oldpad = SCE_CTRL_CROSS;
	for (;;){
		
		vita2d_start_drawing();
		vita2d_clear_screen();
		vita2d_pgf_draw_text(debug_font, 2, 20, text_color, 1.0, "VitaPad v.1.3 by Rinnegatamante");
		vita2d_pgf_draw_textf(debug_font, 2, 60, text_color, 1.0, "Listening on:\nIP: %s\nPort: %d",vita_ip,GAMEPAD_PORT);
		//vita2d_pgf_draw_textf(debug_font, 2, 140, text_color, 1.0, "Mode: %s\nPress X to change mode", mode ? "Vita to PSTV" : "Vita to PC");
		vita2d_pgf_draw_textf(debug_font, 2, 200, text_color, 1.0, "Status: %s",connected ? "Connected!" : "Waiting connection...");
		vita2d_pgf_draw_textf(debug_font, 2, 300, text_color, 1.0, "Thanks to MakiseKurisu & yuntiancherry for the ViGEm client support");
		vita2d_pgf_draw_textf(debug_font, 2, 320, text_color, 1.0, "Thanks to Evengard for the vJoy client support");
		vita2d_pgf_draw_textf(debug_font, 2, 340, text_color, 1.0, "Thanks to nyorem for the Linux client port");
		vita2d_pgf_draw_textf(debug_font, 2, 380, text_color, 1.0, "Thanks to my distinguished Patroners for their awesome support:");
		vita2d_pgf_draw_textf(debug_font, 2, 400, text_color, 1.0, "@Sarkies_Proxy - ArkSource - Freddy Parra");
		vita2d_pgf_draw_textf(debug_font, 2, 420, text_color, 1.0, "RaveHeart - Tain Sueiras - drd7of14 - psymu");
		// vita2d_pgf_draw_textf(debug_font, 2, 440, text_color, 1.0, "Ang.Velocity <x:%+1.3f y:%+1.3f z:%+1.3f>     \n",pkg.gx, pkg.gy,pkg.gz);
		// vita2d_pgf_draw_textf(debug_font, 2, 460, text_color, 1.0, "Bytes sent %d         \n",bytes_sent);
		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		vita2d_swap_buffers();
		
		/*if (!connected) {
			SceCtrlData pad;
			sceCtrlPeekBufferPositive(0, &pad, 1);
			if ((pad.buttons & SCE_CTRL_CROSS) && (!(oldpad & SCE_CTRL_CROSS))) mode = (mode + 1) % 2;
			oldpad = pad.buttons;
		}*/
		
	}
	
}