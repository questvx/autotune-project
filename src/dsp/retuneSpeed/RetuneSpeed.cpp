#include "RetuneSpeed.h"

RetuneSpeed::RetuneSpeed(float speed)
    : speed(speed),
      currentCorrection(0.0f)
{
}

float RetuneSpeed::process(float targetCorrection)
{
    return process(targetCorrection, speed);
}

float RetuneSpeed::process(float targetCorrection, float response)
{
    currentCorrection +=
        (targetCorrection - currentCorrection) * response;

    return currentCorrection;
}