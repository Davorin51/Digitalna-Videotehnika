#ifndef CHANNEL_H
#define CHANNEL_H

#include <stdint.h>

void changeChannel(int channel);
void parsePAT(uint8_t *buffer);
void parsePMT(uint8_t *buffer);

#endif // CHANNEL_H
