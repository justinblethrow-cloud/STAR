#include "ParametersGenome.h"

#include <cstring>
#include <iostream>
#include <new>

int main()
{
    alignas(ParametersGenome) unsigned char storage[sizeof(ParametersGenome)];
    memset(storage, 0xbe, sizeof(storage));

    ParametersGenome *parameters=new (storage) ParametersGenome;
    const bool initialized=!parameters->transform.outYes
            && !parameters->transform.outSAM
            && !parameters->transform.outSJ
            && !parameters->transform.outQuant;
    parameters->~ParametersGenome();

    if (!initialized) {
        std::cerr << "Genome transform output state was not initialized\n";
        return 1;
    }
    return 0;
}
