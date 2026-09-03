//Cents.cpp

#include "Cents.h"
#include <cmath>

float Cents::calculate(float detectedFrequency, float targetFrequency){
    if (detectedFrequency <=0.0f || targetFrequency <= 0.0f) {
        return 0.0f; // Handle invalid frequencies
    }
    return 1200*std::log2(detectedFrequency / targetFrequency);
};
