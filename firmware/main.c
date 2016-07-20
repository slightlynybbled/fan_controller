/*
 * File:   main.c
 * Author: Jason
 */
#include "config.h"
#include "libmathq15.h"
#include "task.h"
#include "dio.h"
#include "eeprom.h"

/*********** Useful defines and macros ****************************************/
typedef enum {eINIT, eFAN_START, eNORMAL, eFAN_ADJ} FanState;

#define FAN_ADJUST_TIMEOUT 5000
#define MIN_FAN_DC  3277
#define NUM_OF_FANS 4
#define MILLISECONDS_AFTER_PWM_TO_FULL_SPEED 10

/*********** Variable Declarations ********************************************/
FanState fanState = eINIT;
uint32_t lastEncoderTime = 0;
uint8_t switchPressed = 0;
q15_t encoderTurned = 0;

q15_t dcFan[NUM_OF_FANS] = {0};
q15_t targetDcFan[NUM_OF_FANS] = {0};
q15_t inputPwmDutyCycle = 0;

uint32_t lastTimeInputPwm = 0;

/*********** Function Declarations ********************************************/
void initOsc(void);
void initInterrupts(void);
void initIO(void);
void initPwm(void);
void initTimer1(void);

void serviceFanState(void);
void serviceSwitch(void);
void serviceEncoder(void);

void setDutyCycleFan(uint8_t fan, q15_t dutyCycle);
void setDutyCycleFan0(q15_t dutyCycle);
void setDutyCycleFan1(q15_t dutyCycle);
void setDutyCycleFan2(q15_t dutyCycle);
void setDutyCycleFan3(q15_t dutyCycle);
q15_t rampDc(q15_t dc, q15_t targetDc);

/*********** Function Implementations *****************************************/
int main(void) {
    /* setup the hardware */
    initOsc();
    initInterrupts();
    initIO();
    initPwm();
    initTimer1();
    
    /* initialize the task manager */
    TASK_init();
    
    /* add tasks */
    TASK_add(&serviceFanState, 10);
    TASK_add(&serviceSwitch, 1);
    TASK_add(&serviceEncoder, 1);
    
    TASK_manage();
    
    return 0;
}

/******************************************************************************/
/* Tasks below this line */
void serviceFanState(void){
    static uint8_t lastFanAdjusted = 0;
    
    switch(fanState){
        case eINIT:
        {
            uint8_t i;
            for(i = 0; i < NUM_OF_FANS; i++){
                dcFan[i] = 0;
                setDutyCycleFan(i, 0);
                
                targetDcFan[i] = EEPROM_read(i);
                
                if(targetDcFan[i] == 0xffff){
                    targetDcFan[i] = 32767;
                    EEPROM_write(i, 32767);
                }
            }
            
            fanState = eFAN_START;
            switchPressed = 0;
            lastFanAdjusted = 0;
            
            break;
        }
        
        case eFAN_START:
        {
            /* start the fans */
            uint8_t i;
            for(i = 0; i < NUM_OF_FANS; i++){
                if(dcFan[i] != targetDcFan[0]){
                    dcFan[i] = rampDc(dcFan[i], targetDcFan[i]);
                    setDutyCycleFan(i, dcFan[i]);
                    break;
                }
            }
            
            /* deal with the adjust button being pressed */
            if(switchPressed){
                fanState = eFAN_ADJ;
                lastFanAdjusted = 0;
                encoderTurned = 0;
            }
            switchPressed = 0;
            
            break;
        }
        
        case eNORMAL:
        {
            /* deal with the adjust button being pressed */
            if(switchPressed){
                fanState = eFAN_ADJ;
                lastFanAdjusted = 0;
                encoderTurned = 0;
            }
            switchPressed = 0;
            
            /* determine when there is a valid input from the 
             * motherboard and how to scale that input, if present */
            uint32_t now = TASK_getTime();
            uint8_t i;
            for(i = 0; i < NUM_OF_FANS; i++){
                q15_t dc;
                
                /* when motherboard PWM is present, then scale the outputs
                 * to the stored maximums AND the input PWM; otherwise,
                 * simply scale to the stored maximums */
                if(now > (lastTimeInputPwm + MILLISECONDS_AFTER_PWM_TO_FULL_SPEED)){
                    dc = q15_mul(inputPwmDutyCycle, targetDcFan[i]);
                }else{
                    dc = targetDcFan[i];
                }
            }
            
            break;
        }
        
        case eFAN_ADJ:
        {
            /* when the encoder is being turned quickly, then move the
             * duty cycle quickly, else when the encoder is being turned
             * slowly, then move the duty cycle slowly */
            q15_t increment, dc;
            if((encoderTurned < 4) && (encoderTurned > -4))
                increment = 10 * encoderTurned;
            else
                increment = 100 * encoderTurned;
            
            /* dc = inc + lastDc */
            dc = q15_add(increment, dcFan[lastFanAdjusted]);
            
            /* place limits on the duty cycle */
            if(dc < 0)
                dc = 0;
            else if((dcFan[lastFanAdjusted] > 0) && (dc < MIN_FAN_DC))
                dc = 0;
            else if((dcFan[lastFanAdjusted] == 0) && (dc > 0))
                dc = MIN_FAN_DC;
            
            dcFan[lastFanAdjusted] = dc;
            
            encoderTurned = 0;
            
            /* deal with a timeout */
            if(TASK_getTime() > (lastEncoderTime + FAN_ADJUST_TIMEOUT)){
                fanState = eINIT;
                
                targetDcFan[lastFanAdjusted] = dcFan[lastFanAdjusted];
                EEPROM_write(lastFanAdjusted, dcFan[lastFanAdjusted]);
            }
            
            /* deal with the adjust button being pressed */
            if(switchPressed){
                targetDcFan[lastFanAdjusted] = dcFan[lastFanAdjusted];
                EEPROM_write(lastFanAdjusted, dcFan[lastFanAdjusted]);
                
                lastFanAdjusted++;
                if(lastFanAdjusted > NUM_OF_FANS)
                    lastFanAdjusted = 0;
            }
            switchPressed = 0;
            
            /* set the appropriate duty cycles - only the fan currently
             * being adjusted should be greater than 0 */
            uint8_t i;
            for(i = 0; i < NUM_OF_FANS; i++){
                if(i == lastFanAdjusted){
                    setDutyCycleFan(i, dcFan[i]);
                }else{
                    setDutyCycleFan(i, 0);
                }
            }
            
            break;
        }
        
        default:
        {
            while(1);   // programmer's trap
        }
        
    }
}

void serviceSwitch(void){
    static uint8_t switchArray = 0;
    static uint8_t state = 0;
    
    /* debounce */
    switchArray <<= 1;
    if(DIO_readPin(DIO_PORT_B, 2)){
        switchArray |= 0x01;
    }else{
        switchArray &= 0xfe;
    }
    
    /* edge detector */
    if((switchArray == 0xff) && (state == 0)){
        /* switch just released */
        state = 1;
        lastEncoderTime = TASK_getTime();
        
        switchPressed = 1;
    }else if((switchArray == 0x00) && (state == 1)){
        /* switch just pressed */
        state = 0;
        lastEncoderTime = TASK_getTime();
    }
}

void serviceEncoder(void){
    /* I found this routine at 
     * https://www.circuitsathome.com/mcu/reading-rotary-encoder-on-arduino 
     * appears to work well enough and I would like to give credit where
     * it is due. */
    
    static int8_t enc_states[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
    static uint8_t old_AB = 0;
    
    old_AB <<= 2;
    old_AB |= (uint8_t)(PORTA & 0x0003);
    
    int8_t state = enc_states[(old_AB & 0x0f)];
    
    if(state == 1){
        /* counter-clockwise */
        lastEncoderTime = TASK_getTime();
        encoderTurned--;    // reverse the direction of CW and CCW
    }else if(state == -1){
        /* clockwise */
        lastEncoderTime = TASK_getTime();
        encoderTurned++;    // reverse the direction of CW and CCW
    }
}

/******************************************************************************/
/* Helper functions below this line */
void setDutyCycleFan(uint8_t fan, q15_t dutyCycle){
    switch(fan){
        case 0:     setDutyCycleFan0(dutyCycle);    break;
        case 1:     setDutyCycleFan1(dutyCycle);    break;
        case 2:     setDutyCycleFan2(dutyCycle);    break;
        case 3:     setDutyCycleFan3(dutyCycle);    break;
        default:                                    break;
    }
}

void setDutyCycleFan0(q15_t dutyCycle){
    CCP2RB = q15_mul(dutyCycle, CCP2PRL);
    
    /* when CCPxRB == 0, PWM doesn't update properly */
    if(CCP2RB < 2)
        CCP2RB = 2;
    else if(CCP2RB >= CCP2PRL)
        CCP2RB = CCP2PRL - 1;
    
    return;
}

void setDutyCycleFan1(q15_t dutyCycle){
    CCP5RB = q15_mul(dutyCycle, CCP5PRL);
    
    /* when CCPxRB == 0, PWM doesn't update properly */
    if(CCP5RB < 2)
        CCP5RB = 2;
    else if(CCP5RB >= CCP5PRL)
        CCP5RB = CCP5PRL - 1;
}

void setDutyCycleFan2(q15_t dutyCycle){
    CCP1RB = q15_mul(dutyCycle, CCP1PRL);
    
    /* when CCPxRB < 2, PWM doesn't update properly */
    if(CCP1RB < 2)
        CCP1RB = 2;
    else if(CCP1RB >= CCP1PRL)
        CCP1RB = CCP1PRL - 1;
    
    return;
}

void setDutyCycleFan3(q15_t dutyCycle){
    CCP4RB = q15_mul(dutyCycle, CCP4PRL);
    
    /* when CCPxRB == 0, PWM doesn't update properly */
    if(CCP4RB < 2)
        CCP4RB = 2;
    else if(CCP4RB >= CCP4PRL)
        CCP4RB = CCP4PRL - 1;
}

q15_t rampDc(q15_t dc, q15_t targetDc){
    const q15_t rampIncrement = 10;
    q15_t newDc = 0;
    
    if(targetDc < MIN_FAN_DC){
        newDc = 0;
    }else if((targetDc >= MIN_FAN_DC) && (dc == 0)){
        newDc = MIN_FAN_DC;
    }else if(targetDc > dc){
        /* ramp the fan to its target */
        q15_t dcDiff = targetDc - dc;
        if(dcDiff > rampIncrement)
            dcDiff = rampIncrement;
        
        newDc = dc + dcDiff;
    }else if(targetDc < dc){
        /* sudden drops in DC are permitted */
        newDc = targetDc;
    }
    
    return newDc;
}

/******************************************************************************/
/* Initialization functions below this line */
void initOsc(void){
    CLKDIV = 0;

    return;
}

void initInterrupts(void){
    /* configure the global interrupt conditions */
    /* interrupt nesting disabled, DISI instruction active */
    INTCON1 = 0x8000;
    INTCON2 = 0x4000;
    
    /* allow change notification interrupts */
    IFS1bits.CNIF = 0;
    IEC1bits.CNIE = 1;
    
    /* enable CN interrupt for PWM and tach measurements */
    CNEN1bits.CN14IE = 1;
    CNEN1bits.CN13IE = 1;
    
    return;
}

void initIO(void){
    /* debugging outputs */
    DIO_makeOutput(DIO_PORT_A, 2);
    DIO_makeOutput(DIO_PORT_A, 3);
        
    /* encoder inputs */
    DIO_makeInput(DIO_PORT_A, 0);
    DIO_makeInput(DIO_PORT_A, 1);
    DIO_makeInput(DIO_PORT_B, 2);
    
    DIO_makeDigital(DIO_PORT_A, 0);
    DIO_makeDigital(DIO_PORT_A, 1);
    DIO_makeDigital(DIO_PORT_B, 2);
    
    /* fan PWM outputs */
    DIO_makeOutput(DIO_PORT_B, 11); /* PWM fan0 */
    DIO_makeOutput(DIO_PORT_A, 7);  /* PWM fan1 */
    DIO_makeOutput(DIO_PORT_B, 7);  /* PWM fan2 */
    
    DIO_makeInput(DIO_PORT_B, 5);   /* PWM fan3 - idle due to board rework */
    DIO_makeOutput(DIO_PORT_B, 9);  /* PWM fan3 */
    
    /* fan tach inputs */
    DIO_makeInput(DIO_PORT_B, 13);  /* tach fan0 */
    DIO_makeInput(DIO_PORT_A, 6);   /* tach fan1 */
    DIO_makeInput(DIO_PORT_B, 8);   /* tach fan2 */
    DIO_makeInput(DIO_PORT_B, 6);   /* tach fan3 */
    
    DIO_makeDigital(DIO_PORT_B, 13);
    
    /* PWM input, should be configured as an input */
    DIO_makeInput(DIO_PORT_B, 12);
    DIO_makeDigital(DIO_PORT_B, 12);
    
    /* tach output */
    DIO_makeOutput(DIO_PORT_B, 14);
}

void initPwm(void){
    /* Initialize MCCP/SCCP modules */
    
    /* period registers */
    CCP1PRH = CCP2PRH = CCP4PRH = CCP5PRH = 0;
    CCP1PRL = CCP2PRL = CCP4PRL = CCP5PRL = 640;
    
    CCP1CON1L = CCP2CON1L = CCP4CON1L = CCP5CON1L = 0x0005;
    CCP1CON1H = CCP2CON1H = CCP4CON1H = CCP5CON1H = 0x0000;
    CCP1CON2L = CCP2CON2L = CCP4CON2L = CCP5CON2L = 0x0000;
    
    CCP2CON2H = 0x8100; // enable output 0C2A (fan0)
    CCP5CON2H = 0x8100; // enable output OC5 (fan1)
    CCP1CON2H = 0x8100; // enable output OC1A (fan2)
    CCP4CON2H = 0x8100; // enable output OC4 (fan3)
    
    CCP1CON3L = CCP2CON3L = 0;  // dead time disabled
    CCP1CON3H = CCP2CON3H = CCP4CON3H = CCP5CON3H = 0x0020;
    
    CCP1CON1Lbits.CCPON = 
            CCP2CON1Lbits.CCPON = 
            CCP4CON1Lbits.CCPON = 
            CCP5CON1Lbits.CCPON = 1;
    
    /* duty cycle registers */
    CCP1RA = CCP2RA = CCP4RA = CCP5RA = 0;
    CCP1RB = CCP2RB = CCP4RB = CCP5RB = 0;
    
    /* set the initial duty cycles */
    setDutyCycleFan0(0);
    setDutyCycleFan1(0);
    setDutyCycleFan2(0);
    setDutyCycleFan3(0);
    
    return;
}

void initTimer1(void){
    T1CON = 0x8000;
    PR1 = 0xffff;
}

void _ISR _CNInterrupt(void){
    /* link with TMR1 */
    uint16_t hwTimer = TMR1;
    
    /* measure the PWM period */
    static uint8_t pwmInLast = 0;
    uint8_t pwmIn = DIO_readPin(DIO_PORT_B, 12);
    if(pwmIn != pwmInLast){
        static uint16_t pwmStartTime;
        static uint16_t period = 1000;
        static uint16_t onTime = 0;
        if(pwmIn){
            period = hwTimer - pwmStartTime;
            pwmStartTime = hwTimer;
        }else{
            onTime = hwTimer - pwmStartTime;
            
            if(onTime >= period)
                onTime = period - 1;
            
            /* calculate the PWM input duty cycle */
            inputPwmDutyCycle = q15_div(onTime, period);
            lastTimeInputPwm = TASK_getTime();
        }
    }
    pwmInLast = pwmIn;
    
    /* reflect FAN0 tach to the motherboard tach */
    static uint8_t lastTachFan0 = 0;
    uint8_t tachFan0 = DIO_readPin(DIO_PORT_B, 13);
    if(tachFan0 != lastTachFan0){
        /* set the output tach based on fan 0 */
        if(tachFan0 == 0){
            DIO_clearPin(DIO_PORT_B, 14);
        }else{
            DIO_setPin(DIO_PORT_B, 14);
        }
    }
    lastTachFan0 = tachFan0;
    
    IFS1bits.CNIF = 0;
}
