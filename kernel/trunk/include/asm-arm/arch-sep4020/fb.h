//#define CLKPOL		(1 << 21)		//LCD 移位时钟极性—设置LCD 移位时钟的有效沿的极性,下降延有效. 10.27 修正为1


#ifndef __ASM_ARCH_FB_H
#define __ASM_ARCH_FB_H


#define LCDWIDTH	640
#define LCDHEIGHT	480

//Macro for SIZE register setting
#define XMAX		((LCDWIDTH/16)	<< 20)
#define YMAX		(LCDHEIGHT)

//Macro for PCR register setting
#define TFT		(1 << 31)
#define COLOR		(1 << 30)		//1为色彩.
#define PBSIZE		(0 << 28)
#define BPIX		(4 << 25)		//存储器中一个像素对应16bit
#define PIXPOL		(0 << 24)		//像素极性—设置像素的极性,高有效
#define FLMPOL		(1 << 23)		//首行标志极性—设置首行标志的极性,低有效.
#define LPPOL		(1 << 22)		//行脉冲极性—设置行脉冲信号的极性,低有效.
#define CLKPOL		(1 << 21)		//LCD 移位时钟极性—设置LCD 移位时钟的有效沿的极性,下降延有效. 1025 修正为1
#define OEPOL		(0 << 20)		//输出使能极性—设置输出使能信号的极性,高有效.
#define END_SEL	(0 << 18)		//印第安格式选择,小印第安.
#define ACD_SEL	(0 << 15)		//ACD时钟源选择—选择晶向变换计数器的时钟,使用FLM 作为ACD 计数时钟
#define ACD		(0 << 8 )		//被动模式.
#define PCD		(8)

//For the 640*480
//Macro for HCR regiter setting	//水平配置寄存器.
#define H_WIDTH	(50 << 26)
#define H_WAIT_1	(15 << 8)
#define H_WAIT_2	(102)




//For the 640*480
//Macro for VCR register setting	//垂直配置寄存器.
#define V_WIDTH	(3 << 26)
#define PASS_FRAME_WAIT	(0 <<16)
#define V_WAIT_1	(10 << 8)
#define V_WAIT_2	(30)


//Macro for LGPR register setting
#define GRAY		(4)			//灰度等级.

//Macro for PWMR register setting
#define SCR		(0 << 9)		//时钟源选择.行脉冲.
#define CC_EN		(0 << 8)		//对比度控制使能,关.
#define PW		(64)			//输出脉冲数目,

//Macro for DMACR register setting
#define BL		(1 << 31)		//固定长度.
#define HM		(7 << 16)
#define TM		(7)			//modified to 7 @ SDK3.2

#endif


