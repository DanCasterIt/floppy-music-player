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
void badineri(void);
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
    badineri();
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

void badineri() {
    Beep(987, 177);
	sleep(62);
	Beep(1174, 85);
	sleep(33);
	Beep(987, 56);
	sleep(81);
	Beep(739, 204);
	sleep(37);
	Beep(987, 70);
	sleep(43);
	Beep(739, 62);
	sleep(91);
	Beep(587, 197);
	sleep(54);
	Beep(739, 56);
	sleep(39);
	Beep(587, 72);
	sleep(64);
	Beep(493, 272);
	sleep(218);
	Beep(369, 112);
	sleep(16);
	Beep(493, 87);
	sleep(54);
	Beep(587, 104);
	sleep(2);
	Beep(493, 72);
	sleep(60);
	Beep(554, 100);
	sleep(12);
	Beep(493, 77);
	sleep(60);
	Beep(554, 104);
	sleep(6);
	Beep(493, 93);
	sleep(52);
	Beep(466, 87);
	sleep(22);
	Beep(554, 102);
	sleep(22);
	Beep(659, 100);
	sleep(18);
	Beep(554, 77);
	sleep(60);
	Beep(587, 108);
	sleep(141);
	Beep(493, 83);
	sleep(166);
	Beep(987, 208);
	sleep(31);
	Beep(1174, 87);
	sleep(25);
	Beep(987, 60);
	sleep(87);
	Beep(739, 208);
	sleep(47);
	Beep(987, 66);
	sleep(33);
	Beep(739, 75);
	sleep(68);
	Beep(587, 202);
	sleep(47);
	Beep(739, 77);
	sleep(41);
	Beep(587, 75);
	sleep(60);
	Beep(493, 329);
	sleep(164);
	Beep(587, 125);
	sleep(120);
	Beep(587, 127);
	sleep(131);
	Beep(587, 122);
	sleep(135);
	Beep(587, 127);
	sleep(127);
	Beep(987, 100);
	sleep(131);
	Beep(587, 131);
	sleep(125);
	Beep(659, 60);
	sleep(50);
	Beep(659, 61);
	sleep(72);
	Beep(554, 155);
	sleep(102);
	Beep(739, 116);
	sleep(141);
	Beep(739, 112);
	sleep(141);
	Beep(739, 112);
	sleep(131);
	Beep(739, 116);
	sleep(133);
	Beep(1174, 118);
	sleep(133);
	Beep(739, 143);
	sleep(100);
	Beep(830, 59);
	sleep(68);
	Beep(830, 84);
	sleep(22);
	Beep(698, 136);
	sleep(137);
	Beep(554, 114);
	sleep(131);
	Beep(880, 81);
	sleep(31);
	Beep(739, 83);
	sleep(50);
	Beep(830, 102);
	sleep(8);
	Beep(739, 66);
	sleep(52);
	Beep(830, 100);
	sleep(25);
	Beep(739, 83);
	sleep(37);
	Beep(698, 100);
	sleep(29);
	Beep(830, 72);
	sleep(50);
	Beep(987, 95);
	sleep(27);
	Beep(830, 62);
	sleep(64);
	Beep(880, 89);
	sleep(39);
	Beep(830, 66);
	sleep(56);
	Beep(880, 89);
	sleep(41);
	Beep(830, 72);
	sleep(60);
	Beep(739, 66);
	sleep(54);
	Beep(880, 60);
	sleep(66);
	Beep(739, 141);
	sleep(99);
	Beep(739, 68);
	sleep(58);
	Beep(987, 66);
	sleep(58);
	Beep(739, 151);
	sleep(108);
	Beep(739, 56);
	sleep(64);
	Beep(1108, 89);
	sleep(29);
	Beep(739, 122);
	sleep(2);
	Beep(698, 142);
	sleep(101);
	Beep(1174, 75);
	sleep(45);
	Beep(739, 106);
	sleep(10);
	Beep(698, 155);
	sleep(90);
	Beep(1174, 79);
	sleep(43);
	Beep(1108, 75);
	sleep(54);
	Beep(987, 95);
	sleep(54);
	Beep(1108, 79);
	sleep(39);
	Beep(880, 81);
	sleep(43);
	Beep(830, 56);
	sleep(41);
	Beep(739, 85);
	sleep(70);
	Beep(880, 102);
	sleep(147);
	Beep(880, 53);
	sleep(45);
	Beep(880, 65);
	sleep(72);
	Beep(739, 218);
	sleep(295);
	Beep(987, 177);
	sleep(62);
	Beep(1174, 85);
	sleep(33);
	Beep(987, 56);
	sleep(81);
	Beep(739, 204);
	sleep(37);
	Beep(987, 70);
	sleep(43);
	Beep(739, 62);
	sleep(91);
	Beep(587, 197);
	sleep(54);
	Beep(739, 56);
	sleep(39);
	Beep(587, 72);
	sleep(64);
	Beep(493, 272);
	sleep(218);
	Beep(369, 112);
	sleep(16);
	Beep(493, 87);
	sleep(54);
	Beep(587, 104);
	sleep(2);
	Beep(493, 72);
	sleep(60);
	Beep(554, 100);
	sleep(12);
	Beep(493, 77);
	sleep(60);
	Beep(554, 104);
	sleep(6);
	Beep(493, 93);
	sleep(52);
	Beep(466, 87);
	sleep(22);
	Beep(554, 102);
	sleep(22);
	Beep(659, 100);
	sleep(18);
	Beep(554, 77);
	sleep(60);
	Beep(587, 108);
	sleep(141);
	Beep(493, 83);
	sleep(166);
	Beep(987, 208);
	sleep(31);
	Beep(1174, 87);
	sleep(25);
	Beep(987, 60);
	sleep(87);
	Beep(739, 208);
	sleep(47);
	Beep(987, 66);
	sleep(33);
	Beep(739, 75);
	sleep(68);
	Beep(587, 202);
	sleep(47);
	Beep(739, 77);
	sleep(41);
	Beep(587, 75);
	sleep(60);
	Beep(493, 329);
	sleep(164);
	Beep(587, 125);
	sleep(120);
	Beep(587, 127);
	sleep(131);
	Beep(587, 122);
	sleep(135);
	Beep(587, 127);
	sleep(127);
	Beep(987, 100);
	sleep(131);
	Beep(587, 131);
	sleep(125);
	Beep(659, 60);
	sleep(50);
	Beep(659, 61);
	sleep(72);
	Beep(554, 155);
	sleep(102);
	Beep(739, 116);
	sleep(141);
	Beep(739, 112);
	sleep(141);
	Beep(739, 112);
	sleep(131);
	Beep(739, 116);
	sleep(133);
	Beep(1174, 118);
	sleep(133);
	Beep(739, 143);
	sleep(100);
	Beep(830, 59);
	sleep(68);
	Beep(830, 84);
	sleep(22);
	Beep(698, 136);
	sleep(137);
	Beep(554, 114);
	sleep(131);
	Beep(880, 81);
	sleep(31);
	Beep(739, 83);
	sleep(50);
	Beep(830, 102);
	sleep(8);
	Beep(739, 66);
	sleep(52);
	Beep(830, 100);
	sleep(25);
	Beep(739, 83);
	sleep(37);
	Beep(698, 100);
	sleep(29);
	Beep(830, 72);
	sleep(50);
	Beep(987, 95);
	sleep(27);
	Beep(830, 62);
	sleep(64);
	Beep(880, 89);
	sleep(39);
	Beep(830, 66);
	sleep(56);
	Beep(880, 89);
	sleep(41);
	Beep(830, 72);
	sleep(60);
	Beep(739, 66);
	sleep(54);
	Beep(880, 60);
	sleep(66);
	Beep(739, 141);
	sleep(99);
	Beep(739, 68);
	sleep(58);
	Beep(987, 66);
	sleep(58);
	Beep(739, 151);
	sleep(108);
	Beep(739, 56);
	sleep(64);
	Beep(1108, 89);
	sleep(29);
	Beep(739, 122);
	sleep(2);
	Beep(698, 142);
	sleep(101);
	Beep(1174, 75);
	sleep(45);
	Beep(739, 106);
	sleep(10);
	Beep(698, 155);
	sleep(90);
	Beep(1174, 79);
	sleep(43);
	Beep(1108, 75);
	sleep(54);
	Beep(987, 95);
	sleep(54);
	Beep(1108, 79);
	sleep(39);
	Beep(880, 81);
	sleep(43);
	Beep(830, 56);
	sleep(41);
	Beep(739, 85);
	sleep(70);
	Beep(880, 102);
	sleep(147);
	Beep(880, 53);
	sleep(45);
	Beep(880, 65);
	sleep(72);
	Beep(739, 218);
	sleep(306);
	Beep(739, 197);
	sleep(50);
	Beep(880, 83);
	sleep(41);
	Beep(739, 52);
	sleep(72);
	Beep(554, 227);
	sleep(18);
	Beep(739, 79);
	sleep(43);
	Beep(554, 83);
	sleep(56);
	Beep(440, 210);
	sleep(41);
	Beep(554, 64);
	sleep(22);
	Beep(440, 100);
	sleep(52);
	Beep(369, 254);
	sleep(239);
	Beep(523, 258);
	sleep(233);
	Beep(659, 225);
	sleep(27);
	Beep(622, 97);
	sleep(14);
	Beep(739, 75);
	sleep(62);
	Beep(880, 156);
	sleep(72);
	Beep(783, 75);
	sleep(41);
	Beep(739, 72);
	sleep(66);
	Beep(783, 91);
	sleep(172);
	Beep(659, 83);
	sleep(168);
	Beep(783, 218);
	sleep(25);
	Beep(987, 81);
	sleep(31);
	Beep(783, 68);
	sleep(66);
	Beep(659, 200);
	sleep(56);
	Beep(783, 77);
	sleep(37);
	Beep(659, 68);
	sleep(66);
	Beep(554, 208);
	sleep(45);
	Beep(659, 81);
	sleep(27);
	Beep(554, 72);
	sleep(66);
	Beep(440, 500);
	sleep(112);
	Beep(587, 87);
	sleep(33);
	Beep(739, 60);
	sleep(60);
	Beep(587, 77);
	sleep(54);
	Beep(659, 120);
	sleep(8);
	Beep(587, 77);
	sleep(56);
	Beep(659, 110);
	sleep(135);
	Beep(554, 97);
	sleep(16);
	Beep(659, 116);
	sleep(14);
	Beep(783, 91);
	sleep(22);
	Beep(659, 95);
	sleep(33);
	Beep(739, 83);
	sleep(31);
	Beep(659, 89);
	sleep(37);
	Beep(739, 100);
	sleep(31);
	Beep(659, 77);
	sleep(45);
	Beep(587, 91);
	sleep(25);
	Beep(739, 62);
	sleep(54);
	Beep(587, 149);
	sleep(112);
	Beep(587, 68);
	sleep(56);
	Beep(783, 126);
	sleep(125);
	Beep(554, 130);
	sleep(114);
	Beep(880, 100);
	sleep(27);
	Beep(587, 132);
	sleep(116);
	Beep(587, 68);
	sleep(54);
	Beep(987, 97);
	sleep(35);
	Beep(587, 110);
	sleep(16);
	Beep(554, 127);
	sleep(4);
	Beep(587, 70);
	sleep(56);
	Beep(987, 81);
	sleep(43);
	Beep(880, 104);
	sleep(27);
	Beep(783, 64);
	sleep(60);
	Beep(880, 95);
	sleep(20);
	Beep(739, 91);
	sleep(33);
	Beep(659, 89);
	sleep(16);
	Beep(587, 97);
	sleep(47);
	Beep(739, 139);
	sleep(95);
	Beep(739, 58);
	sleep(62);
	Beep(739, 77);
	sleep(56);
	Beep(587, 304);
	sleep(210);
	Beep(739, 100);
	sleep(145);
	Beep(739, 108);
	sleep(150);
	Beep(739, 97);
	sleep(150);
	Beep(739, 106);
	sleep(147);
	Beep(1174, 93);
	sleep(145);
	Beep(739, 131);
	sleep(129);
	Beep(783, 66);
	sleep(56);
	Beep(783, 67);
	sleep(50);
	Beep(659, 143);
	sleep(116);
	Beep(659, 120);
	sleep(137);
	Beep(659, 114);
	sleep(122);
	Beep(659, 120);
	sleep(129);
	Beep(659, 131);
	sleep(112);
	Beep(1108, 93);
	sleep(147);
	Beep(659, 125);
	sleep(106);
	Beep(739, 62);
	sleep(54);
	Beep(739, 89);
	sleep(41);
	Beep(587, 145);
	sleep(125);
	Beep(987, 114);
	sleep(120);
	Beep(1174, 79);
	sleep(41);
	Beep(987, 64);
	sleep(58);
	Beep(880, 255);
	sleep(516);
	Beep(987, 110);
	sleep(12);
	Beep(783, 63);
	sleep(74);
	Beep(659, 572);
	sleep(166);
	Beep(783, 116);
	sleep(2);
	Beep(659, 71);
	sleep(75);
	Beep(523, 91);
	sleep(27);
	Beep(659, 93);
	sleep(12);
	Beep(783, 97);
	sleep(20);
	Beep(659, 62);
	sleep(60);
	Beep(523, 120);
	sleep(122);
	Beep(523, 126);
	sleep(141);
	Beep(466, 100);
	sleep(150);
	Beep(369, 110);
	sleep(147);
	Beep(391, 229);
	sleep(31);
	Beep(369, 89);
	sleep(143);
	Beep(493, 187);
	sleep(52);
	Beep(466, 97);
	sleep(27);
	Beep(554, 83);
	sleep(47);
	Beep(659, 202);
	sleep(47);
	Beep(587, 87);
	sleep(22);
	Beep(554, 81);
	sleep(56);
	Beep(587, 183);
	sleep(68);
	Beep(493, 107);
	sleep(53);
	Beep(659, 101);
	sleep(239);
	Beep(587, 95);
	sleep(14);
	Beep(739, 85);
	sleep(50);
	Beep(987, 100);
	sleep(156);
	Beep(739, 70);
	sleep(187);
	Beep(659, 81);
	sleep(14);
	Beep(587, 114);
	sleep(25);
	Beep(554, 89);
	sleep(31);
	Beep(587, 52);
	sleep(91);
	Beep(554, 261);
	sleep(252);
	Beep(739, 197);
	sleep(50);
	Beep(880, 83);
	sleep(41);
	Beep(739, 52);
	sleep(72);
	Beep(554, 227);
	sleep(18);
	Beep(739, 79);
	sleep(43);
	Beep(554, 83);
	sleep(56);
	Beep(440, 210);
	sleep(41);
	Beep(554, 64);
	sleep(22);
	Beep(440, 100);
	sleep(52);
	Beep(369, 254);
	sleep(239);
	Beep(523, 258);
	sleep(233);
	Beep(659, 225);
	sleep(27);
	Beep(622, 97);
	sleep(14);
	Beep(739, 75);
	sleep(62);
	Beep(880, 156);
	sleep(72);
	Beep(783, 75);
	sleep(41);
	Beep(739, 72);
	sleep(66);
	Beep(783, 91);
	sleep(172);
	Beep(659, 83);
	sleep(168);
	Beep(783, 218);
	sleep(25);
	Beep(987, 81);
	sleep(31);
	Beep(783, 68);
	sleep(66);
	Beep(659, 200);
	sleep(56);
	Beep(783, 77);
	sleep(37);
	Beep(659, 68);
	sleep(66);
	Beep(554, 208);
	sleep(45);
	Beep(659, 81);
	sleep(27);
	Beep(554, 72);
	sleep(66);
	Beep(440, 500);
	sleep(112);
	Beep(587, 87);
	sleep(33);
	Beep(739, 60);
	sleep(60);
	Beep(587, 77);
	sleep(54);
	Beep(659, 120);
	sleep(8);
	Beep(587, 77);
	sleep(56);
	Beep(659, 110);
	sleep(135);
	Beep(554, 97);
	sleep(16);
	Beep(659, 116);
	sleep(14);
	Beep(783, 91);
	sleep(22);
	Beep(659, 95);
	sleep(33);
	Beep(739, 83);
	sleep(31);
	Beep(659, 89);
	sleep(37);
	Beep(739, 100);
	sleep(31);
	Beep(659, 77);
	sleep(45);
	Beep(587, 91);
	sleep(25);
	Beep(739, 62);
	sleep(54);
	Beep(587, 149);
	sleep(112);
	Beep(587, 68);
	sleep(56);
	Beep(783, 126);
	sleep(125);
	Beep(554, 130);
	sleep(114);
	Beep(880, 100);
	sleep(27);
	Beep(587, 132);
	sleep(116);
	Beep(587, 68);
	sleep(54);
	Beep(987, 97);
	sleep(35);
	Beep(587, 110);
	sleep(16);
	Beep(554, 127);
	sleep(4);
	Beep(587, 70);
	sleep(56);
	Beep(987, 81);
	sleep(43);
	Beep(880, 104);
	sleep(27);
	Beep(783, 64);
	sleep(60);
	Beep(880, 95);
	sleep(20);
	Beep(739, 91);
	sleep(33);
	Beep(659, 89);
	sleep(16);
	Beep(587, 97);
	sleep(47);
	Beep(739, 139);
	sleep(95);
	Beep(739, 58);
	sleep(62);
	Beep(739, 77);
	sleep(56);
	Beep(587, 304);
	sleep(210);
	Beep(739, 100);
	sleep(145);
	Beep(739, 108);
	sleep(150);
	Beep(739, 97);
	sleep(150);
	Beep(739, 106);
	sleep(147);
	Beep(1174, 93);
	sleep(145);
	Beep(739, 131);
	sleep(129);
	Beep(783, 66);
	Beep(739, 64);
	sleep(43);
	Beep(739, 91);
	sleep(234);
	Beep(659, 120);
	sleep(137);
	Beep(659, 114);
	sleep(122);
	Beep(659, 120);
	sleep(129);
	Beep(659, 131);
	sleep(112);
	Beep(1108, 93);
	sleep(147);
	Beep(659, 125);
	sleep(106);
	Beep(739, 62);
	sleep(54);
	Beep(739, 89);
	sleep(41);
	Beep(587, 145);
	sleep(125);
	Beep(987, 114);
	sleep(120);
	Beep(1174, 79);
	sleep(41);
	Beep(987, 64);
	sleep(58);
	Beep(880, 255);
	sleep(516);
	Beep(987, 110);
	sleep(12);
	Beep(783, 63);
	sleep(74);
	Beep(659, 572);
	sleep(166);
	Beep(783, 116);
	sleep(2);
	Beep(659, 71);
	sleep(75);
	Beep(523, 91);
	sleep(27);
	Beep(659, 93);
	sleep(12);
	Beep(783, 97);
	sleep(20);
	Beep(659, 62);
	sleep(60);
	Beep(523, 120);
	sleep(122);
	Beep(523, 126);
	sleep(141);
	Beep(466, 100);
	sleep(150);
	Beep(369, 110);
	sleep(147);
	Beep(391, 229);
	sleep(31);
	Beep(369, 89);
	sleep(143);
	Beep(493, 187);
	sleep(52);
	Beep(466, 97);
	sleep(27);
	Beep(554, 83);
	sleep(47);
	Beep(659, 202);
	sleep(47);
	Beep(587, 87);
	sleep(22);
	Beep(554, 81);
	sleep(56);
	Beep(587, 183);
	sleep(68);
	Beep(493, 107);
	sleep(53);
	Beep(659, 101);
	sleep(239);
	Beep(587, 95);
	sleep(14);
	Beep(739, 85);
	sleep(50);
	Beep(987, 100);
	sleep(156);
	Beep(739, 70);
	sleep(187);
	Beep(659, 81);
	sleep(14);
	Beep(587, 114);
	sleep(25);
	Beep(554, 89);
	sleep(31);
	Beep(587, 52);
	sleep(91);
	Beep(554, 261);
}