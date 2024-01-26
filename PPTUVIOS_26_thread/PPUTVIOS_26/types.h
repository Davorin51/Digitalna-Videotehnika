#ifndef TYPES_H
#define TYPES_H


typedef struct{
    uint8_t streamType;
    uint16_t elementaryPID;
    uint16_t esInfoLength;
    uint8_t descriptor;
} Stream;

typedef struct{
    uint16_t sectionLength;
    uint16_t programNumber;
    uint16_t programInfoLength;
    uint8_t hasTTX;
    uint16_t audioPID;
    uint16_t videoPID;
    uint8_t streamCount;
    Stream streams[15];
} PMT;
PMT tablePMT[8];

typedef struct{
    uint16_t sectionLength;
    uint16_t transportStream;
    uint8_t versionNumber;
    uint16_t programNumber[8];
    uint16_t PID[8];
} PAT;
PAT tablePAT;


typedef struct {
  int bandwidth;
  int frequency;
  char module[50];
}tunerData;

typedef struct {
    int audioPID;
    int videoPID;
    char audioType[50];
    char videoType[50];
}initService;


#endif