/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details. */

#include "ima_adpcm.h"
#include "wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* バージョン文字列 */
#define IMAADPCMCUI_VERSION_STRING  "1.1.0"

/* ブロックサイズ 今の所1024で固定 */
#define IMAADPCMCUI_BLOCK_SIZE      256

/* デコード処理 */
static int do_decode(const char *adpcm_filename, const char *decoded_filename)
{
  FILE                          *fp;
  struct stat                   fstat;
  uint8_t                       *buffer;
  uint32_t                      buffer_size;
  struct IMAADPCMWAVDecoder     *decoder;
  struct IMAADPCMWAVHeaderInfo  header;
  struct WAVFile                *wav;
  struct WAVFileFormat          wavformat;
  int16_t                       *output[IMAADPCM_MAX_NUM_CHANNELS];
  uint32_t                      ch, smpl;
  IMAADPCMApiResult             ret;

  /* ファイルオープン */
  fp = fopen(adpcm_filename, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s. \n", adpcm_filename);
    return 1;
  }

  /* 入力ファイルのサイズ取得 / バッファ領域割り当て */
  stat(adpcm_filename, &fstat);
  buffer_size = (uint32_t)fstat.st_size;
  buffer = (uint8_t *)malloc(buffer_size);
  /* バッファ領域にデータをロード */
  fread(buffer, sizeof(uint8_t), buffer_size, fp);
  fclose(fp);

  /* デコーダ作成 */
  decoder = IMAADPCMWAVDecoder_Create(NULL, 0);

  /* ヘッダ読み取り */
  if ((ret = IMAADPCMWAVDecoder_DecodeHeader(buffer, buffer_size, &header))
      != IMAADPCM_APIRESULT_OK) {
    fprintf(stderr, "Failed to read header. API result: %d \n", ret);
    return 1;
  }

  /* 出力バッファ領域確保 */
  for (ch = 0; ch < header.num_channels; ch++) {
    output[ch] = malloc(sizeof(int16_t) * header.num_samples);
  }

  /* 全データをデコード */
  if ((ret = IMAADPCMWAVDecoder_DecodeWhole(decoder, 
        buffer, buffer_size, output, 
        header.num_channels, header.num_samples)) != IMAADPCM_APIRESULT_OK) {
    fprintf(stderr, "Failed to decode. API result: %d \n", ret);
    return 1;
  }

  /* 出力ファイルを作成 */
  wavformat.data_format = WAV_DATA_FORMAT_PCM;
  wavformat.num_channels = header.num_channels;
  wavformat.sampling_rate = header.sampling_rate;
  wavformat.bits_per_sample = 16;
  wavformat.num_samples = header.num_samples;
  wav = WAV_Create(&wavformat);

  /* PCM書き出し */
  for (ch = 0; ch < header.num_channels; ch++) {
    for (smpl = 0; smpl < header.num_samples; smpl++) {
      WAVFile_PCM(wav, smpl, ch) = (output[ch][smpl] << 16);
    }
  }

  WAV_WriteToFile(decoded_filename, wav);

  IMAADPCMWAVDecoder_Destroy(decoder);
  for (ch = 0; ch < header.num_channels; ch++) {
    free(output[ch]);
  }
  WAV_Destroy(wav);
  free(buffer);

  return 0;
}

/* エンコード処理 */
static int do_encode(const char *wav_file, const char *encoded_filename)
{
  FILE                              *fp;
  struct WAVFile                    *wavfile;
  struct stat                       fstat;
  int16_t                           *input[IMAADPCM_MAX_NUM_CHANNELS];
  uint32_t                          ch, smpl, buffer_size, output_size;
  uint32_t                          num_channels, num_samples;
  uint8_t                           *buffer;
  struct IMAADPCMWAVEncodeParameter enc_param;
  struct IMAADPCMWAVEncoder         *encoder;
  IMAADPCMApiResult                 api_result;

  /* 入力wav取得 */
  wavfile = WAV_CreateFromFile(wav_file);
  if (wavfile == NULL) {
    fprintf(stderr, "Failed to open %s. \n", wav_file);
    return 1;
  }

  num_channels = wavfile->format.num_channels;
  num_samples = wavfile->format.num_samples;

  /* 出力データの領域割当て */
  for (ch = 0; ch < num_channels; ch++) {
    input[ch] = malloc(sizeof(int16_t) * num_samples);
  }
  /* 入力wavと同じサイズの出力領域を確保（増えることはないと期待） */
  stat(wav_file, &fstat);
  buffer_size = (uint32_t)fstat.st_size;
  buffer = malloc(buffer_size);

  /* 16bit幅でデータ取得 */
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      input[ch][smpl] = (WAVFile_PCM(wavfile, smpl, ch) >> 16);
    }
  }

  /* ハンドル作成 */
  encoder = IMAADPCMWAVEncoder_Create(NULL, 0);

  /* エンコードパラメータをセット */
  enc_param.num_channels    = (uint16_t)num_channels;
  enc_param.sampling_rate   = wavfile->format.sampling_rate;
  enc_param.bits_per_sample = 4;
  enc_param.block_size      = IMAADPCMCUI_BLOCK_SIZE;
  if ((api_result = IMAADPCMWAVEncoder_SetEncodeParameter(encoder, &enc_param))
      != IMAADPCM_APIRESULT_OK) {
    fprintf(stderr, "Failed to set encode parameter. API result:%d \n", api_result);
    return 1;
  }

  /* エンコード */
  if ((api_result = IMAADPCMWAVEncoder_EncodeWhole(
        encoder, (const int16_t *const *)input, num_samples,
        buffer, buffer_size, &output_size)) != IMAADPCM_APIRESULT_OK) {
    fprintf(stderr, "Failed to encode. API result:%d \n", api_result);
    return 1;
  }

  /* ファイル書き出し */
  fp = fopen(encoded_filename, "wb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open output file %s \n", encoded_filename);
    return 1;
  }
  if (fwrite(buffer, sizeof(uint8_t), output_size, fp) < output_size) {
    fprintf(stderr, "Warning: failed to write encoded data \n");
    return 1;
  }
  fclose(fp);

  /* 領域開放 */
  IMAADPCMWAVEncoder_Destroy(encoder);
  free(buffer);
  for (ch = 0; ch < num_channels; ch++) {
    free(input[ch]);
  }
  WAV_Destroy(wavfile);

  return 0;
}

/* 残差出力処理 */
static int do_residual_output(const char *wav_file, const char *residual_filename)
{
  struct WAVFile                    *wavfile;
  struct stat                       fstat;
  int16_t                           *pcmdata[IMAADPCM_MAX_NUM_CHANNELS];
  uint32_t                          ch, smpl, buffer_size, output_size;
  uint32_t                          num_channels, num_samples;
  uint8_t                           *buffer;
  struct IMAADPCMWAVEncodeParameter enc_param;
  struct IMAADPCMWAVEncoder         *encoder;
  struct IMAADPCMWAVDecoder         *decoder;
  IMAADPCMApiResult                 api_result;

  /* 入力wav取得 */
  wavfile = WAV_CreateFromFile(wav_file);
  if (wavfile == NULL) {
    fprintf(stderr, "Failed to open %s. \n", wav_file);
    return 1;
  }

  num_channels = wavfile->format.num_channels;
  num_samples = wavfile->format.num_samples;

  /* 出力データの領域割当て */
  for (ch = 0; ch < num_channels; ch++) {
    pcmdata[ch] = malloc(sizeof(int16_t) * num_samples);
  }
  /* 入力wavと同じサイズの出力領域を確保（増えることはないと期待） */
  stat(wav_file, &fstat);
  buffer_size = (uint32_t)fstat.st_size;
  buffer = malloc(buffer_size);

  /* 16bit幅でデータ取得 */
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      pcmdata[ch][smpl] = (WAVFile_PCM(wavfile, smpl, ch) >> 16);
    }
  }

  /* ハンドル作成 */
  encoder = IMAADPCMWAVEncoder_Create(NULL, 0);
  decoder = IMAADPCMWAVDecoder_Create(NULL, 0);

  /* エンコードパラメータをセット */
  enc_param.num_channels    = (uint16_t)num_channels;
  enc_param.sampling_rate   = wavfile->format.sampling_rate;
  enc_param.bits_per_sample = 4;
  enc_param.block_size      = IMAADPCMCUI_BLOCK_SIZE;
  if ((api_result = IMAADPCMWAVEncoder_SetEncodeParameter(encoder, &enc_param))
      != IMAADPCM_APIRESULT_OK) {
    fprintf(stderr, "Failed to set encode parameter. API result:%d \n", api_result);
    return 1;
  }

  /* エンコード */
  if ((api_result = IMAADPCMWAVEncoder_EncodeWhole(
        encoder, (const int16_t *const *)pcmdata, num_samples,
        buffer, buffer_size, &output_size)) != IMAADPCM_APIRESULT_OK) {
    fprintf(stderr, "Failed to encode. API result:%d \n", api_result);
    return 1;
  }

  /* そのままデコード */
  if ((api_result = IMAADPCMWAVDecoder_DecodeWhole(decoder, 
        buffer, output_size, pcmdata, num_channels, num_samples)) != IMAADPCM_APIRESULT_OK) {
    fprintf(stderr, "Failed to decode. API result: %d \n", api_result);
    return 1;
  }

  /* 残差（量子化誤差）計算 */
  for (ch = 0; ch < num_channels; ch++) {
    for (smpl = 0; smpl < num_samples; smpl++) {
      int32_t decoded = pcmdata[ch][smpl] << 16;
      WAVFile_PCM(wavfile, smpl, ch) -= decoded;
    }
  }

  /* 残差をファイルに書き出し */
  WAV_WriteToFile(residual_filename, wavfile);

  /* 領域開放 */
  IMAADPCMWAVEncoder_Destroy(encoder);
  IMAADPCMWAVDecoder_Destroy(decoder);
  free(buffer);
  for (ch = 0; ch < num_channels; ch++) {
    free(pcmdata[ch]);
  }
  WAV_Destroy(wavfile);

  return 0;
}

/* 使用法の印字 */
static void print_usage(const char* program_name)
{
  printf(
      "IMA-ADPCM encoder/decoder Version." IMAADPCMCUI_VERSION_STRING "\n" \
      "Usage: %s -[ed] INPUT.wav OUTPUT.wav \n" \
      "-e: encode mode (PCM wav -> IMA-ADPCM wav)\n" \
      "-d: decode mode (IMA-ADPCM wav -> PCM wav)\n" \
      "-r: output residual (PCM wav -> Residual PCM wav)\n", 
      program_name);
}

/* メインエントリ */
int main(int argc, char **argv)
{
  int ret;
  const char *option;
  const char *in_filename, *out_filename;

  /* 引数の数が想定外 */
  if (argc != 4) {
    print_usage(argv[0]);
    return 1;
  }

  /* オプション文字列の取得 */
  option        = argv[1];
  in_filename   = argv[2];
  out_filename  = argv[3];

  /* 引数が無効 */
  if ((option == NULL) || (in_filename == NULL) || (out_filename == NULL)) {
    print_usage(argv[0]);
    return 1;
  }
  
  /* エンコード/デコード呼び分け */
  if (strncmp(option, "-e", 2) == 0) {
    ret = do_encode(in_filename, out_filename);
  } else if (strncmp(option, "-d", 2) == 0) {
    ret = do_decode(in_filename, out_filename);
  } else if (strncmp(option, "-r", 2) == 0) {
    ret = do_residual_output(in_filename, out_filename);
  } else {
    print_usage(argv[0]);
    return 1;
  }

  return ret;
}
