#include "ima_adpcm.h"
#include "wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
  FILE        *fp;
  struct stat fstat;
  uint8_t     *buffer;
  uint32_t    buffer_size;
  const char  *filename;

  if ((argc != 2) || (argv[1] == NULL)) {
    return 1;
  }
  
  filename = argv[1];

  /* ファイルオープン */
  fp = fopen(filename, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open file. \n");
  }

  /* 入力ファイルのサイズ取得 / バッファ領域割り当て */
  stat(filename, &fstat);
  buffer_size = (uint32_t)fstat.st_size;
  buffer = (uint8_t *)malloc(buffer_size);
  /* バッファ領域にデータをロード */
  fread(buffer, sizeof(uint8_t), buffer_size, fp);
  fclose(fp);

  {
    struct IMAADPCMWAVDecoder *decoder;
    struct IMAADPCMWAVHeaderInfo header_info;
    struct WAVFile *wav;
    struct WAVFileFormat wavformat;
    int16_t *output[2];
    uint32_t ch, smpl, output_num_samples;

    /* ヘッダ読み取り */
    if (IMAADPCMWAVDecoder_DecodeHeader(buffer, buffer_size, &header_info) != IMAADPCMWAV_APIRESULT_OK) {
      fprintf(stderr, "Failed to read header. \n");
      return 1;
    }

    decoder = IMAADPCMWAVDecoder_Create(NULL, 0);
    
    for (ch = 0; ch < header_info.num_channels; ch++) {
      output[ch] = malloc(sizeof(int16_t) * header_info.num_samples);
    }

    if (IMAADPCMWAVDecoder_DecodeWhole(decoder, 
          buffer, buffer_size, output, 
          header_info.num_samples, &output_num_samples) != IMAADPCMWAV_APIRESULT_OK) {
      return 1;
    }

    wavformat.data_format = WAV_DATA_FORMAT_PCM;
    wavformat.num_channels = header_info.num_channels;
    wavformat.sampling_rate = header_info.sampling_rate;
    wavformat.bits_per_sample = 16;
    wavformat.num_samples = header_info.num_samples;
    wav = WAV_Create(&wavformat);

    for (ch = 0; ch < header_info.num_channels; ch++) {
      for (smpl = 0; smpl < header_info.num_samples; smpl++) {
        WAVFile_PCM(wav, smpl, ch) = (output[ch][smpl] << 16);
      }
    }

    WAV_WriteToFile("a.wav", wav);

    IMAADPCMWAVDecoder_Destroy(decoder);
    for (ch = 0; ch < header_info.num_channels; ch++) {
      free(output[ch]);
    }
    WAV_Destroy(wav);
  }

  free(buffer);

  return 0;
}
