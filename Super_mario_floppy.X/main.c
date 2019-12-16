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
void Beep(int frequency, double duration);
void sleep(int ms);
void supermario(void);
void __interrupt() ISR(void);

uint16_t offset;
uint8_t cnt = 0;

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
    printf("Super Mario Bros Theme.\n\r");
    supermario();
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

void Beep(int frequency, double duration) {
    int i;
    frequency = frequency / 2;
    offset = (uint16_t) ((double) 65535 - ((double) 1000000 / (double) frequency)); //load start timer value
    TMR1 = offset;
    T1CONbits.TMR1ON = 1; //start tone generator timer
    for (i = 0; i < duration; i++) {
        __delay_ms(1);
    }
    T1CONbits.TMR1ON = 0; //stop tone generator timer
}

void sleep(int ms) {
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
    if (cnt == 150) { //inversion counter
        cnt = 0;
        LATAbits.LA1 = ~LATAbits.LA1;
    }
}

void supermario() {
    Beep(660, 100);
    sleep(150);
    Beep(660, 100);
    sleep(300);
    Beep(660, 100);
    sleep(300);
    Beep(510, 100);
    sleep(100);
    Beep(660, 100);
    sleep(300);
    Beep(770, 100);
    sleep(550);
    Beep(380, 100);
    sleep(575);

    Beep(510, 100);
    sleep(450);
    Beep(380, 100);
    sleep(400);
    Beep(320, 100);
    sleep(500);
    Beep(440, 100);
    sleep(300);
    Beep(480, 80);
    sleep(330);
    Beep(450, 100);
    sleep(150);
    Beep(430, 100);
    sleep(300);
    Beep(380, 100);
    sleep(200);
    Beep(660, 80);
    sleep(200);
    Beep(760, 50);
    sleep(150);
    Beep(860, 100);
    sleep(300);
    Beep(700, 80);
    sleep(150);
    Beep(760, 50);
    sleep(350);
    Beep(660, 80);
    sleep(300);
    Beep(520, 80);
    sleep(150);
    Beep(580, 80);
    sleep(150);
    Beep(480, 80);
    sleep(500);

    Beep(510, 100);
    sleep(450);
    Beep(380, 100);
    sleep(400);
    Beep(320, 100);
    sleep(500);
    Beep(440, 100);
    sleep(300);
    Beep(480, 80);
    sleep(330);
    Beep(450, 100);
    sleep(150);
    Beep(430, 100);
    sleep(300);
    Beep(380, 100);
    sleep(200);
    Beep(660, 80);
    sleep(200);
    Beep(760, 50);
    sleep(150);
    Beep(860, 100);
    sleep(300);
    Beep(700, 80);
    sleep(150);
    Beep(760, 50);
    sleep(350);
    Beep(660, 80);
    sleep(300);
    Beep(520, 80);
    sleep(150);
    Beep(580, 80);
    sleep(150);
    Beep(480, 80);
    sleep(500);

    Beep(500, 100);
    sleep(300);

    Beep(760, 100);
    sleep(100);
    Beep(720, 100);
    sleep(150);
    Beep(680, 100);
    sleep(150);
    Beep(620, 150);
    sleep(300);

    Beep(650, 150);
    sleep(300);
    Beep(380, 100);
    sleep(150);
    Beep(430, 100);
    sleep(150);

    Beep(500, 100);
    sleep(300);
    Beep(430, 100);
    sleep(150);
    Beep(500, 100);
    sleep(100);
    Beep(570, 100);
    sleep(220);

    Beep(500, 100);
    sleep(300);

    Beep(760, 100);
    sleep(100);
    Beep(720, 100);
    sleep(150);
    Beep(680, 100);
    sleep(150);
    Beep(620, 150);
    sleep(300);

    Beep(650, 200);
    sleep(300);

    Beep(1020, 80);
    sleep(300);
    Beep(1020, 80);
    sleep(150);
    Beep(1020, 80);
    sleep(300);

    Beep(380, 100);
    sleep(300);
    Beep(500, 100);
    sleep(300);

    Beep(760, 100);
    sleep(100);
    Beep(720, 100);
    sleep(150);
    Beep(680, 100);
    sleep(150);
    Beep(620, 150);
    sleep(300);

    Beep(650, 150);
    sleep(300);
    Beep(380, 100);
    sleep(150);
    Beep(430, 100);
    sleep(150);

    Beep(500, 100);
    sleep(300);
    Beep(430, 100);
    sleep(150);
    Beep(500, 100);
    sleep(100);
    Beep(570, 100);
    sleep(420);

    Beep(585, 100);
    sleep(450);

    Beep(550, 100);
    sleep(420);

    Beep(500, 100);
    sleep(360);

    Beep(380, 100);
    sleep(300);
    Beep(500, 100);
    sleep(300);
    Beep(500, 100);
    sleep(150);
    Beep(500, 100);
    sleep(300);

    Beep(500, 100);
    sleep(300);

    Beep(760, 100);
    sleep(100);
    Beep(720, 100);
    sleep(150);
    Beep(680, 100);
    sleep(150);
    Beep(620, 150);
    sleep(300);

    Beep(650, 150);
    sleep(300);
    Beep(380, 100);
    sleep(150);
    Beep(430, 100);
    sleep(150);

    Beep(500, 100);
    sleep(300);
    Beep(430, 100);
    sleep(150);
    Beep(500, 100);
    sleep(100);
    Beep(570, 100);
    sleep(220);

    Beep(500, 100);
    sleep(300);

    Beep(760, 100);
    sleep(100);
    Beep(720, 100);
    sleep(150);
    Beep(680, 100);
    sleep(150);
    Beep(620, 150);
    sleep(300);

    Beep(650, 200);
    sleep(300);

    Beep(1020, 80);
    sleep(300);
    Beep(1020, 80);
    sleep(150);
    Beep(1020, 80);
    sleep(300);

    Beep(380, 100);
    sleep(300);
    Beep(500, 100);
    sleep(300);

    Beep(760, 100);
    sleep(100);
    Beep(720, 100);
    sleep(150);
    Beep(680, 100);
    sleep(150);
    Beep(620, 150);
    sleep(300);

    Beep(650, 150);
    sleep(300);
    Beep(380, 100);
    sleep(150);
    Beep(430, 100);
    sleep(150);

    Beep(500, 100);
    sleep(300);
    Beep(430, 100);
    sleep(150);
    Beep(500, 100);
    sleep(100);
    Beep(570, 100);
    sleep(420);

    Beep(585, 100);
    sleep(450);

    Beep(550, 100);
    sleep(420);

    Beep(500, 100);
    sleep(360);

    Beep(380, 100);
    sleep(300);
    Beep(500, 100);
    sleep(300);
    Beep(500, 100);
    sleep(150);
    Beep(500, 100);
    sleep(300);

    Beep(500, 60);
    sleep(150);
    Beep(500, 80);
    sleep(300);
    Beep(500, 60);
    sleep(350);
    Beep(500, 80);
    sleep(150);
    Beep(580, 80);
    sleep(350);
    Beep(660, 80);
    sleep(150);
    Beep(500, 80);
    sleep(300);
    Beep(430, 80);
    sleep(150);
    Beep(380, 80);
    sleep(600);

    Beep(500, 60);
    sleep(150);
    Beep(500, 80);
    sleep(300);
    Beep(500, 60);
    sleep(350);
    Beep(500, 80);
    sleep(150);
    Beep(580, 80);
    sleep(150);
    Beep(660, 80);
    sleep(550);

    Beep(870, 80);
    sleep(325);
    Beep(760, 80);
    sleep(600);

    Beep(500, 60);
    sleep(150);
    Beep(500, 80);
    sleep(300);
    Beep(500, 60);
    sleep(350);
    Beep(500, 80);
    sleep(150);
    Beep(580, 80);
    sleep(350);
    Beep(660, 80);
    sleep(150);
    Beep(500, 80);
    sleep(300);
    Beep(430, 80);
    sleep(150);
    Beep(380, 80);
    sleep(600);

    Beep(660, 100);
    sleep(150);
    Beep(660, 100);
    sleep(300);
    Beep(660, 100);
    sleep(300);
    Beep(510, 100);
    sleep(100);
    Beep(660, 100);
    sleep(300);
    Beep(770, 100);
    sleep(550);
    Beep(380, 100);
    sleep(575);
}