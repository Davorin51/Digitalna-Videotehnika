#ifndef REMOTECONTROLLER_H
#define REMOTECONTROLLER_H

void *remoteThreadTask();
int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventRead);

#endif // REMOTECONTROLLER_H
