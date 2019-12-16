#include <xc.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#define _XTAL_FREQ 64000000

#pragma config FOSC = INTIO67   // Oscillator Selection bits (Internal oscillator block)
#pragma config PLLCFG = 0     // 4X PLL Enable (Oscillator used directly)
#pragma config PRICLKEN = 1    // Primary clock enable bit (Primary clock enabled)
#pragma config FCMEN = 0      // Fail-Safe Clock Monitor Enable bit (Fail-Safe Clock Monitor disabled)
#pragma config IESO = 0       // Internal/External Oscillator Switchover bit (Oscillator Switchover mode disabled)

// CONFIG2L
#pragma config PWRTEN = 0     // Power-up Timer Enable bit (Power up timer disabled)
#pragma config BOREN = SBORDIS  // Brown-out Reset Enable bits (Brown-out Reset enabled in hardware only (SBOREN is disabled))
#pragma config BORV = 190       // Brown Out Reset Voltage bits (VBOR set to 1.90 V nominal)

// CONFIG2H
#pragma config WDTEN = 0       // Watchdog Timer Enable bits (WDT is always enabled. SWDTEN bit has no effect)
#pragma config WDTPS = 32768    // Watchdog Timer Postscale Select bits (1:32768)

void OSCILLATOR_Initialize(void);
void TIMER1_Initialize(void);
void UART_Initialize(void);
void putch(char c);
char read_char(void);
void read_line(char * s, int max_len);
void Beep(uint16_t frequency, uint16_t duration);
void sleep(uint16_t ms);
void happy_birthday(void);
void __interrupt() ISR(void);

uint16_t offset;
uint8_t cnt = 0, on = 0;

void main(void) {
    int f, ms = 100;
    char str[20];
    OSCILLATOR_Initialize();
    UART_Initialize();
    printf("\n\rRESET\n\r"); //debug
    TIMER1_Initialize(); //tone generator timer
    INTCONbits.GIE = 1; // global enable interrupts
    INTCONbits.PEIE = 1; // peripheral enable interrupts
    ANSELAbits.ANSA0 = 0; //set RA0 as digital
    ANSELAbits.ANSA1 = 0; //set RA1 as digital
    TRISAbits.RA0 = 0; //RA0 as output
    TRISAbits.RA1 = 0; //RA1 as output
    LATAbits.LA1 = 0; //floppy dyrection
    LATAbits.LA0 = 0; //floppy square wave
    Beep(698, 2714); //put head in 0 position
    on = 1;
    cnt = 0;
    LATAbits.LA1 = ~LATAbits.LA1;
    sleep(1714);
    printf("Bach - badineri.\n\r");
    happy_birthday();
    while (1) {
        printf("Digitare una frequenza intera: ");
        read_line(str, 20);
        if (str[0] != 0) f = atoi(str);
        Beep(f, ms);
    }
    return;
}

void OSCILLATOR_Initialize(void) {
    // setup oscillator
    OSCCONbits.SCS = 0b00; // primary clock determined by CONFIG1H
    OSCCONbits.IRCF = 0b111; // 16 MHz internal oscillator
    OSCTUNEbits.PLLEN = 1; // Enable PLL
    while (OSCCONbits.HFIOFS == 0); // busy-wait until high frequency
    // oscillator becomes stable
    // now, using 16MHz + 4xPLL, we have an FOSC of 64 MHz
}

void TIMER1_Initialize(void) { //tone generator timer
    T1CONbits.TMR1ON = 0; //stop the timer
    T1CONbits.TMR1CS = 0; //Timer1 clock source is instruction clock (FOSC/4)
    T1CONbits.T1RD16 = 1; //Enables register read/write of Timer1 in one 16-bit operation
    T1CONbits.T1CKPS = 3; //1:8 Prescale value
    TMR1 = 0x0000;
    PIR1bits.TMR1IF = 0;
    PIE1bits.TMR1IE = 1;
}

void UART_Initialize(void) {
    ANSELA = 0;
    ANSELC = 0;
    // setup UART
    TRISCbits.TRISC6 = 0; // TX
    TRISCbits.TRISC7 = 1; // RX

    TXSTA1bits.SYNC = 0;
    TXSTA1bits.TX9 = 0;
    TXSTA1bits.TXEN = 1;

    RCSTA1bits.RX9 = 0;
    RCSTA1bits.CREN = 1;
    RCSTA1bits.SPEN = 1;

    BAUDCON1bits.BRG16 = 0;
    TXSTA1bits.BRGH = 0;
    //SPBRG1 = 8; // 115200
    SPBRG1 = 51; // 19200
    SPBRGH1 = 0;
}

void putch(char c) {
    // wait the end of transmission
    while (TXSTA1bits.TRMT == 0) {
    };
    TXREG1 = c; // send the new byte
}

char read_char(void) {
    while (PIR1bits.RC1IF == 0) {
        if (RCSTA1bits.OERR == 1) {
            RCSTA1bits.OERR = 0;
            RCSTA1bits.CREN = 0;
            RCSTA1bits.CREN = 1;
        }
    }
    return RCREG1;
}

void read_line(char * s, int max_len) {
    int i = 0;
    for (;;) {
        char c = read_char();
        if (c == 13) {
            putchar(c);
            putchar(10);
            s[i] = 0;
            return;
        } else if (c == 127 || c == 8) {
            if (i > 0) {
                putchar(c);
                putchar(' ');
                putchar(c);
                --i;
            }
        } else if (c >= 32) {
            if (i < max_len) {
                putchar(c);
                s[i] = c;
                ++i;
            }
        }
    }
}

void Beep(uint16_t frequency, uint16_t duration) {
    int i;
    frequency = (uint16_t)((double)frequency/4);
    offset = (uint16_t) ((double) 65535 - ((double) 1000000 / (double) frequency)); //load start timer value
    TMR1 = offset;
    T1CONbits.TMR1ON = 1; //start tone generator timer
    for (i = 0; i < duration; i++) {
        __delay_ms(1);
    }
    T1CONbits.TMR1ON = 0; //stop tone generator timer
}

void sleep(uint16_t ms) {
    int i;
    for (i = 0; i < ms; i++) {
        __delay_ms(1);
    }
}

void __interrupt() ISR(void) {
    if (PIR1bits.TMR1IF) { //tone generator timer
        TMR1 = offset;
        cnt++;
        LATAbits.LA0 = ~LATAbits.LA0;
        PIR1bits.TMR1IF = 0;
    }
    if (cnt == 140 && on == 1) { //inversion counter
        cnt = 0;
        LATAbits.LA1 = ~LATAbits.LA1;
    }
}

void happy_birthday(void)   {	Beep(391, 230);
	Beep(391, 230);
	Beep(440, 461);
	Beep(391, 461);
	Beep(523, 461);
	Beep(493, 923);
	Beep(391, 230);
	Beep(391, 230);
	Beep(440, 461);
	Beep(391, 461);
	Beep(587, 461);
	Beep(523, 923);
	Beep(391, 230);
	Beep(391, 230);
	Beep(783, 461);
	Beep(659, 461);
	Beep(523, 230);
	Beep(523, 230);
	Beep(493, 461);
	Beep(440, 461);
	Beep(698, 230);
	Beep(698, 230);
	Beep(659, 461);
	Beep(523, 461);
	Beep(587, 461);
	Beep(523, 923);
	Beep(391, 230);
	Beep(391, 230);
	Beep(440, 461);
	Beep(391, 461);
	Beep(523, 461);
	Beep(493, 923);
	Beep(391, 230);
	Beep(391, 230);
	Beep(440, 461);
	Beep(391, 461);
	Beep(587, 461);
	Beep(523, 923);
	Beep(391, 230);
	Beep(391, 230);
	Beep(783, 465);
	Beep(659, 465);
	Beep(523, 232);
	Beep(523, 232);
	Beep(493, 480);
	Beep(440, 480);
	Beep(698, 247);
	Beep(698, 247);
	Beep(659, 512);
	Beep(523, 512);
	Beep(587, 512);
	Beep(523, 1025);
}