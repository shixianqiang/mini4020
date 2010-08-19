/**
 * @file	s1r72xxx-hcd.c
 * @brief	S1R72xxx HCD (Host Controller Driver) for USB
 * @date	2007/04/17
 * @author	Luxun9 project team
 * Copyright (C)SEIKO EPSON CORPORATION. 2006-2007 All rights reserved.
 * This file is licenced under the GPLv2.
 *
 */

#include "s1r72xxx-hcd.h"
#include <linux/platform_device.h>

/******************************************
 * Declarations of function prototype
 ******************************************/
static irqreturn_t s1r72xxx_hcd_irq(struct usb_hcd *hcd,
	struct pt_regs *regs);
static int s1r72xxx_hcd_reset(struct usb_hcd *hcd);
static int s1r72xxx_hcd_start(struct usb_hcd *hcd);
static int s1r72xxx_hcd_suspend(struct usb_hcd *hcd, pm_message_t msg);
static void s1r72xxx_hcd_stop(struct usb_hcd *hcd);
static int s1r72xxx_hcd_resume(struct usb_hcd *hcd);
static int s1r72xxx_hcd_get_frame(struct usb_hcd *hcd);
static void s1r72xxx_hcd_endpoint_disable(struct usb_hcd *hcd,
	struct usb_host_endpoint *ep);
static int s1r72xxx_hcd_sys_suspend(struct device *dev, pm_message_t msg
	, u32 level);
static int s1r72xxx_hcd_sys_resume(struct device *dev, u32 level);

unsigned char port_control(struct usb_hcd *hcd, unsigned char cmd,
	unsigned char setval);
static unsigned char sie0_irq_handler(struct usb_hcd *hcd,
	struct hcd_priv *priv, unsigned char intstat);
static void set_connect_flags(struct usb_hcd *hcd, struct hcd_priv *);
static void set_disconnect_flags(struct usb_hcd *hcd, struct hcd_priv *priv);
static unsigned char sie1_irq_handler(struct usb_hcd *hcd,
	struct hcd_priv *priv, unsigned char intstat);
static unsigned char frame_irq_handler(struct usb_hcd *hcd,
	struct hcd_priv *priv, unsigned char intstat);
static unsigned char chx_irq_handler(struct usb_hcd *hcd,
	struct hcd_priv *priv, unsigned char ch, unsigned char intstat);
static unsigned char chg_cond_irq_handler(struct usb_hcd *hcd,
	struct hcd_priv *priv, unsigned char ch, unsigned char intstat,
	unsigned char condition);
static unsigned char totalsizecmp_irq_handler(struct usb_hcd *hcd,
	struct hcd_priv *priv, unsigned char ch);

void host_start(struct usb_hcd *hcd);
void hc_suspend(struct usb_hcd *hcd);
void hc_resume(struct usb_hcd *hcd);

static int s1r72xxx_hcd_enqueue(struct usb_hcd *hcd,
	struct usb_host_endpoint *ep, struct urb *urb, unsigned mflag);
static int s1r72xxx_hcd_dequeue(struct usb_hcd *hcd, struct urb *urb);

static int submit_control(struct usb_hcd *,
	struct usb_host_endpoint *, unsigned);
static int submit_bulk_intr(int,struct usb_hcd *,
	struct usb_host_endpoint *, struct urb *, unsigned);
static void free_ch_resource(struct usb_hcd *hcd, S_R_HC_CH *ch_info,
	int status);
static void complete_urb(struct usb_hcd *hcd, S_R_HCD_QUEUE *hcd_queue,
	S_R_HC_CH *ch_info, struct urb *urb, int status_flag);

static int s1r72xxx_hcd_hub_status(struct usb_hcd *hcd, char *buf);
static int s1r72xxx_hcd_hub_control(struct usb_hcd* hcd, u16 treq, u16 value,
	u16 index, char *buf, u16 len);
static int s1r72xxx_hcd_hub_suspend(struct usb_hcd *hcd);
static int s1r72xxx_hcd_hub_resume(struct usb_hcd *hcd);
static void s1r72xxx_init_virtual_root_hub(struct hcd_priv *priv);
static void init_ch_info(struct hcd_priv *hcd);
static void init_lsi(void);
static void init_fifo(void);

/* debub log string table (/proc/driver/udc) */
#ifdef DEBUG_PROC
#include <linux/seq_file.h>

static const char proc_filename[] = "driver/hcd";
struct hcd_priv *temp_hcd;
struct usb_hcd *temp_usb_hcd;

#define PROC_HCD(x, y)	temp_hcd = y;\
			temp_usb_hcd = x

#define S_R_DEBUG_TABLE_SIZE 3000	

S_R_HCD_QUEUE_DEBUG	queue_dbg_tbl[S_R_DEBUG_TABLE_SIZE];

/* debub log string table (/proc/driver/udc) */
S_R_HCD_QUEUE_DEBUG_TITLE dbg_name_tbl[S_R_DEBUG_MAX]= {
	{"enqueue  ","val1=","val2="},
	{"dequeue  ","val1=","val2="},
	{"reset    ","val1=","val2="},
	{"start    ","val1=","val2="},
	{"suspend  ","val1=","val2="},
	{"resume   ","val1=","val2="},
	{"sysuspend","val1=","val2="},
	{"sysresume","val1=","val2="},
	{"stop     ","val1=","val2="},
	{"epdisable","val1=","val2="},
	{"hub cont ","val1=","val2="},
	{"irq      ","val1=","val2="},
	{"register ","addr=","  at="},
	{"submit   ","val1=","val2="},
	{"out      ","val1=","val2="},
	{"in       ","val1=","val2="},
	{"complete ","val1=","val2="},
	{"error    ","func=","val ="},
	{"others   ","val1=","val2="}
};
unsigned int queue_dbg_counter = 0;
unsigned int queue_dbg_carry = 0;
unsigned long queue_seq_no = 0;
void s1r72xxx_queue_log(unsigned char	func_name,
		unsigned int	data1,
		unsigned int	data2)
{
	queue_dbg_tbl[queue_dbg_counter].func_name = func_name;
	queue_dbg_tbl[queue_dbg_counter].data1 = data1;
	queue_dbg_tbl[queue_dbg_counter].data2 = data2;

	queue_dbg_counter++;
	if (queue_dbg_counter > S_R_DEBUG_TABLE_SIZE - 1){
		queue_dbg_counter = 0;
		queue_dbg_carry = 1;
	}
}
unsigned char test_toggle = 1;

static int proc_udc_show(struct seq_file *s , void * a)
{
	unsigned int i;
	unsigned long flags;
#ifdef S_R_SUSPEND_TEST
	pm_message_t pm_msg;
#endif /* S_R_SUSPEND_TEST */

	spin_lock_irqsave(&temp_hcd->lock, flags);

#ifdef S_R_SUSPEND_TEST
	if (test_toggle){
		s1r72xxx_hcd_suspend(temp_usb_hcd,
			pm_msg);
		test_toggle = 0;
	} else {
		s1r72xxx_hcd_resume(temp_usb_hcd);
		test_toggle = 1;
	}
#endif /* S_R_SUSPEND_TEST */

	seq_printf(s,"Address: +00 +01 +02 +03 +04 +05 +06 +07");
	seq_printf(s," +08 +09 +0A +0B +0C +0D +0E +0F\n");
	for (i = 0 ; i < 0x200 ; i += 16){
		switch( i ){
//		case 0x020:
		case 0x030:
		case 0x040:
		case 0x050:
		case 0x060:
//		case 0x070:
//		case 0x080:
//		case 0x090:
//		case 0x0A0:
			break;
		default:
			seq_printf(s,
				"0x%04X :  %02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x"
				"  %02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x\n",
				i,
				S_R_REGS8(i     ), S_R_REGS8(i + 1 ),
				S_R_REGS8(i + 2 ), S_R_REGS8(i + 3 ),
				S_R_REGS8(i + 4 ), S_R_REGS8(i + 5 ),
				S_R_REGS8(i + 6 ), S_R_REGS8(i + 7 ),
				S_R_REGS8(i + 8 ), S_R_REGS8(i + 9 ),
				S_R_REGS8(i + 10), S_R_REGS8(i + 11),
				S_R_REGS8(i + 12), S_R_REGS8(i + 13),
				S_R_REGS8(i + 14), S_R_REGS8(i + 15));
		}
	}
#ifdef DEBUG_PROC_LOG
	if ( queue_dbg_carry != 0){
		i = queue_dbg_counter + 1;
	} else {
		i = 0;
	}

	while (i != queue_dbg_counter ){
		if (i > S_R_DEBUG_TABLE_SIZE - 1){
			i = 0;
		}
		seq_printf(s, "%s: %s%02x: %s%02x\n",
				dbg_name_tbl[queue_dbg_tbl[i].func_name].action_string,
				dbg_name_tbl[queue_dbg_tbl[i].func_name].target_string,
				queue_dbg_tbl[i].data1,
				dbg_name_tbl[queue_dbg_tbl[i].func_name].value_string,
				queue_dbg_tbl[i].data2);
		++i;
	}
#endif /* DEBUG_PROC_LOG */

	spin_unlock_irqrestore(&temp_hcd->lock, flags);

	return 0;
}

static int proc_udc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_udc_show, NULL);
}

/**
 * @struct	proc_ops
 * @brief	for proc file
 */
static struct file_operations proc_ops = {
	.open           = proc_udc_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

inline static void create_proc_file(void)
{
	struct proc_dir_entry *pde;

	pde = create_proc_entry (proc_filename, 0, NULL);
	if (pde)
		pde->proc_fops = &proc_ops;
}

inline static void remove_proc_file(void)
{
	remove_proc_entry(proc_filename, NULL);
}

#else	/* DEBUG_PROC */
#define PROC_HCD(x, y)
#define create_proc_file(x)
#define remove_proc_file(x)
#define s1r72xxx_queue_log(x,y,z)
#endif	/* DEBUG_PROC */

/**
 * @brief	- interrupt
 * @par		usage:
 *				interrupt handler called by kernel
 * @param	*hcd;	struct of usb_hcd
 * @param	*regs;	struct of pt_regs
 * @retval	return IRQ_HANDLED. if interrupt could not handle, return IRQ_NONE.
 */
static irqreturn_t s1r72xxx_hcd_irq(struct usb_hcd *hcd, struct pt_regs *regs)
{
		
	unsigned char	ch;
	struct hcd_priv	*priv = (struct hcd_priv *)hcd->hcd_priv[0];
	unsigned char	ismain;
	unsigned char	ishost;
	unsigned char	intstat;
	unsigned long	lock_flags;
	//printk("now irq handler\n");
	disable_irq(INTSRC_EXTINT5);
	spin_lock_irqsave(&priv->lock, lock_flags);
	*(volatile unsigned long*)GPIO_PORTA_INTRCLR_V |= 1 << 5 ;    //清除中断
      *(volatile unsigned long*)GPIO_PORTA_INTRCLR_V = 0x0000;     //清除中断
	priv->pregs = regs;

	/**
	 * - 1. Read interrupt source:
	 *  - read MainIntStat and HostIntStat.
	 */
	ismain = S_R_REGS8(rcC_MainIntStat) & (ValidMainIntStat);
	ishost = S_R_REGS8(rcC_HostIntStat) & (ValidHostIntStat);

	if (!ismain){
		spin_unlock_irqrestore(&priv->lock, lock_flags);
		enable_irq(INTSRC_EXTINT5);
		return IRQ_HANDLED;
		//return IRQ_NONE;
	}

	/**
	 * - 2. Handle interrupt:
	 *  - continue until interrupt sources are cleared.
	 */
	priv->called_state = S_R_CALLED_FROM_OTH;
	while (ismain){
		DPRINT(DBG_IRQ,"%s:main 0x%02x\n",__FUNCTION__, ismain);
		DPRINT(DBG_IRQ,"%s:host 0x%02x\n",__FUNCTION__, ishost);
		s1r72xxx_queue_log(S_R_DEBUG_IRQ, ismain, ishost);

		/**
		 * - 2.1. Handle FinishedPM interrupt:
		 *  - this interrupt is caused power management. clear interrupt and 
		 *    set state values.
		 */
		if (ismain & FinishedPM) {
			S_R_REGS8(rcC_MainIntStat) = FinishedPM;
			S_R_REGS8(rcC_MainIntEnb) &= ~FinishedPM;
			if ( ((S_R_REGS8(rc_PM_Control_State) & PM_StateMask) == (SLEEP))
#if defined(CONFIG_USB_V05_HCD) || defined(CONFIG_USB_V05_HCD_MODULE)
				|| ((S_R_REGS8(rc_PM_Control_State) & PM_StateMask)
					== (ACTIVE60)) ){
#else
				) {
#endif
				priv->hw_state	= S_R_HW_SLEEP;
				hcd->state		= HC_STATE_HALT;
			} else if ( (S_R_REGS8(rc_PM_Control_State) & PM_StateMask)
				== ACT_HOST ){
				init_fifo();
				host_start(hcd);
				priv->hw_state	= S_R_HW_ACT_HOST;
				hcd->state		= HC_STATE_RUNNING;
			}
		}

		/**
		 * - 2.2. Handle HostIntStat interrupt:
		 *  - this interrupt is caused USB host event. clear interrupt and 
		 *    call functions.
		 */
		if (ismain & HostIntStat) {
			/**
			 * - 2.2.1. VBUS Err interrupt:
			 */
			if (ishost & H_VBUS_Err) {
				/* interrupt clear */
				S_R_REGS8(rcC_HostIntStat) = H_VBUS_Err;

				/* set port to IDLE */
				port_control(hcd, S_R_PORT_CNT_POW_OFF, GoIDLE);

				/* set port/hub status bit */
				priv->rh.port_status.wPortStatus |= USB_PORT_FEAT_OVER_CURRENT;
				priv->rh.port_status.wPortStatus &= ~USB_PORT_FEAT_POWER;
				priv->rh.hub_status.wHubStatus |= HUB_STATUS_OVERCURRENT;

				/* set port/hub change bit */
				priv->rh.port_status.wPortChange
					|= USB_PORT_STAT_C_OVERCURRENT;
				priv->rh.hub_status.wHubChange |= HUB_CHANGE_OVERCURRENT;

				priv->rh.wPortTrigger = RH_PORT_STATUS_CHANGED;

				/* start overcurrent polling timer */
				mod_timer(&priv->oc_timer, S1R72_OVER_CURRENT_TIMEOUT);
				priv->oc_timer_state = S1R72_OVER_CURRENT_TIMER_ACT;
			}

			/**
			 * - 2.2.2. Connect or Disconnect interrupt:
			 */
			if (ishost & H_SIE_IntStat_0) {
				intstat = S_R_REGS8(rcH_SIE_IntStat_0);

				sie0_irq_handler(hcd, priv,	intstat);
			}

			/**
			 * - 2.2.3. Host state changed interrupt:
			 */
			if (ishost & H_SIE_IntStat_1) {
				intstat = S_R_REGS8(rcH_SIE_IntStat_1);

				sie1_irq_handler(hcd, priv, intstat);
			}

			/**
			 * - 2.2.4. Host state changed interrupt:
			 */
			if (ishost & H_FrameIntStat) {
				intstat = S_R_REGS8(rcH_Frame_IntStat);

				frame_irq_handler(hcd, priv, intstat);
			}

			/**
			 * - 2.2.5. CH0 interrupt:
			 */
			if (ishost & H_CH0IntStat) {
				intstat = S_R_REGS8(rcH_CH0_IntStat);

					chx_irq_handler(hcd, priv, CH_0, intstat);
			}
			
			/**
			 * - 2.2.6. CHr interrupt:
			 */
			if (ishost & H_CHrIntStat) {
				intstat = S_R_REGS8(rcH_CHr_IntStat);
				
				/* serch CH number */
				for (ch = CH_A ; ch < CH_MAX ; ++ch){
					if (GET_CHR_NUM(intstat, ch)){
						break;
					}
				}
				if (ch < CH_MAX){
					intstat = S_R_REGS8(priv->ch_info[ch].CHxIntStat);
					chx_irq_handler(hcd, priv, ch, intstat);
				}
			}
		}

		/**
		 * - 3. Read interrupt source again:
		 *  - read MainIntStat and HostIntStat.
		 */
		ismain = S_R_REGS8(rcC_MainIntStat) & (ValidMainIntStat);
		ishost = S_R_REGS8(rcC_HostIntStat) & (ValidHostIntStat);

		DPRINT(DBG_IRQ,"%s:main 0x%02x\n",__FUNCTION__, ismain);
	}

	priv->called_state = S_R_CALLED_FROM_API;

	spin_unlock_irqrestore(&priv->lock, lock_flags);
	enable_irq(INTSRC_EXTINT5);
	DPRINT(DBG_IRQ,"%s:exit\n",__FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_IRQ, ismain, 0xFF);

	return IRQ_HANDLED;
}

/**
 * @brief	- Host port control.
 * @param	*hcd;	struct of usb_hcd
 * @param	cmd;	read or write command
 * @param	setval;	value set to register
 * @retval	read value or 0.
 */
unsigned char port_control(struct usb_hcd *hcd, unsigned char cmd,
	unsigned char setval)
{
	struct hcd_priv* priv
		= (struct hcd_priv*)hcd->hcd_priv[0];/* private data */
	unsigned char	tmpval = 0;				/* register value */
	unsigned char	retval = 0;				/* return value */
	unsigned char	wait_ct = 0;			/* wait counter */

	/*
	 * - 1. check AutoMode is not completed:
	 */
	if (priv->automode_flag != AUTO_NOT_USED){
		/**
		 * - 1.1. auto mode cancel:
		 */
		S_R_REGS8(rcH_NegoControl_0) = AutoModeCancel | GoCANCEL;

		while(S_R_REGS8(rcH_NegoControl_0) & AutoModeCancel){
			if (wait_ct > S_R_PORT_CNT_AUTO_CANCEL_WAIT){
				break;
			}
			wait_ct++;
		}

	}

	/*
	 * - 2. set AutoMode:
	 */
	switch(cmd){
	case S_R_PORT_CNT_REG_WRITE:
		/**
		 * - 2.1. register write:
		 */
		priv->automode_flag = automode_cmp_tbl[setval];
		tmpval = S_R_REGS8(rcH_NegoControl_0) & AutoModeCancel;
		S_R_REGS8(rcH_NegoControl_0) = setval | tmpval;
		break;

	case S_R_PORT_CNT_REG_READ:
		/**
		 * - 2.2. register read:
		 */
		retval = S_R_REGS8(rcH_NegoControl_0) 
			& (AutoModeCancel | HostStateMask);
		break;

	case S_R_PORT_CNT_POW_OFF:
		/**
		 * - 2.3. power off the port:
		 */
		S_R_REGS8(rcC_HostIntEnb) &= ~H_VBUS_Err;
		S_R_REGS8(rcH_NegoControl_0) = AutoModeCancel | GoCANCEL;

		/* wait over 6 clock at 60MHz (hardware spec)*/
		while(S_R_REGS8(rcH_NegoControl_0) & AutoModeCancel){
			if (wait_ct > S_R_PORT_CNT_POW_OFF_WAIT){
				break;
			}
			wait_ct++;
		}

		if (wait_ct < S_R_PORT_CNT_POW_OFF_WAIT + 1){
			/**
			 * - 3.1. set wPortStatus and wPortChange:
			 */
			S_R_REGS8(rcH_NegoControl_0) = GoIDLE;
			priv->rh.port_status.wPortStatus = 0;
			priv->rh.port_status.wPortChange = 0;
			priv->rh.wPortTrigger = RH_PORT_STATUS_CHANGED;
		}
		break;

	case S_R_PORT_CNT_AUTO_CANCEL:
		/**
		 * - 2.4. auto mode cancel:
		 */
		S_R_REGS8(rcH_NegoControl_0) = AutoModeCancel | GoCANCEL;

		while(S_R_REGS8(rcH_NegoControl_0) & AutoModeCancel){
			if (wait_ct > S_R_PORT_CNT_AUTO_CANCEL_WAIT){
				break;
			}
			wait_ct++;
		}
		break;
	default:
		break;
	}
	
	return retval;
}

/**
 * @brief	- process SIE_IntStat_0 interrupt
 * @param	*hcd;		usb_hcd structure
 * @param	*priv;		private area of HCD
 * @param	intstat;	interrupt source
 * @return	interrupt status to be cleared.
 */
static unsigned char sie0_irq_handler(struct usb_hcd *hcd,
	struct hcd_priv *priv, unsigned char intstat)
{
	unsigned char retval = 0;

	DPRINT(DBG_IRQ,"%s:enter\n", __FUNCTION__);

	/**
	 * - 1. device connected:
	 */
	if (intstat & DetectCon) {
		DPRINT(DBG_IRQ,"%s:DetectCon\n",__FUNCTION__);

		retval = (DetectCon | DetectChirpOK | DetectChirpNG);

		S_R_REGS8(rcH_SIE_IntStat_0) = retval;

		/* set wPortStatus and wPortChange */
		set_connect_flags(hcd, priv);

	/**
	 * - 2. device disconnected:
	 */
	} else  if (intstat & DetectDiscon) {
		DPRINT(DBG_IRQ,"%s:DetectDiscon\n",__FUNCTION__);

		retval = (DetectDiscon  | DetectChirpOK | DetectChirpNG);

		S_R_REGS8(rcH_SIE_IntStat_0) = retval;

		/* set wPortStatus and wPortChange */
		set_disconnect_flags(hcd, priv);

	/**
	 * - 3. remote wakeup detected:
	 */
	} else  if (intstat & DetectRmtWkup) {
		DPRINT(DBG_IRQ,"%s:RemoteWakeup\n",__FUNCTION__);

		retval = (DetectDiscon  | DetectChirpOK | DetectRmtWkup);

		S_R_REGS8(rcH_SIE_IntStat_0) = retval;

		/* resume hardware */
		hc_resume(hcd);

	}


	DPRINT(DBG_IRQ,"%s:exit\n", __FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_IRQ, intstat, retval);

	return retval;
}

/**
 * @brief	- Set connected flags reports to core driver.
 * @param	*hcd;	usb_hcd structure
 * @param	*priv;	private data
 * @retval	irqreturn_t
 */
static void set_connect_flags(struct usb_hcd *hcd, struct hcd_priv *priv)
{
	/**
	 * - 1. Set status to wPortStatus and wPortChange:
	 */
	priv->rh.port_status.wPortStatus |= USB_PORT_STAT_CONNECTION;
	priv->rh.port_status.wPortChange |= USB_PORT_STAT_C_CONNECTION;
	priv->rh.wPortTrigger = RH_PORT_STATUS_CHANGED;
}

/**
 * @brief	- process of disconnect under interrupt
 * @param	*hcd;	struct of usb_hcd
 * @param	*priv;	private data
 * @retval	none;
 */
static void set_disconnect_flags(struct usb_hcd *hcd, struct hcd_priv *priv)
{
	unsigned char	ch_ct;		/* CH counter */

	/**
	 * - 1. Clear all CH informations and interrupt:
	 */
	for (ch_ct = CH_0 ; ch_ct < CH_MAX ; ch_ct++){
		free_ch_resource(hcd, &priv->ch_info[ch_ct], -ESHUTDOWN);
	}

	/**
	 * - 2. Set Automode to Wait Connect to Disable:
	 */
	port_control(hcd, S_R_PORT_CNT_REG_WRITE, GoWAIT_CONNECTtoDIS);

	/**
	 * - 3. Set status to wPortStatus and wPortChange:
	 */
	priv->rh.port_status.wPortStatus
		&= ~(USB_PORT_STAT_CONNECTION | USB_PORT_STAT_ENABLE
			| USB_PORT_STAT_LOW_SPEED | USB_PORT_STAT_HIGH_SPEED);
	priv->rh.port_status.wPortChange
		|= USB_PORT_STAT_C_CONNECTION;
	priv->rh.wPortTrigger = RH_PORT_STATUS_CHANGED;
}

/**
 * @brief	- process SIE_IntStat_1 interrupt
 * @param	*hcd;		usb_hcd structure
 * @param	*priv;		private area of HCD
 * @param	intstat;	interrupt source
 * @return	interrupt status to be cleared.
 */
static unsigned char sie1_irq_handler(struct usb_hcd *hcd,
	struct hcd_priv *priv, unsigned char intstat)
{
	unsigned char retval = 0;

	DPRINT(DBG_IRQ,"%s:enter\n", __FUNCTION__);

	/**
	 * - 1. DisabledCmp:
	 */
	if (intstat & DisabledCmp) {
		DPRINT(DBG_IRQ,"%s:DisabledCmp\n", __FUNCTION__);
		/**
		 * - 1.1. set wPortStatus:
		 */
		S_R_REGS8(rcH_SIE_IntStat_1) = DisabledCmp;
		priv->rh.port_status.wPortStatus |= USB_PORT_STAT_POWER;
		priv->rh.wPortTrigger = RH_PORT_STATUS_CHANGED;

		if (priv->automode_flag == AUTO_DISABLED_CMP){
			priv->automode_flag = AUTO_NOT_USED;
		}

		retval = DisabledCmp;

	/**
	 * - 2. ResumeCmp:
	 */
	} else if (intstat & ResumeCmp) {
		DPRINT(DBG_IRQ,"%s:ResumeCmp\n", __FUNCTION__);
		/**
		 * - 2.1. set wPortStatus and wPortChange:
		 */
		S_R_REGS8(rcH_SIE_IntStat_1) = ResumeCmp;

		priv->rh.port_status.wPortStatus |= USB_PORT_STAT_POWER;
		priv->rh.port_status.wPortStatus &= ~(USB_PORT_STAT_SUSPEND);
		priv->rh.port_status.wPortChange |= USB_PORT_STAT_C_SUSPEND;

		priv->rh.wPortTrigger = RH_PORT_STATUS_CHANGED;

		if (priv->automode_flag == AUTO_RESUME_CMP){
			priv->automode_flag = AUTO_NOT_USED;
		}

		/**
		 * - 2.2. set HCD state HC_STATE_RUNNING:
		 */
		hcd->state = HC_STATE_RUNNING;

		retval = ResumeCmp;

	/**
	 * - 3. SuspendCmp:
	 */
	} else if (intstat & SuspendCmp) {
		DPRINT(DBG_IRQ,"%s:SuspendCmp\n", __FUNCTION__);
		/**
		 * - 3.1. set wPortStatus and wPortChange:
		 */
		S_R_REGS8(rcH_SIE_IntStat_1) = SuspendCmp;

		priv->rh.port_status.wPortStatus |= USB_PORT_STAT_SUSPEND;
		priv->rh.wPortTrigger = RH_PORT_STATUS_CHANGED;

		if (priv->automode_flag == AUTO_SUSPEND_CMP){
			priv->automode_flag = AUTO_NOT_USED;
		}

		/**
		 * - 3.2. set HCD state HC_STATE_SUSPENDED:
		 */
		hcd->state = HC_STATE_SUSPENDED;

		retval = SuspendCmp;

	/**
	 * - 4. ResetCmp:
	 */
	} else if (intstat & ResetCmp) {
		DPRINT(DBG_IRQ,"%s:ResetCmp\n", __FUNCTION__);
		/**
		 * - 4.1. set wPortStatus and wPortChange:
		 */
		S_R_REGS8(rcH_SIE_IntStat_1) = ResetCmp;
		priv->rh.port_status.wPortStatus &= ~(USB_PORT_STAT_RESET);
		priv->rh.port_status.wPortStatus |= USB_PORT_STAT_ENABLE;
		priv->rh.port_status.wPortChange |= USB_PORT_STAT_C_RESET;
		priv->rh.wPortTrigger = RH_PORT_STATUS_CHANGED;

		if (priv->automode_flag == AUTO_RESET_CMP){
			priv->automode_flag = AUTO_NOT_USED;
		}
	
		retval = ResetCmp;

	}
	DPRINT(DBG_API,"%s:exit\n", __FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_IRQ, intstat, retval);

	return retval;
}

/**
 * @brief	- process FrameIntStat interrupt
 * @param	*hcd;		usb_hcd structure
 * @param	*priv;		private area of HCD
 * @param	intstat;	interrupt source
 * @return	interrupt status to be cleared.
 */
static unsigned char frame_irq_handler(struct usb_hcd *hcd,
	struct hcd_priv *priv, unsigned char intstat)
{
	unsigned char	timeout_ct = 0;
	unsigned char	dummy;
	unsigned char	retval = 0;

	DPRINT(DBG_IRQ,"%s:enter\n", __FUNCTION__);

	/**
	 * - 1. Port Err:
	 */
	if (intstat & PortErr) {
		DPRINT(DBG_IRQ,"%s:Port Err\n", __FUNCTION__);
		/**
		 * - 1.1. set GoDISABLED:
		 */
		S_R_REGS8(rcH_Frame_IntStat) = PortErr;
		if (priv->rh.port_status.wPortStatus & USB_PORT_STAT_ENABLE){
			priv->rh.port_status.wPortStatus &= ~USB_PORT_STAT_ENABLE;
			priv->rh.port_status.wPortChange |= USB_PORT_STAT_C_ENABLE;
		}

		port_control(hcd, S_R_PORT_CNT_REG_WRITE, GoDISABLED);

		while(timeout_ct < S_R_GO_DISABLED_TIMEOUT){
			if (S_R_REGS8(rcH_SIE_IntStat_1) & DisabledCmp){
				S_R_REGS8(rcH_SIE_IntStat_1) = DisabledCmp;
				break;
			}
			timeout_ct++;
		}

   		S_R_REGS8(rc_Reset) = ResetHTM;

		timeout_ct = 0;
		while (timeout_ct < S_R_PORT_ERR_RESET_TIMEOUT){
			dummy = S_R_REGS8(rc_Reset);
			timeout_ct++;
		}
		S_R_REGS8(rc_Reset) &= ~ResetHTM;
		

		retval = PortErr;
	}
	DPRINT(DBG_API,"%s:s1r72xxx_host_stat exit\n", __FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_IRQ, intstat, retval);

	return retval;
}

/**
 * @brief	- process CHxIntStat interrupt
 * @param	*hcd;		usb_hcd structure
 * @param	*priv;		private area of HCD
 * @param	ch;			CH number
 * @param	intstat;	interrupt source
 * @return	interrupt status to be cleared.
 */
static unsigned char chx_irq_handler(struct usb_hcd *hcd,
	struct hcd_priv *priv, unsigned char ch, unsigned char intstat)
{
	unsigned char chg_condition = 0;
	unsigned char retval = 0;

	DPRINT(DBG_IRQ,"%s:enter 0x%02x\n", __FUNCTION__, intstat);
	s1r72xxx_queue_log(S_R_DEBUG_IRQ, ch, intstat);

	/**
	 * - 1. check condition and act:
	 */
	/**
	 * - 1.1. condition changed
	 */
	if ( intstat & ChangeCondition ) {
		DPRINT(DBG_IRQ,"%s:ChangeCondition",__FUNCTION__);

		S_R_REGS8(priv->ch_info[ch].CHxIntStat) = ChangeCondition;

		/* read ConditionCode */
		chg_condition = S_R_REGS8(priv->ch_info[ch].CHxCondCode)
			& ConditionCode_MASK;

		s1r72xxx_queue_log(S_R_DEBUG_IRQ, intstat, chg_condition);

		/* call function */
		retval = chg_cond_irq_handler(hcd, priv, ch, intstat, chg_condition);

	/**
	 * - 1.2. TotalSizeCmp
	 */
	} else if ( intstat & TotalSizeCmp ){
		DPRINT(DBG_IRQ,"%s:TotalSizeCmp\n",__FUNCTION__);

		S_R_REGS8(priv->ch_info[ch].CHxIntStat) = TotalSizeCmp;

		/* call function */
		retval = totalsizecmp_irq_handler(hcd, priv, ch);

	} else {
		DPRINT(DBG_IRQ,"%s:Unexpected 0x%02x\n",__FUNCTION__, intstat);
		retval = intstat;
	}
	
	DPRINT(DBG_IRQ,"%s:exit\n", __FUNCTION__);

	return retval;
}

/**
 * @brief	- process Chage Condition interrupt
 * @param	*hcd;		usb_hcd structure
 * @param	*priv;		private area of HCD
 * @param	ch;		CH number
 * @param	intstat;	interrupt source
 * @param	condition;	change code condition source
 * @return	interrupt status to be cleared.
 */
static unsigned char chg_cond_irq_handler(struct usb_hcd *hcd, 
	struct hcd_priv *priv, unsigned char ch, unsigned char intstat,
	unsigned char condition)
{
	S_R_HCD_QUEUE	*hcd_queue = NULL;	/* handled urb */
	struct urb		*urb_queued;		/* queued urb */
	unsigned char	retval = 0;
	
	/**
	 * - 1. get queued urb from queue list:
	 */
	if (list_empty(&priv->ch_info[ch].urb_list)) {
		/* empty */
		DPRINT(DBG_DATA,"%s:queue list is empty\n",__FUNCTION__);
		return retval;
	} else {
		DPRINT(DBG_DATA,"%s:already queued\n",__FUNCTION__);
		hcd_queue = list_entry(priv->ch_info[ch].urb_list.next,
			S_R_HCD_QUEUE, urb_list);
		urb_queued = hcd_queue->urb;
	}

	/**
	 * - 2. check condition and act:
	 */
	switch (condition){
	case CCode_NoError:
		/**
		 * - 2.1. No Error:
		 *  - nothing to do.
		 */
		DPRINT(DBG_IRQ,":NoError\n");
		break;

	case CCode_Stall:
		/**
		 * - 2.2. Stall:
		 *  - return -EPIPE that means STALL.
		 */
		DPRINT(DBG_IRQ,":Stall\n");
		if (urb_queued != NULL){
			complete_urb(hcd, hcd_queue, &priv->ch_info[ch],
				urb_queued, -EPIPE);
		}
		if (list_empty(&priv->ch_info[ch].urb_list)) {
			/* empty */
			DPRINT(DBG_DATA,"%s:free CH%d\n",__FUNCTION__, ch);
			free_ch_resource(hcd, &priv->ch_info[ch], 0);
		}
		break;

	case CCode_OverRun:
		/**
		 * - 2.3. Over Run:
		 *  - return -EOVERFLOW that means Over Run.
		 */
		DPRINT(DBG_IRQ,":OverRun\n");
		if (intstat & TotalSizeCmp){
			retval |= (TotalSizeCmp | TranACK);
		}
		if (urb_queued != NULL){
			complete_urb(hcd, hcd_queue, &priv->ch_info[ch],
				urb_queued, -EOVERFLOW);
		}
		if (list_empty(&priv->ch_info[ch].urb_list)) {
			/* empty */
			DPRINT(DBG_DATA,"%s:free CH%d\n",__FUNCTION__, ch);
			free_ch_resource(hcd, &priv->ch_info[ch], 0);
		}
		break;

	case CCode_UnderRun:
		/**
		 * - 2.4. Under Run:
		 *  - this condition includes short packet. process is normal.
		 */
		DPRINT(DBG_IRQ,":UnderRun\n");
		if (intstat & TotalSizeCmp){
			retval |= (TotalSizeCmp | TranACK);
		}

		priv->ch_info[ch].is_short = S_R_CH_LAST_IS_SHORT;

		switch(priv->ch_info[ch].cur_tran_type){
		case S_R_CH_TR_CONTROL:
			submit_control(hcd, NULL, 0);
			if (list_empty(&priv->ch_info[ch].urb_list)) {
				/* empty */
				DPRINT(DBG_DATA,"%s:free CH%d\n",__FUNCTION__, ch);
				free_ch_resource(hcd, &priv->ch_info[ch], 0);
			} else {
				DPRINT(DBG_DATA,"%s:queue is not empty %d\n"
					,__FUNCTION__, ch);
			}
			break;
		case S_R_CH_TR_BULK:
		case S_R_CH_TR_INT:
			submit_bulk_intr(ch, hcd, NULL, NULL, 0);
			/* if urb_list is empty, free CH. */
			if (list_empty(&priv->ch_info[ch].urb_list)) {
				/* empty */
				DPRINT(DBG_DATA,"%s:free CH%d\n",__FUNCTION__, ch);
				free_ch_resource(hcd, &priv->ch_info[ch], 0);
			} else {
				DPRINT(DBG_DATA,"%s:queue is not empty %d\n"
					,__FUNCTION__, ch);
			}
			break;
		case S_R_CH_TR_ISOC:
		default:
			DPRINT(DBG_DATA,"%s:unknown transfer type 0x%02x at CH%d\n"
				,__FUNCTION__, priv->ch_info[ch].cur_tran_type, ch);
			break;
		}
		break;

	case CCode_RetryError:
		/**
		 * - 2.5. Retry Error:
		 *  - return -ETIMEDOUT. Retry Error means various error.
		 *  - -ETIMEDOUT is better to report this condition.
		 */
		DPRINT(DBG_IRQ,":RetryError\n");
		if (urb_queued != NULL){
			complete_urb(hcd, hcd_queue, &priv->ch_info[ch],
				urb_queued, -ETIMEDOUT);
		}
		if (list_empty(&priv->ch_info[ch].urb_list)) {
			/* empty */
			DPRINT(DBG_DATA,"%s:free CH%d\n",__FUNCTION__, ch);
			free_ch_resource(hcd, &priv->ch_info[ch], 0);
		} else {
			DPRINT(DBG_DATA,"%s:queue is not empty %d\n"
				,__FUNCTION__, ch);
		}
		break;
	default:
		DPRINT(DBG_IRQ,":others\n");
		break;
	}

	return retval;
}

/**
 * @brief	- process TotalSizeCmp interrupt
 * @param	*hcd;	usb_hcd structure
 * @param	*priv;	private area of HCD
 * @param	ch;		CH number
 * @return	interrupt status to be cleared
 */
static unsigned char totalsizecmp_irq_handler(struct usb_hcd *hcd, 
	struct hcd_priv *priv, unsigned char ch)
{
	unsigned char	retval = TotalSizeCmp;
	
	s1r72xxx_queue_log(S_R_DEBUG_IRQ, ch, priv->ch_info[ch].cur_tran_type);

	/**
	 * - 1. set "this is not short packet":
	 */
	priv->ch_info[ch].is_short = S_R_CH_LAST_ISNOT_SHORT;

	/**
	 * - 2. check transfer type and functions:
	 */
	switch(priv->ch_info[ch].cur_tran_type){
	case S_R_CH_TR_CONTROL:
		submit_control(hcd, NULL, 0);
		if (list_empty(&priv->ch_info[ch].urb_list)) {
			/* empty */
			DPRINT(DBG_DATA,"%s:free CH%d\n",__FUNCTION__, ch);
			free_ch_resource(hcd, &priv->ch_info[ch], 0);
		} else {
			DPRINT(DBG_DATA,"%s:queue is not empty %d\n"
				,__FUNCTION__, ch);
		}
		break;
	case S_R_CH_TR_BULK:
	case S_R_CH_TR_INT:
		submit_bulk_intr(ch, hcd, NULL, NULL, 0);
		if (list_empty(&priv->ch_info[ch].urb_list)) {
			/* empty */
			DPRINT(DBG_DATA,"%s:free CH%d\n",__FUNCTION__, ch);
			free_ch_resource(hcd, &priv->ch_info[ch], 0);
		} else {
			DPRINT(DBG_DATA,"%s:queue is not empty %d\n"
				,__FUNCTION__, ch);
		}
		break;
	case S_R_CH_TR_ISOC:
		default:
		DPRINT(DBG_DATA,"%s:unknown transfer type 0x%02x at CH%d\n"
			,__FUNCTION__, priv->ch_info[ch].cur_tran_type, ch);
		break;
	}

	return retval;
}

/**
 * @brief	- API for reseting hardware
 * @param	*hcd;	struct of usb_hcd
 * @retval	0;	complete
 */
static int s1r72xxx_hcd_reset(struct usb_hcd *hcd)
{
	struct hcd_priv* priv
		= (struct hcd_priv*)hcd->hcd_priv[0];	/* private data */
	unsigned char	ch;						/* CH number */
	unsigned long	lock_flags;				/* spin lock flags */
	
	DPRINT(DBG_API,"%s:enter\n", __FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_RESET, 0, priv->hw_state);

	if (priv->hw_state != S_R_HW_PROBE){
		spin_lock_irqsave(&priv->lock, lock_flags);
	}

	/**
	 * - 1. initialize hardware:
	 */
	init_lsi();

	/**
	 * - 2. initialize CH settings:
	 */
	init_ch_info(priv);
	for (ch = CH_0 ; ch < CH_MAX ; ch++){
		free_ch_resource(hcd, &priv->ch_info[ch], -ECONNRESET);
	}

	/**
	 * - 3. initialize root hub:
	 */
	s1r72xxx_init_virtual_root_hub(priv);

	/**
	 * - 4. set HCD state HC_STATE_HALT:
	 */
	hcd->state = HC_STATE_HALT;

	if (priv->hw_state != S_R_HW_PROBE){
		spin_unlock_irqrestore(&priv->lock, lock_flags);
	}
	DPRINT(DBG_API,"%s:exit\n", __FUNCTION__);

	return 0;
}

/**
 * @brief	- API for starting hardware
 * @param	*hcd;	struct of usb_hcd
 * @retval	0;	complete
 */
static int s1r72xxx_hcd_start(struct usb_hcd *hcd)
{
	struct hcd_priv* priv
		= (struct hcd_priv*)hcd->hcd_priv[0];	/* private data */
	unsigned long	lock_flags;				/* spin lock flags */

	DPRINT(DBG_API,"%s:enter\n", __FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_START, 0, priv->hw_state);

	/**
	 * - 1. enable interrupt to use hardware :
	 */
	if (priv->hw_state != S_R_HW_PROBE){
		spin_lock_irqsave(&priv->lock, lock_flags);
		host_start(hcd);
		spin_unlock_irqrestore(&priv->lock, lock_flags);
	}

	/**
	 * - 2. set HCD state HC_STATE_RUNNING:
	 */
	hcd->state = HC_STATE_RUNNING;

	DPRINT(DBG_API,"%s:exit\n", __FUNCTION__);

	return 0;
}

/**
 * @brief	- Start host function. enables interrupt.
 * @param	*hcd;	struct of usb_hcd
 * @retval	irqreturn_t
 */
void host_start(struct usb_hcd *hcd)
{
	struct hcd_priv* priv
		= (struct hcd_priv*)hcd->hcd_priv[0];	/* private data */
	unsigned char	ch_ct;					/* CH counter */

	/**
	 * - 1. Set negotiation control:
	 */
	priv->rh.port_status.wPortStatus |= USB_PORT_STAT_POWER;
	port_control(hcd, S_R_PORT_CNT_REG_WRITE, GoWAIT_CONNECTtoDIS);

	/**
	 * - 2. Set interrupt enable:
	 */
	/* set MainIntEnb */
	S_R_REGS8(rcC_MainIntStat)	= HostIntStat;
	S_R_REGS8(rcC_MainIntEnb)	|= EnHostIntStat;

	/* set SIEIntEnb0 */
	S_R_REGS8(rcH_SIE_IntStat_0)	= ~S_R_REG_CLEAR;
	S_R_REGS8(rcH_SIE_IntEnb_0)	= SIE_IntEnb_0_AllBit;

	/* set SIEIntEnb1 */
	S_R_REGS8(rcH_SIE_IntStat_1)	= ~S_R_REG_CLEAR;
	S_R_REGS8(rcH_SIE_IntEnb_1)	= SIE_IntEnb_1_AllBit;

	/* set Frame_IntEnb */
	S_R_REGS8(rcH_Frame_IntStat)	= ~S_R_REG_CLEAR;
	S_R_REGS8(rcH_Frame_IntEnb)	= PortErr;

	/* clear all CH interrupt */
	S_R_REGS8(rcH_CHr_IntEnb)	= S_R_REG_CLEAR;
	for (ch_ct = CH_0 ; ch_ct < CH_MAX ; ch_ct++ ){
		S_R_REGS8(priv->ch_info[ch_ct].CHxIntStat)	= ~S_R_REG_CLEAR;
		S_R_REGS8(priv->ch_info[ch_ct].CHxIntEnb)	= S_R_REG_CLEAR;
	}

	/* clear USB device interrupt */
	S_R_REGS8(rcC_DeviceIntEnb)	= S_R_REG_CLEAR;

	/* set USB host interrupt */
	S_R_REGS8(rcC_HostIntStat)	= S_R_REG_CLEAR;
	S_R_REGS8(rcC_HostIntEnb)	= EnValidHostEnb;

	hcd->state = HC_STATE_RUNNING;
		
}

/**
 * @brief	- process of suspend
 * @param	*hcd;	struct of usb_hcd
 * @param	msg;	struct of pm_message_t
 * @retval	0;	complete
 */
static int s1r72xxx_hcd_suspend(struct usb_hcd *hcd, pm_message_t msg)
{
	struct hcd_priv* priv
		= (struct hcd_priv*)hcd->hcd_priv[0];	/* private data */
	unsigned long		lock_flags;		/* spin lock flags */

	DPRINT(DBG_API,"%s:\n", __FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_SUSPEND, 0, priv->hw_state);
	
	spin_lock_irqsave(&priv->lock, lock_flags);

	/**
	 * - 1. call function to change state to USB suspend:
	 */
	hc_suspend(hcd);

	spin_unlock_irqrestore(&priv->lock, lock_flags);

	DPRINT(DBG_API,"%s:\n", __FUNCTION__);

	return 0;
}

/**
 * @brief	- process of suspend
 * @param	*dev;	device structure
 * @param	msg;	struct of pm_message_t
 * @param	level;	
 * @retval	0;	complete
 */
static int s1r72xxx_hcd_sys_suspend(struct device *dev, pm_message_t msg,
	u32 level)
{
	struct usb_hcd		*hcd = dev_get_drvdata(dev);
	struct hcd_priv* priv
		= (struct hcd_priv*)hcd->hcd_priv[0];	/* private data */
	unsigned long		lock_flags;		/* spin lock flags */

	DPRINT(DBG_API,"%s:\n", __FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_SYS_SUSPEND, 0, priv->hw_state);
	
	spin_lock_irqsave(&priv->lock, lock_flags);

	/**
	 * - 1. call function to change state to USB suspend:
	 */
	hc_suspend(hcd);

	spin_unlock_irqrestore(&priv->lock, lock_flags);

	DPRINT(DBG_API,"%s:\n", __FUNCTION__);

	return 0;
}

/**
 * @brief	- process of suspend
 * @param	*hcd;	struct of usb_hcd
 * @retval	0;		complete
 */
void hc_suspend(struct usb_hcd *hcd)
{
	DPRINT(DBG_API,"%s:\n", __FUNCTION__);

	if (hcd->state == HC_STATE_RUNNING){
		/**
		 * - 1. set HCD state HC_STATE_QUIESCING:
		 */
		hcd->state = HC_STATE_QUIESCING;
		/**
		 * - 2. set hardware change to suspend state:
		 */
		port_control(hcd, S_R_PORT_CNT_REG_WRITE, GoSUSPEND);
		if (hcd->remote_wakeup) {
			S_R_REGS8(rcH_NegoControl_1) |= RmtWkupDetEnb;
		}
	}

	DPRINT(DBG_API,"%s:\n", __FUNCTION__);

	return;
}


/**
 * @brief	- process of resume
 * @param	*hcd	struct of usb_hcd
 * @retval	0;	complete
 */
static int s1r72xxx_hcd_resume(struct usb_hcd *hcd)
{
	struct hcd_priv* priv
		= (struct hcd_priv*)hcd->hcd_priv[0];	/* private data */
	unsigned long		lock_flags;		/* spin lock flags */

	DPRINT(DBG_API,"%s:\n", __FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_RESUME, 0, priv->hw_state);
	
	spin_lock_irqsave(&priv->lock, lock_flags);

	/**
	 * - 1. call function to change state to USB resume:
	 */
	hc_resume(hcd);
	
	spin_unlock_irqrestore(&priv->lock, lock_flags);

	DPRINT(DBG_API,"%s:\n", __FUNCTION__);

	return 0;
}

/**
 * @brief	- process of resume
 * @param	*dev	struct of device
 * @param	level;	level of resume
 * @retval	0;	complete
 */
static int s1r72xxx_hcd_sys_resume(struct device *dev, u32 level)
{
	struct usb_hcd		*hcd = dev_get_drvdata(dev);
	struct hcd_priv* priv
		= (struct hcd_priv*)hcd->hcd_priv[0];	/* private data */
	unsigned long		lock_flags;		/* spin lock flags */

	DPRINT(DBG_API,"%s:\n", __FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_SYS_RESUME, 0, priv->hw_state);
	
	spin_lock_irqsave(&priv->lock, lock_flags);

	/**
	 * - 1. call function to change state to USB resume:
	 */
	hc_resume(hcd);
	
	spin_unlock_irqrestore(&priv->lock, lock_flags);

	DPRINT(DBG_API,"%s:\n", __FUNCTION__);

	return 0;
}

/**
 * @brief	- process of resume
 * @param	*hcd	struct of usb_hcd
 * @retval	0;	complete
 */
void hc_resume(struct usb_hcd *hcd)
{
	DPRINT(DBG_API,"%s:\n", __FUNCTION__);
	
	if (hcd->state == HC_STATE_SUSPENDED){
		/**
		 * - 1. set HCD state HC_STATE_QUIESCING:
		 */
		hcd->state = HC_STATE_RESUMING;
	
		/**
		 * - 2. set hardware change to operational:
		 */
		port_control(hcd, S_R_PORT_CNT_REG_WRITE, GoRESUMEtoOP);
		S_R_REGS8(rcH_NegoControl_1) &= ~RmtWkupDetEnb;
	}

	DPRINT(DBG_API,"%s:\n", __FUNCTION__);

	return;
}

/**
 * @brief	- process of stop
 * @param	*hcd;	struct of usb_hcd
 * @retval	none;
 */
static void s1r72xxx_hcd_stop(struct usb_hcd *hcd)
{
	struct hcd_priv*	priv
		= (struct hcd_priv*)hcd->hcd_priv[0];	/* private data */
	unsigned char		ch_ct;				/* CH counter */
	unsigned char		finished_pm;		/* FinishedPM */
	unsigned char		main_int_enb;		/* MainIntEnb */

	DPRINT(DBG_API,"%s:enter \n", __FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_STOP, 0, priv->hw_state);

	/**
	 * - 1. set automode to wait connect to disabled:
	 */
	port_control(hcd, S_R_PORT_CNT_REG_WRITE, GoWAIT_CONNECTtoDIS);
	/**
	 * - 2. set interrupt enable:
	 */
	finished_pm		= S_R_REGS8(rcC_MainIntEnb) & FinishedPM;
	main_int_enb	= S_R_REGS8(rcC_MainIntEnb) & (~EnValidMainIntStat);

	/* set MainIntEnb */
	S_R_REGS8(rcC_MainIntStat)		= ~(finished_pm | main_int_enb);
	S_R_REGS8(rcC_MainIntEnb)		= finished_pm | main_int_enb;

	/* set SIEIntEnb0 */
	S_R_REGS8(rcH_SIE_IntStat_0)	= ~S_R_REG_CLEAR;
	S_R_REGS8(rcH_SIE_IntEnb_0)		= S_R_REG_CLEAR;

	/* set SIEIntEnb1 */
	S_R_REGS8(rcH_SIE_IntStat_1)	= ~S_R_REG_CLEAR;
	S_R_REGS8(rcH_SIE_IntEnb_1)		= S_R_REG_CLEAR;

	/* set Frame_IntEnb */
	S_R_REGS8(rcH_Frame_IntStat)	= ~S_R_REG_CLEAR;
	S_R_REGS8(rcH_Frame_IntEnb)		= S_R_REG_CLEAR;

	/* clear all CH interrupt */
	for (ch_ct = CH_0 ; ch_ct < CH_MAX ; ch_ct++){
		free_ch_resource(hcd, &priv->ch_info[ch_ct], -ECONNRESET);
	}

	/* clear USB device interrupt */
	S_R_REGS8(rcC_DeviceIntEnb)	= S_R_REG_CLEAR;

	/* clear USB host interrupt */
	S_R_REGS8(rcC_HostIntStat) = ~S_R_REG_CLEAR;
	S_R_REGS8(rcC_HostIntEnb) = S_R_REG_CLEAR;

	/* modified setting from immediately to bit control for ide function */
	hcd->state = HC_STATE_HALT;

	DPRINT(DBG_API,"%s:exit\n", __FUNCTION__);

}

/**
 * @brief	- process of get frame number
 * @param	*hcd;	struct of usb_hcd
 * @retval	frame number;
 */
static int s1r72xxx_hcd_get_frame(struct usb_hcd *hcd)
{
	DPRINT(DBG_API,"%s:0x%04x\n", __FUNCTION__,
		(S_R_REGS16(rsH_FrameNumber_HL) & FrameNumberMask));

	/**
	 * - 1. return register value:
	 */
	return (S_R_REGS16(rsH_FrameNumber_HL) & FrameNumberMask);
}

/**
 * @brief	- process of endpoint disable
 * @param	*hcd;	struct of usb_hcd
 * @param	*ep;	struct of usb_host_endpoint
 * @retval	none;
 */
static void s1r72xxx_hcd_endpoint_disable(struct usb_hcd *hcd,
	struct usb_host_endpoint *ep)
{
	struct hcd_priv *priv = (struct hcd_priv *)hcd->hcd_priv[0];
	int ch;
	S_R_HCD_QUEUE	*hcd_queue;
	unsigned long	lock_flags;		/* spin lock flags */
	DPRINT(DBG_API,"%s:enter\n", __FUNCTION__);
	s1r72xxx_queue_log(S_R_DEBUG_EP_DISABLE, 0, priv->hw_state);
	priv->called_state = S_R_CALLED_FROM_OTH;

	/**
	 * - 1. Serch EP matches *ep:
	 */
	spin_lock_irqsave(&priv->lock, lock_flags);
	if ( (ep->desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
		== USB_ENDPOINT_XFER_CONTROL){
		ch = CH_0;
	} else {
		for (ch = CH_A ; ch < CH_MAX ; ch++){
		
			if (priv->ch_info[ch].ep == NULL){
				continue;
			} else if (priv->ch_info[ch].ep == ep){
				DPRINT(DBG_API,"%s:found CH%d\n",__FUNCTION__, ch);
				break;
			}
		}
	}

	/**
	 * - 2. Complete all urb and free CH informations:
	 */
	if (ch < CH_MAX){
		while(!list_empty(&priv->ch_info[ch].urb_list)){
			hcd_queue = list_entry(priv->ch_info[ch].urb_list.next,
				S_R_HCD_QUEUE, urb_list);
			if (hcd_queue->urb->status == -EINPROGRESS){
				complete_urb(hcd, hcd_queue, &priv->ch_info[ch],
					hcd_queue->urb, -ECONNRESET);
			} else {
				complete_urb(hcd, hcd_queue, &priv->ch_info[ch],
					hcd_queue->urb, hcd_queue->urb->status);
			}
		}
	}

	DPRINT(DBG_API,"%s:exit\n", __FUNCTION__);
	priv->called_state = S_R_CALLED_FROM_API;
	spin_unlock_irqrestore(&priv->lock, lock_flags);

	return;
}

/**
 * @brief	- port reset
 * @param	*hcd;	usb_hcd
 * @param	pnum;	port number
 * @retval	0;	complete
 */
static int s1r72xxx_start_port_reset(struct usb_hcd *hcd, unsigned pnum)
{
	DPRINT(DBG_API,"%s:", __FUNCTION__);
	/**
	 * - 1. no operation because this function is used for OTG:
	 */
	return 0;
}

/**
 * @brief	- over current state check.
 * @param	*_dev;	usb_hcd
 */
void over_current_timer(unsigned long _dev)
{
	struct usb_hcd *hcd;
	struct hcd_priv *priv;
	unsigned long	lock_flags;
	
	hcd = (struct usb_hcd *)_dev;
	priv = (struct hcd_priv *)hcd->hcd_priv[0];

	spin_lock_irqsave(&priv->lock, lock_flags);

	/**
	 * - 1. Check VBUS state:
	 */
	if( (S_R_REGS8(rcC_H_USB_Status) & VBUS_State)
			== VBUS_State_High){
		/**
		 * - 1.1. Initialize hardware and values.
		 */
		S_R_REGS8(rcC_HostIntEnb) |= H_VBUS_Err;
		init_lsi();
		host_start(hcd);

		priv->rh.port_status.wPortStatus &= ~USB_PORT_FEAT_OVER_CURRENT;
		priv->rh.port_status.wPortStatus |= USB_PORT_FEAT_POWER;
		priv->rh.hub_status.wHubStatus |= HUB_STATUS_OVERCURRENT;
		priv->rh.port_status.wPortChange
			|= USB_PORT_STAT_C_OVERCURRENT;
		priv->rh.hub_status.wHubChange &= ~HUB_CHANGE_OVERCURRENT;
		priv->rh.wPortTrigger = RH_PORT_STATUS_CHANGED;

		S_R_REGS8(rcC_MainIntStat)	= FinishedPM;
		S_R_REGS8(rcC_MainIntEnb)	|= EnFinishedPM;
		S_R_REGS8(rc_PM_Control)	|= GoActHost;

		del_timer_sync(&priv->oc_timer);
		priv->oc_timer_state = S1R72_OVER_CURRENT_TIMER_IDLE;
	} else {
		/**
		 * - 1.2. Restart timer.
		 */
		mod_timer(&priv->oc_timer, S1R72_OVER_CURRENT_TIMEOUT);
	}
	
	spin_unlock_irqrestore(&priv->lock, lock_flags);
}

/**
 * @struct	s1r72xxx_hc_driver
 * @brief	host controller driver
 */
static struct hc_driver s1r72xxx_hc_driver = {
	.description		= DRV_NAME,
	.hcd_priv_size		= sizeof(struct hcd_priv),
	.irq			= s1r72xxx_hcd_irq,
	.flags			= HCD_USB2 | HCD_MEMORY,
	.reset			= s1r72xxx_hcd_reset,
	.start			= s1r72xxx_hcd_start,
	.suspend		= s1r72xxx_hcd_suspend,
	.resume			= s1r72xxx_hcd_resume,
	.stop			= s1r72xxx_hcd_stop,
	.get_frame_number	= s1r72xxx_hcd_get_frame,
	.urb_enqueue		= s1r72xxx_hcd_enqueue,
	.urb_dequeue		= s1r72xxx_hcd_dequeue,
	.endpoint_disable	= s1r72xxx_hcd_endpoint_disable,
	.hub_status_data	= s1r72xxx_hcd_hub_status,
	.hub_control		= s1r72xxx_hcd_hub_control,
	.bus_suspend		= s1r72xxx_hcd_hub_suspend, //kyon
	.bus_resume		= s1r72xxx_hcd_hub_resume,
	.start_port_reset	= s1r72xxx_start_port_reset,
};

/*
 *	HC functions
 */
#ifdef CONFIG_PCI
/**
 * @brief	- probe HCD initialize device for pci device.
 * @param	*dev;	struct of pci_dev
 * @param	*id;	struct of pci_devce_id
 * @retval	return 0;	complete
 *				if error,then error code
 */
static int __init s1r72xxx_hcd_probe(struct pci_dev *dev,
	const struct pci_device_id *id)
{
	int			ret;
	struct hcd_priv		*priv;
	struct hc_driver	*driver;
	struct usb_hcd		*hcd;
	void __iomem		*reg;

	/**
	 * - 1. allocate private area:
	 * - if error then return NULL pointer
	 */
	priv = kmalloc(sizeof(struct hcd_priv), GFP_KERNEL);
	if (priv == (struct hcd_priv *)NULL) {
		return -ENOMEM;
	}
	/**
	 * - 2. clear private area:
	 */
	memset(priv, 0, sizeof(struct hcd_priv));

	/**
	 * - 3. clear channel list:
	 */
	s1r72xxx_clear_ch_list(priv);

	/**
	 * - 4. initialize virtual root hub:
	 */
	s1r72xxx_init_virtual_root_hub(priv);
	spin_lock_init(&priv->lock);

	/**
	 * - 5. parameter check:
	 */

	if (!id || !(driver = (struct hc_driver *) id->driver_data))
		return -ENODEV;	

	/**
	 * - 6. PCI enable
	 */
	ret = pci_enable_device(dev);
	if (ret) {
		DPRINT(DBG_API,"pci_enable_device fail\n.");
		return -ENODEV;
	}
	DPRINT(DBG_API,"pci_enable_device success.\n");

	dev->current_state = PCI_D0;
	dev->dev.power.power_state = PMSG_ON;

	if (!dev->irq) {
		dev_err(&dev->dev, "Found HC with no IRQ.  Check BIOS/PCI %s setup!\n",
			pci_name(dev));
		ret = -ENODEV;
		goto err1;
	}
	DPRINT(DBG_API,"IRQ = %d\n", dev->irq);

	/**
	 * - 7. crate hcd:
	 * - if error then return -ENOMEM
	 */
	hcd = usb_create_hcd(driver, &dev->dev, pci_name(dev));
	if (!hcd) {
		DPRINT(DBG_API,"usb_create_hcd fail\n");
		ret = -ENOMEM;
		goto err1;
	}

	DPRINT(DBG_API,"driver->flags=0x%X, len=0x%X\n", driver->flags,
		HCD_MEMORY);
	if (!(driver->flags & HCD_MEMORY)) {
		DPRINT(DBG_API,"bad (driver->flags & HCD_MEMORY)\n");
		ret = -EFAULT;
		goto err2;
	}

	/**
	 * - 8. fill struct hcd by necessary:
	 */
	hcd->rsrc_start = pci_resource_start(dev, 2);	// region2
	hcd->rsrc_len = pci_resource_len(dev, 2);
	DPRINT(DBG_API,"ioaddr=0x%LX, len=0x%Ld\n",
		hcd->rsrc_start, hcd->rsrc_len);
	hcd->state = __ACTIVE;
	hcd->hcd_priv[0] = (unsigned long)priv;
	s1r72xxx_init_ch_info(hcd);

	/**
	 * - 9. PCI memory assign:
	 */
	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
		driver->description)) {
		dev_dbg(&dev->dev, "controller already in use\n");
		ret = -EBUSY;
		goto err2;
	}
	hcd->regs = ioremap_nocache(hcd->rsrc_start, hcd->rsrc_len);
	if (hcd->regs == NULL) {
		dev_dbg(&dev->dev, "error mapping memory\n");
		ret = -EFAULT;
		goto err3;
	}
	pci_set_master(dev);

	/* Configuration of the PLX PCI board */

	priv->pcimem_start = pci_resource_start(dev, 0);		// region0
	priv->pcimem_len = pci_resource_len(dev, 0);
	DPRINT(DBG_API,"region0 addr=0x%LX, len=0x%Ld\n", priv->pcimem_start,
		priv->pcimem_len);

	if (!request_mem_region(priv->pcimem_start, priv->pcimem_len,
		driver->description)) {
		dev_dbg(&dev->dev, "controller already in use\n");
		ret = -EBUSY;
		goto err4;
	}
	reg = ioremap_nocache(priv->pcimem_start, priv->pcimem_len);
	if (reg == NULL) {
		dev_dbg(&dev->dev, "error mapping memory\n");
		ret = -EFAULT;
		goto err5;
	}

	/**
	 * - 10. Device Chip base memory:
	 */
	priv->ioaddr = s_r_ioaddr = (void *)reg;

	/**
	 * - 11. Device Chip Setup:
	 */
	S_R_REGS32(0x68) = 0x0f000900;		// INTCSR
	DPRINT(DBG_API,"INTCSR = 0x%X\n", S_R_REGS32(0x68));
	
	s_r_ioaddr = (void *)hcd->regs;
	
	/**
	 * - 12. register hcd:
	 */
	ret = usb_add_hcd(hcd, dev->irq, SA_SHIRQ);
	if (ret != 0)
		goto err6;

	DPRINT(DBG_API,"EOF success!\n");
	return ret;

err6:
	DPRINT(DBG_API,"fail error 6! ret=%d\n", ret);
	iounmap (reg);
err5:
	DPRINT(DBG_API,"fail error 5! ret=%d\n", ret);
	release_mem_region(priv->pcimem_start, priv->pcimem_len);
err4:
	DPRINT(DBG_API,"fail error 4! ret=%d\n", ret);
	if (driver->flags & HCD_MEMORY) {
		iounmap(hcd->regs);
err3:
	DPRINT(DBG_API,"fail error 3! ret=%d\n", ret);
		release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	} else
		release_region(hcd->rsrc_start, hcd->rsrc_len);
err2:
	DPRINT(DBG_API,"fail error 2! ret=%d\n", ret);
	usb_put_hcd (hcd);
err1:
	DPRINT(DBG_API,"fail error 1! ret=%d\n", ret);
	pci_disable_device (dev);
	kfree(priv);
	dev_err(&dev->dev, "init %s fail, %d\n", pci_name(dev), ret);
	return ret;	
}

/**
 * @brief	remove HCD
 * @param	*dev;	struct of device
 * @retval	none; 
 */
static void  s1r72xxx_hcd_remove(struct pci_dev *dev)	//__exit
{
	struct usb_hcd	*hcd;			/* HCD structure */
	struct hcd_priv *priv;			/* private */

	DPRINT(DBG_API,"%s-usb:remove \n", CHIP_NAME);
	hcd = pci_get_drvdata(dev);
	if (!hcd)
		return;

	priv = (struct hcd_priv *)hcd->hcd_priv[0];
	usb_remove_hcd(hcd);
	if (hcd->driver->flags & HCD_MEMORY) {
		iounmap(hcd->regs);
		release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	} else {
		release_region(hcd->rsrc_start, hcd->rsrc_len);
	}

	if (priv->pcimem_start)
		release_mem_region(priv->pcimem_start, priv->pcimem_len);

	usb_put_hcd(hcd);
	pci_disable_device(dev);
	kfree(priv);

	return;
}
#else		// CONFIG_PCI

/**
 * @brief	- probe HCD initialize device
 * @param	*dev;	struct of device
 * @retval	return 0;	complete
 *				if error,then error code
 */
static int __init s1r72xxx_hcd_probe(struct device *dev)
{
	int ret;
	int irq;							/* irq number */
	struct resource* res;
	struct platform_device* pdev = to_platform_device(dev);
	struct hcd_priv *priv;				/* private data */
	struct usb_hcd *hcd;				/* HCD structure */
	unsigned long lock_flags;			/* spin lock flags */

	DPRINT(DBG_API, "%s:enter\n", __FUNCTION__);

	/**
	 * - 1. allocate private area:
	 * - if error then return NULL pointer
	 */
	priv = kmalloc(sizeof(struct hcd_priv), GFP_KERNEL);
	if (priv == (struct hcd_priv *)NULL) {
		return -ENOMEM;
	}
	priv->hw_state = S_R_HW_PROBE;
	priv->automode_flag = AUTO_NOT_USED;
	spin_lock_init(&priv->lock);

	/**
	 * - 2. clear private area:
	 */
	memset(priv, 0, sizeof(struct hcd_priv));

	/**
	 * - 3. clear channel list:
	 */
	init_ch_info(priv);

	/**
	 * - 4. get resource of IRQ:
	 * - if error then return -ENODEV
	 */
	 //init extinit5
	  *(volatile unsigned long*)GPIO_PORTA_SEL_V |= 1 << 5 ;       //for common use
        *(volatile unsigned long*)GPIO_PORTA_DIR_V |= 1 << 5 ;       //1 stands for in
        *(volatile unsigned long*)GPIO_PORTA_INTRCTL_V |= (0x3<<10) ;//中断类型为low电平解发
        *(volatile unsigned long*)GPIO_PORTA_INCTL_V |= 1 << 5 ;     //中断输入方式
        *(volatile unsigned long*)GPIO_PORTA_INTRCLR_V |= 1 << 5 ;    //清除中断
        *(volatile unsigned long*)GPIO_PORTA_INTRCLR_V = 0x0000;     //清除中断
		enable_irq(INTSRC_EXTINT5);


	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, DRV_NAME);
	if (!res) {
		DPRINT(DBG_API, "%s driver irq resource not found.\n", CHIP_NAME);
		return -ENODEV;
	}
	irq = res->start;

	/**
	 * - 5. get resource of memory:
	 * - if error then return -ENODEV
	 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, DRV_NAME);
	if (!res) {
		DPRINT(DBG_API,KERN_ERR "%s driver resouce not found.\n", CHIP_NAME);
		return -ENODEV;
	}

	/**
	 * - 6. I/O remap:
	 * - if error then return -ENODEV
	 */
	priv->ioaddr = s_r_ioaddr = (void*)ioremap(res->start, 
		res->end-res->start);
	if (!s_r_ioaddr) {
		return -ENODEV;
	}
	
	/**
	 * - 7. Device Chip Check:
	 * - if error then return -ENODEV
	 */
	spin_lock_irqsave(&priv->lock, lock_flags);
	init_lsi();
	DPRINT(DBG_INIT,"%s Value ioaddr: 0x%04X, irq %d, Chip Rev: 0x%02X \n",
		DRV_NAME, (unsigned)s_r_ioaddr, irq, S_R_REGS8(rcC_RevisionNum));
		
	/**
	 * - 8. crate hcd:
	 * - if error then return -1
	 */
	hcd = usb_create_hcd(&s1r72xxx_hc_driver, &pdev->dev, (char*)hcd_name);
	if (!hcd)
	{
		kfree(priv);
		spin_unlock_irqrestore(&priv->lock, lock_flags);
		return -ENOMEM;
	}

	/**
	 * - 9. fill struct hcd by necessary:
	 */
	hcd->rsrc_start = pdev->resource[0].start;
	hcd->rsrc_len = pdev->resource[0].end-pdev->resource[0].start + 1;
	request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
		s1r72xxx_hc_driver.description);
	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	hcd->state = __ACTIVE;
	hcd->hcd_priv[0] = (unsigned long)priv;

	create_proc_file();
	PROC_HCD(hcd, priv);

	/**
	 * - 10. register hcd:
	 */
#if defined(CONFIG_USB_V17_HCD) || defined(CONFIG_USB_V17_HCD_MODULE)
	ret = usb_add_hcd(hcd, irq, SA_INTERRUPT);
#else
	/* V05 & C05 */
	ret = usb_add_hcd(hcd, irq, SA_INTERRUPT | SA_SHIRQ);
#endif
	if (ret < 0) {
		kfree(priv);
		spin_unlock_irqrestore(&priv->lock, lock_flags);
		return -ENODEV;
	}
	
	/**
	 * - 11. turn power on:
	 */
	/* initialize timer */
	init_timer(&priv->oc_timer);
	priv->oc_timer.function = over_current_timer;
	priv->oc_timer.data = (unsigned long)hcd;
	priv->oc_timer_state = S1R72_OVER_CURRENT_TIMER_IDLE;

	priv->hw_state = S_R_HW_SLEEP;
	priv->called_state = S_R_CALLED_FROM_API;
	S_R_REGS8(rcC_MainIntStat)		= FinishedPM;
	S_R_REGS8(rcC_MainIntEnb)		|= EnFinishedPM;
	S_R_REGS8(rc_PM_Control)		|= GoActHost;

	spin_unlock_irqrestore(&priv->lock, lock_flags);

	DPRINT(DBG_API, "%s:exit\n", __FUNCTION__);
	return ret;
}

/**
 * @brief	remove HCD
 * @param	*dev;	struct of device
 * @retval	none; 
 */
static int  s1r72xxx_hcd_remove(struct device* dev) //__exit
{
	struct usb_hcd		*hcd = dev_get_drvdata(dev);
	struct hcd_priv *priv = (struct hcd_priv *)hcd->hcd_priv[0];
#if defined(CONFIG_USB_V05_HCD) || defined(CONFIG_USB_V05_HCD_MODULE)
	unsigned char		finished_pm;		/* FinishedPM */
	unsigned char		main_int_enb;		/* MainIntEnb */
	unsigned int		timeout_ct = 0;		/* time out counter */
#endif
	
	DPRINT(DBG_API,"%s:enter\n", __FUNCTION__);

	/**
	 * - 1. change hardwre state to sleep
	 */
	usb_remove_hcd(hcd);
	/* port power off */
	port_control(hcd, S_R_PORT_CNT_POW_OFF, GoIDLE);
	/* delete timer */
	if (priv->oc_timer_state == S1R72_OVER_CURRENT_TIMER_ACT){
		del_timer_sync(&priv->oc_timer);
	}
#if defined(CONFIG_USB_V05_HCD) || defined(CONFIG_USB_V05_HCD_MODULE)
	/* add for v05 ide function */
	finished_pm		= S_R_REGS8(rcC_MainIntEnb) & FinishedPM;
	main_int_enb	= S_R_REGS8(rcC_MainIntEnb) & (~EnValidMainIntStat);
	S_R_REGS8(rcC_MainIntEnb) &= ~EnValidMainIntStat;

	/* if IDE driver exist, change power state to Active60 */
	if (S_R_REGS8(rcC_IDE_Config_1) & ActiveIDE) {
		/* if PM state is Active, changes to Sleep */
		if (S_R_REGS8(rcC_PM_Control_1) & ACT_HOST) {
			S_R_REGS8(rcC_PM_Control_0) = GoActive60;
			
			while ( timeout_ct < S_R_FINISHED_PM_TIMEOUT){
				if (S_R_REGS8(rcC_MainIntStat) & FinishedPM){
					break;
				}
				timeout_ct++;
			}

			S_R_REGS8(rcC_MainIntStat) = FinishedPM;	/* clear FinishedPM */

			/* restore interrupt enable */
			S_R_REGS8(rcC_MainIntEnb) = finished_pm | main_int_enb;
		}
	} else {
		/* if PM state is Active, changes to Sleep */
		if (S_R_REGS8(rcC_PM_Control_1) & ACT_HOST) {	/* ACT_HOST */
			S_R_REGS8(rcC_PM_Control_0) = GoSleep;	/* GoSLEEP */

			while ( timeout_ct < S_R_FINISHED_PM_TIMEOUT){
				if (S_R_REGS8(rcC_MainIntStat) & FinishedPM){
					break;
				}
				timeout_ct++;
			}

			S_R_REGS8(rcC_MainIntStat) = FinishedPM;	/* clear FinishedPM */
		}
	}
#endif

	/**
	 * - 2. free I/O mapped area:
	 */	
	iounmap(s_r_ioaddr);

	usb_put_hcd(hcd);

	remove_proc_file();

	return 0;
}
#endif		// CONFIG_PCI

#ifdef	CONFIG_PCI
#define	PCI_VENDER_ID_PLX		(0x10b5)
#define	PCI_DEVICE_ID_PLX		(0x9054)

/* PCI driver selection metadata; PCI hotplugging uses this */
static struct pci_device_id pci_ids [] = {
	{ PCI_DEVICE(PCI_VENDER_ID_PLX, PCI_DEVICE_ID_PLX),
	  .driver_data =  (unsigned long) &s1r72xxx_hc_driver,
	},
	{ 0, },
};

MODULE_DEVICE_TABLE(pci, pci_ids);

/**
 * @struct	s1r72xxx_dev_driver
 * @brief	pci driver glue; this is a "new style" PCI driver module
 */
static struct pci_driver s1r72xxx_dev_driver = {
	.name		= DRV_NAME,
	.id_table	= pci_ids,
	.probe		= s1r72xxx_hcd_probe,
	.remove		= s1r72xxx_hcd_remove,
#if 0
	.suspend	= NULL,
	.resume		= NULL,
#endif
};

#else

/**
 * @struct	s1r72xxx_dev_driver
 * @brief	device driver
 */
static struct device_driver s1r72xxx_dev_driver = {
	.name		= DRV_NAME,
	.bus		= &platform_bus_type,
	.probe		= s1r72xxx_hcd_probe,
	.remove		= s1r72xxx_hcd_remove,
	.suspend	= s1r72xxx_hcd_sys_suspend,
	.resume		= s1r72xxx_hcd_sys_resume,
};
#endif

static int __init HC_INIT(void)
{
	/**
	 * - 1. register driver:
	 */

	DPRINT(DBG_API,"driver %s, %s\n", hcd_name, "2007/2/15");

#ifdef	CONFIG_PCI
	return pci_module_init(&s1r72xxx_dev_driver);
#else
	if(driver_find(s1r72xxx_dev_driver.name, s1r72xxx_dev_driver.bus) != NULL){
		return -EBUSY;
	}

	return driver_register(&s1r72xxx_dev_driver);
#endif
}

static void __exit HC_EXIT(void)
{

	DPRINT(DBG_API,"%s-usb:exit \n", CHIP_NAME);

	/**
	 * - 1. unregister driver:
	 */
#ifdef	CONFIG_PCI
	pci_unregister_driver(&s1r72xxx_dev_driver);
#else
	driver_unregister(&s1r72xxx_dev_driver);
#endif
	return;
}

