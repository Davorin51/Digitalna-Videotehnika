//Subtitle Parse

typedef struct {
    uint8_t streamType;
    uint16_t elementaryPID;
    uint16_t esInfoLength;
    uint8_t descriptor;
} Stream;

typedef struct {
    uint16_t sectionLength;
    uint16_t programNumber;
    uint16_t programInfoLength;
    uint8_t hasTTX;
    uint16_t audioPID;
    uint16_t videoPID;

    // Dodajemo polje za subtitles:
    uint16_t subtitlePID;  
    uint8_t hasSubtitles; 

    uint8_t streamCount;
    Stream streams[15];
} PMT;
PMT tablePMT[8];

parsePMT(uint8_t *buffer) {
    parseFlag = 0;
    tablePMT[i].sectionLength = (uint16_t)( ((*(buffer+1)<<8)+*(buffer+2)) & 0x0FFF );
    tablePMT[i].programNumber = (uint16_t)( (*(buffer+3)<<8)+*(buffer+4) );
    tablePMT[i].programInfoLength = (uint16_t)( ((*(buffer+10)<<8)+*(buffer+11)) & 0x0FFF );
    tablePMT[i].streamCount = 0;

    tablePMT[i].hasTTX = 0;
    tablePMT[i].hasSubtitles = 0;      // Inicijalno nema titlova
    tablePMT[i].subtitlePID = 0;       // Inicijalno 0

    int j;

    printf("\n\nSection arrived!!! PMT: %d \t%d\t%d\n",
           tablePMT[i].sectionLength,
           tablePMT[i].programNumber,
           tablePMT[i].programInfoLength);

    uint8_t *m_buffer = (uint8_t *)buffer + 12 + tablePMT[i].programInfoLength;

    for (j = 0; ((uint16_t)(m_buffer - buffer) + 5 < tablePMT[i].sectionLength); j++)
    {
        tablePMT[i].streams[j].streamType     = *(m_buffer);
        tablePMT[i].streams[j].elementaryPID  = (uint16_t)((*(m_buffer+1)<<8) + *(m_buffer+2)) & 0x1FFF;
        tablePMT[i].streams[j].esInfoLength   = (uint16_t)((*(m_buffer+3)<<8) + *(m_buffer+4)) & 0x0FFF;
        tablePMT[i].streams[j].descriptor     = (uint8_t)*(m_buffer+5);

        // find audio stream
        if (tablePMT[i].streams[j].streamType == 3) {
            tablePMT[i].audioPID = tablePMT[i].streams[j].elementaryPID;
        } 
        else if (tablePMT[i].streams[j].streamType == 2) {
            tablePMT[i].videoPID = tablePMT[i].streams[j].elementaryPID;
        }

        // find teletext
        if (tablePMT[i].streams[j].streamType == 6 && tablePMT[i].streams[j].descriptor == 86) {
            tablePMT[i].hasTTX = 1;
        }

        // *NOVA LOGIKA* - find subtitling descriptor
        // Tipično je streamType == 6 ( privatni data ), descriptor == 0x59
        if (tablePMT[i].streams[j].streamType == 6 && tablePMT[i].streams[j].descriptor == 0x59) {
            tablePMT[i].hasSubtitles  = 1;
            tablePMT[i].subtitlePID   = tablePMT[i].streams[j].elementaryPID;
            printf("Found SUBTITLE stream on PID: %d\n", tablePMT[i].subtitlePID);
        }

        printf("streamtype: %d epid: %d len %d desc: %d\n",
               tablePMT[i].streams[j].streamType,
               tablePMT[i].streams[j].elementaryPID,
               tablePMT[i].streams[j].esInfoLength,
               tablePMT[i].streams[j].descriptor);

        m_buffer += 5 + tablePMT[i].streams[j].esInfoLength;
        tablePMT[i].streamCount++;
    }
    printf("%d\thasTTX: %d, hasSubtitles: %d\n",
           tablePMT[i].streamCount,
           tablePMT[i].hasTTX,
           tablePMT[i].hasSubtitles);
}
