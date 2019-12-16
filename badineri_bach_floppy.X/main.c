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
	Beep(123, 122);
	sleep(120);
	Beep(1174, 86);
	sleep(31);
	Beep(987, 57);
	sleep(70);
	Beep(146, 109);
	sleep(134);
	Beep(123, 125);
	sleep(103);
	Beep(587, 156);
	sleep(90);
	Beep(146, 144);
	sleep(118);
	Beep(493, 179);
	sleep(70);
	Beep(184, 122);
	sleep(129);
	Beep(369, 114);
	sleep(14);
	Beep(493, 88);
	sleep(22);
	Beep(123, 121);
	sleep(12);
	Beep(493, 74);
	sleep(31);
	Beep(391, 138);
	Beep(493, 78);
	sleep(45);
	Beep(493, 146);
	sleep(97);
	Beep(554, 125);
	sleep(6);
	Beep(554, 102);
	sleep(20);
	Beep(659, 101);
	sleep(16);
	Beep(554, 78);
	sleep(54);
	Beep(493, 131);
	sleep(101);
	Beep(146, 90);
	sleep(32);
	Beep(138, 87);
	sleep(50);
	Beep(123, 118);
	sleep(124);
	Beep(1174, 89);
	sleep(22);
	Beep(987, 62);
	sleep(85);
	Beep(739, 207);
	sleep(27);
	Beep(123, 123);
	sleep(126);
	Beep(587, 139);
	sleep(102);
	Beep(146, 132);
	sleep(2);
	Beep(587, 77);
	sleep(29);
	Beep(293, 185);
	sleep(87);
	Beep(184, 118);
	sleep(120);
	Beep(369, 126);
	sleep(117);
	Beep(587, 309);
	sleep(60);
	Beep(554, 100);
	sleep(22);
	Beep(587, 305);
	sleep(68);
	Beep(987, 78);
	sleep(43);
	Beep(587, 193);
	sleep(50);
	Beep(440, 133);
	sleep(2);
	Beep(659, 56);
	sleep(60);
	Beep(329, 118);
	sleep(121);
	Beep(440, 131);
	sleep(123);
	Beep(587, 123);
	sleep(8);
	Beep(293, 87);
	sleep(25);
	Beep(415, 148);
	sleep(94);
	Beep(587, 116);
	sleep(2);
	Beep(246, 86);
	sleep(37);
	Beep(587, 115);
	Beep(1174, 90);
	sleep(35);
	Beep(739, 173);
	sleep(80);
	Beep(830, 54);
	sleep(18);
	Beep(739, 72);
	sleep(6);
	Beep(830, 85);
	sleep(6);
	Beep(415, 127);
	sleep(124);
	Beep(739, 122);
	sleep(18);
	Beep(739, 89);
	sleep(18);
	Beep(739, 119);
	sleep(22);
	Beep(739, 85);
	sleep(16);
	Beep(246, 121);
	sleep(18);
	Beep(739, 67);
	sleep(47);
	Beep(207, 117);
	sleep(8);
	Beep(739, 85);
	sleep(35);
	Beep(698, 101);
	sleep(26);
	Beep(830, 74);
	sleep(43);
	Beep(987, 109);
	sleep(16);
	Beep(830, 64);
	sleep(58);
	Beep(184, 113);
	sleep(18);
	Beep(830, 68);
	sleep(45);
	Beep(739, 128);
	sleep(10);
	Beep(830, 74);
	sleep(27);
	Beep(739, 118);
	sleep(30);
	Beep(880, 61);
	sleep(43);
	Beep(554, 119);
	sleep(10);
	Beep(698, 118);
	sleep(10);
	Beep(739, 107);
	sleep(20);
	Beep(987, 67);
	sleep(56);
	Beep(493, 122);
	sleep(123);
	Beep(739, 107);
	sleep(25);
	Beep(1108, 90);
	sleep(27);
	Beep(739, 124);
	Beep(698, 144);
	sleep(99);
	Beep(1174, 75);
	sleep(41);
	Beep(293, 123);
	sleep(120);
	Beep(246, 124);
	sleep(115);
	Beep(1108, 76);
	sleep(52);
	Beep(987, 96);
	sleep(20);
	Beep(739, 115);
	sleep(33);
	Beep(880, 82);
	sleep(27);
	Beep(830, 119);
	sleep(129);
	Beep(440, 121);
	sleep(130);
	Beep(554, 108);
	sleep(2);
	Beep(880, 67);
	sleep(70);
	Beep(739, 220);
	sleep(287);
	Beep(123, 122);
	sleep(120);
	Beep(1174, 86);
	sleep(31);
	Beep(987, 57);
	sleep(70);
	Beep(146, 109);
	sleep(134);
	Beep(123, 125);
	sleep(103);
	Beep(587, 156);
	sleep(90);
	Beep(146, 144);
	sleep(118);
	Beep(493, 179);
	sleep(70);
	Beep(184, 122);
	sleep(129);
	Beep(369, 114);
	sleep(14);
	Beep(493, 88);
	sleep(22);
	Beep(123, 121);
	sleep(12);
	Beep(493, 74);
	sleep(31);
	Beep(391, 138);
	Beep(493, 78);
	sleep(45);
	Beep(493, 146);
	sleep(97);
	Beep(554, 125);
	sleep(6);
	Beep(554, 102);
	sleep(20);
	Beep(659, 101);
	sleep(16);
	Beep(554, 78);
	sleep(54);
	Beep(493, 131);
	sleep(101);
	Beep(146, 90);
	sleep(32);
	Beep(138, 87);
	sleep(50);
	Beep(123, 118);
	sleep(124);
	Beep(1174, 89);
	sleep(22);
	Beep(987, 62);
	sleep(85);
	Beep(739, 207);
	sleep(27);
	Beep(123, 123);
	sleep(126);
	Beep(587, 139);
	sleep(102);
	Beep(146, 132);
	sleep(2);
	Beep(587, 77);
	sleep(29);
	Beep(293, 185);
	sleep(87);
	Beep(184, 118);
	sleep(120);
	Beep(369, 126);
	sleep(117);
	Beep(587, 309);
	sleep(60);
	Beep(554, 100);
	sleep(22);
	Beep(587, 305);
	sleep(68);
	Beep(987, 78);
	sleep(43);
	Beep(587, 193);
	sleep(50);
	Beep(440, 133);
	sleep(2);
	Beep(659, 56);
	sleep(60);
	Beep(329, 118);
	sleep(121);
	Beep(440, 131);
	sleep(123);
	Beep(587, 123);
	sleep(8);
	Beep(293, 87);
	sleep(25);
	Beep(415, 148);
	sleep(94);
	Beep(587, 116);
	sleep(2);
	Beep(246, 86);
	sleep(37);
	Beep(587, 115);
	Beep(1174, 90);
	sleep(35);
	Beep(739, 173);
	sleep(80);
	Beep(830, 54);
	sleep(18);
	Beep(739, 72);
	sleep(6);
	Beep(830, 85);
	sleep(6);
	Beep(415, 127);
	sleep(124);
	Beep(739, 122);
	sleep(18);
	Beep(739, 89);
	sleep(18);
	Beep(739, 119);
	sleep(22);
	Beep(739, 85);
	sleep(16);
	Beep(246, 121);
	sleep(18);
	Beep(739, 67);
	sleep(47);
	Beep(207, 117);
	sleep(8);
	Beep(739, 85);
	sleep(35);
	Beep(698, 101);
	sleep(26);
	Beep(830, 74);
	sleep(43);
	Beep(987, 109);
	sleep(16);
	Beep(830, 64);
	sleep(58);
	Beep(184, 113);
	sleep(18);
	Beep(830, 68);
	sleep(45);
	Beep(739, 128);
	sleep(10);
	Beep(830, 74);
	sleep(27);
	Beep(739, 118);
	sleep(30);
	Beep(880, 61);
	sleep(43);
	Beep(554, 119);
	sleep(10);
	Beep(698, 118);
	sleep(10);
	Beep(739, 107);
	sleep(20);
	Beep(987, 67);
	sleep(56);
	Beep(493, 122);
	sleep(123);
	Beep(739, 107);
	sleep(25);
	Beep(1108, 90);
	sleep(27);
	Beep(739, 124);
	Beep(698, 144);
	sleep(99);
	Beep(1174, 75);
	sleep(41);
	Beep(293, 123);
	sleep(120);
	Beep(246, 124);
	sleep(115);
	Beep(1108, 76);
	sleep(52);
	Beep(987, 96);
	sleep(20);
	Beep(739, 115);
	sleep(33);
	Beep(880, 82);
	sleep(27);
	Beep(830, 119);
	sleep(129);
	Beep(440, 121);
	sleep(130);
	Beep(554, 108);
	sleep(2);
	Beep(880, 67);
	sleep(70);
	Beep(739, 220);
	sleep(281);
	Beep(440, 156);
	sleep(108);
	Beep(880, 85);
	sleep(39);
	Beep(739, 54);
	sleep(41);
	Beep(369, 170);
	sleep(81);
	Beep(92, 105);
	sleep(37);
	Beep(554, 85);
	sleep(20);
	Beep(440, 181);
	sleep(72);
	Beep(110, 109);
	sleep(6);
	Beep(440, 102);
	sleep(29);
	Beep(220, 174);
	sleep(77);
	Beep(220, 94);
	Beep(184, 91);
	sleep(56);
	Beep(369, 186);
	sleep(60);
	Beep(493, 159);
	sleep(86);
	Beep(123, 218);
	sleep(32);
	Beep(123, 207);
	sleep(41);
	Beep(123, 181);
	sleep(47);
	Beep(123, 163);
	sleep(93);
	Beep(164, 167);
	sleep(74);
	Beep(246, 106);
	sleep(6);
	Beep(195, 89);
	sleep(45);
	Beep(164, 187);
	sleep(63);
	Beep(195, 111);
	sleep(22);
	Beep(783, 70);
	sleep(54);
	Beep(138, 155);
	sleep(109);
	Beep(110, 143);
	sleep(88);
	Beep(659, 183);
	sleep(67);
	Beep(138, 126);
	sleep(132);
	Beep(329, 196);
	sleep(43);
	Beep(164, 170);
	sleep(81);
	Beep(184, 150);
	sleep(91);
	Beep(739, 62);
	sleep(58);
	Beep(587, 78);
	sleep(47);
	Beep(493, 113);
	sleep(18);
	Beep(587, 78);
	sleep(27);
	Beep(391, 128);
	sleep(4);
	Beep(587, 107);
	sleep(14);
	Beep(220, 131);
	sleep(120);
	Beep(440, 133);
	sleep(115);
	Beep(739, 84);
	sleep(29);
	Beep(659, 90);
	sleep(33);
	Beep(440, 125);
	sleep(6);
	Beep(659, 78);
	sleep(43);
	Beep(587, 93);
	sleep(22);
	Beep(739, 64);
	sleep(51);
	Beep(587, 150);
	sleep(110);
	Beep(587, 69);
	sleep(54);
	Beep(783, 127);
	sleep(4);
	Beep(195, 126);
	sleep(118);
	Beep(369, 177);
	sleep(59);
	Beep(220, 126);
	sleep(2);
	Beep(554, 132);
	Beep(391, 193);
	sleep(38);
	Beep(246, 134);
	sleep(6);
	Beep(554, 128);
	sleep(2);
	Beep(587, 72);
	sleep(52);
	Beep(987, 83);
	sleep(14);
	Beep(164, 130);
	sleep(26);
	Beep(783, 66);
	sleep(31);
	Beep(369, 143);
	sleep(101);
	Beep(493, 139);
	sleep(104);
	Beep(440, 123);
	sleep(131);
	Beep(391, 128);
	sleep(43);
	Beep(659, 79);
	sleep(2);
	Beep(146, 138);
	sleep(122);
	Beep(110, 110);
	sleep(120);
	Beep(73, 137);
	sleep(113);
	Beep(739, 316);
	sleep(49);
	Beep(659, 80);
	sleep(50);
	Beep(440, 119);
	sleep(119);
	Beep(146, 164);
	sleep(79);
	Beep(739, 195);
	sleep(58);
	Beep(659, 74);
	sleep(12);
	Beep(739, 65);
	Beep(587, 101);
	sleep(4);
	Beep(659, 143);
	sleep(95);
	Beep(116, 119);
	sleep(132);
	Beep(659, 316);
	sleep(41);
	Beep(587, 90);
	sleep(37);
	Beep(466, 105);
	sleep(4);
	Beep(233, 93);
	sleep(45);
	Beep(466, 115);
	sleep(14);
	Beep(1108, 72);
	sleep(29);
	Beep(233, 113);
	sleep(135);
	Beep(739, 63);
	sleep(43);
	Beep(587, 94);
	sleep(27);
	Beep(587, 224);
	sleep(57);
	Beep(987, 116);
	sleep(118);
	Beep(1174, 81);
	sleep(39);
	Beep(987, 65);
	sleep(56);
	Beep(164, 461);
	sleep(42);
	Beep(391, 132);
	sleep(130);
	Beep(987, 111);
	sleep(10);
	Beep(783, 64);
	sleep(64);
	Beep(164, 255);
	sleep(8);
	Beep(493, 107);
	sleep(6);
	Beep(391, 93);
	sleep(24);
	Beep(329, 155);
	sleep(90);
	Beep(783, 118);
	Beep(659, 73);
	sleep(70);
	Beep(164, 237);
	sleep(2);
	Beep(329, 99);
	sleep(4);
	Beep(659, 64);
	sleep(58);
	Beep(523, 120);
	sleep(119);
	Beep(523, 126);
	sleep(123);
	Beep(493, 236);
	Beep(466, 158);
	sleep(122);
	Beep(391, 228);
	sleep(27);
	Beep(277, 121);
	sleep(111);
	Beep(493, 187);
	sleep(49);
	Beep(466, 99);
	sleep(25);
	Beep(554, 84);
	sleep(43);
	Beep(554, 157);
	sleep(92);
	Beep(587, 89);
	sleep(20);
	Beep(554, 83);
	sleep(45);
	Beep(493, 226);
	sleep(30);
	Beep(493, 108);
	sleep(6);
	Beep(246, 81);
	sleep(45);
	Beep(739, 125);
	sleep(6);
	Beep(493, 87);
	sleep(37);
	Beep(587, 96);
	sleep(12);
	Beep(739, 86);
	sleep(46);
	Beep(987, 101);
	sleep(139);
	Beep(123, 108);
	sleep(125);
	Beep(184, 128);
	Beep(587, 116);
	sleep(22);
	Beep(554, 90);
	sleep(20);
	Beep(587, 73);
	sleep(68);
	Beep(554, 256);
	sleep(241);
	Beep(440, 156);
	sleep(108);
	Beep(880, 85);
	sleep(39);
	Beep(739, 54);
	sleep(41);
	Beep(369, 170);
	sleep(81);
	Beep(92, 105);
	sleep(37);
	Beep(554, 85);
	sleep(20);
	Beep(440, 181);
	sleep(72);
	Beep(110, 109);
	sleep(6);
	Beep(440, 102);
	sleep(29);
	Beep(220, 174);
	sleep(77);
	Beep(220, 94);
	Beep(184, 91);
	sleep(56);
	Beep(369, 186);
	sleep(60);
	Beep(493, 159);
	sleep(86);
	Beep(123, 218);
	sleep(32);
	Beep(123, 207);
	sleep(41);
	Beep(123, 181);
	sleep(47);
	Beep(123, 163);
	sleep(93);
	Beep(164, 167);
	sleep(74);
	Beep(246, 106);
	sleep(6);
	Beep(195, 89);
	sleep(45);
	Beep(164, 187);
	sleep(63);
	Beep(195, 111);
	sleep(22);
	Beep(783, 70);
	sleep(54);
	Beep(138, 155);
	sleep(109);
	Beep(110, 143);
	sleep(88);
	Beep(659, 183);
	sleep(67);
	Beep(138, 126);
	sleep(132);
	Beep(329, 196);
	sleep(43);
	Beep(164, 170);
	sleep(81);
	Beep(184, 150);
	sleep(91);
	Beep(739, 62);
	sleep(58);
	Beep(587, 78);
	sleep(47);
	Beep(493, 113);
	sleep(18);
	Beep(587, 78);
	sleep(27);
	Beep(391, 128);
	sleep(4);
	Beep(587, 107);
	sleep(14);
	Beep(220, 131);
	sleep(120);
	Beep(440, 133);
	sleep(115);
	Beep(739, 84);
	sleep(29);
	Beep(659, 90);
	sleep(33);
	Beep(440, 125);
	sleep(6);
	Beep(659, 78);
	sleep(43);
	Beep(587, 93);
	sleep(22);
	Beep(739, 64);
	sleep(51);
	Beep(587, 150);
	sleep(110);
	Beep(587, 69);
	sleep(54);
	Beep(783, 127);
	sleep(4);
	Beep(195, 126);
	sleep(118);
	Beep(369, 177);
	sleep(59);
	Beep(220, 126);
	sleep(2);
	Beep(554, 132);
	Beep(391, 193);
	sleep(38);
	Beep(246, 134);
	sleep(6);
	Beep(554, 128);
	sleep(2);
	Beep(587, 72);
	sleep(52);
	Beep(987, 83);
	sleep(14);
	Beep(164, 130);
	sleep(26);
	Beep(783, 66);
	sleep(31);
	Beep(369, 143);
	sleep(101);
	Beep(493, 139);
	sleep(104);
	Beep(440, 123);
	sleep(131);
	Beep(391, 128);
	sleep(43);
	Beep(659, 79);
	sleep(2);
	Beep(146, 138);
	sleep(122);
	Beep(110, 110);
	sleep(120);
	Beep(73, 137);
	sleep(113);
	Beep(739, 316);
	sleep(49);
	Beep(659, 80);
	sleep(50);
	Beep(440, 119);
	sleep(119);
	Beep(146, 164);
	sleep(79);
	Beep(739, 195);
	sleep(58);
	Beep(659, 74);
	sleep(12);
	Beep(739, 65);
	Beep(587, 101);
	sleep(4);
	Beep(659, 143);
	sleep(95);
	Beep(116, 119);
	sleep(132);
	Beep(659, 316);
	sleep(41);
	Beep(587, 90);
	sleep(37);
	Beep(466, 105);
	sleep(4);
	Beep(233, 93);
	sleep(45);
	Beep(466, 115);
	sleep(14);
	Beep(1108, 72);
	sleep(29);
	Beep(233, 113);
	sleep(135);
	Beep(739, 63);
	sleep(43);
	Beep(587, 94);
	sleep(27);
	Beep(587, 224);
	sleep(57);
	Beep(987, 116);
	sleep(118);
	Beep(1174, 81);
	sleep(39);
	Beep(987, 65);
	sleep(56);
	Beep(164, 461);
	sleep(42);
	Beep(391, 132);
	sleep(130);
	Beep(987, 111);
	sleep(10);
	Beep(783, 64);
	sleep(64);
	Beep(164, 255);
	sleep(8);
	Beep(493, 107);
	sleep(6);
	Beep(391, 93);
	sleep(24);
	Beep(329, 155);
	sleep(90);
	Beep(783, 118);
	Beep(659, 73);
	sleep(70);
	Beep(164, 237);
	sleep(2);
	Beep(329, 99);
	sleep(4);
	Beep(659, 64);
	sleep(58);
	Beep(523, 120);
	sleep(119);
	Beep(523, 126);
	sleep(123);
	Beep(493, 236);
	Beep(466, 158);
	sleep(122);
	Beep(391, 228);
	sleep(27);
	Beep(277, 121);
	sleep(111);
	Beep(493, 187);
	sleep(49);
	Beep(466, 99);
	sleep(25);
	Beep(554, 84);
	sleep(43);
	Beep(554, 157);
	sleep(92);
	Beep(587, 89);
	sleep(20);
	Beep(554, 83);
	sleep(45);
	Beep(493, 226);
	sleep(30);
	Beep(493, 108);
	sleep(6);
	Beep(246, 81);
	sleep(45);
	Beep(739, 125);
	sleep(6);
	Beep(493, 87);
	sleep(37);
	Beep(587, 96);
	sleep(12);
	Beep(739, 86);
	sleep(46);
	Beep(987, 101);
	sleep(139);
	Beep(123, 108);
	sleep(125);
	Beep(184, 129);
	Beep(587, 125);
	sleep(27);
	Beep(554, 113);
	sleep(25);
	Beep(587, 91);
	sleep(85);
	Beep(554, 320);
}