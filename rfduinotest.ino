#include <msp430g2553.h>
#include <string.h>
#include <stdlib.h>
#include <stdlib.h>

#define TXLED BIT0
#define RXLED BIT6
// Transmit pin on port 1
#define TXD BIT2
// Receive pin on port 1
#define RXD BIT1
//encoder A pin on port 1
#define ENCA BIT7
//encoder B pin on port 2
#define ENCB BIT2
//direction pin
#define DIRP BIT0


volatile unsigned int enc = 0;

volatile char FG = 0; //direction flag
volatile unsigned int counta = 0;
volatile unsigned int countb = 0;
unsigned char rxCount = 0;
const unsigned char txMsgLen = 4;
char txBuf[4];
signed char pwmVal = 40;
volatile char sendEncoderDataFlag = 0; //0 = nothing, 1 = sendData



//---------------------------------------------------------------
//transmit 'msg' over UART
void uartTX(char* msg)
{
    int i;
    for(i = 0; i < txMsgLen; i++)
    {
        while (!(IFG2&UCA0TXIFG)); // USCI_A0 TX buffer ready?
         UCA0TXBUF = msg[i];
    }
}
//---------------------------------------------------------------

//---------------------------------------------------------------
void sendEncoderData()
{
    //apply new pwm
    if(pwmVal > 0)
    {
      TA0CCR1 = pwmVal;
      P1OUT |= BIT0;
    }
    else
    {
      TA0CCR1 = -pwmVal;
      P1OUT &= ~BIT0;
    }

    // USCI_A0 TX buffer ready?
    while (!(IFG2&UCA0TXIFG));
    //compile reply (big endian) counta, countb
    txBuf[0] = (counta >> 8) & 0xFF;
    txBuf[1] = counta & 0xFF;
    txBuf[2] = (countb >> 8) & 0xFF;
    txBuf[3] = countb & 0xFF;
/*
    txBuf[0] = 'a';
    txBuf[1] = 'b';
    txBuf[2] = 'c';
    txBuf[3] = 'd';
*/
    //send reply
    uartTX(txBuf);
}
//---------------------------------------------------------------

//_______________________________________________________________
void main(void)
{
    WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
    P1DIR = 0xFF;                             // All P1.x outputs
    P1OUT = 0;                                // All P1.x reset
    P2DIR = 0xFF;                             // All P2.x outputs
    P2OUT = 0;                                // All P2.x reset
    P1SEL = RXD + TXD + BIT6;                     // P1.1 = RXD, P1.2=TXD


    P1SEL2 = 0;
    P2SEL2 = 0;
    P1SEL2 = RXD + TXD;                    // P1.1 = RXD, P1.2=TXD
    P2SEL |= RXD + TXD;

    P1DIR &= ~ENCA; //set the direction of the input P1.7 as an input
    P1REN |=  ENCA; //enable the pullup resistor
    P1SEL &= ~ENCA; //selects p1.7

    P2DIR &= ~ENCB; //set the direction of the input P2.0 as an input
    P2REN |=  ENCB; //enable the pullup resistor
    P2SEL &= ~ENCB; //selects p2.0

    P1IE  |=  ENCA; //interrupt enable
    P1IES |=  ENCA; //Sets interrupt to trigger high to low (|=) or low to high (&= ~)
    P1IFG &= ~ENCA; //zero the interrupt flag. Do this before enabling interrupts

    P2IE  |=  ENCB; //interrupt enable
    P2IES |=  ENCB; //Sets interrupt to trigger high to low (|=) or low to high (&= ~)
    P2IFG &= ~ENCB; //zero the interrupt flag. Do this before enabling interrupts

    // Setup the direction pin on P1.0
    P1DIR |= DIRP;
    P1OUT |= DIRP;

    //This portion is for handling PWM output- notice that it uses the ACLK, which is the external crystal.
    //Bit 6 on port 1 is the selected PWM output.

    //TA0CCTL0 = CCIE;

    TA0CCR0 = 100;                      // PWM period, 12000 ACLK ticks or 1/second
    TA0CCR1 = 40;                       // PWM duty cycle, time cycle on vs. off, on 10% initially for LED
    TA0CCTL1 = OUTMOD_7;                  // TA0CCR1 reset/set -- high voltage below TA0CCR1 count                                            /
    TA0CTL = TASSEL_1 + MC_1;                    //clock selection and up mode


    if (CALBC1_1MHZ==0xFF)                    // If calibration constant erased
    {
        P1OUT |= 0x01;
        while(1);                               // do not load, trap CPU!!
    }

    //Setup serial Communication
    DCOCTL = 0;                               // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;                    // Set DCO
    DCOCTL = CALDCO_1MHZ;
    UCA0CTL1 |= UCSSEL_2;                     // SMCLK
    UCA0BR0 = 0x68;                             // 1MHz 9600
    UCA0BR1 = 0;                              // 1MHz 9600
    UCA0MCTL = UCBRS1+UCBRS0;                        // Modulation UCBRSx = 1
    UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
    IE2 |= UCA0RXIE;                          // Enable USCI_A0 RX interrupt
    _BIS_SR(GIE);                       // enable global interrupts

    while(1)
    {
        if(sendEncoderDataFlag == 1)
        {
            sendEncoderData();
            sendEncoderDataFlag = 0;
        }
    }
}
//_______________________________________________________________

// You will need to add a serial communications handler here for coming and going messages

//You will also need to handle the timing for when to retrieve a count value, i.e. every
//.05, capture the count from the QEI handler.


//---------------------------------------------------------------
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
    P1IFG &= ~ENCA; //reset the interrupt flag
    P1IES ^= ENCA; //alternate the trigger behavior

    if(FG == 0)
    {
        if(counta == 0)
        {
            counta = 720;
        }
        counta--;
    }
    else
        if(FG == 1)
        {
            if(counta == 720)
            {
                counta = 0;
            }
            counta++;
        }
}
//---------------------------------------------------------------

//---------------------------------------------------------------
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
{
    P2IFG &= ~ENCB; //reset the interrupt flag
    P2IES ^= ENCB; //alternate the trigger behavior

    if((((P2IES & ENCB) | (P1IES & ENCA)) == 0x81) | ((P2IES & ENCB) | (P1IES & ENCA)) == 0)
    {
        FG = 0;
        if(countb==0)
        {
            countb = 720;
        }
        countb--;
    }
    else
    {
        FG = 1;
        if(countb == 720)
        {
            countb = 0;
        }
                  countb++;
    }
}
//---------------------------------------------------------------

