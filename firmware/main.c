/*
 * File:   main.c
 * Author: Jason
 */
#include "config.h"
#include "libmathq15.h"
#include "task.h"
#include "dio.h"

/*********** Useful defines and macros ****************************************/

/*********** Variable Declarations ********************************************/

/*********** Function Declarations ********************************************/
void initOsc(void);
void initInterrupts(void);
void initPwm(void);

void setDutyCyclePWM1(q15_t dutyCycle);
void setDutyCyclePWM2(q15_t dutyCycle);
void setDutyCyclePWM3(q15_t dutyCycle);
void setDutyCyclePWM4(q15_t dutyCycle);

void changePeriod(void);
void receiveOffsetCalibration(void);
void setGateVoltage(void);
void setPeakVoltage(void);
void setOffsetVoltage(void);
void toggleMode(void);

/*********** Function Implementations *****************************************/
int main(void) {
    /* setup the hardware */
    initOsc();
    initInterrupts();
    initPwm();
    
    DIO_makeOutput(DIO_PORT_B, 9);  /* use for debugging */
    
    /* initialize the task manager */
    TASK_init();
    
    /* set the initial duty cycles */
    setDutyCyclePWM1(16384);
    setDutyCyclePWM2(8192);
    
    TASK_manage();
    
    return 0;
}
/******************************************************************************/
/* Tasks below this line */


/******************************************************************************/
/* Helper functions below this line */
void setDutyCyclePWM1(q15_t dutyCycle){
    CCP1RB = q15_mul(dutyCycle, CCP1PRL);
    
    /* when CCPxRB < 2, PWM doesn't update properly */
    if(CCP1RB < 2)
        CCP1RB = 2;
    else if(CCP1RB >= CCP1PRL)
        CCP1RB = CCP1PRL - 1;
    
    return;
}

void setDutyCyclePWM2(q15_t dutyCycle){
    CCP2RB = q15_mul(dutyCycle, CCP2PRL);
    
    /* when CCPxRB == 0, PWM doesn't update properly */
    if(CCP2RB < 2)
        CCP2RB = 2;
    else if(CCP2RB >= CCP2PRL)
        CCP2RB = CCP2PRL - 1;
    
    return;
}

void setDutyCyclePWM3(q15_t dutyCycle){
    CCP4RB = q15_mul(dutyCycle, CCP4PRL);
    
    /* when CCPxRB == 0, PWM doesn't update properly */
    if(CCP4RB < 2)
        CCP4RB = 2;
    else if(CCP4RB >= CCP4PRL)
        CCP5RB = CCP4PRL - 1;
}

void setDutyCyclePWM4(q15_t dutyCycle){
    CCP5RB = q15_mul(dutyCycle, CCP5PRL);
    
    /* when CCPxRB == 0, PWM doesn't update properly */
    if(CCP5RB < 2)
        CCP5RB = 2;
    else if(CCP5RB >= CCP5PRL)
        CCP5RB = CCP5PRL - 1;
}

q15_t getDutyCyclePWM2(void){
    return q15_div((q15_t)CCP2RB, (q15_t)CCP2PRL);
}

/******************************************************************************/
/* Initialization functions below this line */
void initOsc(void){
    /* for the moment, initialize the oscillator
     * on the highest internal frequency;  this will likely
     * change soon */
    CLKDIV = 0;

    return;
}

void initInterrupts(void){
    /* configure the global interrupt conditions */
    /* interrupt nesting disabled, DISI instruction active */
    INTCON1 = 0x8000;
    INTCON2 = 0x4000;
    
    /* initialize the period */
    PR1 = 1567;
    
    /* timer interrupts */
    T1CON = 0x0000;
    IFS0bits.T1IF = 0;
    IEC0bits.T1IE = 1;
    T1CONbits.TON = 1;
    
    return;
}

void initPwm(void){
    // use OC1C (pin 21, RB10) and OC2A (pin 22, RB11)
    DIO_makeDigital(DIO_PORT_B, 10);
    DIO_makeDigital(DIO_PORT_B, 11);
    DIO_makeDigital(DIO_PORT_B, 7);
    DIO_makeDigital(DIO_PORT_B, 9);
    
    /* Initialize MCCP/SCCP modules */
    
    /* period registers */
    CCP1PRH = CCP2PRH = CCP4PRH = CCP5PRH = 0;
    CCP1PRL = CCP2PRL = 1024;
    CCP4PRL = CCP5PRL = 256;
    
    CCP1CON1L = CCP2CON1L = CCP4CON1L = CCP5CON1L = 0x0005;
    CCP1CON1H = CCP2CON1H = CCP4CON1H = CCP5CON1H = 0x0000;
    CCP1CON2L = CCP2CON2L = CCP4CON2L = CCP5CON2L = 0x0000;
    
    CCP1CON2H = 0x8400; // enable output OC1C
    CCP2CON2H = 0x8100; // enable output 0C2A
    CCP4CON2H = 0x8100; // enable output OC4
    CCP5CON2H = 0x8100; // enable output OC5
    
    CCP1CON3L = CCP2CON3L = 0;  // dead time disabled
    CCP1CON3H = CCP2CON3H = CCP4CON3H = CCP5CON3H = 0x0000;
    
    CCP1CON1Lbits.CCPON = 
            CCP2CON1Lbits.CCPON = 
            CCP4CON1Lbits.CCPON = 
            CCP5CON1Lbits.CCPON = 1;
    
    /* duty cycle registers */
    CCP1RA = CCP2RA = CCP4RA = CCP5RA = 0;
    CCP1RB = CCP2RB = CCP4RB = CCP5RB = 0;
    
    return;
}

