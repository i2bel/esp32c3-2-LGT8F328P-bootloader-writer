#ifndef _SWD_LGT8FX8P_H_
#define _SWD_LGT8FX8P_H_

#include <Arduino.h>

// Макросы управления (назначаются через platformio.ini или здесь)
#ifndef SWC_PIN
#define SWC_PIN 4
#define SWD_PIN 5
#define RSTN_PIN 6
#endif

#define SWC_CLR()   digitalWrite(SWC_PIN, LOW)
#define SWC_SET()   digitalWrite(SWC_PIN, HIGH)
#define SWD_CLR()   digitalWrite(SWD_PIN, LOW)
#define SWD_SET()   digitalWrite(SWD_PIN, HIGH)
#define SWD_IND()   pinMode(SWD_PIN, INPUT)
#define SWD_OUD()   pinMode(SWD_PIN, OUTPUT)
#define RSTN_CLR()  digitalWrite(RSTN_PIN, LOW)
#define RSTN_SET()  digitalWrite(RSTN_PIN, HIGH)
#define RSTN_IND()  pinMode(RSTN_PIN, INPUT)
#define RSTN_OUD()  pinMode(RSTN_PIN, OUTPUT)

#define SWD_Delay() delayMicroseconds(10)
#ifndef delayus
#define delayus(n)  delayMicroseconds(n)
#endif

// Прототипы функций
void SWD_init();
void SWD_exit();
void SWD_Idle(uint8_t cnt);
void SWD_WriteByte(uint8_t start, uint8_t data, uint8_t stop);
uint8_t SWD_ReadByte(uint8_t start, uint8_t stop);
void SWD_ReadGUID(char *guid);
uint8_t SWD_UnLock(uint8_t chip_erase);

class LGTISPClass {
  private:
    char guid[4];
    uint8_t chip_erased; // Добавлено сюда
  public:
    LGTISPClass();
    bool begin();
    void end();
    bool write(uint32_t addr, uint8_t buf[], int size);
    bool read(uint32_t addr, uint8_t buf[], int size);
    bool isPmode();      // Добавлено сюда
    uint32_t getGUID();  // Добавлено сюда
};

extern LGTISPClass LGTISP;
extern volatile uint8_t pmode;

#endif
