#include "RetuneSpeed.h"

RetuneSpeed::RetuneSpeed(float speed)
    : speed(speed),
      currentCorrection(0.0f)
{
}

float RetuneSpeed::process(float targetCorrection)
{
    currentCorrection +=
        (targetCorrection - currentCorrection) * speed;

    return currentCorrection;
}