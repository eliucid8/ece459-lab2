/*
 * File:   main.c
 * Author: erict
 *
 * Created on September 9, 2025, 11:44 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <xc.h>
#include <htc.h>
#include <pic.h>

// CONFIG1
#pragma config FOSC = XT        // Oscillator Selection bits (XT oscillator: Crystal/resonator on RA6/OSC2/CLKOUT and RA7/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled and can be enabled by SWDTEN bit of the WDTCON register)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = ON       // RE3/MCLR pin function select bit (RE3/MCLR pin function is MCLR)
#pragma config CP = OFF         // Code Protection bit (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Code Protection bit (Data memory code protection is disabled)
#pragma config BOREN = OFF      // Brown Out Reset Selection bits (BOR disabled)
#pragma config IESO = OFF       // Internal External Switchover bit (Internal/External Switchover mode is disabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enabled bit (Fail-Safe Clock Monitor is disabled)
#pragma config LVP = OFF        // Low Voltage Programming Enable bit (RB3 pin has digital I/O, HV on MCLR must be used for programming)

// CONFIG2
#pragma config BOR4V = BOR40V   // Brown-out Reset Selection bit (Brown-out Reset set to 4.0V)
#pragma config WRT = OFF        // Flash Program Memory Self Write Enable bits (Write protection off)

#define _XTAL_FREQ 4000000

#define DOT_SEGMENTS   0x7F // ~0b10000000
#define DASH_SEGMENTS  0xBF // ~0b01000000
#define ERROR_SEGMENTS 0x84 // ~0b01111011

#define DOT_LOW_THRESHOLD 569 // 30 ms
#define DOT_DASH_THRESHOLD 3791 // 200 ms
#define DASH_HIGH_THRESHOLD 7582 // 400 ms

uint16_t counter = 0; // counts timer interrupts

// track if we are currently timing a user input
bool running = false;
// track previous state of button
bool pressed = false;

uint8_t input_buf = 0;
uint8_t input_count = 0;

void main() {
    ANSEL = 0x00;
    ANSELH = 0x00;

    // 7seg display select
    TRISA = 0x00;
    // input
    TRISB = 0xFF;
    // 7seg output
    TRISC = 0x00;

    INTCONbits.GIE = 1; // enable all unmasked interrupts
    INTCONbits.PEIE = 1; // hardware peripheral interrupt enable
    INTCONbits.RBIE = 1; // B register change interrupt enable (disabled)
    INTCONbits.RBIF = 0; // B register change interrupt flag (reset in software)
    IOCBbits.IOCB1 = 1; // enable interrupt on change of B1
    
    // I like the idea of a morse tree, but i think the best solution in terms
    // of memory is going to be a switch statement-like structure.
    // this means we essentially save our morse tree into the program memory
    
    // 16x8 bits, 128
    uint8_t map_7seg[] = {
        0x3F, // 0
        0x06, // 1
        0x5B, // 2
        0x4F, // 3
        0x66, // 4
        0x6D, // 5
        0x7D, // 6
        0x07, // 7
        0x7F, // 8
        0x67, // 9
        0x77, // A
        0x7C, // b
        0x79, // C
        0x5E, // d
        0x79, // E
        0x71, // F
    };
    
    // 
    T2CONbits.TMR2ON = 0; // keep timer paused at first
    PIE1bits.TMR2IE = 1; // enable timer interrupt
    // freq. of dry timer2 = Fosc/4 = 1MHz
    // we need to divide down to 20 kHz
    T2CONbits.T2CKPS1 = 0;
    T2CONbits.T2CKPS0 = 0;
    // match PR2 == 50 -> 20000 Hz
    PR2 = 50;
    // TOUTPS = 0b0000 = 1:1 postscaler
    T2CON &= 0b10000111;
    // this gives us a 20kHz timer.
    
    // set display 1 permanently on
    PORTA = 0x01;
    // set output to initially 0.
    PORTC = 0xFF;
    
    while(1) {

    }
    
    return;
}


void __interrupt() isr() {
    // hoping the compiler makes this branchless
    if(PIR1bits.TMR2IF) {
       PIR1bits.TMR2IF = 0;
       counter += 1;
    }
    
    if (PIR1bits.TMR2IF && counter >= 40000) {
        T2CONbits.TMR2ON = 0; // pause timer
        running = false;
        INTCONbits.RBIF = 0; // enable button presses
        counter = 0; // reset counter
        PORTC = 0xFF; // clear display
    }
        
    // NIT: could do a better job debouncing here :(
    // rising edge
    if(!running && !pressed && PORTBbits.RB1 == 1) {
        T2CONbits.TMR2ON = 1;
        counter = 0;
        running = true;
        pressed = true;
        INTCONbits.RBIF = 0;
    }
    
    // falling edge
    if(pressed && PORTBbits.RB1 == 0) {
        if(counter > 8000) {
            PORTC = ERROR_SEGMENTS;
        }
        if(counter > DOT_DASH_THRESHOLD && counter <= DASH_HIGH_THRESHOLD) {
            // shift left and set lsb to 1.
            input_buf <<= 1;
            input_buf |= 1;
            input_count += 1;
        }
        if(counter >= DOT_LOW_THRESHOLD && counter <= DOT_DASH_THRESHOLD) {
            // shift left (and nset lsb to 0)
            input_buf <<= 1;
            input_count += 1;
        }
        if(counter < DOT_LOW_THRESHOLD) {
            PORTC = ERROR_SEGMENTS;
        }
        
        pressed = false;
    }
    

}