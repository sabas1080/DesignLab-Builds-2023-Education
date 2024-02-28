/****************************************************************************************************************************
  Andres Sabas @ DesignLab
  Original Creation Date: Jan 23, 2023

  Development environment specifics:
  IDE: Arduino 1.8.19
  Hardware Platform:
  - RP2040
  - MicroSD

  This code is beerware; if you see me (or any other Electronic Cats
  member) at the local, and you've found our code helpful,
  please buy us a round!

  Distributed as-is; no warranty is given.

  SD card connection
  SD card attached to SPI bus as follows:
   // Arduino-mbed core
   ** MISO - pin 4
   ** MOSI - pin 3
   ** CS   - pin 5
   ** SCK  - pin 2
***************************************************************************************/

//#define DEBUG

//Only RP2040
#if !defined(ARDUINO_ARCH_RP2040)
#error For RP2040 only
#endif

//Define SPI pins for MicroSD
#define PIN_SD_MOSI       PIN_SPI_MOSI
#define PIN_SD_MISO       PIN_SPI_MISO
#define PIN_SD_SCK        PIN_SPI_SCK
#define PIN_SD_SS         PIN_SPI_SS

#include <SPI.h>
#include <RP2040_SD.h> // For MicroSD, Library Manager
#include <stdio.h>
#include "stdlib.h"   // stdlib
#include "hardware/irq.h"  // interrupts
#include "hardware/pwm.h"  // pwm
#include "hardware/sync.h" // wait for interrupt
#include "hardware/pll.h"
#include "hardware/clocks.h"
#include <Wire.h>
#include <SW_MCP4017.h> //Volume Control, digital pot, https://github.com/SparkysWidgets/SW_MCP4017-Library

//Config MCP4017
uint8_t dpMaxSteps = 128; //remember even though the the digital pot has 128 steps it looses one on either end (usually cant go all the way to last tick)
int maxRangeOhms = 50000; //this is a 50K potentiometer
MCP4017 i2cDP(MCP4017ADDRESS, dpMaxSteps, maxRangeOhms);

uint8_t v = 1; //Level volume 1-3

char fname1[64];

File f;

// Audio pins
#define AUDIO_PIN 28  //Output audio
#define AMP_EN 0 //Enable Ampli, ON/OFF Ampli

//Buttons
#define Button1 8
#define Button2 7
#define Button3 2
#define Button4 29
#define Button5 11
#define Button6 10
#define Button7 9
#define Button8 20
#define Button9 13
#define Button10 12

#define VOL 1
#define REC 3

#define LANG 27
#define WORD 26

//Sensors page
#define S1 14
#define S2 6
#define S3 21
#define S4 24

//Detect MicroSD
#define CD 15

//Internal LED
#define LED 25

#define maskBtn(B) 1<<B

String path ="";
int soundWay = 1;

uint32_t cols;

uint32_t wav_position = 0;

uint32_t ChunkSize = 0;                 // this is the actual sound data size
uint8_t* WAV_DATA;

void printDirectory(File dir, int numTabs)
{
  while (true)
  {
    File entry =  dir.openNextFile();

    if (! entry)
    {
      // no more files
      break;
    }

    for (uint8_t i = 0; i < numTabs; i++)
    {
      Serial.print('\t');
    }

    Serial.print(entry.name());

    if (entry.isDirectory())
    {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    }
    else
    {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }

    entry.close();
  }
}
/**************************************************************************************/

typedef struct  WAV_HEADER
{
  /* RIFF Chunk Descriptor */
  uint8_t         RIFF[4];        // RIFF Header Magic header
  uint32_t        ChunkSize;      // RIFF Chunk Size
  uint8_t         WAVE[4];        // WAVE Header
  /* "fmt" sub-chunk */
  uint8_t         fmt[4];         // FMT header
  uint32_t        Subchunk1Size;  // Size of the fmt chunk
  uint16_t        AudioFormat;    // Audio format 1=PCM,6=mulaw,7=alaw,     257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
  uint16_t        NumOfChan;      // Number of channels 1=Mono 2=Sterio
  uint32_t        SamplesPerSec;  // Sampling Frequency in Hz
  uint32_t        bytesPerSec;    // bytes per second
  uint16_t        blockAlign;     // 2=16-bit mono, 4=16-bit stereo
  uint16_t        bitsPerSample;  // Number of bits per sample
  /* "data" sub-chunk */
  uint8_t         Subchunk2ID[4]; // "data"  string
  uint32_t        Subchunk2Size;  // Sampled data length
} wav_hdr;


/***********************************************************************************
   Inline functions to enable overclocking

************************************************************************************/
void set_sys_clock_pll(uint32_t vco_freq, uint post_div1, uint post_div2) {
  if (!running_on_fpga()) {
    clock_configure(clk_sys,
                    CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48 * MHZ,
                    48 * MHZ);

    pll_init(pll_sys, 1, vco_freq, post_div1, post_div2);
    uint32_t freq = vco_freq / (post_div1 * post_div2);

    // Configure clocks
    // CLK_REF = XOSC (12MHz) / 1 = 12MHz
    clock_configure(clk_ref,
                    CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC,
                    0,  // No aux mux
                    12 * MHZ,
                    12 * MHZ);

    // CLK SYS = PLL SYS (125MHz) / 1 = 125MHz
    clock_configure(clk_sys,
                    CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                    freq, freq);

    clock_configure(clk_peri,
                    0,  // Only AUX mux on ADC
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48 * MHZ,
                    48 * MHZ);
  }
}
bool check_sys_clock_khz(uint32_t freq_khz, uint *vco_out, uint *postdiv1_out, uint *postdiv_out) {
  uint crystal_freq_khz = clock_get_hz(clk_ref) / 1000;
  for (uint fbdiv = 320; fbdiv >= 16; fbdiv--) {
    uint vco = fbdiv * crystal_freq_khz;
    if (vco < 400000 || vco > 1600000) continue;
    for (uint postdiv1 = 7; postdiv1 >= 1; postdiv1--) {
      for (uint postdiv2 = postdiv1; postdiv2 >= 1; postdiv2--) {
        uint out = vco / (postdiv1 * postdiv2);
        if (out == freq_khz && !(vco % (postdiv1 * postdiv2))) {
          *vco_out = vco * 1000;
          *postdiv1_out = postdiv1;
          *postdiv_out = postdiv2;
          return true;
        }
      }
    }
  }
  return false;
}
static inline bool set_sys_clock_khz(uint32_t freq_khz, bool required) {
  uint vco, postdiv1, postdiv2;
  if (check_sys_clock_khz(freq_khz, &vco, &postdiv1, &postdiv2)) {
    set_sys_clock_pll(vco, postdiv1, postdiv2);
    return true;
  } else if (required) {
    panic("System clock of %u kHz cannot be exactly achieved", freq_khz);
  }
  return false;
}

/********************************************************************/


/*
   PWM Interrupt Handler which outputs PWM level and advances the
   current sample.

   We repeat the same value for 8 cycles this means sample rate etc
   adjust by factor of 8   (this is what bitshifting <<3 is doing)

*/

void pwm_interrupt_handler() {
  pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));
  if (wav_position < (ChunkSize << 3) - 1) {
    // set pwm level
    // allow the pwm value to repeat for 8 cycles this is >>3
    pwm_set_gpio_level(AUDIO_PIN, WAV_DATA[wav_position >> 3]);
    wav_position++;
  } else {
    // reset to start
    wav_position = 0;
    disableInt();
    digitalWrite(AMP_EN, LOW);
    path = "";
  }
}

bool readContents(const char *fname) {
  wav_hdr wavHeader;
  int headerSize = sizeof(wav_hdr);

  digitalWrite(AMP_EN, HIGH);

  f = SD.open(fname, FILE_READ);
  if (f) {
    size_t bytesRead = f.read(&wavHeader, headerSize);
#ifdef DEBUG
    Serial.print("Header Read "); Serial.print(bytesRead); Serial.println(" bytes.");
#endif
    if (bytesRead > 0) {
      //Read the data
      uint16_t bytesPerSample = wavHeader.bitsPerSample / 8;      //Number     of bytes per sample
      uint64_t numSamples = wavHeader.ChunkSize / bytesPerSample; //How many samples are in the wav file?
      //static const uint16_t BUFFER_SIZE = 4096;
#ifdef DEBUG
      Serial.println();
      Serial.print("RIFF header                :"); Serial.print((char)wavHeader.RIFF[0]); Serial.print((char)wavHeader.RIFF[1]); Serial.print((char)wavHeader.RIFF[2]); Serial.println((char)wavHeader.RIFF[3]);
      Serial.print("Chunk Size                 :"); Serial.print(wavHeader.ChunkSize); Serial.print(" TOTAL: "); Serial.print(wavHeader.ChunkSize + 8);  Serial.print(" DATA: "); Serial.println(wavHeader.ChunkSize - (36 + wavHeader.Subchunk2Size) - 8);
      Serial.print("WAVE header                :"); Serial.print((char)wavHeader.WAVE[0]); Serial.print((char)wavHeader.WAVE[1]); Serial.print((char)wavHeader.WAVE[2]); Serial.println((char)wavHeader.WAVE[3]);
      Serial.print("Subchunk1 ID (fmt)         :"); Serial.print((char)wavHeader.fmt[0]); Serial.print((char)wavHeader.fmt[1]); Serial.print((char)wavHeader.fmt[2]); Serial.println((char)wavHeader.fmt[3]);
      Serial.print("Subchunk1 size             :"); Serial.print(wavHeader.Subchunk1Size); Serial.println(wavHeader.Subchunk1Size == 16 ? " PCM" : "");

      // Display the sampling Rate from the header
      Serial.print("Audio Format               :"); Serial.println(wavHeader.AudioFormat == 1 ? "PCM" : (wavHeader.AudioFormat == 6 ? "mulaw" : (wavHeader.AudioFormat == 7 ? "alaw" : (wavHeader.AudioFormat == 257 ? "IBM Mu-Law" : (wavHeader.AudioFormat == 258 ? "IBM A-Law" : "ADPCM")))));
      Serial.print("Number of channels         :"); Serial.println(wavHeader.NumOfChan == 1 ? "Mono" : (wavHeader.NumOfChan == 2 ? "Mono" : "Other"));
      Serial.print("Sampling Rate              :"); Serial.println(wavHeader.SamplesPerSec);
      Serial.print("Number of bytes per second :"); Serial.println(wavHeader.bytesPerSec);
      Serial.print("Block align                :"); Serial.print(wavHeader.blockAlign); Serial.print(" validate: "); Serial.println(bytesPerSample * wavHeader.NumOfChan);
      Serial.print("Number of bits per sample  :"); Serial.println(wavHeader.bitsPerSample);
      Serial.print("Data length                :"); Serial.println(wavHeader.Subchunk2Size);
      // Audio format 1=PCM,6=mulaw,7=alaw, 257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM

      Serial.print("Data string (Subchunk2 ID) :"); Serial.print((char)wavHeader.Subchunk2ID[0]); Serial.print((char)wavHeader.Subchunk2ID[1]); Serial.print((char)wavHeader.Subchunk2ID[2]); Serial.println((char)wavHeader.Subchunk2ID[3]);
      Serial.print("Subchunk2 size             :"); Serial.print(wavHeader.Subchunk2Size); Serial.print(" validate: "); Serial.println(numSamples * wavHeader.NumOfChan * bytesPerSample);
#endif

      if (memcmp((char*)wavHeader.Subchunk2ID, "LIST", 4) == 0) {
#ifdef DEBUG
        Serial.println("List chunk (of a RIFF file):");
#endif
        uint8_t ListType[wavHeader.Subchunk2Size];        // RIFF Header Magic header

        bytesRead = f.read(&ListType, wavHeader.Subchunk2Size);

        if ( bytesRead > 0 ) {
#ifdef DEBUG
          Serial.print(" --- List type ID :"); Serial.print((char)ListType[0]); Serial.print((char)ListType[1]); Serial.print((char)ListType[2]); Serial.println((char)ListType[3]);
#endif
          if (memcmp((char*)ListType, "INFO", 4) == 0) {
            // INFO tag
#ifdef DEBUG
            Serial.print(" --- --- INFO1 type ID :"); Serial.print((char)ListType[4]); Serial.print((char)ListType[5]); Serial.print((char)ListType[6]); Serial.println((char)ListType[7]);
#endif
            uint8_t sizeTxt = (ListType[11] << 24) | (ListType[10] << 16) | (ListType[9] << 8) | ListType[8];
#ifdef DEBUG
            Serial.print(" --- --- SizeD :"); Serial.print(sizeTxt); Serial.print(" Validate: "); Serial.println(wavHeader.Subchunk2Size - sizeTxt);
#endif
          }
          for (uint8_t x = 12; x < wavHeader.Subchunk2Size; x++) {
            if (ListType[x] >= 32 && ListType[x] <= 126) {
              Serial.print((char)ListType[x]);
            }
            else if (ListType[x] > 0) {
              Serial.print("0x"); Serial.print(ListType[x], HEX);
            }
            Serial.print(" ");
          }
          Serial.println("");

          // Checking for data
          bytesRead = f.read(&ListType, 4);
          if (bytesRead > 0) {
#ifdef DEBUG
            Serial.print("Data string (Subchunk3 ID) :");
#endif

            for (uint8_t x = 0; x < 4; x++) {
              if (ListType[x] >= 32 && ListType[x] <= 126) {
                Serial.print((char)ListType[x]);
              }
              else if (ListType[x] > 0) {
                Serial.print("0x"); Serial.print(ListType[x], HEX);
              }
            }
            Serial.println("");
          }
          bytesRead = f.read(&ChunkSize, 4);
#ifdef DEBUG
          if (bytesRead > 0) {
            Serial.print("Subchunk3 size             :");
            Serial.print(ChunkSize); Serial.print(" validate: "); Serial.println(8 + numSamples * wavHeader.NumOfChan * bytesPerSample);
          }
#endif
        }
      }
      else {
        ChunkSize = wavHeader.Subchunk2Size;
      }

      WAV_DATA = new uint8_t[ChunkSize];

      bytesRead = f.read(WAV_DATA, ChunkSize / (sizeof WAV_DATA[0]));
      if (bytesRead)
      {
#ifdef DEBUG
        Serial.println("Sound File Data Read ");
#endif

        int fileSize = 0;
        f.seek(SEEK_END);
        fileSize = f.size();
        f.seek(SEEK_SET);
#ifdef DEBUG
        Serial.print("File size is: "); Serial.print(fileSize); Serial.println(" bytes.");
#endif
        f.close();
        enableInt();
      }
      delete [] WAV_DATA;
    }

  }
  else {
    Serial.println("File not found");
    return false;
  }
  return true;
}

void enableInt() {
  gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

  int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);

  // Setup PWM interrupt to fire when PWM cycle is complete
  pwm_clear_irq(audio_pin_slice);
  pwm_set_irq_enabled(audio_pin_slice, true);
  // set the handle function above
  irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_interrupt_handler);
  irq_set_enabled(PWM_IRQ_WRAP, true);

  // Setup PWM for audio output
  pwm_config config = pwm_get_default_config();
  /* Base clock 176,000,000 Hz divide by wrap 250 then the clock divider further divides
     to set the interrupt rate.

     11 KHz is fine for speech. Phone lines generally sample at 8 KHz


     So clkdiv should be as follows for given sample rate
      8.0f for 11 KHz
      4.0f for 22 KHz
      2.0f for 44 KHz etc
  */

  pwm_config_set_clkdiv(&config, 8.0f);
  pwm_config_set_wrap(&config, 250);
  //testing
  pwm_init(audio_pin_slice,pwm_gpio_to_channel(AUDIO_PIN),&config, true);

  pwm_set_gpio_level(AUDIO_PIN, 0);
}

void disableInt() {

  // Setup PWM interrupt to fire when PWM cycle is complete
  //pwm_clear_irq(audio_pin_slice);
  //pwm_set_irq_enabled(audio_pin_slice, true);

  // set the handle function above
  //irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_interrupt_handler);
  irq_set_enabled(PWM_IRQ_WRAP, false);
}

void readSensor() {
  cols = ~gpio_get_all();
}

void selectPage() {
  uint8_t pageNumber = ((cols & maskBtn(S1)) >> 14) + ((cols & maskBtn(S2)) >> 5) + ((cols & maskBtn(S3)) >> 19) + ((cols & maskBtn(S4)) >> 21);
#ifdef DEBUG
  Serial.print("pageNumber is: ");
  Serial.println(pageNumber);
#endif
  if (pageNumber == 0) {
    #ifdef DEBUG
    Serial.println("Flashcard not Found");
    #endif
  }
  if (pageNumber == 1) {
    path += "1/";
    //Serial.println("Page 1");
  }
  if (pageNumber == 2) {
    path += "2/";
    //
    //Serial.println("Page 2");
  }
  if (pageNumber == 3) {
    path += "3/";
    //
    //Serial.println("Page 3");
  }
  if (pageNumber == 4) {
    path += "4/";
    //
    //Serial.println("Page 4");
  }
  if (pageNumber == 5) {
    path += "5/";
    //
    //Serial.println("Page 5");
  }
  if (pageNumber == 6) {
    path += "6/";
    //
    //Serial.println("Page 6");
  }
  if (pageNumber == 7) {
    path += "7/";
    //
    //Serial.println("Page 7");
  }
  if (pageNumber == 8) {
    path += "8/";
    //
    //Serial.println("Page 8");
  }
  if (pageNumber == 9) {
    path += "9/";
    //
    //Serial.println("Page 9");
  }
  if (pageNumber == 10) {
    path += "10/";
    //
    //Serial.println("Page 10");
  }
  if (pageNumber == 11) {
    path += "11/";
    //
    //Serial.println("Page 11");
  }
  if (pageNumber == 12) {
    path += "12/";
    //
    //Serial.println("Page 12");
  }
  if (pageNumber == 13) {
    path += "13/";
    //
    //Serial.println("Page 13");
  }
  if (pageNumber == 14) {
    path += "14/";
    //
    //Serial.println("Page 14");
  }
  if (pageNumber == 15) {
    path += "15/";
    //Serial.println("Page 15");
  }
}

//
void selectButton() {
  if (cols & maskBtn(Button1)) {
    if(soundWay == 1){
      path += "a.wav";
    }
    if(soundWay ==2){
     path += "sa.wav";
    }
    Serial.println("Button 1");
  }
  if (cols & maskBtn(Button2)) {
    if(soundWay == 1){
      path += "b.wav";
    }
    if(soundWay == 2){
      path += "sb.wav";
    }
    Serial.println("Button 2");
  }
  if (cols & maskBtn(Button3)) {
    if(soundWay == 1){
    path += "c.wav";
    }
    if(soundWay == 2){
    path += "sc.wav";
    }
    Serial.println("Button 3");
  }
  if (cols & maskBtn(Button4)) {
    if(soundWay == 1){
    path += "d.wav";
    }
    if(soundWay == 2){
    path += "sd.wav";
    }
    Serial.println("Button 4");
  }
  if (cols & maskBtn(Button5)) {
    if(soundWay == 1){
    path += "e.wav";
    }
    if(soundWay == 2){
    path += "se.wav";
    }
    Serial.println("Button 5");
  }
  if (cols & maskBtn(Button6)) {
    if(soundWay == 1){
    path += "f.wav";
    }
    if(soundWay == 2){
    path += "sf.wav";
    }
    Serial.println("Button 6");
  }
  if (cols & maskBtn(Button7)) {
    if(soundWay == 1){
    path += "g.wav";
    }
    if(soundWay == 2){
    path += "sg.wav";
    }
    Serial.println("Button 7");
  }
  if (cols & maskBtn(Button8)) {
    if(soundWay == 1){
    path += "h.wav";
    }
    if(soundWay == 2){
    path += "sh.wav";
    }
    Serial.println("Button 8");
  }
  if (cols & maskBtn(Button9)) {
    if(soundWay == 1){
    path += "i.wav";
    }
    if(soundWay == 2){
    path += "si.wav";
    }
    Serial.println("Button 9");
  }
  if (cols & maskBtn(Button10)) {
    if(soundWay == 1){
    path += "j.wav";
    }
    if(soundWay == 2){
    path += "sj.wav";
    }
    Serial.println("Button 10");
  }

#ifdef DEBUG
  Serial.println("Path is: ");
  Serial.println(path);

  Serial.print("SoundWay: ");
  Serial.print(soundWay);
#endif
  if ((cols & maskBtn(Button1)) || (cols & maskBtn(Button2)) || (cols & maskBtn(Button3)) || (cols & maskBtn(Button4)) || (cols & maskBtn(Button5)) || (cols & maskBtn(Button6)) || (cols & maskBtn(Button7)) || (cols & maskBtn(Button8)) || (cols & maskBtn(Button9)) || (cols & maskBtn(Button10)))
  {
#ifdef DEBUG
    Serial.println("Playing...");
#endif
      readContents(path.c_str());
      soundWay = 2;
  }
}

void selecLang() {
  if (cols & maskBtn(LANG)) {
    path = "LANGA/";
    #ifdef DEBUG
    Serial.println("LANGA/");
    #endif
  }
  else {
    path = "LANGB/";
    #ifdef DEBUG
    Serial.println("LANGB/");
    #endif
  }
}

void selectwordPhrase() {

  if (cols & maskBtn(WORD)) {
    path += "w/";
    #ifdef DEBUG
    Serial.println("words");
    #endif
    selectButton();
  }
  else {
    path += "p/";
    #ifdef DEBUG
    Serial.println("phrase");
    #endif
    selectButton();
  }
}

void volume() {
  if (cols & maskBtn(VOL)) {
    #ifdef DEBUG
    Serial.print("Setting to volume: ");
    #endif
    if (v == 1) {
      i2cDP.setSteps(64);
      #ifdef DEBUG
      Serial.print("Medium ");
      Serial.println(v);
      #endif
    }
    if (v == 2) {
      i2cDP.setSteps(127);
      #ifdef DEBUG
      Serial.print("High ");
      Serial.println(v);
      #endif
    }
    if (v == 3) {
      i2cDP.setSteps(32);
      #ifdef DEBUG
      Serial.print("Low ");
      Serial.println(v);
      #endif
      v = 0;
    }
    v++;
    path = "sys/decide.wav";
    readContents(path.c_str());
  }
}

//blink with pin, time and times
void blink(int pin, int msdelay, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(msdelay);
    digitalWrite(pin, LOW);
    delay(msdelay);
  }
}

void setup()
{
  set_sys_clock_khz(176000, true);

  pinMode(LED_BUILTIN, OUTPUT); // LED OUTPUT
  pinMode(AMP_EN, OUTPUT); // ENABLE AMPLI OUTPUT

  // ALL PINPUT PULLUP
  pinMode(Button1, INPUT_PULLUP);
  pinMode(Button2, INPUT_PULLUP);
  pinMode(Button3, INPUT_PULLUP);
  pinMode(Button4, INPUT_PULLUP);
  pinMode(Button5, INPUT_PULLUP);
  pinMode(Button6, INPUT_PULLUP);
  pinMode(Button7, INPUT_PULLUP);
  pinMode(Button8, INPUT_PULLUP);
  pinMode(Button9, INPUT_PULLUP);
  pinMode(Button10, INPUT_PULLUP);

  // Sensor INPUT
  pinMode(S1, INPUT);
  pinMode(S2, INPUT);
  pinMode(S3, INPUT);
  pinMode(S4, INPUT);

  pinMode(VOL, INPUT_PULLUP);
  pinMode(REC, INPUT_PULLUP);

  pinMode(LANG, INPUT_PULLUP);
  pinMode(WORD, INPUT_PULLUP);

  pinMode(CD, INPUT_PULLUP);

  pinMode(LED, OUTPUT);

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
#ifdef DEBUG
  while (!Serial); //Must open Serial first
#endif

#ifdef DEBUG
  Serial.print("Starting SD Card");
  Serial.println(BOARD_NAME);

  Serial.print("Initializing SD card with SS = ");  Serial.println(PIN_SD_SS);
  Serial.print("SCK = ");   Serial.println(PIN_SD_SCK);
  Serial.print("MOSI = ");  Serial.println(PIN_SD_MOSI);
  Serial.print("MISO = ");  Serial.println(PIN_SD_MISO);
#endif

  digitalWrite(AMP_EN, LOW); // Disable Ampli

  if (!cols & maskBtn(CD)) {
    Serial.println("No Insert SD Card");
    while(1){
      blink(LED, 300, 5);
    }
  }

  if (!SD.begin(PIN_SD_SS))
  {
    Serial.println("Initialization SD failed!");
    while(1){
      blink(LED, 300, 5);
    }
  }

  f = SD.open("/");// Open SD

  #ifdef DEBUG
  printDirectory(f, 0);
  #endif

  Wire.begin();
  i2cDP.setSteps(32);

  #ifdef DEBUG
  Serial.println("Initialization done.");
  #endif
  blink(LED, 200, 3);
}

void loop()
{
  if ((wav_position == 0) && (soundWay == 1)) {
    readSensor();
    volume();
    selecLang();
    selectPage();
    selectwordPhrase();
  }
  if ((wav_position == 0) && (soundWay == 2)) {
    selecLang();
    selectPage();
    selectwordPhrase();
    soundWay = 1;
  }


#ifdef DEBUG
  delay(500);
#endif
}
