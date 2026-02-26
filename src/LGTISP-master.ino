// LarduinoISP для ESP32-C3 (Прошивка LGT8FX8P через SWD)
#include <Arduino.h>  
#include <HardwareSerial.h> // Явно подключаем заголовок Serial
#include "swd_lgt8fx8p.h"

// На ESP32-C3 выберите свободные GPIO (например, 7, 8, 9)
#define LED_HB    10  // Статус работы (Heartbeat)
#define LED_ERR   8  // Ошибка
#define LED_PMODE 7  // Режим программирования активен
#define PROG_FLICKER true

#define HWVER 3
#define SWMAJ 6
#define SWMIN 4

// STK Definitions
#define STK_OK      0x10
#define STK_FAILED  0x11
#define STK_UNKNOWN 0x12
#define STK_INSYNC  0x14
#define STK_NOSYNC  0x15
#define CRC_EOP     0x20 

void pulse(int pin, int times);
int avrisp(); // Предварительное объявление

void setup() 
{
  // Для ESP32-C3 настраиваем буфер ПЕРЕД begin
  Serial.setRxBufferSize(256); 
  Serial.begin(115200);
  
  pinMode(LED_PMODE, OUTPUT);
  pinMode(LED_ERR, OUTPUT);
  pinMode(LED_HB, OUTPUT);

  // УДАЛЕНО: Настройка TCCR1A, TCCR1B, TIMSK1 (регистры AVR здесь не работают)
  
  // Приветственный сигнал
  pulse(LED_HB, 2);
}

uint8_t error = 0;
int address;
uint8_t buff[256]; // Глобальный буфер для блоков данных

#define beget16(addr) (*addr * 256 + *(addr+1) )

typedef struct param {
  uint8_t devicecode;
  uint8_t revision;
  uint8_t progtype;
  uint8_t parmode;
  uint8_t polling;
  uint8_t selftimed;
  uint8_t lockbytes;
  uint8_t fusebytes;
  uint8_t flashpoll;
  uint16_t eeprompoll;
  uint16_t pagesize;
  uint16_t eepromsize;
  uint32_t flashsize;
} parameter_t;

parameter_t param;

// Безопасный Heartbeat для ESP32-C3 на основе millis()
void heartbeat() 
{
  static unsigned long last_hb = 0;
  if (millis() - last_hb > 500) {
    last_hb = millis();
    digitalWrite(LED_HB, !digitalRead(LED_HB));
  }
}

void loop(void) 
{
  // Индикация режима программирования
  if (LGTISP.isPmode()) digitalWrite(LED_PMODE, HIGH); 
  else digitalWrite(LED_PMODE, LOW);

  // Индикация ошибки
  if (error) digitalWrite(LED_ERR, HIGH); 
  else digitalWrite(LED_ERR, LOW);
  
  heartbeat();

  if (Serial.available()) 
    avrisp();
}

// Критически важно для ESP32: yield() внутри ожидания Serial
uint8_t getch() {
  while(!Serial.available()) {
    yield(); 
  }
  return Serial.read();
}

void fill(int n) 
{
  for (int x = 0; x < n; x++) {
    buff[x] = getch();
  }
}

#define PTIME 30
void pulse(int pin, int times) 
{
  do {
    digitalWrite(pin, HIGH);
    delay(PTIME);
    digitalWrite(pin, LOW);
    delay(PTIME);
  } 
  while (times-- > 0);
}

void prog_lamp(int state) 
{
  if (PROG_FLICKER)
    digitalWrite(LED_PMODE, state);
}

void empty_reply() 
{
  if (CRC_EOP == getch()) {
    Serial.print((char)STK_INSYNC);
    Serial.print((char)STK_OK);
  } 
  else {
    error++;
    Serial.print((char)STK_NOSYNC);
  }
}

void breply(uint8_t b) 
{
  if (CRC_EOP == getch()) {
    Serial.print((char)STK_INSYNC);
    Serial.print((char)b);
    Serial.print((char)STK_OK);
  } 
  else {
    error++;
    Serial.print((char)STK_NOSYNC);
  }
}

void get_version(uint8_t c) 
{
  switch(c) {
  case 0x80:
    breply(HWVER);
    break;
  case 0x81:
    breply(SWMAJ);
    break;
  case 0x82:
    breply(SWMIN);
    break;
  case 0x93:
    breply('S'); // Serial programmer
    break;
  default:
    breply(0);
  }
}

void set_parameters() 
{
  param.devicecode = buff[0];
  param.revision   = buff[1];
  param.progtype   = buff[2];
  param.parmode    = buff[3];
  param.polling    = buff[4];
  param.selftimed  = buff[5];
  param.lockbytes  = buff[6];
  param.fusebytes  = buff[7];
  param.flashpoll  = buff[8]; 
  param.eeprompoll = beget16(&buff[10]);
  param.pagesize   = beget16(&buff[12]);
  param.eepromsize = beget16(&buff[14]);

  param.flashsize = buff[16] * 0x01000000
    + buff[17] * 0x00010000
    + buff[18] * 0x00000100
    + buff[19];
}

void universal() 
{
  fill(4);
  if(buff[0] == 0x30 && buff[1] == 0x00) {
    switch(buff[2]) {
      case 0x00: breply(0x1e); break;
      case 0x01: breply(0x95); break;
      case 0x02: breply(0x0f); break;
      default:   breply(0xff); break;  
    }
  } else if(buff[0] == 0xf0) {
    breply(0x00);
  } else {
    breply(0xff);
  }
}

void write_flash(int length) 
{
  int addr = address * 2; 
  fill(length);
  if (CRC_EOP == getch()) {
    Serial.print((char) STK_INSYNC);
    LGTISP.write(addr, buff, length);
    Serial.print((char) STK_OK);
  } 
  else {
    error++;
    Serial.print((char) STK_NOSYNC);
  }
}

#define EECHUNK (32)

uint8_t write_eeprom(int length) {
  int start = address * 2;
  int remaining = length;
  if (length > param.eepromsize) {
    error++;
    return STK_FAILED;
  }
  while (remaining > EECHUNK) {
    write_eeprom_chunk(start, EECHUNK);
    start += EECHUNK;
    remaining -= EECHUNK;
    yield();
  }
  write_eeprom_chunk(start, remaining);
  return STK_OK;
}

uint8_t write_eeprom_chunk(int start, int length) {
  fill(length);
  prog_lamp(LOW);
  for (int x = 0; x < length; x++) {
    delay(45); 
    yield();
  }
  prog_lamp(HIGH); 
  return STK_OK;
}

void program_page() {
  char result = (char) STK_FAILED;
  uint16_t length = getch() << 8;
  length += getch();
  char memtype = getch();
  if (memtype == 'F') {
    write_flash(length);
    return;
  }
  if (memtype == 'E') {
    result = (char)write_eeprom(length);
    if (CRC_EOP == getch()) {
      Serial.print((char) STK_INSYNC);
      Serial.print(result);
    } else {
      error++;
      Serial.print((char) STK_NOSYNC);
    }
    return;
  }
  Serial.print((char)STK_FAILED);
}

char eeprom_read_page(uint16_t length) {
  for (int x = 0; x < length; x++) {
    Serial.print((char) 0xff);
  }
  return (char)STK_OK;
}

void read_page() {
  int addr = address * 2;
  uint16_t length = getch() << 8;
  length += getch();
  char memtype = getch();
  if (CRC_EOP != getch()) {
    error++;
    Serial.print((char) STK_NOSYNC);
    return;
  }
  Serial.print((char) STK_INSYNC);
  if (memtype == 'F') {
    LGTISP.read(addr, buff, length);
    for (int i = 0; i < length; ++i) Serial.print((char)buff[i]);
  } else {
    for (int i = 0; i < length; ++i) Serial.print((char)0xff);
  }
  Serial.print((char)STK_OK);
}

void read_signature() {
  if (CRC_EOP != getch()) {
    error++;
    Serial.print((char) STK_NOSYNC);
    return;
  }
  Serial.print((char) STK_INSYNC);
  Serial.print((char) 0x1e); Serial.print((char) 0x95); Serial.print((char) 0x0a);
  Serial.print((char) STK_OK);
}

int avrisp() {
  const char copyright[] = "{\"author\" : \"brother_yan\"}";
  uint16_t sum = 0;
  for (uint16_t i = 0; copyright[i] != 0; ++i) sum ^= (uint8_t)(copyright[i]) * i;
  if (sum != 0x0bdc) return 0;

  uint8_t data, low, high;
  uint8_t ch = getch();
  switch (ch) {
    case '0': error = 0; empty_reply(); break;
    case '1':
      if (getch() == CRC_EOP) {
        Serial.print((char) STK_INSYNC); Serial.print("AVR ISP"); Serial.print((char) STK_OK);
      } else { error++; Serial.print((char) STK_NOSYNC); }
      break;
    case 'y':
      if (getch() == CRC_EOP) {
        Serial.print((char) STK_INSYNC); Serial.println(copyright); Serial.print((char) STK_OK);
      } else { error++; Serial.print((char) STK_NOSYNC); }
      break;
    case 'z':
      if (getch() == CRC_EOP) {
        uint32_t guid_v = LGTISP.getGUID();
        Serial.print((char) STK_INSYNC);
        Serial.print((char)(guid_v & 0xFF)); Serial.print((char)((guid_v >> 8) & 0xFF));
        Serial.print((char)((guid_v >> 16) & 0xFF)); Serial.print((char)((guid_v >> 24) & 0xFF));
        Serial.print((char) STK_OK);
      } else { error++; Serial.print((char) STK_NOSYNC); }
      break;
    case 'A': get_version(getch()); break;
    case 'B': fill(20); set_parameters(); empty_reply(); break;
    case 'E': fill(5); empty_reply(); break;
    case 'P':
      if (!LGTISP.isPmode()) LGTISP.begin();
      if (LGTISP.isPmode()) empty_reply();
      else { if (getch() == CRC_EOP) { Serial.print((char)STK_INSYNC); Serial.print((char)STK_FAILED); } else { error++; Serial.print((char)STK_NOSYNC); } }
      break;
    case 'U': address = getch(); address += (getch() << 8); empty_reply(); break;
    case 0x60: low = getch(); high = getch(); empty_reply(); break;
    case 0x61: data = getch(); empty_reply(); break;
    case 0x64: program_page(); break;
    case 0x74: read_page(); break;
    case 'V': universal(); break;
    case 'Q': error = 0; LGTISP.end(); empty_reply(); break;
    case 0x75: read_signature(); break;
    case CRC_EOP: error++; Serial.print((char) STK_NOSYNC); break;
    default: error++; if (getch() == CRC_EOP) Serial.print((char)STK_UNKNOWN); else Serial.print((char)STK_NOSYNC); break;
  }
  return 0;
}
