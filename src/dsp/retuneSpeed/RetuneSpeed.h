#pragma once

class RetuneSpeed
{
public:
    RetuneSpeed(float speed);

    float process(float targetCorrection);

private:
    float speed;
    float currentCorrection;
};