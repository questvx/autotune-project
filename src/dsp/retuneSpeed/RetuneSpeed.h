#pragma once

class RetuneSpeed
{
public:
    RetuneSpeed(float speed);

    float process(float targetCorrection);
    float process(float targetCorrection, float response);

private:
    float speed;
    float currentCorrection;
};