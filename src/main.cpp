//main.cpp
#define DR_WAV_IMPLEMENTATION
#include "../external/dr_wav.h"

#include <iostream>
#include "WavReader/WavReader.h"

using namespace std;

int main()
{
    cout << "Launching Star Tune..." << endl;
    WavReader reader;
    reader.load("../../assets/sine_440.wav");
}   