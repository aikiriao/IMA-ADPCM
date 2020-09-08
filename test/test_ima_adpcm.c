#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <assert.h>
#include <math.h>
#include "test.h"

/* テスト対象のモジュール */
#include "../ima_adpcm.c"

/* 追加でwavを使用 */
#include "../wav.c"

/* テストのセットアップ関数 */
void testIMAADPCM_Setup(void);

static int testByteArray_Initialize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

static int testByteArray_Finalize(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);
  return 0;
}

/* ヘッダエンコードデコードテスト */
static void testIMAADPCM_HeaderEncodeDecodeTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 有効なヘッダをセット */
#define IMAADPCM_SetValidHeader(p_header) {                           \
  struct IMAADPCMWAVHeaderInfo *header__p = p_header;                 \
  header__p->num_channels           = 1;                              \
  header__p->sampling_rate          = 44100;                          \
  header__p->bytes_per_sec          = 89422;                          \
  header__p->block_size             = 256;                            \
  header__p->bits_per_sample        = 4;                              \
  header__p->num_samples_per_block  = 505;                            \
  header__p->num_samples            = 1024;                           \
  header__p->header_size            = IMAADPCMWAVENCODER_HEADER_SIZE; \
}

  /* 成功例 */
  {
    uint8_t data[IMAADPCMWAVENCODER_HEADER_SIZE] = { 0, };
    struct IMAADPCMWAVHeaderInfo header = { 0, }, tmp_header = { 0, };

    IMAADPCM_SetValidHeader(&header);

    /* エンコード->デコード */
    Test_AssertEqual(IMAADPCMWAVEncoder_EncodeHeader(&header, data, sizeof(data)), IMAADPCM_APIRESULT_OK);
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, sizeof(data), &tmp_header), IMAADPCM_APIRESULT_OK);

    /* デコードしたヘッダの一致確認 */
    Test_AssertEqual(header.num_channels,           tmp_header.num_channels);
    Test_AssertEqual(header.sampling_rate,          tmp_header.sampling_rate);
    Test_AssertEqual(header.bytes_per_sec,          tmp_header.bytes_per_sec);
    Test_AssertEqual(header.block_size,             tmp_header.block_size);
    Test_AssertEqual(header.bits_per_sample,        tmp_header.bits_per_sample);
    Test_AssertEqual(header.num_samples_per_block,  tmp_header.num_samples_per_block);
    Test_AssertEqual(header.num_samples,            tmp_header.num_samples);
    Test_AssertEqual(header.header_size,            tmp_header.header_size);
  }

  /* ヘッダエンコード失敗ケース */
  {
    struct IMAADPCMWAVHeaderInfo header;
    uint8_t data[IMAADPCMWAVENCODER_HEADER_SIZE] = { 0, };

    /* 引数が不正 */
    IMAADPCM_SetValidHeader(&header);
    Test_AssertEqual(IMAADPCMWAVEncoder_EncodeHeader(NULL, data, sizeof(data)), IMAADPCM_APIRESULT_INVALID_ARGUMENT);
    Test_AssertEqual(IMAADPCMWAVEncoder_EncodeHeader(&header, NULL, sizeof(data)), IMAADPCM_APIRESULT_INVALID_ARGUMENT);

    /* データサイズ不足 */
    IMAADPCM_SetValidHeader(&header);
    Test_AssertEqual(IMAADPCMWAVEncoder_EncodeHeader(&header, data, sizeof(data) - 1), IMAADPCM_APIRESULT_INSUFFICIENT_DATA);
    Test_AssertEqual(IMAADPCMWAVEncoder_EncodeHeader(&header, data, IMAADPCMWAVENCODER_HEADER_SIZE - 1), IMAADPCM_APIRESULT_INSUFFICIENT_DATA);

    /* チャンネル数異常 */
    IMAADPCM_SetValidHeader(&header);
    header.num_channels = 3;
    Test_AssertEqual(IMAADPCMWAVEncoder_EncodeHeader(&header, data, sizeof(data)), IMAADPCM_APIRESULT_INVALID_FORMAT);

    /* ビット深度異常 */
    IMAADPCM_SetValidHeader(&header);
    header.bits_per_sample = 2;
    Test_AssertEqual(IMAADPCMWAVEncoder_EncodeHeader(&header, data, sizeof(data)), IMAADPCM_APIRESULT_INVALID_FORMAT);
  }

  /* ヘッダデコード失敗ケース */
  {
    struct IMAADPCMWAVHeaderInfo header, getheader;
    uint8_t valid_data[IMAADPCMWAVENCODER_HEADER_SIZE] = { 0, };
    uint8_t data[IMAADPCMWAVENCODER_HEADER_SIZE];

    /* 有効な内容を作っておく */
    IMAADPCM_SetValidHeader(&header);
    IMAADPCMWAVEncoder_EncodeHeader(&header, valid_data, sizeof(valid_data));

    /* チャンクIDが不正 */
    /* RIFFの破壊 */
    memcpy(data, valid_data, sizeof(valid_data));
    data[0] = 'a';
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, sizeof(data), &getheader), IMAADPCM_APIRESULT_INVALID_FORMAT);
    /* WAVEの破壊 */
    memcpy(data, valid_data, sizeof(valid_data));
    data[8] = 'a';
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, sizeof(data), &getheader), IMAADPCM_APIRESULT_INVALID_FORMAT);
    /* FMTの破壊 */
    memcpy(data, valid_data, sizeof(valid_data));
    data[12] = 'a';
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, sizeof(data), &getheader), IMAADPCM_APIRESULT_INVALID_FORMAT);
    /* factの破壊 */
    memcpy(data, valid_data, sizeof(valid_data));
    data[40] = 'a';
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, sizeof(data), &getheader), IMAADPCM_APIRESULT_INVALID_FORMAT);
    /* dataの破壊: dataチャンクが見つけられない */
    memcpy(data, valid_data, sizeof(valid_data));
    data[52] = 'a';
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, sizeof(data), &getheader), IMAADPCM_APIRESULT_INSUFFICIENT_DATA);

    /* クソデカfmtチャンクサイズ */
    memcpy(data, valid_data, sizeof(valid_data));
    ByteArray_WriteUint32LE(&data[16], sizeof(data));
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, sizeof(data), &getheader), IMAADPCM_APIRESULT_INSUFFICIENT_DATA);
    /* 異常なWAVEフォーマットタイプ */
    memcpy(data, valid_data, sizeof(valid_data));
    ByteArray_WriteUint32LE(&data[20], 0);
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, sizeof(data), &getheader), IMAADPCM_APIRESULT_INVALID_FORMAT);
    /* 異常なチャンネル数 */
    memcpy(data, valid_data, sizeof(valid_data));
    ByteArray_WriteUint32LE(&data[22], IMAADPCM_MAX_NUM_CHANNELS + 1);
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, sizeof(data), &getheader), IMAADPCM_APIRESULT_INVALID_FORMAT);
    /* 異常なfmtチャンクのエキストラサイズ */
    memcpy(data, valid_data, sizeof(valid_data));
    ByteArray_WriteUint32LE(&data[36], 0);
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, sizeof(data), &getheader), IMAADPCM_APIRESULT_INVALID_FORMAT);
    /* 異常なFACTチャンクサイズ */
    memcpy(data, valid_data, sizeof(valid_data));
    ByteArray_WriteUint32LE(&data[44], 0);
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, sizeof(data), &getheader), IMAADPCM_APIRESULT_INVALID_FORMAT);
  }
}

/* デコードハンドル作成破棄テスト */
static void testIMAADPCMWAVDecoder_CreateDestroyTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* ワークサイズ計算テスト */
  {
    int32_t work_size;

    work_size = IMAADPCMWAVDecoder_CalculateWorkSize();
    Test_AssertCondition(work_size >= (int32_t)sizeof(struct IMAADPCMWAVDecoder));
  }

  /* ワーク領域渡しによるハンドル作成（成功例） */
  {
    void *work;
    int32_t work_size;
    struct IMAADPCMWAVDecoder *decoder;

    work_size = IMAADPCMWAVDecoder_CalculateWorkSize();
    work = malloc(work_size);

    decoder = IMAADPCMWAVDecoder_Create(work, work_size);
    Test_AssertCondition(decoder != NULL);
    Test_AssertCondition(decoder->work == NULL);

    IMAADPCMWAVDecoder_Destroy(decoder);
    free(work);
  }

  /* 自前確保によるハンドル作成（成功例） */
  {
    struct IMAADPCMWAVDecoder *decoder;

    decoder = IMAADPCMWAVDecoder_Create(NULL, 0);
    Test_AssertCondition(decoder != NULL);
    Test_AssertCondition(decoder->work != NULL);

    IMAADPCMWAVDecoder_Destroy(decoder);
  }

  /* ワーク領域渡しによるハンドル作成（失敗ケース） */
  {
    void *work;
    int32_t work_size;
    struct IMAADPCMWAVDecoder *decoder;

    work_size = IMAADPCMWAVDecoder_CalculateWorkSize();
    work = malloc(work_size);

    /* 引数が不正 */
    decoder = IMAADPCMWAVDecoder_Create(NULL, work_size);
    Test_AssertCondition(decoder == NULL);
    decoder = IMAADPCMWAVDecoder_Create(work, 0);
    Test_AssertCondition(decoder == NULL);

    /* ワークサイズ不足 */
    decoder = IMAADPCMWAVDecoder_Create(work, work_size - 1);
    Test_AssertCondition(decoder == NULL);

    free(work);
  }
}

/* デコード結果が一致するか確認するサブルーチン 一致していたら1, していなければ0を返す */
static uint8_t testIMAADPCMDecoder_CheckDecodeResult(const char *adpcm_filename, const char *decodedwav_filename)
{
  FILE        *fp;
  uint8_t     *data;
  struct stat fstat;
  uint32_t    data_size;
  uint32_t    ch, smpl, is_ok;
  int16_t     *output[IMAADPCM_MAX_NUM_CHANNELS];
  struct IMAADPCMWAVDecoder *decoder;
  struct IMAADPCMWAVHeaderInfo header;
  struct WAVFile *wavfile;

  /* データロード */
  fp = fopen(adpcm_filename, "rb");
  assert(fp != NULL);
  stat(adpcm_filename, &fstat);
  data_size = (uint32_t)fstat.st_size;
  data = (uint8_t *)malloc(data_size);
  fread(data, sizeof(uint8_t), data_size, fp);
  fclose(fp);

  wavfile = WAV_CreateFromFile(decodedwav_filename);
  assert(wavfile != NULL);

  /* ヘッダデコード */
  Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, data_size, &header), IMAADPCM_APIRESULT_OK);

  /* 出力領域確保 */
  for (ch = 0; ch < header.num_channels; ch++) {
    output[ch] = malloc(sizeof(int16_t) * header.num_samples);
  }
  decoder = IMAADPCMWAVDecoder_Create(NULL, 0);

  /* デコード実行 */
  Test_AssertEqual(
      IMAADPCMWAVDecoder_DecodeWhole(decoder,
        data, data_size, output, header.num_channels, header.num_samples), IMAADPCM_APIRESULT_OK);

  /* ffmpegでデコードしたものと一致するか？ */
  is_ok = 1;
  for (ch = 0; ch < header.num_channels; ch++) {
    for (smpl = 0; smpl < header.num_samples; smpl++) {
      if (WAVFile_PCM(wavfile, smpl, ch) != (output[ch][smpl] << 16)) {
        is_ok = 0;
        goto CHECK_END;
      }
    }
  }

CHECK_END:
  /* 確保した領域の開放 */
  WAV_Destroy(wavfile);
  IMAADPCMWAVDecoder_Destroy(decoder);
  for (ch = 0; ch < header.num_channels; ch++) {
    free(output[ch]);
  }
  free(data);
  
  return is_ok;
}

/* デコードテスト */
static void testIMAADPCMWAVDecoder_DecodeTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 実データのヘッダデコード */
  {
    const char  test_filename[] = "sin300Hz_mono_adpcm_ffmpeg.wav";
    FILE        *fp;
    uint8_t     *data;
    struct stat fstat;
    uint32_t    data_size;
    struct IMAADPCMWAVHeaderInfo header;

    /* データロード */
    fp = fopen(test_filename, "rb");
    assert(fp != NULL);
    stat(test_filename, &fstat);
    data_size = (uint32_t)fstat.st_size;
    data = (uint8_t *)malloc(data_size);
    fread(data, sizeof(uint8_t), data_size, fp);
    fclose(fp);

    /* ヘッダデコード */
    Test_AssertEqual(IMAADPCMWAVDecoder_DecodeHeader(data, data_size, &header), IMAADPCM_APIRESULT_OK);

    /* 想定した内容になっているか */
    Test_AssertEqual(header.num_channels,           1    );
    Test_AssertEqual(header.sampling_rate,          48000);
    Test_AssertEqual(header.bytes_per_sec,          16000);
    Test_AssertEqual(header.block_size,             1024 );
    Test_AssertEqual(header.num_samples_per_block,  2041 );
    Test_AssertEqual(header.num_samples,            24492);
    Test_AssertEqual(header.header_size,            94   );

    free(data);
  }

  /* 実データのデータデコード一致確認 */
  {
    Test_AssertEqual(
        testIMAADPCMDecoder_CheckDecodeResult(
          "sin300Hz_mono_adpcm_ffmpeg.wav", "sin300Hz_mono_adpcm_ffmpeg_decoded.wav"), 1);
    Test_AssertEqual(
        testIMAADPCMDecoder_CheckDecodeResult(
          "sin300Hz_adpcm_ffmpeg.wav", "sin300Hz_adpcm_ffmpeg_decoded.wav"), 1);
    Test_AssertEqual(
        testIMAADPCMDecoder_CheckDecodeResult(
          "unit_impulse_mono_adpcm_ffmpeg.wav", "unit_impulse_mono_adpcm_ffmpeg_decoded.wav"), 1);
    Test_AssertEqual(
        testIMAADPCMDecoder_CheckDecodeResult(
          "unit_impulse_adpcm_ffmpeg.wav", "unit_impulse_adpcm_ffmpeg_decoded.wav"), 1);
  }
}

/* エンコードハンドル作成破棄テスト */
static void testIMAADPCMWAVEncoder_CreateDestroyTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* ワークサイズ計算テスト */
  {
    int32_t work_size;

    work_size = IMAADPCMWAVEncoder_CalculateWorkSize();
    Test_AssertCondition(work_size >= (int32_t)sizeof(struct IMAADPCMWAVEncoder));
  }

  /* ワーク領域渡しによるハンドル作成（成功例） */
  {
    void *work;
    int32_t work_size;
    struct IMAADPCMWAVEncoder *encoder;

    work_size = IMAADPCMWAVEncoder_CalculateWorkSize();
    work = malloc(work_size);

    encoder = IMAADPCMWAVEncoder_Create(work, work_size);
    Test_AssertCondition(encoder != NULL);
    Test_AssertCondition(encoder->work == NULL);
    Test_AssertCondition(encoder->set_parameter == 0);

    IMAADPCMWAVEncoder_Destroy(encoder);
    free(work);
  }

  /* 自前確保によるハンドル作成（成功例） */
  {
    struct IMAADPCMWAVEncoder *encoder;

    encoder = IMAADPCMWAVEncoder_Create(NULL, 0);
    Test_AssertCondition(encoder != NULL);
    Test_AssertCondition(encoder->work != NULL);

    IMAADPCMWAVEncoder_Destroy(encoder);
  }

  /* ワーク領域渡しによるハンドル作成（失敗ケース） */
  {
    void *work;
    int32_t work_size;
    struct IMAADPCMWAVEncoder *encoder;

    work_size = IMAADPCMWAVEncoder_CalculateWorkSize();
    work = malloc(work_size);

    /* 引数が不正 */
    encoder = IMAADPCMWAVEncoder_Create(NULL, work_size);
    Test_AssertCondition(encoder == NULL);
    encoder = IMAADPCMWAVEncoder_Create(work, 0);
    Test_AssertCondition(encoder == NULL);

    /* ワークサイズ不足 */
    encoder = IMAADPCMWAVEncoder_Create(work, work_size - 1);
    Test_AssertCondition(encoder == NULL);

    free(work);
  }
}

/* エンコードパラメータ設定テスト */
static void testIMAADPCMWAVEncoder_SetEncodeParameterTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 有効なパラメータをセット */
#define IMAADPCM_SetValidParameter(p_param) {               \
    struct IMAADPCMWAVEncodeParameter *p__param = p_param;  \
    p__param->num_channels    = 1;                          \
    p__param->sampling_rate   = 8000;                       \
    p__param->bits_per_sample = 4;                          \
    p__param->block_size      = 256;                        \
}

  /* 成功例 */
  {
    struct IMAADPCMWAVEncoder *encoder;
    struct IMAADPCMWAVEncodeParameter param;

    encoder = IMAADPCMWAVEncoder_Create(NULL, 0);
    
    IMAADPCM_SetValidParameter(&param);
    Test_AssertEqual(IMAADPCMWAVEncoder_SetEncodeParameter(encoder, &param), IMAADPCM_APIRESULT_OK);
    Test_AssertEqual(encoder->set_parameter, 1);
    Test_AssertEqual(memcmp(&(encoder->encode_paramemter), &param, sizeof(struct IMAADPCMWAVEncodeParameter)), 0);

    IMAADPCMWAVEncoder_Destroy(encoder);
  }

  /* 失敗ケース */
  {
    struct IMAADPCMWAVEncoder *encoder;
    struct IMAADPCMWAVEncodeParameter param;

    encoder = IMAADPCMWAVEncoder_Create(NULL, 0);

    /* 引数が不正 */
    IMAADPCM_SetValidParameter(&param);
    Test_AssertEqual(IMAADPCMWAVEncoder_SetEncodeParameter(NULL,  &param), IMAADPCM_APIRESULT_INVALID_ARGUMENT);
    Test_AssertEqual(IMAADPCMWAVEncoder_SetEncodeParameter(encoder, NULL), IMAADPCM_APIRESULT_INVALID_ARGUMENT);

    /* サンプルあたりビット数が異常 */
    IMAADPCM_SetValidParameter(&param);
    param.bits_per_sample = 0;
    Test_AssertEqual(IMAADPCMWAVEncoder_SetEncodeParameter(encoder, &param), IMAADPCM_APIRESULT_INVALID_FORMAT);

    /* ブロックサイズが小さすぎる */
    IMAADPCM_SetValidParameter(&param);
    param.block_size = 0;
    Test_AssertEqual(IMAADPCMWAVEncoder_SetEncodeParameter(encoder, &param), IMAADPCM_APIRESULT_INVALID_FORMAT);
    IMAADPCM_SetValidParameter(&param);
    param.block_size = param.num_channels * 4;
    Test_AssertEqual(IMAADPCMWAVEncoder_SetEncodeParameter(encoder, &param), IMAADPCM_APIRESULT_INVALID_FORMAT);

    IMAADPCMWAVEncoder_Destroy(encoder);
  }
}

/* エンコードテスト */
static void testIMAADPCMWAVDecoder_EncodeTest(void *obj)
{
  TEST_UNUSED_PARAMETER(obj);

  /* 簡単なデータをエンコード→デコードしてみる */
  {
#define NUM_CHANNELS  1
#define NUM_SAMPLES   1024
    int16_t *input[IMAADPCM_MAX_NUM_CHANNELS];
    int16_t *decoded[IMAADPCM_MAX_NUM_CHANNELS];
    uint32_t ch, smpl, buffer_size, output_size, is_ok;
    uint8_t *buffer;
    struct IMAADPCMWAVEncodeParameter enc_param;
    struct IMAADPCMWAVEncoder *encoder;
    struct IMAADPCMWAVDecoder *decoder;

    /* 出力データの領域割当て */
    for (ch = 0; ch < NUM_CHANNELS; ch++) {
      input[ch] = malloc(sizeof(int16_t) * NUM_SAMPLES);
      decoded[ch] = malloc(sizeof(int16_t) * NUM_SAMPLES);
    }
    /* 入力wavと同じサイズの出力領域を確保（増えることはないと期待） */
    buffer_size = NUM_CHANNELS * NUM_SAMPLES * sizeof(int16_t);
    buffer = malloc(buffer_size);

    /* データ作成: 単位インパルス */
    for (ch = 0; ch < NUM_CHANNELS; ch++) {
      input[ch][0] = INT16_MAX;
      for (smpl = 1; smpl < NUM_SAMPLES; smpl++) {
        input[ch][smpl] = 0;
      }
    }

    /* ハンドル作成 */
    encoder = IMAADPCMWAVEncoder_Create(NULL, 0);
    decoder = IMAADPCMWAVDecoder_Create(NULL, 0);

    /* エンコードパラメータをセット */
    enc_param.num_channels    = NUM_CHANNELS;
    enc_param.sampling_rate   = 8000;
    enc_param.bits_per_sample = 4;
    enc_param.block_size      = 256;
    Test_AssertEqual(IMAADPCMWAVEncoder_SetEncodeParameter(encoder, &enc_param), IMAADPCM_APIRESULT_OK);

    /* エンコード */
    Test_AssertEqual(
        IMAADPCMWAVEncoder_EncodeWhole(
          encoder, (const int16_t *const *)input, NUM_SAMPLES,
          buffer, buffer_size, &output_size), IMAADPCM_APIRESULT_OK);
    Test_AssertCondition(output_size < (buffer_size / 2));

    /* デコード */
    Test_AssertEqual(
        IMAADPCMWAVDecoder_DecodeWhole(
          decoder, buffer, output_size,
          decoded, NUM_CHANNELS, NUM_SAMPLES), IMAADPCM_APIRESULT_OK);

    /* ロスがあるのでマージンを設けて比較 */
    is_ok = 1;
    for (ch = 0; ch < NUM_CHANNELS; ch++) {
      for (smpl = 0; smpl < NUM_SAMPLES; smpl++) {
        double pcm1, pcm2;
        pcm1 = (double)input[ch][smpl] / INT16_MAX;
        pcm2 = (double)decoded[ch][smpl] / INT16_MAX;
        if (fabs(pcm1 - pcm2) > 1.0e-2) {
          printf("%d %d %d vs %d \n", ch, smpl, input[ch][smpl], decoded[ch][smpl]);
          is_ok = 0;
          goto CHECK_END;
        }
      }
    }

CHECK_END:
    Test_AssertEqual(is_ok, 1);
    /* 領域開放 */
    IMAADPCMWAVEncoder_Destroy(encoder);
    IMAADPCMWAVDecoder_Destroy(decoder);
    free(buffer);
    for (ch = 0; ch < NUM_CHANNELS; ch++) {
      free(input[ch]);
    }
  }

#if 0
  {
    const char test_filename[] = "sin300Hz_mono.wav";
    struct WAVFile *wavfile;
    struct stat fstat;
    int16_t *input[IMAADPCM_MAX_NUM_CHANNELS];
    uint32_t ch, smpl;
    uint8_t *output;

    /* 入力wav取得 */
    wavfile = WAV_CreateFromFile(test_filename);
    /* 出力データの領域割当て */
    for (ch = 0; ch < wavfile->format.num_channels; ch++) {
      input[ch] = malloc(sizeof(int16_t) * wavfile->format.num_samples);
    }
    /* 入力wavと同じサイズの出力領域を確保（増えることはないと期待） */
    stat(test_filename, &fstat);
    output = malloc(fstat.st_size);

    /* 16bit幅でデータ取得 */
    for (ch = 0; ch < wavfile->format.num_channels; ch++) {
      for (smpl = 0; smpl < wavfile->format.num_samples; smpl++) {
        input[ch][smpl] = WAVFile_PCM(wavfile, smpl, ch) >> 16;
      }
    }

    /* 領域開放 */
    free(output);
    for (ch = 0; ch < wavfile->format.num_channels; ch++) {
      free(input[ch]);
    }
    WAV_Destroy(wavfile);
  }
#endif
}

void testIMAADPCM_Setup(void)
{
  struct TestSuite *suite
    = Test_AddTestSuite("IMA-ADPCM Test Suite",
        NULL, testByteArray_Initialize, testByteArray_Finalize);

  Test_AddTest(suite, testIMAADPCM_HeaderEncodeDecodeTest);
  Test_AddTest(suite, testIMAADPCMWAVDecoder_CreateDestroyTest);
  Test_AddTest(suite, testIMAADPCMWAVDecoder_DecodeTest);
  Test_AddTest(suite, testIMAADPCMWAVEncoder_CreateDestroyTest);
  Test_AddTest(suite, testIMAADPCMWAVEncoder_SetEncodeParameterTest);
  Test_AddTest(suite, testIMAADPCMWAVDecoder_EncodeTest);
}
