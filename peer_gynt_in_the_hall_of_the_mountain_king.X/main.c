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
void peer_gynt_mountain_king(void);
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
    printf("Peeg Gynt - In the hall of the mountain king.\n\r");
    peer_gynt_mountain_king();
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
    frequency = (uint16_t)((double)frequency / 4);
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

void peer_gynt_mountain_king() {
    Beep(184, 1654);
	sleep(84);
	Beep(61, 106);
	sleep(109);
	Beep(69, 106);
	sleep(110);
	Beep(73, 106);
	sleep(109);
	Beep(82, 106);
	sleep(110);
	Beep(92, 106);
	sleep(109);
	Beep(73, 106);
	sleep(110);
	Beep(92, 213);
	sleep(220);
	Beep(87, 106);
	sleep(109);
	Beep(69, 106);
	sleep(110);
	Beep(87, 213);
	sleep(220);
	Beep(82, 106);
	sleep(109);
	Beep(65, 106);
	sleep(110);
	Beep(82, 213);
	sleep(220);
	Beep(61, 106);
	sleep(109);
	Beep(69, 106);
	sleep(110);
	Beep(73, 106);
	sleep(109);
	Beep(82, 106);
	sleep(110);
	Beep(92, 106);
	sleep(109);
	Beep(73, 106);
	sleep(110);
	Beep(92, 106);
	sleep(109);
	Beep(123, 106);
	sleep(110);
	Beep(110, 106);
	sleep(109);
	Beep(92, 106);
	sleep(110);
	Beep(73, 106);
	sleep(109);
	Beep(92, 106);
	sleep(110);
	Beep(110, 781);
	sleep(84);
	Beep(123, 106);
	sleep(109);
	Beep(138, 106);
	sleep(110);
	Beep(146, 106);
	sleep(109);
	Beep(164, 106);
	sleep(110);
	Beep(184, 106);
	sleep(109);
	Beep(146, 106);
	sleep(110);
	Beep(184, 213);
	sleep(220);
	Beep(174, 106);
	sleep(109);
	Beep(138, 106);
	sleep(110);
	Beep(174, 213);
	sleep(220);
	Beep(164, 106);
	sleep(109);
	Beep(130, 106);
	sleep(110);
	Beep(164, 213);
	sleep(220);
	Beep(123, 106);
	sleep(109);
	Beep(138, 106);
	sleep(110);
	Beep(146, 106);
	sleep(109);
	Beep(164, 106);
	sleep(110);
	Beep(184, 106);
	sleep(109);
	Beep(146, 106);
	sleep(110);
	Beep(184, 106);
	sleep(109);
	Beep(246, 106);
	sleep(110);
	Beep(220, 106);
	sleep(109);
	Beep(184, 106);
	sleep(110);
	Beep(146, 106);
	sleep(109);
	Beep(184, 106);
	sleep(110);
	Beep(110, 781);
	sleep(84);
	Beep(92, 106);
	sleep(109);
	Beep(103, 106);
	sleep(110);
	Beep(116, 106);
	sleep(109);
	Beep(123, 106);
	sleep(110);
	Beep(138, 106);
	sleep(109);
	Beep(116, 106);
	sleep(110);
	Beep(138, 213);
	sleep(220);
	Beep(146, 106);
	sleep(109);
	Beep(116, 106);
	sleep(110);
	Beep(146, 213);
	sleep(220);
	Beep(138, 106);
	sleep(109);
	Beep(116, 106);
	sleep(110);
	Beep(138, 213);
	sleep(220);
	Beep(92, 106);
	sleep(109);
	Beep(103, 106);
	sleep(110);
	Beep(116, 106);
	sleep(109);
	Beep(123, 106);
	sleep(110);
	Beep(138, 106);
	sleep(109);
	Beep(116, 106);
	sleep(110);
	Beep(138, 213);
	sleep(220);
	Beep(146, 106);
	sleep(109);
	Beep(116, 106);
	sleep(110);
	Beep(146, 213);
	sleep(220);
	Beep(138, 425);
	sleep(8);
	Beep(69, 213);
	sleep(220);
	Beep(184, 106);
	sleep(109);
	Beep(207, 106);
	sleep(110);
	Beep(233, 106);
	sleep(109);
	Beep(246, 106);
	sleep(110);
	Beep(277, 106);
	sleep(109);
	Beep(233, 106);
	sleep(110);
	Beep(277, 213);
	sleep(220);
	Beep(293, 106);
	sleep(109);
	Beep(233, 106);
	sleep(110);
	Beep(293, 213);
	sleep(220);
	Beep(277, 106);
	sleep(109);
	Beep(233, 106);
	sleep(110);
	Beep(277, 213);
	sleep(220);
	Beep(184, 106);
	sleep(109);
	Beep(207, 106);
	sleep(110);
	Beep(233, 106);
	sleep(109);
	Beep(246, 106);
	sleep(110);
	Beep(277, 106);
	sleep(109);
	Beep(233, 106);
	sleep(110);
	Beep(277, 213);
	sleep(220);
	Beep(293, 106);
	sleep(109);
	Beep(233, 106);
	sleep(110);
	Beep(293, 213);
	sleep(220);
	Beep(277, 425);
	sleep(8);
	Beep(69, 213);
	sleep(220);
	Beep(61, 103);
	sleep(106);
	Beep(69, 103);
	sleep(107);
	Beep(73, 103);
	sleep(106);
	Beep(82, 103);
	sleep(107);
	Beep(92, 103);
	sleep(106);
	Beep(73, 103);
	sleep(107);
	Beep(92, 207);
	sleep(214);
	Beep(87, 103);
	sleep(106);
	Beep(69, 103);
	sleep(107);
	Beep(87, 207);
	sleep(214);
	Beep(82, 103);
	sleep(106);
	Beep(65, 103);
	sleep(107);
	Beep(82, 207);
	sleep(214);
	Beep(61, 103);
	sleep(106);
	Beep(69, 103);
	sleep(107);
	Beep(73, 103);
	sleep(106);
	Beep(82, 103);
	sleep(107);
	Beep(92, 103);
	sleep(106);
	Beep(73, 103);
	sleep(107);
	Beep(92, 103);
	sleep(106);
	Beep(123, 103);
	sleep(107);
	Beep(110, 103);
	sleep(106);
	Beep(92, 103);
	sleep(107);
	Beep(73, 103);
	sleep(106);
	Beep(92, 103);
	sleep(107);
	Beep(110, 760);
	sleep(82);
	Beep(123, 103);
	sleep(106);
	Beep(138, 103);
	sleep(107);
	Beep(146, 103);
	sleep(106);
	Beep(164, 103);
	sleep(107);
	Beep(184, 103);
	sleep(106);
	Beep(146, 103);
	sleep(107);
	Beep(184, 207);
	sleep(214);
	Beep(174, 103);
	sleep(106);
	Beep(138, 103);
	sleep(107);
	Beep(174, 207);
	sleep(214);
	Beep(164, 103);
	sleep(106);
	Beep(130, 103);
	sleep(107);
	Beep(164, 207);
	sleep(214);
	Beep(123, 103);
	sleep(106);
	Beep(138, 103);
	sleep(107);
	Beep(146, 103);
	sleep(106);
	Beep(164, 103);
	sleep(107);
	Beep(184, 103);
	sleep(106);
	Beep(146, 103);
	sleep(107);
	Beep(184, 103);
	sleep(106);
	Beep(246, 103);
	sleep(107);
	Beep(184, 103);
	sleep(106);
	Beep(146, 103);
	sleep(107);
	Beep(184, 103);
	sleep(106);
	Beep(246, 103);
	sleep(107);
	Beep(123, 760);
	sleep(82);
	Beep(246, 103);
	sleep(106);
	Beep(277, 103);
	sleep(107);
	Beep(184, 103);
	sleep(1);
	Beep(87, 95);
	sleep(9);
	Beep(329, 103);
	sleep(107);
	Beep(369, 103);
	sleep(106);
	Beep(293, 103);
	sleep(107);
	Beep(184, 205);
	sleep(3);
	Beep(92, 103);
	sleep(107);
	Beep(349, 103);
	sleep(106);
	Beep(277, 103);
	sleep(107);
	Beep(184, 205);
	sleep(3);
	Beep(92, 103);
	sleep(107);
	Beep(329, 103);
	sleep(106);
	Beep(261, 103);
	sleep(107);
	Beep(184, 205);
	sleep(3);
	Beep(92, 103);
	sleep(107);
	Beep(246, 103);
	sleep(106);
	Beep(277, 103);
	sleep(107);
	Beep(184, 103);
	sleep(1);
	Beep(87, 95);
	sleep(9);
	Beep(329, 103);
	sleep(107);
	Beep(369, 103);
	sleep(106);
	Beep(293, 103);
	sleep(107);
	Beep(369, 103);
	sleep(1);
	Beep(87, 95);
	sleep(9);
	Beep(493, 103);
	sleep(107);
	Beep(440, 103);
	sleep(106);
	Beep(369, 103);
	sleep(107);
	Beep(220, 103);
	sleep(1);
	Beep(103, 95);
	sleep(9);
	Beep(369, 103);
	sleep(107);
	Beep(440, 756);
	sleep(82);
	Beep(493, 103);
	sleep(106);
	Beep(554, 103);
	sleep(107);
	Beep(369, 103);
	sleep(1);
	Beep(87, 95);
	sleep(9);
	Beep(659, 103);
	sleep(107);
	Beep(739, 103);
	sleep(106);
	Beep(587, 103);
	sleep(107);
	Beep(369, 205);
	sleep(3);
	Beep(92, 103);
	sleep(107);
	Beep(698, 103);
	sleep(106);
	Beep(554, 103);
	sleep(107);
	Beep(369, 205);
	sleep(3);
	Beep(92, 103);
	sleep(107);
	Beep(659, 103);
	sleep(106);
	Beep(523, 103);
	sleep(107);
	Beep(369, 205);
	sleep(3);
	Beep(92, 103);
	sleep(107);
	Beep(493, 103);
	sleep(106);
	Beep(554, 103);
	sleep(107);
	Beep(369, 103);
	sleep(1);
	Beep(87, 95);
	sleep(9);
	Beep(659, 103);
	sleep(107);
	Beep(739, 103);
	sleep(106);
	Beep(587, 103);
	sleep(107);
	Beep(739, 103);
	sleep(1);
	Beep(87, 95);
	sleep(9);
	Beep(987, 103);
	sleep(107);
	Beep(880, 103);
	sleep(106);
	Beep(739, 103);
	sleep(107);
	Beep(440, 103);
	sleep(1);
	Beep(103, 95);
	sleep(9);
	Beep(739, 103);
	sleep(107);
	Beep(880, 756);
	sleep(82);
	Beep(369, 103);
	sleep(106);
	Beep(415, 103);
	sleep(107);
	Beep(466, 103);
	sleep(1);
	Beep(130, 95);
	sleep(9);
	Beep(493, 103);
	sleep(107);
	Beep(554, 103);
	sleep(106);
	Beep(466, 103);
	sleep(107);
	Beep(554, 205);
	sleep(3);
	Beep(138, 103);
	sleep(107);
	Beep(587, 103);
	sleep(106);
	Beep(466, 103);
	sleep(107);
	Beep(587, 205);
	sleep(3);
	Beep(146, 103);
	sleep(107);
	Beep(554, 103);
	sleep(106);
	Beep(466, 103);
	sleep(107);
	Beep(554, 205);
	sleep(3);
	Beep(138, 103);
	sleep(107);
	Beep(369, 103);
	sleep(106);
	Beep(415, 103);
	sleep(107);
	Beep(466, 103);
	sleep(1);
	Beep(130, 95);
	sleep(9);
	Beep(493, 103);
	sleep(107);
	Beep(554, 103);
	sleep(106);
	Beep(466, 103);
	sleep(107);
	Beep(554, 205);
	sleep(3);
	Beep(138, 103);
	sleep(107);
	Beep(587, 103);
	sleep(106);
	Beep(466, 103);
	sleep(107);
	Beep(587, 205);
	sleep(3);
	Beep(146, 103);
	sleep(107);
	Beep(554, 413);
	sleep(8);
	Beep(369, 377);
	sleep(41);
	Beep(739, 103);
	sleep(106);
	Beep(830, 103);
	sleep(107);
	Beep(932, 103);
	sleep(1);
	Beep(130, 95);
	sleep(9);
	Beep(987, 103);
	sleep(107);
	Beep(1108, 103);
	sleep(106);
	Beep(932, 103);
	sleep(107);
	Beep(1108, 205);
	sleep(3);
	Beep(138, 103);
	sleep(107);
	Beep(1174, 103);
	sleep(106);
	Beep(932, 103);
	sleep(107);
	Beep(1174, 205);
	sleep(3);
	Beep(146, 103);
	sleep(107);
	Beep(1108, 103);
	sleep(106);
	Beep(932, 103);
	sleep(107);
	Beep(1108, 205);
	sleep(3);
	Beep(138, 103);
	sleep(107);
	Beep(739, 103);
	sleep(106);
	Beep(830, 103);
	sleep(107);
	Beep(932, 103);
	sleep(1);
	Beep(130, 95);
	sleep(9);
	Beep(987, 103);
	sleep(107);
	Beep(1108, 103);
	sleep(106);
	Beep(932, 103);
	sleep(107);
	Beep(1108, 205);
	sleep(3);
	Beep(138, 103);
	sleep(107);
	Beep(1174, 103);
	sleep(106);
	Beep(932, 103);
	sleep(107);
	Beep(1174, 205);
	sleep(3);
	Beep(146, 103);
	sleep(107);
	Beep(1108, 413);
	sleep(8);
	Beep(739, 377);
	sleep(41);
	Beep(246, 101);
	sleep(34);
	Beep(87, 123);
	sleep(12);
	Beep(92, 125);
	sleep(12);
	Beep(184, 101);
	sleep(34);
	Beep(87, 123);
	sleep(12);
	Beep(92, 125);
	sleep(12);
	Beep(369, 101);
	sleep(34);
	Beep(87, 123);
	sleep(12);
	Beep(92, 125);
	sleep(12);
	Beep(184, 202);
	sleep(70);
	Beep(92, 126);
	sleep(12);
	Beep(349, 101);
	sleep(34);
	Beep(87, 123);
	sleep(12);
	Beep(92, 125);
	sleep(12);
	Beep(184, 202);
	sleep(70);
	Beep(92, 126);
	sleep(12);
	Beep(329, 101);
	sleep(34);
	Beep(87, 123);
	sleep(12);
	Beep(92, 125);
	sleep(12);
	Beep(184, 202);
	sleep(70);
	Beep(92, 126);
	sleep(12);
	Beep(246, 101);
	sleep(34);
	Beep(87, 123);
	sleep(12);
	Beep(92, 125);
	sleep(12);
	Beep(184, 101);
	sleep(34);
	Beep(87, 123);
	sleep(12);
	Beep(92, 125);
	sleep(12);
	Beep(369, 101);
	sleep(34);
	Beep(87, 123);
	sleep(12);
	Beep(92, 125);
	sleep(12);
	Beep(246, 101);
	sleep(34);
	Beep(87, 123);
	sleep(12);
	Beep(92, 125);
	sleep(12);
	Beep(440, 101);
	sleep(34);
	Beep(103, 123);
	sleep(12);
	Beep(110, 125);
	sleep(12);
	Beep(220, 101);
	sleep(34);
	Beep(103, 123);
	sleep(12);
	Beep(110, 125);
	sleep(12);
	Beep(440, 780);
	sleep(39);
	Beep(493, 99);
	sleep(34);
	Beep(87, 121);
	sleep(12);
	Beep(92, 123);
	sleep(12);
	Beep(369, 99);
	sleep(34);
	Beep(87, 121);
	sleep(12);
	Beep(92, 123);
	sleep(12);
	Beep(739, 99);
	sleep(34);
	Beep(87, 121);
	sleep(12);
	Beep(92, 123);
	sleep(12);
	Beep(369, 197);
	sleep(69);
	Beep(92, 123);
	sleep(12);
	Beep(698, 99);
	sleep(34);
	Beep(87, 121);
	sleep(12);
	Beep(92, 123);
	sleep(12);
	Beep(369, 197);
	sleep(69);
	Beep(92, 123);
	sleep(12);
	Beep(659, 99);
	sleep(34);
	Beep(87, 121);
	sleep(12);
	Beep(92, 123);
	sleep(12);
	Beep(369, 197);
	sleep(69);
	Beep(92, 123);
	sleep(12);
	Beep(493, 98);
	sleep(33);
	Beep(87, 118);
	sleep(12);
	Beep(92, 119);
	sleep(12);
	Beep(369, 94);
	sleep(97);
	Beep(659, 92);
	sleep(94);
	Beep(739, 90);
	sleep(93);
	Beep(587, 88);
	sleep(91);
	Beep(739, 86);
	sleep(89);
	Beep(987, 85);
	sleep(87);
	Beep(739, 83);
	sleep(86);
	Beep(587, 82);
	sleep(84);
	Beep(493, 242);
	sleep(81);
	Beep(493, 568);
	sleep(60);
	Beep(987, 77);
	sleep(80);
	Beep(1108, 77);
	sleep(80);
	Beep(1174, 77);
	sleep(80);
	Beep(1318, 77);
	sleep(79);
	Beep(1479, 77);
	sleep(80);
	Beep(1174, 77);
	sleep(80);
	Beep(1479, 154);
	sleep(2);
	Beep(92, 77);
	sleep(79);
	Beep(1396, 77);
	sleep(80);
	Beep(1108, 77);
	sleep(80);
	Beep(1396, 154);
	sleep(2);
	Beep(92, 77);
	sleep(79);
	Beep(1318, 77);
	sleep(80);
	Beep(1046, 77);
	sleep(80);
	Beep(1318, 154);
	sleep(2);
	Beep(92, 77);
	sleep(79);
	Beep(987, 77);
	sleep(80);
	Beep(1108, 77);
	sleep(80);
	Beep(1174, 77);
	sleep(80);
	Beep(1318, 77);
	sleep(79);
	Beep(1479, 77);
	sleep(80);
	Beep(1174, 77);
	sleep(80);
	Beep(1479, 77);
	sleep(80);
	Beep(1975, 77);
	sleep(79);
	Beep(1760, 77);
	sleep(80);
	Beep(1479, 77);
	sleep(80);
	Beep(1174, 77);
	sleep(80);
	Beep(1479, 77);
	sleep(79);
	Beep(1760, 566);
	sleep(61);
	Beep(987, 77);
	sleep(80);
	Beep(1108, 77);
	sleep(80);
	Beep(1174, 77);
	sleep(80);
	Beep(1318, 77);
	sleep(79);
	Beep(1479, 77);
	sleep(80);
	Beep(1174, 77);
	sleep(80);
	Beep(1479, 154);
	sleep(2);
	Beep(92, 77);
	sleep(79);
	Beep(1396, 77);
	sleep(80);
	Beep(1108, 77);
	sleep(80);
	Beep(1396, 154);
	sleep(2);
	Beep(92, 77);
	sleep(79);
	Beep(1318, 77);
	sleep(80);
	Beep(1046, 77);
	sleep(80);
	Beep(1318, 154);
	sleep(2);
	Beep(92, 77);
	sleep(79);
	Beep(987, 77);
	sleep(80);
	Beep(1108, 77);
	sleep(80);
	Beep(1174, 77);
	sleep(80);
	Beep(1318, 77);
	sleep(79);
	Beep(1479, 77);
	sleep(80);
	Beep(1174, 77);
	sleep(80);
	Beep(1479, 77);
	sleep(80);
	Beep(1975, 77);
	sleep(79);
	Beep(1760, 77);
	sleep(80);
	Beep(1479, 77);
	sleep(80);
	Beep(880, 77);
	sleep(80);
	Beep(1479, 77);
	sleep(79);
	Beep(1760, 566);
	sleep(61);
	Beep(1479, 77);
	sleep(80);
	Beep(1661, 77);
	sleep(80);
	Beep(1864, 77);
	sleep(80);
	Beep(1975, 77);
	sleep(79);
	Beep(2217, 77);
	sleep(80);
	Beep(1864, 77);
	sleep(79);
	Beep(2217, 154);
	sleep(2);
	Beep(138, 77);
	sleep(79);
	Beep(2349, 77);
	sleep(79);
	Beep(1864, 77);
	sleep(80);
	Beep(2349, 154);
	sleep(2);
	Beep(164, 77);
	sleep(80);
	Beep(2217, 77);
	sleep(79);
	Beep(1864, 77);
	sleep(80);
	Beep(2217, 154);
	sleep(2);
	Beep(138, 77);
	sleep(79);
	Beep(1479, 77);
	sleep(79);
	Beep(1661, 77);
	sleep(80);
	Beep(1864, 77);
	sleep(80);
	Beep(1975, 77);
	sleep(80);
	Beep(2217, 77);
	sleep(80);
	Beep(1864, 77);
	sleep(80);
	Beep(2217, 154);
	sleep(2);
	Beep(138, 77);
	sleep(80);
	Beep(2349, 77);
	sleep(80);
	Beep(1864, 77);
	sleep(80);
	Beep(2349, 154);
	sleep(2);
	Beep(164, 77);
	sleep(80);
	Beep(2217, 308);
	sleep(6);
	Beep(146, 77);
	sleep(80);
	Beep(138, 77);
	sleep(79);
	Beep(1479, 77);
	sleep(80);
	Beep(1661, 77);
	sleep(80);
	Beep(1864, 77);
	sleep(80);
	Beep(1975, 77);
	sleep(79);
	Beep(2217, 77);
	sleep(80);
	Beep(1864, 77);
	sleep(79);
	Beep(2217, 154);
	sleep(2);
	Beep(138, 77);
	sleep(79);
	Beep(2489, 77);
	sleep(79);
	Beep(1864, 77);
	sleep(80);
	Beep(2489, 154);
	sleep(2);
	Beep(174, 77);
	sleep(80);
	Beep(2217, 77);
	sleep(79);
	Beep(1864, 77);
	sleep(80);
	Beep(2217, 154);
	sleep(2);
	Beep(138, 77);
	sleep(79);
	Beep(1479, 77);
	sleep(79);
	Beep(1661, 77);
	sleep(80);
	Beep(1864, 77);
	sleep(80);
	Beep(1975, 77);
	sleep(80);
	Beep(2217, 77);
	sleep(80);
	Beep(1864, 77);
	sleep(80);
	Beep(2217, 154);
	sleep(2);
	Beep(138, 77);
	sleep(80);
	Beep(2489, 77);
	sleep(80);
	Beep(1864, 77);
	sleep(80);
	Beep(2489, 154);
	sleep(2);
	Beep(174, 77);
	sleep(80);
	Beep(2217, 308);
	sleep(6);
	Beep(155, 77);
	sleep(80);
	Beep(138, 77);
	sleep(79);
	Beep(987, 73);
	sleep(76);
	Beep(1108, 73);
	sleep(76);
	Beep(1174, 73);
	sleep(76);
	Beep(1318, 73);
	sleep(75);
	Beep(1479, 73);
	sleep(76);
	Beep(1174, 73);
	sleep(76);
	Beep(1479, 146);
	sleep(2);
	Beep(92, 73);
	sleep(75);
	Beep(1396, 73);
	sleep(76);
	Beep(1108, 73);
	sleep(76);
	Beep(1396, 146);
	sleep(2);
	Beep(92, 73);
	sleep(75);
	Beep(1318, 73);
	sleep(76);
	Beep(1046, 73);
	sleep(76);
	Beep(1318, 146);
	sleep(2);
	Beep(92, 73);
	sleep(75);
	Beep(987, 73);
	sleep(76);
	Beep(1108, 73);
	sleep(76);
	Beep(1174, 73);
	sleep(76);
	Beep(1318, 73);
	sleep(75);
	Beep(1479, 73);
	sleep(76);
	Beep(1174, 73);
	sleep(76);
	Beep(1479, 73);
	sleep(76);
	Beep(1975, 73);
	sleep(75);
	Beep(1760, 73);
	sleep(76);
	Beep(1479, 73);
	sleep(76);
	Beep(1174, 73);
	sleep(76);
	Beep(1479, 73);
	sleep(75);
	Beep(1760, 537);
	sleep(58);
	Beep(987, 72);
	sleep(73);
	Beep(1108, 72);
	sleep(74);
	Beep(1174, 72);
	sleep(74);
	Beep(1318, 72);
	sleep(74);
	Beep(1479, 72);
	sleep(74);
	Beep(1174, 72);
	sleep(74);
	Beep(1479, 144);
	sleep(2);
	Beep(92, 72);
	sleep(74);
	Beep(1396, 72);
	sleep(73);
	Beep(1108, 72);
	sleep(73);
	Beep(1396, 143);
	sleep(2);
	Beep(92, 72);
	sleep(74);
	Beep(1318, 72);
	sleep(73);
	Beep(1046, 72);
	sleep(73);
	Beep(1318, 143);
	sleep(2);
	Beep(92, 72);
	sleep(74);
	Beep(987, 72);
	sleep(73);
	Beep(1108, 72);
	sleep(74);
	Beep(1174, 72);
	sleep(74);
	Beep(1318, 72);
	sleep(74);
	Beep(1479, 70);
	sleep(72);
	Beep(1174, 70);
	sleep(72);
	Beep(1479, 68);
	sleep(70);
	Beep(1975, 68);
	sleep(70);
	Beep(1479, 67);
	sleep(69);
	Beep(1174, 67);
	sleep(69);
	Beep(1479, 67);
	sleep(69);
	Beep(1975, 67);
	sleep(69);
	Beep(987, 490);
	sleep(53);
	Beep(61, 207);
	sleep(22);
	Beep(1174, 207);
	sleep(484);
	Beep(61, 207);
	sleep(22);
	Beep(1174, 207);
	sleep(484);
	Beep(493, 56);
	sleep(58);
	Beep(554, 56);
	sleep(58);
	Beep(587, 56);
	sleep(58);
	Beep(659, 56);
	sleep(58);
	Beep(739, 56);
	sleep(58);
	Beep(587, 56);
	sleep(58);
	Beep(739, 56);
	sleep(58);
	Beep(987, 56);
	sleep(58);
	Beep(932, 56);
	sleep(58);
	Beep(739, 56);
	sleep(58);
	Beep(932, 56);
	sleep(58);
	Beep(1108, 56);
	sleep(58);
	Beep(987, 415);
	sleep(44);
	Beep(61, 207);
	sleep(22);
	Beep(1174, 207);
	sleep(484);
	Beep(61, 207);
	sleep(22);
	Beep(1174, 207);
	sleep(484);
	Beep(987, 56);
	sleep(58);
	Beep(1108, 56);
	sleep(58);
	Beep(1174, 56);
	sleep(58);
	Beep(1318, 56);
	sleep(58);
	Beep(1479, 56);
	sleep(58);
	Beep(1174, 56);
	sleep(58);
	Beep(1479, 56);
	sleep(58);
	Beep(1975, 56);
	sleep(58);
	Beep(1864, 56);
	sleep(58);
	Beep(1479, 56);
	sleep(58);
	Beep(1864, 56);
	sleep(58);
	Beep(2217, 56);
	sleep(58);
	Beep(1975, 415);
	sleep(44);
	Beep(61, 207);
	sleep(22);
	Beep(1174, 208);
	sleep(484);
	Beep(61, 207);
	sleep(22);
	Beep(1174, 207);
	sleep(484);
	Beep(61, 207);
	sleep(22);
	Beep(1174, 206);
	sleep(22);
	Beep(1174, 206);
	sleep(22);
	Beep(1174, 207);
	sleep(22);
	Beep(1661, 41);
	Beep(1760, 68);
	Beep(1864, 39);
	Beep(1174, 56);
	sleep(22);
	Beep(1174, 206);
	sleep(22);
	Beep(1174, 206);
	sleep(22);
	Beep(1174, 207);
	sleep(484);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(61, 52);
	sleep(5);
	Beep(123, 52);
	sleep(5);
	Beep(30, 207);
	sleep(22);
	Beep(1174, 207);
	sleep(40110);
	Beep(12, 8496);
}