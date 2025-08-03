#ifndef TB6612FNGMOTOR_H
#define TB6612FNGMOTOR_H

#include <Arduino.h>

class TB6612FNGMotor {
public:
    TB6612FNGMotor(uint8_t in1, uint8_t in2, uint8_t pwm, uint8_t pwmChannel = 0, uint32_t pwmFreq = 1000, uint8_t pwmResolution = 8);

    void begin();
    void setSpeed(uint8_t speed);
    void forward(uint8_t speed);
    void reverse(uint8_t speed);
    void stop();

    enum class Direction { STOP, FORWARD, REVERSE };
    Direction getDirection() const;

private:
    uint8_t pinIN1, pinIN2, pinPWM;
    uint8_t channel;
    uint32_t freq;
    uint8_t resolution;
    uint32_t maxDuty;
    Direction currentDirection;

    void applyDirection(Direction dir);
};

#endif // TB6612FNGMOTOR_H
