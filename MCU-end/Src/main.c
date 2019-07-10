// Ver 3.0.0
/*
    1. Since the complicate step to setup wifi module(esp8266) (but I don't think so, you use wifi you should configure connection on PC each time), we temporary deprecated the wifi transfer method and use USB-TTL instead.
    2. The IR sensors (RX) we using now got unstable output signal, which cause us can't use timer-trigger way to implement the system, which is more power-efficient. Maybe a short timer trigger is satisfied, this is TODO.
    3. This version of running wheel has three set of wheel, which may cause previous sending trigger (wifi.isDataChanged) lost update of position data. To fix this, you can add trigger for each wheel instead of only one for all wheels, this is TODO.
*/

#include "main.h"

#define BC_DISCONNECTED 0              // 0 = Board Controller disconnected to EFM8 UART pins (p0.4, p0.5)
#define BC_CONNECTED    1              // 1 = Board Controller connected                     

volatile Mcu mcu;
volatile uint8_t CONVERSION_COMPLETE = 0;
extern volatile Escalator escalator;
SI_SBIT (BC_EN, SFR_P2, 2);
SI_SBIT(LED0, SFR_P1, 4);// P3.4 LED0
//uint32_t debugTick = 0;

void main()
{
    Init();
    uartInit();
    wifiInit();
    BC_EN = BC_CONNECTED /*BC_CONNECTED*/; /* Since we are now temporary using UART to send training data instead of wifi module, hence we assign UART to Board Controller to send data through usb virtual port directly */
    //LED0 = 1; /* debug use */
    IE_EA = 1;	// Enable each interrupt according to its individual mask setting, you can call this a master switch to interrupt
        
    while(1)
    {	
/*			
				if (!p1_0)
					LED0 = 0;
				else
					LED0 = 1;
			*/
        //wifiProcess(); re-enable once wifi got smooth setup and cleanup
        uartTransmission();
        escalatorProcess();
    }
}

void Init(void)
{
    uint8_t index;
    uint8_t SFRPAGE_save = SFRPAGE; /* save SFRPAGE */
    /* Disable WDT ------------------*/
    SFRPAGE = 0x00;
    WDTCN = 0xDE; //First key (use this write sequence to disable WDT)
    WDTCN = 0xAD; //Second key

    /* SYSCLK select ----------------*/
    CLKSEL = CLKSEL_CLKSL__HFOSC0 | CLKSEL_CLKDIV__SYSCLK_DIV_1; // SYSCLK = 24 Mhz divided by 1.
    while (CLKSEL & CLKSEL_DIVRDY__BMASK == CLKSEL_DIVRDY__NOT_READY); // wait till clock stabilize

    /* GPIO setting -----------------*/ // WARN: any change of pinout should check out this context
    P0MASK = P0MASK_B0__IGNORED  | P0MASK_B1__IGNORED | P0MASK_B2__IGNORED //P0.n pin logic value is ignored and will not cause a port mismatch event
			| P0MASK_B3__IGNORED | P0MASK_B4__IGNORED | P0MASK_B5__IGNORED
			| P0MASK_B6__IGNORED | P0MASK_B7__IGNORED;

    P0MDOUT = P0MDOUT_B0__OPEN_DRAIN | P0MDOUT_B1__OPEN_DRAIN 	//P0.n pin  Output Mode
			| P0MDOUT_B2__PUSH_PULL | P0MDOUT_B3__PUSH_PULL
			| P0MDOUT_B4__PUSH_PULL  | P0MDOUT_B5__OPEN_DRAIN
			| P0MDOUT_B6__OPEN_DRAIN | P0MDOUT_B7__OPEN_DRAIN;

    P0MDIN = P0MDIN_B0__DIGITAL  | P0MDIN_B1__DIGITAL | P0MDIN_B2__DIGITAL //P0.n Inpput Mode
			| P0MDIN_B3__DIGITAL | P0MDIN_B4__DIGITAL | P0MDIN_B5__DIGITAL
			| P0MDIN_B6__DIGITAL | P0MDIN_B7__DIGITAL;

    P0SKIP = P0SKIP_B0__SKIPPED | P0SKIP_B1__SKIPPED 	//P0.n skipped by the crossba
			| P0SKIP_B2__SKIPPED | P0SKIP_B3__SKIPPED
			| P0SKIP_B4__NOT_SKIPPED | P0SKIP_B5__NOT_SKIPPED
			| P0SKIP_B6__SKIPPED | P0SKIP_B7__SKIPPED;

    
    P1MASK = P1MASK_B0__IGNORED  | P1MASK_B1__IGNORED | P1MASK_B2__IGNORED
			| P1MASK_B3__IGNORED | P1MASK_B4__IGNORED | P1MASK_B5__IGNORED
			| P1MASK_B6__IGNORED | P1MASK_B7__IGNORED;

    P1MDOUT = P1MDOUT_B0__OPEN_DRAIN | P1MDOUT_B1__OPEN_DRAIN
			| P1MDOUT_B2__OPEN_DRAIN | P1MDOUT_B3__OPEN_DRAIN
			| P1MDOUT_B4__PUSH_PULL| P1MDOUT_B5__OPEN_DRAIN
			| P1MDOUT_B6__OPEN_DRAIN | P1MDOUT_B7__OPEN_DRAIN;
                                                                                                                                
    P1MDIN = P1MDIN_B0__DIGITAL  | P1MDIN_B1__DIGITAL | P1MDIN_B2__DIGITAL
			| P1MDIN_B3__DIGITAL | P1MDIN_B4__DIGITAL | P1MDIN_B5__DIGITAL
			| P1MDIN_B6__DIGITAL | P1MDIN_B7__DIGITAL;

    P1SKIP = P1SKIP_B0__SKIPPED  | P1SKIP_B1__SKIPPED
			| P1SKIP_B2__SKIPPED | P1SKIP_B3__SKIPPED
			| P1SKIP_B4__SKIPPED | P1SKIP_B5__SKIPPED
			| P1SKIP_B6__SKIPPED | P1SKIP_B7__SKIPPED;


    P2MDOUT = P2MDOUT_B0__OPEN_DRAIN | P2MDOUT_B1__OPEN_DRAIN
			| P2MDOUT_B2__OPEN_DRAIN | P2MDOUT_B3__OPEN_DRAIN
			| P2MDOUT_B4__OPEN_DRAIN | P2MDOUT_B5__OPEN_DRAIN
			| P2MDOUT_B6__OPEN_DRAIN;

    SFRPAGE = 0x20; //used special Register have to set up sfrpage
    P2SKIP = P2SKIP_B0__SKIPPED | P2SKIP_B1__SKIPPED
			| P2SKIP_B2__SKIPPED | P2SKIP_B3__SKIPPED;


    SFRPAGE = 0x00;
    XBR2 = XBR2_WEAKPUD__PULL_UPS_ENABLED | XBR2_XBARE__ENABLED 				// Weak Pullups enabled (except for Ports whose I/O are configured for analog mode)
			| XBR2_URT1E__DISABLED | XBR2_URT1RTSE__DISABLED 									// Crossbar enabled. UART1 CTS1 unavailable at Port pin
			| XBR2_URT1CTSE__DISABLED; 																				//UART1 RTS1 unavailable at Port pin. UART1 I/O unavailable at Port pin. 

    XBR0 = XBR0_URT0E__ENABLED | XBR0_SPI0E__DISABLED | XBR0_SMB0E__DISABLED //UART0 TX0, RX0 routed to Port pins P0.4 and P0.5. SPI I/O unavailable at Port pins. 
			| XBR0_CP0E__DISABLED | XBR0_CP0AE__DISABLED | XBR0_CP1E__DISABLED		// SMBus 0 I/O unavailable at Port pins.  CP0 unavailable at Port pin. 
			| XBR0_CP1AE__DISABLED | XBR0_SYSCKE__DISABLED;												// Asynchronous CP0 unavailable at Port pin.  CP1 unavailable at Port pin. 
																																						// Asynchronous CP1 unavailable at Port pin. SYSCLK unavailable at Port pin. 

    /* Timer setting ----------------*/
    TCON &= ~TCON_TR0__BMASK & ~TCON_TR1__BMASK;    //close timer
    TH1 = (/*0xe7*/0x96 << TH1_TH1__SHIFT);

    CKCON0 = CKCON0_SCA__SYSCLK_DIV_12 | CKCON0_T0M__PRESCALE       // System clock divided by 12.  Counter/Timer 0 uses the clock defined by the prescale field, SCA. 
			| CKCON0_T2MH__EXTERNAL_CLOCK | CKCON0_T2ML__EXTERNAL_CLOCK   // Timer 2 low/high byte uses the clock defined by T2XCLK in TMR2CN0. 
			| CKCON0_T3MH__EXTERNAL_CLOCK | CKCON0_T3ML__EXTERNAL_CLOCK   // Timer 3 low/high byte uses the clock defined by T3XCLK in TMR3CN0. 
			| CKCON0_T1M__SYSCLK;  																				//Timer 1 uses the system clock.
            
    TMOD = TMOD_T0M__MODE0 | TMOD_T1M__MODE2 | TMOD_CT0__TIMER      		// 13-bit Counter/Timer , 8-bit Counter/Timer with Auto-Reload 
			| TMOD_GATE0__DISABLED | TMOD_CT1__TIMER | TMOD_GATE1__DISABLED; 	//	Timer 0 increments on the clock defined by T0M in the CKCON0 register. 
    TCON |= TCON_TR1__RUN;	// open timer																// Timer 0 enabled when TR0 = 1 irrespective of INT0 logic level.	

    /* with current SYSCLK and timer2 setting, tick of timer is 0.4897us, and with 63493 set to timer2's H and L byte we get a timer which interrupt interval is 1 ms */
    
    TMR2H = 0xf8; /* may be this is not necessary */
    TMR2L = 0x05; /* may be this is not necessary */
    TMR2RLH = 0xf8; // TMR2RLH holds the reload value for the high byte of Timer 2 (TMR2H)
    TMR2RLL = 0x05; // TMR2RLL holds the reload value for the low byte of Timer 2 (TMR2L)
    TMR2CN0 = 0x04; // Timer 2 operates as two 8-bit auto-reload timers.
    IP |= 0x20; // This bit sets the LSB of the priority field for the Timer 2 interrupt.
    IPH |= 0x20; // This bit sets the MSB of the priority field for the Timer 2 interrupt. 
    TMR2CN1 = 0x00; // Timer will only reload on overflow events. Capture high-to-low transitions on the T2 input pin.

    /* set IR 38kHZ ,TIMER 3 CLOCK = 24M Hz/12 = 2.04M Hz, 2.04M/38K=53.684 , 53=110101, 0xFFFF=1111_1111_1111_1111, 0xFFFF-53=0xFFCA  */		 
    TMR3L=0xca; // Timer 3 current value (low byte)
    TMR3H=0xff; // Timer 3 current value (high byte)
    TMR3RLL=0xca; // Timer 3 Reload Low Byte
    TMR3RLH=0xff; // Timer 3 Reload Hight Byte
    TMR3CN1 |= 0x60; // Timer will only reload on overflow events
    TMR3CN0 = 0x04;	// Enable timer3 , Timer 3 clock is the system clock divided by 12. 
    
    
    EIE1	|= 0x80;				 // Enable interrupt requests generated by the TF3L or TF3H flags.
    //EIP1H |=0x80;

    /* DAC setting ------------------*/    
    SFRPAGE = 0x20; 
    P3MDOUT = P3MDOUT_B0__OPEN_DRAIN | P3MDOUT_B1__OPEN_DRAIN
			| P3MDOUT_B2__OPEN_DRAIN | P3MDOUT_B3__OPEN_DRAIN
			| P3MDOUT_B4__PUSH_PULL | P3MDOUT_B7__OPEN_DRAIN;
    
    SFRPAGE = 0x30;
    DACGCF0 = 0x88; // DVC0/1/2/3 Reference Voltage Select. Select the VDD supply.
    DACGCF1 |= 0x08;
    DACGCF1 &= ~0x07;
    DACGCF2 &= ~0x33;
    DAC0CF0 = 0x80; /* last should have TODO which makes system more efficient  */
    DAC0CF1 = 0x00; // DAC0 output gain is 2. 
    DAC1CF0 = 0x80; // DAC1 is enabled and will drive the output pin.
    DAC1CF1 = 0x00;	// DAC1 output gain is 2. 
    DAC2CF0 = 0x80; // DAC2 is enabled and will drive the output pin.
    DAC2CF1 = 0x00;	// DAC2 output gain is 2. 
    
    /* Uart setting -----------------*/
    // wrapped inside the timer setting

    /* Struct member initialization ----------------*/
    mcu.sysTick = 0;

    escalator.intervalFlag = 0;
    escalator.mode = NORMAL;
    for (index = 0; index < 3; index++)
    {
        escalator.arm[index].variability[0] = 0;
        escalator.arm[index].variability[1] = 0;
        escalator.arm[index].variability[2] = 0;
        escalator.arm[index].variability[3] = 0;
        escalator.arm[index].variability[4] = 0;
        escalator.arm[index].lastPos = POS_INIT;
        escalator.arm[index].currentPos = 0;
    }
    
    SFRPAGE = PG4_PAGE;
    DAC0L = 0x0;  //Low byte of data input.
    DAC0H = 0x00; //High byte of data input.
    DAC1L = 0x0;
    DAC1H = 0x00;
    DAC2L = 0x0;
    DAC2H = 0x00;

    /* Interrupt setting-------------*/
    IE = IE_EA__DISABLED | IE_EX0__DISABLED | IE_EX1__DISABLED     //Disable all interrupt sources.  Disable external interrupt 0/1.
        | IE_ESPI0__DISABLED | IE_ET0__DISABLED | IE_ET1__DISABLED  //  Disable all SPI0 interrupts. Disable all Timer 0/1 interrupt.
        | IE_ET2__ENABLED | IE_ES0__ENABLED;				//  Enable interrupt requests generated by the TF2L or TF2H flags. Enable UART0 interrupt.
    SFRPAGE = PG2_PAGE;
   
    SFRPAGE = SFRPAGE_save; /* restore SFRPAGE */
}







/*************flawless0714 * END OF FILE****/
