// Tutorial : https://youtu.be/mnOzfRFQJIM
// Use board "ESP32S3 Dev Module" (last tested on v3.2.0)
//
#include <PINS_JC4827W543.h>    // Install "GFX Library for Arduino" with the Library Manager (last tested on v1.5.6)
                                // Install "Dev Device Pins" with the Library Manager (last tested on v0.0.2)
#include <AnimatedGIF.h>        // Install "AnimatedGIF" with the Library Manager (last tested on v2.2.0)
#include <SD.h>                 // Included with the Espressif Arduino Core (last tested on v3.2.0)

const char *GIF_FOLDER = "/gif";
const uint8_t VIDEO_PINS[] = {46, 9, 14};
const uint8_t VIDEO_COUNT = sizeof(VIDEO_PINS) / sizeof(VIDEO_PINS[0]);
const uint8_t VIDEO_ACTIVE_LEVEL = HIGH;

AnimatedGIF gif;
int16_t display_width, display_height;

// Storage for files to read on the SD card, adjust maximum value as needed
#define MAX_FILES 20 // Adjust as needed
String gifFileList[MAX_FILES];
uint32_t gifFileSizes[MAX_FILES] = {0}; // Store each GIF file's size in bytes
int fileCount = 0;
static File FSGifFile; // temp gif file holder
static int lastSelectedVideo = -2;

static SPIClass spiSD{HSPI};

// PSRAM for GIF playing optimization
#define PSRAM_RESERVE_SIZE (100 * 1024) // Leave 100KB of PSRAM free
uint8_t *psramBuffer = NULL;
size_t reservedPSRAMSize = 0;

void loadGifFilesList();
void sortGifFilesByName();
int getSelectedVideoIndex();
bool isVideoStillSelected(int expectedVideo);
void playSelectedFile(int fileindex);
uint8_t *reservePSRAM();
void gifPlayFromSDCard(const char *gifPath, int expectedVideo);
static void *GIFOpenFile(const char *fname, int32_t *pSize);
static void GIFCloseFile(void *pHandle);
static int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
static int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition);
void GIFDraw(GIFDRAW *pDraw);

void setup()
{
  Serial.begin(115200);
  delay(2000); // Give time to the serial port to show initial messages printed on the serial port upon reset

  // SD Card initialization
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD, 10000000))
  {
    Serial.println("ERROR: SD Card mount failed!");
    while (true)
    {
      /* no need to continue */
    }
  }

  // Start the Display
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
    while (true)
    {
      /* no need to continue */
    }
  }
  gfx->fillScreen(RGB565_BLACK);

  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH); // Set the backlight of the screen to High intensity
  for (uint8_t i = 0; i < VIDEO_COUNT; i++)
  {
    pinMode(VIDEO_PINS[i], INPUT);
  }

  display_width = gfx->width();
  display_height = gfx->height();
  gif.begin(BIG_ENDIAN_PIXELS);

  if (!psramFound())
  {
    Serial.println("No PSRAM found > Enable it by selecting OPI PSRAM in the board configuration");
  }

  // Reserve PSRAM (leaving PSRAM_RESERVE_SIZE free for other usage)
  uint8_t *myBuffer = reservePSRAM();
  if (myBuffer == NULL)
  {
    Serial.println("PSRAM reserve failed!");
    // Handle error...
  }

  Serial.println("Loading GIF files list");
  loadGifFilesList();
  if (fileCount < VIDEO_COUNT)
  {
    Serial.printf("Need at least %u GIF files in %s, found %d.\n", VIDEO_COUNT, GIF_FOLDER, fileCount);
    while (true)
    {
      delay(1000);
    }
  }

  sortGifFilesByName();
  Serial.println("Pin to GIF mapping:");
  for (uint8_t i = 0; i < VIDEO_COUNT; i++)
  {
    Serial.printf("Pin %u -> %s\n", VIDEO_PINS[i], gifFileList[i].c_str());
  }
}

void loop()
{
  int selectedVideo = getSelectedVideoIndex();

  if (selectedVideo != lastSelectedVideo)
  {
    if (selectedVideo < 0)
    {
      Serial.println("No active pin, waiting...");
      gfx->fillScreen(RGB565_BLACK);
    }
    else
    {
      Serial.printf("Pin %u active -> %s\n", VIDEO_PINS[selectedVideo], gifFileList[selectedVideo].c_str());
    }
    lastSelectedVideo = selectedVideo;
  }

  if (selectedVideo < 0)
  {
    delay(20);
    return;
  }

  playSelectedFile(selectedVideo);
}

int getSelectedVideoIndex()
{
  for (uint8_t i = 0; i < VIDEO_COUNT; i++)
  {
    if (digitalRead(VIDEO_PINS[i]) == VIDEO_ACTIVE_LEVEL)
    {
      return i;
    }
  }
  return -1;
}

bool isVideoStillSelected(int expectedVideo)
{
  return getSelectedVideoIndex() == expectedVideo;
}

void sortGifFilesByName()
{
  for (int i = 0; i < fileCount - 1; i++)
  {
    for (int j = i + 1; j < fileCount; j++)
    {
      if (gifFileList[j].compareTo(gifFileList[i]) < 0)
      {
        String tempName = gifFileList[i];
        gifFileList[i] = gifFileList[j];
        gifFileList[j] = tempName;

        uint32_t tempSize = gifFileSizes[i];
        gifFileSizes[i] = gifFileSizes[j];
        gifFileSizes[j] = tempSize;
      }
    }
  }
}

// Play the selected gif file
void playSelectedFile(int fileindex)
{
  if (fileindex < 0 || fileindex >= fileCount)
  {
    return;
  }

  // Build the full path for the selected GIF.
  String fullPath = String(GIF_FOLDER) + "/" + gifFileList[fileindex];
  char gifFilename[128];
  fullPath.toCharArray(gifFilename, sizeof(gifFilename));

  Serial.printf("Playing %s\n", gifFilename);

  // Check if the file can fit in the reserved PSRAM, playing from PSRAM instead of the SD card is faster
  if (psramBuffer != NULL && gifFileSizes[fileindex] <= reservedPSRAMSize)
  {
    File gifFile = SD.open(gifFilename);
    if (gifFile)
    {
      size_t fileSize = gifFile.size();
      size_t bytesRead = gifFile.read(psramBuffer, fileSize);
      gifFile.close();
      Serial.printf("Read %u bytes into PSRAM\n", bytesRead);

      // Try opening the GIF from the PSRAM buffer.
      if (gif.open(psramBuffer, fileSize, GIFDraw))
      {
        Serial.printf("Successfully opened GIF from PSRAM.\n");
        while (isVideoStillSelected(fileindex) && gif.playFrame(false, NULL))
        {
        }
        gif.close();
      }
      else
      {
        Serial.printf("Failed to open GIF from PSRAM, falling back to SD.\n");
        gifPlayFromSDCard(gifFilename, fileindex);
      }
    }
    else
    {
      Serial.printf("Failed to open %s for reading into PSRAM.\n", gifFilename);
      gifPlayFromSDCard(gifFilename, fileindex);
    }
  }
  else
  {
    if (psramBuffer == NULL)
    {
      Serial.println("PSRAM buffer unavailable; opening GIF directly from SD.");
    }
    else
    {
      Serial.println("File too big for reserved PSRAM; opening GIF directly from SD.");
    }
    gifPlayFromSDCard(gifFilename, fileindex);
  }
}

// Reserve a block of PSRAM to play gif since it is faster than the SD card
uint8_t *reservePSRAM()
{
  if (!psramFound())
  {
    Serial.println("No PSRAM found!");
    return NULL;
  }

  // Get the total free PSRAM size (in bytes)
  size_t freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  // Ensure we leave at least PSRAM_RESERVE_MARGIN free
  if (freePSRAM <= PSRAM_RESERVE_SIZE)
  {
    Serial.println("Not enough free PSRAM available to reserve!");
    return NULL;
  }

  // Calculate the amount we can reserve
  reservedPSRAMSize = freePSRAM - PSRAM_RESERVE_SIZE;

  // Allocate the buffer from PSRAM
  psramBuffer = (uint8_t *)heap_caps_malloc(reservedPSRAMSize, MALLOC_CAP_SPIRAM);

  if (psramBuffer != NULL)
  {
    Serial.printf("Reserved %u bytes from PSRAM, leaving %u bytes free.\n",
                  reservedPSRAMSize, PSRAM_RESERVE_SIZE);
  }
  else
  {
    Serial.println("Failed to allocate PSRAM!");
  }

  return psramBuffer;
}

// Play a gif directly from the SD card
void gifPlayFromSDCard(const char *gifPath, int expectedVideo)
{

  if (!gif.open(gifPath, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    Serial.printf("Could not open gif %s", gifPath);
  }
  else
  {
    gfx->fillScreen(RGB565_BLACK);
    Serial.printf("Starting playing gif %s\n", gifPath);

    while (isVideoStillSelected(expectedVideo) && gif.playFrame(false /*change to true to use the internal gif frame duration*/, NULL))
    {
    }

    gif.close();
  }
}

// Callback function to open a gif file from the SD card
static void *GIFOpenFile(const char *fname, int32_t *pSize)
{
  Serial.printf("Opening %s from SD\n", fname);
  FSGifFile = SD.open(fname);
  if (FSGifFile)
  {
    *pSize = FSGifFile.size();
    return (void *)&FSGifFile;
  }
  return NULL;
}

// Callback function to close a gif file from the SD card
static void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
    f->close();
}

// Callback function to read a gif file from the SD card
static int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
  int32_t iBytesRead;
  iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  // Note: If you read a file all the way to the last byte, seek() stops working
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
  if (iBytesRead <= 0)
    return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}

// Callback function to seek a gif file from the SD card
static int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
  // log_d("Seek time = %d us\n", i);
  return pFile->iPos;
}

// Read the gif file list in the gif folder
void loadGifFilesList()
{
  File gifDir = SD.open(GIF_FOLDER);
  if (!gifDir)
  {
    Serial.println("Failed to open GIF folder");
    return;
  }
  fileCount = 0;
  while (true)
  {
    File file = gifDir.openNextFile();
    if (!file)
      break;
    if (!file.isDirectory())
    {
      String name = file.name();
      if (name.endsWith(".gif") || name.endsWith(".GIF"))
      {
        gifFileList[fileCount] = name;
        gifFileSizes[fileCount] = file.size(); // Save file size (in bytes)
        fileCount++;
        if (fileCount >= MAX_FILES)
          break;
      }
    }
    file.close();
  }
  gifDir.close();
  Serial.printf("%d gif files read\n", fileCount);
  // Optionally, print out each file's size for debugging:
  for (int i = 0; i < fileCount; i++)
  {
    Serial.printf("File %d: %s, Size: %lu bytes\n", i, gifFileList[i].c_str(), gifFileSizes[i]);
  }
}

// Callback function to Draw a line of image directly on the screen
void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[display_width];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > display_width)
  {
    iWidth = display_width - pDraw->iX;
  }
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line
  if (y >= display_height || pDraw->iX >= display_width || iWidth < 1)
  {
    return;
  }
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++)
    {
      if (s[x] == pDraw->ucTransparent)
      {
        s[x] = pDraw->ucBackground;
      }
    }
    pDraw->ucHasTransparency = 0;
  }

  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < iWidth)
    {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent) // done, stop
        {
          s--; // back up to treat it like transparent
        }
        else // opaque
        {
          *d++ = usPalette[c];
          iCount++;
        }
      } // while looking for opaque pixels
      if (iCount) // any opaque pixels?
      {
        gfx->draw16bitBeRGBBitmap(pDraw->iX + x, y, usTemp, iCount, 1);
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent)
        {
          iCount++;
        }
        else
        {
          s--;
        }
      }
      if (iCount)
      {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  }
  else
  {
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x = 0; x < iWidth; x++)
    {
      usTemp[x] = usPalette[*s++];
    }
    gfx->draw16bitBeRGBBitmap(pDraw->iX, y, usTemp, iWidth, 1);
  }
}

// Get human-readable error related to GIF
void printGifErrorMessage(int errorCode)
{
  switch (errorCode)
  {
  case GIF_DECODE_ERROR:
    Serial.println("GIF Decoding Error");
    break;
  case GIF_TOO_WIDE:
    Serial.println("GIF Too Wide");
    break;
  case GIF_INVALID_PARAMETER:
    Serial.println("Invalid Parameter for gif open");
    break;
  case GIF_UNSUPPORTED_FEATURE:
    Serial.println("Unsupported feature in GIF");
    break;
  case GIF_FILE_NOT_OPEN:
    Serial.println("GIF File not open");
    break;
  case GIF_EARLY_EOF:
    Serial.println("GIF early end of file");
    break;
  case GIF_EMPTY_FRAME:
    Serial.println("GIF with empty frame");
    break;
  case GIF_BAD_FILE:
    Serial.println("GIF bad file");
    break;
  case GIF_ERROR_MEMORY:
    Serial.println("GIF memory Error");
    break;
  default:
    Serial.println("Unknown Error");
    break;
  }
}

