#include "mbed.h"
#include "rtos.h"
#include "Serial.h"
#include "Servo.h"
#include "Motor.h"
#include "SoftPWM.h"
#include "explosion.h"
#include "shooting.h"
#include "ricochet.h"

#define sample_freq 11025.0
//get and set the frequency from wav conversion tool GUI
int i=0;
Ticker sampletick;

RawSerial blue(p9, p10);
Serial pc(USBTX, USBRX);
SoftPWM servo(p24, true);
Motor left_motor(p23, p15, p16); // pwm, fwd, rev
Motor right_motor(p22, p17, p19); // pwm, fwd, rev

// IR 
Serial ir_device(p13, p14);  // tx, rx
DigitalOut rx_led(LED1);  //LED1 and LED2 indicate TX/RX activity for IR
DigitalOut tx_led(LED2);
PwmOut IRLED(p21);

// Lives LEDs
BusOut lives_leds(p5, p6, p7);

// Speaker
AnalogOut speaker(p18);

volatile float speed_left = 0;
volatile float speed_right = 0;
volatile float turret_delta = 0;
volatile bool  fire = false;


Thread thread_drive;
Thread thread_turret;
Thread thread_ir_receive;

void input_loop();
inline void set_speed(float l, float r);
inline void set_turret_delta(float delta);
inline void fire_cannon(bool f);
inline void decrease_lives();


//interrupt routine to play next audio sample from array in flash
void explosion_audio_sample ()
{
    speaker.write_u16(explosion_sound_data[i]);
    i++;
    if (i>= EXPLOSION_NUM_ELEMENTS) {
        i = 0;
        sampletick.detach();
    }
}

//interrupt routine to play next audio sample from array in flash
void shooting_audio_sample ()
{
    speaker.write_u16(shooting_sound_data[i]);
    i++;
    if (i>= SHOOTING_NUM_ELEMENTS) {
        i = 0;
        sampletick.detach();
    }
}


//interrupt routine to play next audio sample from array in flash
void ricochet_audio_sample ()
{
    speaker.write_u16(ricochet_sound_data[i]);
    i++;
    if (i>= RICOCHET_NUM_ELEMENTS) {
        i = 0;
        sampletick.detach();
    }
}

void input_thread() {
    
}

void drive_thread() {
    while (1) {
        
        left_motor.speed(speed_left);
        right_motor.speed(speed_right);
        //pc.printf("left speed: %f, right speed: %f\n", speed_left, speed_right);
        Thread::wait(100);
    }
}

void turret_thread() {
    float pulsewidth = 0.0015;      // servo is between 1 ms and 2 ms    
    
    while (1) {
        servo.period(0.020);            // servo requires a 20ms period
        pulsewidth += turret_delta;
        if (pulsewidth > 0.002) pulsewidth = 0.002;
        else if (pulsewidth < 0.001) pulsewidth = 0.001;
        servo.pulsewidth(pulsewidth);
        
        Thread::wait(25);
    }
}

void ir_receive_thread() {
    // IR setup
    //Drive IR LED data pin with 38Khz PWM Carrier
    //Drive IR LED gnd pin with serial TX
    Thread::wait(1000); //Let other PWM threads initialize
    IRLED.period(1.0/38000.0);
    IRLED = 0.5;
    ir_device.baud(2400);
    while(1) {
        while (!ir_device.readable()) Thread::wait(0);
        char c = ir_device.getc();
        if (c == 'f') {   
            rx_led = 1;
            pc.putc(c);
            decrease_lives();
            rx_led = 0;
        }
    }
}

 
int main() {
    lives_leds = 7;

    thread_drive.start(drive_thread);
    thread_turret.start(turret_thread);
    thread_ir_receive.start(ir_receive_thread);
    
    input_loop();
}

void input_loop() {
    char bnum=0;
    char bhit=0;
    while(1) {
        if (blue.getc()=='!') {
            if (blue.getc()=='B') { //button data packet
                bnum = blue.getc(); //button number
                bhit = blue.getc(); //1=hit, 0=release
                if (blue.getc()==char(~('!' + 'B' + bnum + bhit))) { //checksum OK?
                    switch(bnum) {
                        case '1': //Turret Left Button
                            if (bhit == '1') {
                                set_turret_delta(-0.000025);
                            } else {
                                set_turret_delta(0);
                            }
                            break;
                        case '2': //Turret Right Button
                            if (bhit == '1') {
                                set_turret_delta(0.000025);
                            } else {
                                set_turret_delta(0);
                            }
                            break;
                        case '3': //Fire Button
                            if (bhit == '1') {
                                fire_cannon(true);
                            } else {
                                fire_cannon(false);
                            }
                            break;
                        case '5': //button 5 up arrow
                            if (bhit=='1') {
                                set_speed(0.5, 0.5);
                            } else {
                                set_speed(0, 0);
                            }
                            break;
                        case '6': //button 6 down arrow
                            if (bhit=='1') {
                                set_speed(-0.5, -0.5);
                            } else {
                                set_speed(0, 0);
                            }
                            break;
                        case '7': //button 7 left arrow
                            if (bhit=='1') {
                                set_speed(-0.5, 0.5);
                            } else {
                                set_speed(0, 0);
                            }
                            break;
                        case '8': //button 8 right arrow
                            if (bhit=='1') {
                                set_speed(0.5, -0.5);
                            } else {
                                set_speed(0, 0);
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        Thread::wait(50);
    }
}

inline void set_speed(float left, float right) {
    speed_left = left;
    speed_right = right;
}

inline void set_turret_delta(float delta) {
    turret_delta = delta;
}

inline void fire_cannon(bool f) {
    if (f) {
        sampletick.attach(&shooting_audio_sample, 1.0 / sample_freq);
        tx_led = 1;
        ir_device.putc('f');
        tx_led = 0;
        wait(1.0);
    }
}

inline void decrease_lives() {
    Thread::wait(1000);
    if (lives_leds == 7) {
        sampletick.attach(&ricochet_audio_sample, 1.0 / sample_freq);
        lives_leds = 3;
    } else if (lives_leds == 3) {
        sampletick.attach(&ricochet_audio_sample, 1.0 / sample_freq);
        lives_leds = 1;
    } else {
        lives_leds = 0;
        sampletick.attach(&explosion_audio_sample, 1.0 / sample_freq);
    }
}