#include "tb6612fng_motor.h"

TB6612FNGMotor::TB6612FNGMotor(uint8_t in1, uint8_t in2, uint8_t pwm, uint8_t pwmChannel, uint32_t pwmFreq, uint8_t pwmResolution)
    : pinIN1(in1), pinIN2(in2), pinPWM(pwm), channel(pwmChannel), freq(pwmFreq), resolution(pwmResolution), currentDirection(Direction::STOP) {
    maxDuty = (1 << resolution) - 1;
}

void TB6612FNGMotor::begin() {
    pinMode(pinIN1, OUTPUT);
    pinMode(pinIN2, OUTPUT);
    ledcSetup(channel, freq, resolution);
    ledcAttachPin(pinPWM, channel);
    stop();
}

void TB6612FNGMotor::applyDirection(Direction dir) {
    switch (dir) {
        case Direction::FORWARD:
            digitalWrite(pinIN1, HIGH);
            digitalWrite(pinIN2, LOW);
            break;
        case Direction::REVERSE:
            digitalWrite(pinIN1, LOW);
            digitalWrite(pinIN2, HIGH);
            break;
        case Direction::STOP:
        default:
            digitalWrite(pinIN1, LOW);
            digitalWrite(pinIN2, LOW);
            break;
    }
    currentDirection = dir;
}

void TB6612FNGMotor::setSpeed(uint8_t speed) {
    ledcWrite(channel, map(speed, 0, 255, 0, maxDuty));
}

void TB6612FNGMotor::forward(uint8_t speed) {
    applyDirection(Direction::FORWARD);
    setSpeed(speed);
}

void TB6612FNGMotor::reverse(uint8_t speed) {
    applyDirection(Direction::REVERSE);
    setSpeed(speed);
}

void TB6612FNGMotor::stop() {
    applyDirection(Direction::STOP);
    setSpeed(0);
}

TB6612FNGMotor::Direction TB6612FNGMotor::getDirection() const {
    return currentDirection;
}
