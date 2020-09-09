#include "ima_adpcm.h"
#include "byte_array.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* アラインメント */
#define IMAADPCM_ALIGNMENT              16

/* エンコード時に書き出すヘッダサイズ（データブロック直前までのファイルサイズ） */
#define IMAADPCMWAVENCODER_HEADER_SIZE  60

/* nの倍数への切り上げ */
#define IMAADPCM_ROUND_UP(val, n) ((((val) + ((n) - 1)) / (n)) * (n))

/* 最大値を選択 */
#define IMAADPCM_MAX_VAL(a, b) (((a) > (b)) ? (a) : (b))

/* 最小値を選択 */
#define IMAADPCM_MIN_VAL(a, b) (((a) < (b)) ? (a) : (b))

/* min以上max未満に制限 */
#define IMAADPCM_INNER_VAL(val, min, max) \
  IMAADPCM_MAX_VAL(min, IMAADPCM_MIN_VAL(max, val))

/* 指定サンプル数が占めるデータサイズ[byte]を計算 */
#define IMAADPCM_CALCULATE_DATASIZE_BYTE(num_samples, bits_per_sample) \
  (IMAADPCM_ROUND_UP((num_samples) * (bits_per_sample), 8) / 8)

/* FourCCの一致確認 */
#define IMAADPCM_CHECK_FOURCC(u32lebuf, c1, c2, c3, c4) \
  ((u32lebuf) == ((c1 << 0) | (c2 << 8) | (c3 << 16) | (c4 << 24)))

/* 内部エラー型 */
typedef enum IMAADPCMErrorTag {
  IMAADPCM_ERROR_OK = 0,              /* OK */
  IMAADPCM_ERROR_NG,                  /* 分類不能な失敗 */
  IMAADPCM_ERROR_INVALID_ARGUMENT,    /* 不正な引数 */
  IMAADPCM_ERROR_INVALID_FORMAT,      /* 不正なフォーマット       */
  IMAADPCM_ERROR_INSUFFICIENT_BUFFER, /* バッファサイズが足りない */
  IMAADPCM_ERROR_INSUFFICIENT_DATA    /* データサイズが足りない   */
} IMAADPCMError;

/* コア処理デコーダ */
struct IMAADPCMCoreDecoder {
  int16_t sample_val;             /* サンプル値                                   */
  int8_t  stepsize_index;         /* ステップサイズテーブルの参照インデックス     */
};

/* デコーダ */
struct IMAADPCMWAVDecoder {
  struct IMAADPCMWAVHeaderInfo  header;
  struct IMAADPCMCoreDecoder    core_decoder[IMAADPCM_MAX_NUM_CHANNELS];
  void                          *work;
};

/* コア処理エンコーダ */
struct IMAADPCMCoreEncoder {
  int16_t prev_sample;            /* サンプル値                                   */
  int8_t  stepsize_index;         /* ステップサイズテーブルの参照インデックス     */
};

/* エンコーダ */
struct IMAADPCMWAVEncoder {
  struct IMAADPCMWAVEncodeParameter encode_paramemter;
  uint8_t                           set_parameter;
  struct IMAADPCMCoreEncoder        core_encoder[IMAADPCM_MAX_NUM_CHANNELS];
  void                              *work;
};

/* 1サンプルデコード */
static int16_t IMAADPCMCoreDecoder_DecodeSample(
    struct IMAADPCMCoreDecoder *decoder, uint8_t nibble);

/* モノラルブロックのデコード */
static IMAADPCMError IMAADPCMWAVDecoder_DecodeBlockMono(
    struct IMAADPCMCoreDecoder *core_decoder,
    const uint8_t *read_pos, uint32_t data_size, 
    int16_t **buffer, uint32_t buffer_num_samples,
    uint32_t num_decode_samples, uint32_t *read_size);

/* ステレオブロックのデコード */
static IMAADPCMError IMAADPCMWAVDecoder_DecodeBlockStereo(
    struct IMAADPCMCoreDecoder *core_decoder,
    const uint8_t *read_pos, uint32_t data_size, 
    int16_t **buffer, uint32_t buffer_num_samples, 
    uint32_t num_decode_samples, uint32_t *read_size);

/* 単一データブロックエンコード */
/* デコードとは違いstaticに縛る: エンコーダが内部的に状態を持ち、連続でEncodeBlockを呼ぶ必要があるから */
static IMAADPCMApiResult IMAADPCMWAVEncoder_EncodeBlock(
    struct IMAADPCMWAVEncoder *encoder,
    const int16_t *const *input, uint32_t num_samples, 
    uint8_t *data, uint32_t data_size, uint32_t *output_size);

/* モノラルブロックのエンコード */
static IMAADPCMError IMAADPCMWAVEncoder_EncodeBlockMono(
    struct IMAADPCMCoreEncoder *core_encoder,
    const int16_t *const *input, uint32_t num_samples,
    uint8_t *data, uint32_t data_size, uint32_t *output_size);

/* ステレオブロックのエンコード */
static IMAADPCMError IMAADPCMWAVEncoder_EncodeBlockStereo(
    struct IMAADPCMCoreEncoder *core_encoder,
    const int16_t *const *input, uint32_t num_samples,
    uint8_t *data, uint32_t data_size, uint32_t *output_size);

/* インデックス変動テーブル */
static const int8_t IMAADPCM_index_table[16] = {
  -1, -1, -1, -1, 2, 4, 6, 8, 
  -1, -1, -1, -1, 2, 4, 6, 8 
};

/* ステップサイズ量子化テーブル */
static const uint16_t IMAADPCM_stepsize_table[89] = {
      7,     8,     9,    10,    11,    12,    13,    14, 
     16,    17,    19,    21,    23,    25,    28,    31, 
     34,    37,    41,    45,    50,    55,    60,    66,
     73,    80,    88,    97,   107,   118,   130,   143, 
    157,   173,   190,   209,   230,   253,   279,   307,
    337,   371,   408,   449,   494,   544,   598,   658,
    724,   796,   876,   963,  1060,  1166,  1282,  1411, 
   1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,
   3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,
   7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
  15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
  32767
};

/* ワークサイズ計算 */
int32_t IMAADPCMWAVDecoder_CalculateWorkSize(void)
{
  return IMAADPCM_ALIGNMENT + sizeof(struct IMAADPCMWAVDecoder);
}

/* デコードハンドル作成 */
struct IMAADPCMWAVDecoder *IMAADPCMWAVDecoder_Create(void *work, int32_t work_size)
{
  struct IMAADPCMWAVDecoder *decoder;
  uint8_t *work_ptr;
  uint32_t alloced_by_malloc = 0;

  /* 領域自前確保の場合 */
  if ((work == NULL) && (work_size == 0)) {
    work_size = IMAADPCMWAVDecoder_CalculateWorkSize();
    work = malloc((uint32_t)work_size);
    alloced_by_malloc = 1;
  }

  /* 引数チェック */
  if ((work == NULL) || (work_size < IMAADPCMWAVDecoder_CalculateWorkSize())) {
    return NULL;
  }

  work_ptr = (uint8_t *)work;

  /* アラインメントを揃えてから構造体を配置 */
  work_ptr = (uint8_t *)IMAADPCM_ROUND_UP((uintptr_t)work_ptr, IMAADPCM_ALIGNMENT);
  decoder = (struct IMAADPCMWAVDecoder *)work_ptr;

  /* ハンドルの中身を0初期化 */
  memset(decoder, 0, sizeof(struct IMAADPCMWAVDecoder));

  /* 自前確保の場合はメモリを記憶しておく */
  decoder->work = alloced_by_malloc ? work : NULL;

  return decoder;
}

/* デコードハンドル破棄 */
void IMAADPCMWAVDecoder_Destroy(struct IMAADPCMWAVDecoder *decoder)
{
  if (decoder != NULL) {
    /* 自分で領域確保していたら破棄 */
    if (decoder->work != NULL) {
      free(decoder->work);
    }
  }
}

/* ヘッダデコード */
IMAADPCMApiResult IMAADPCMWAVDecoder_DecodeHeader(
    const uint8_t *data, uint32_t data_size, struct IMAADPCMWAVHeaderInfo *header_info)
{
  const uint8_t *data_pos;
  uint32_t u32buf;
  uint16_t u16buf;
  struct IMAADPCMWAVHeaderInfo tmp_header_info;

  /* 引数チェック */
  if ((data == NULL) || (header_info == NULL)) {
    return IMAADPCM_APIRESULT_INVALID_ARGUMENT;
  }

  /* 読み出し用ポインタ設定 */
  data_pos = data;

  /* RIFFチャンクID */
  ByteArray_GetUint32LE(data_pos, &u32buf);
  if (!IMAADPCM_CHECK_FOURCC(u32buf, 'R', 'I', 'F', 'F')) {
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }
  /* RIFFチャンクサイズ（読み飛ばし） */
  ByteArray_GetUint32LE(data_pos, &u32buf);

  /* WAVEチャンクID */
  ByteArray_GetUint32LE(data_pos, &u32buf);
  if (!IMAADPCM_CHECK_FOURCC(u32buf, 'W', 'A', 'V', 'E')) {
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }

  /* FMTチャンクID */
  ByteArray_GetUint32LE(data_pos, &u32buf);
  if (!IMAADPCM_CHECK_FOURCC(u32buf, 'f', 'm', 't', ' ')) {
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }
  /* fmtチャンクサイズ */
  ByteArray_GetUint32LE(data_pos, &u32buf);
  if (data_size <= u32buf) {
    fprintf(stderr, "Data size too small. fmt chunk size:%d data size:%d \n", u32buf, data_size);
    return IMAADPCM_APIRESULT_INSUFFICIENT_DATA;
  }
  /* WAVEフォーマットタイプ: IMA-ADPCM以外は受け付けない */
  ByteArray_GetUint16LE(data_pos, &u16buf);
  if (u16buf != 17) {
    fprintf(stderr, "Unsupported format: %d \n", u16buf);
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }
  /* チャンネル数 */
  ByteArray_GetUint16LE(data_pos, &u16buf);
  if (u16buf > IMAADPCM_MAX_NUM_CHANNELS) {
    fprintf(stderr, "Unsupported channels: %d \n", u16buf);
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }
  tmp_header_info.num_channels = u16buf;
  /* サンプリングレート */
  ByteArray_GetUint32LE(data_pos, &u32buf);
  tmp_header_info.sampling_rate = u32buf;
  /* データ速度[byte/sec] */
  ByteArray_GetUint32LE(data_pos, &u32buf);
  tmp_header_info.bytes_per_sec = u32buf;
  /* ブロックサイズ */
  ByteArray_GetUint16LE(data_pos, &u16buf);
  tmp_header_info.block_size = u16buf;
  /* サンプルあたりビット数 */
  ByteArray_GetUint16LE(data_pos, &u16buf);
  tmp_header_info.bits_per_sample = u16buf;
  /* fmtチャンクのエキストラサイズ: 2以外は想定していない */
  ByteArray_GetUint16LE(data_pos, &u16buf);
  if (u16buf != 2) {
    fprintf(stderr, "Unsupported fmt chunk extra size: %d \n", u16buf);
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }
  /* ブロックあたりサンプル数 */
  ByteArray_GetUint16LE(data_pos, &u16buf);
  tmp_header_info.num_samples_per_block = u16buf;

  /* FACTチャンクID */
  ByteArray_GetUint32LE(data_pos, &u32buf);
  if (!IMAADPCM_CHECK_FOURCC(u32buf, 'f', 'a', 'c', 't')) {
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }
  /* FACTチャンクサイズ: 4以外は想定していない */
  ByteArray_GetUint32LE(data_pos, &u32buf);
  if (u32buf != 4) {
    fprintf(stderr, "Unsupported fact chunk size: %d \n", u16buf);
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }
  /* サンプル数 */
  ByteArray_GetUint32LE(data_pos, &u32buf);
  tmp_header_info.num_samples = u32buf;

  /* dataチャンクまで読み飛ばし */
  while (1) {
    uint32_t chunkid;
    /* サイズ超過 */
    if (data_size < (uint32_t)(data_pos - data)) {
      return IMAADPCM_APIRESULT_INSUFFICIENT_DATA;
    }
    /* チャンクID取得 */
    ByteArray_GetUint32LE(data_pos, &chunkid);
    if (IMAADPCM_CHECK_FOURCC(chunkid, 'd', 'a', 't', 'a')) {
      /* データチャンクを見つけたら終わり */
      break;
    } else {
      uint32_t size;
      /* 他のチャンクはサイズだけ取得してシークにより読み飛ばす */
      ByteArray_GetUint32LE(data_pos, &size);
      /* printf("chunk:%8X size:%d \n", chunkid, (int32_t)size); */
      data_pos += size;
    }
  }

  /* データチャンクサイズ（読み飛ばし） */
  ByteArray_GetUint32LE(data_pos, &u32buf);

  /* データ領域先頭までのオフセット */
  tmp_header_info.header_size = (uint32_t)(data_pos - data);

  /* 成功終了 */
  (*header_info) = tmp_header_info;
  return IMAADPCM_APIRESULT_OK;
}

/* 1サンプルデコード */
static int16_t IMAADPCMCoreDecoder_DecodeSample(
    struct IMAADPCMCoreDecoder *decoder, uint8_t nibble)
{
  int8_t  idx;
  int32_t predict, diff, delta, stepsize;

  assert(decoder != NULL);

  /* 頻繁に参照する変数をオート変数に受ける */
  predict = decoder->sample_val;
  idx = decoder->stepsize_index;

  /* ステップサイズの取得 */
  stepsize = IMAADPCM_stepsize_table[idx];

  /* インデックス更新 */
  idx += IMAADPCM_index_table[nibble];
  idx = IMAADPCM_INNER_VAL(idx, 0, 88);

  /* 差分算出 */
  /* diff = stepsize * (delta * 2 + 1) / 8 */
  /* memo:ffmpegを参考に、よくある分岐多用の実装はしない。
   * 分岐多用の実装は近似で結果がおかしいし、分岐ミスの方が負荷が大きいと判断 */
  delta = nibble & 7;
  diff = (stepsize * ((delta << 1) + 1)) >> 3;

  /* 差分を加える 符号ビットで加算/減算を切り替え */
  if (nibble & 8) {
    predict -= diff;
  } else {
    predict += diff;
  }

  /* 16bit幅にクリップ */
  predict = IMAADPCM_INNER_VAL(predict, -32768, 32767);

  /* 計算結果の反映 */
  decoder->sample_val = (int16_t)predict;
  decoder->stepsize_index = idx;

  return decoder->sample_val;
}

/* モノラルブロックのデコード */
static IMAADPCMError IMAADPCMWAVDecoder_DecodeBlockMono(
    struct IMAADPCMCoreDecoder *core_decoder,
    const uint8_t *read_pos, uint32_t data_size, 
    int16_t **buffer, uint32_t buffer_num_samples,
    uint32_t num_decode_samples, uint32_t *read_size)
{
  uint8_t u8buf;
  uint8_t nibble[2];
  uint32_t smpl;
  const uint8_t *read_head = read_pos;

  /* 引数チェック */
  if ((core_decoder == NULL) || (read_pos == NULL)
      || (buffer == NULL) || (buffer[0] == NULL) || (read_size == NULL)) {
    return IMAADPCM_ERROR_INVALID_ARGUMENT;
  }

  /* バッファサイズチェック */
  if (buffer_num_samples < num_decode_samples) {
    return IMAADPCM_ERROR_INSUFFICIENT_BUFFER;
  }

  /* 指定サンプル分デコードしきれるか確認 */
  /* -1はヘッダに入っているサンプル分 */
  if ((num_decode_samples - 1) > 2 * (data_size - 4)) {
    return IMAADPCM_ERROR_INSUFFICIENT_DATA;
  }

  /* ブロックヘッダデコード */
  ByteArray_GetUint16LE(read_pos, (uint16_t *)&(core_decoder->sample_val));
  ByteArray_GetUint8(read_pos, (uint8_t *)&(core_decoder->stepsize_index));
  ByteArray_GetUint8(read_pos, &u8buf); /* reserved */
  if (u8buf != 0) {
    return IMAADPCM_ERROR_INVALID_FORMAT;
  }

  /* 先頭サンプルはヘッダに入っている */
  buffer[0][0] = core_decoder->sample_val;

  /* ブロックデータデコード */
  for (smpl = 1; smpl < num_decode_samples; smpl += 2) {
    assert((uint32_t)(read_pos - read_head) < data_size);
    ByteArray_GetUint8(read_pos, &u8buf);
    nibble[0] = (u8buf >> 0) & 0xF;
    nibble[1] = (u8buf >> 4) & 0xF;
    buffer[0][smpl + 0] = IMAADPCMCoreDecoder_DecodeSample(core_decoder, nibble[0]);
    buffer[0][smpl + 1] = IMAADPCMCoreDecoder_DecodeSample(core_decoder, nibble[1]);
  }

  /* 読み出しサイズをセット */
  (*read_size) = (uint32_t)(read_pos - read_head);
  return IMAADPCM_ERROR_OK;
}

/* ステレオブロックのデコード */
static IMAADPCMError IMAADPCMWAVDecoder_DecodeBlockStereo(
    struct IMAADPCMCoreDecoder *core_decoder,
    const uint8_t *read_pos, uint32_t data_size, 
    int16_t **buffer, uint32_t buffer_num_samples, 
    uint32_t num_decode_samples, uint32_t *read_size)
{
  uint32_t u32buf;
  uint8_t nibble[8];
  uint32_t ch, smpl;
  const uint8_t *read_head = read_pos;

  /* 引数チェック */
  if ((core_decoder == NULL) || (read_pos == NULL)
      || (buffer == NULL) || (buffer[0] == NULL) || (buffer[1] == NULL)
      || (read_size == NULL)) {
    return IMAADPCM_ERROR_INVALID_ARGUMENT;
  }

  /* バッファサイズチェック */
  if (buffer_num_samples < num_decode_samples) {
    return IMAADPCM_ERROR_INSUFFICIENT_BUFFER;
  }

  /* 指定サンプル分デコードしきれるか確認 */
  if (num_decode_samples > 2 * (data_size - 8)) {
    return IMAADPCM_ERROR_INSUFFICIENT_BUFFER;
  }

  /* ブロックヘッダデコード */
  for (ch = 0; ch < 2; ch++) {
    uint8_t reserved;
    ByteArray_GetUint16LE(read_pos, (uint16_t *)&(core_decoder[ch].sample_val));
    ByteArray_GetUint8(read_pos, (uint8_t *)&(core_decoder[ch].stepsize_index));
    ByteArray_GetUint8(read_pos, &reserved);
    if (reserved != 0) {
      return IMAADPCM_ERROR_INVALID_FORMAT;
    }
  }

  /* 最初のサンプルの取得 */
  for (ch = 0; ch < 2; ch++) {
    buffer[ch][0] = core_decoder[ch].sample_val;
  }

  /* ブロックデータデコード */
  for (smpl = 1; smpl < num_decode_samples; smpl += 8) {
    for (ch = 0; ch < 2; ch++) {
      assert((uint32_t)(read_pos - read_head) < data_size);
      ByteArray_GetUint32LE(read_pos, &u32buf);
      nibble[0] = (u32buf >>  0) & 0xF;
      nibble[1] = (u32buf >>  4) & 0xF;
      nibble[2] = (u32buf >>  8) & 0xF;
      nibble[3] = (u32buf >> 12) & 0xF;
      nibble[4] = (u32buf >> 16) & 0xF;
      nibble[5] = (u32buf >> 20) & 0xF;
      nibble[6] = (u32buf >> 24) & 0xF;
      nibble[7] = (u32buf >> 28) & 0xF;

      buffer[ch][smpl + 0] = IMAADPCMCoreDecoder_DecodeSample(&(core_decoder[ch]), nibble[0]);
      buffer[ch][smpl + 1] = IMAADPCMCoreDecoder_DecodeSample(&(core_decoder[ch]), nibble[1]);
      buffer[ch][smpl + 2] = IMAADPCMCoreDecoder_DecodeSample(&(core_decoder[ch]), nibble[2]);
      buffer[ch][smpl + 3] = IMAADPCMCoreDecoder_DecodeSample(&(core_decoder[ch]), nibble[3]);
      buffer[ch][smpl + 4] = IMAADPCMCoreDecoder_DecodeSample(&(core_decoder[ch]), nibble[4]);
      buffer[ch][smpl + 5] = IMAADPCMCoreDecoder_DecodeSample(&(core_decoder[ch]), nibble[5]);
      buffer[ch][smpl + 6] = IMAADPCMCoreDecoder_DecodeSample(&(core_decoder[ch]), nibble[6]);
      buffer[ch][smpl + 7] = IMAADPCMCoreDecoder_DecodeSample(&(core_decoder[ch]), nibble[7]);
    }
  }

  /* 読み出しサイズをセット */
  (*read_size) = (uint32_t)(read_pos - read_head);
  return IMAADPCM_ERROR_OK;
}

/* 単一データブロックデコード */
static IMAADPCMApiResult IMAADPCMWAVDecoder_DecodeBlock(
    struct IMAADPCMWAVDecoder *decoder,
    const uint8_t *data, uint32_t data_size, 
    int16_t **buffer, uint32_t buffer_num_channels, uint32_t buffer_num_samples, 
    uint32_t num_decode_samples, uint32_t *read_size)
{
  IMAADPCMError err;
  const struct IMAADPCMWAVHeaderInfo *header;

  /* 引数チェック */
  if ((decoder == NULL) || (data == NULL) || (buffer == NULL)
      || (read_size == NULL)) {
    return IMAADPCM_APIRESULT_INVALID_ARGUMENT;
  }

  header = &(decoder->header);

  /* バッファサイズチェック */
  if (buffer_num_channels < header->num_channels) {
    return IMAADPCM_APIRESULT_INSUFFICIENT_BUFFER;
  }

  /* ブロックデコード */
  switch (header->num_channels) {
    case 1:
      err = IMAADPCMWAVDecoder_DecodeBlockMono(decoder->core_decoder, 
          data, data_size, buffer, buffer_num_samples, 
          num_decode_samples, read_size);
      break;
    case 2:
      err = IMAADPCMWAVDecoder_DecodeBlockStereo(decoder->core_decoder, 
          data, data_size, buffer, buffer_num_samples, 
          num_decode_samples, read_size);
      break;
    default:
      return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }

  /* デコード時のエラーハンドル */
  if (err != IMAADPCM_ERROR_OK) {
    switch (err) {
      case IMAADPCM_ERROR_INVALID_ARGUMENT:
        return IMAADPCM_APIRESULT_INVALID_ARGUMENT;
      case IMAADPCM_ERROR_INVALID_FORMAT:
        return IMAADPCM_APIRESULT_INVALID_FORMAT;
      case IMAADPCM_ERROR_INSUFFICIENT_BUFFER:
        return IMAADPCM_APIRESULT_INSUFFICIENT_BUFFER;
      default:
        return IMAADPCM_APIRESULT_NG;
    }
  }

  return IMAADPCM_APIRESULT_OK;
}

/* ヘッダ含めファイル全体をデコード */
IMAADPCMApiResult IMAADPCMWAVDecoder_DecodeWhole(
    struct IMAADPCMWAVDecoder *decoder, const uint8_t *data, uint32_t data_size,
    int16_t **buffer, uint32_t buffer_num_channels, uint32_t buffer_num_samples)
{
  IMAADPCMApiResult ret;
  uint32_t progress, ch, read_size, read_offset, num_decode_samples;
  const uint8_t *read_pos;
  int16_t *buffer_ptr[IMAADPCM_MAX_NUM_CHANNELS];
  const struct IMAADPCMWAVHeaderInfo *header;

  /* 引数チェック */
  if ((decoder == NULL) || (data == NULL) || (buffer == NULL)) {
    return IMAADPCM_APIRESULT_INVALID_ARGUMENT;
  }

  /* ヘッダデコード */
  if ((ret = IMAADPCMWAVDecoder_DecodeHeader(data, data_size, &(decoder->header)))
      != IMAADPCM_APIRESULT_OK) {
    return ret;
  }
  header = &(decoder->header);

  /* バッファサイズチェック */
  if ((buffer_num_channels < header->num_channels)
      || (buffer_num_samples < header->num_samples)) {
    return IMAADPCM_APIRESULT_INSUFFICIENT_BUFFER;
  }

  progress = 0;
  read_offset = header->header_size;
  read_pos = data + header->header_size;
  while (progress < header->num_samples) {
    /* デコードサンプル数の確定 */
    num_decode_samples
      = IMAADPCM_MIN_VAL(header->num_samples_per_block, header->num_samples - progress);
    /* サンプル書き出し位置のセット */
    for (ch = 0; ch < header->num_channels; ch++) {
      buffer_ptr[ch] = &buffer[ch][progress];
    }

    /* ブロックデコード */
    if ((ret = IMAADPCMWAVDecoder_DecodeBlock(decoder,
          read_pos, data_size - read_offset,
          buffer_ptr, buffer_num_channels, buffer_num_samples - progress, 
          num_decode_samples, &read_size)) != IMAADPCM_APIRESULT_OK) {
      return ret;
    }

    /* 進捗更新 */
    read_pos    += read_size;
    read_offset += read_size;
    progress    += num_decode_samples;
    assert(read_size <= header->block_size);
    assert(read_offset <= data_size);
  }

  /* 成功終了 */
  return IMAADPCM_APIRESULT_OK;
}

/* ヘッダエンコード */
IMAADPCMApiResult IMAADPCMWAVEncoder_EncodeHeader(
    const struct IMAADPCMWAVHeaderInfo *header_info, uint8_t *data, uint32_t data_size)
{
  uint8_t *data_pos;
  uint32_t num_blocks, data_chunk_size;
  uint32_t tail_block_num_samples, tail_block_size;

  /* 引数チェック */
  if ((header_info == NULL) || (data == NULL)) {
    return IMAADPCM_APIRESULT_INVALID_ARGUMENT;
  }

  /* ヘッダサイズと入力データサイズの比較 */
  if (data_size < IMAADPCMWAVENCODER_HEADER_SIZE) {
    return IMAADPCM_APIRESULT_INSUFFICIENT_DATA;
  }

  /* ヘッダの簡易チェック: ブロックサイズはサンプルデータを全て入れられるはず */
  if (IMAADPCM_CALCULATE_DATASIZE_BYTE(header_info->num_samples_per_block, header_info->bits_per_sample) > header_info->block_size) {
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }
  
  /* データサイズ計算 */
  assert(header_info->num_samples_per_block != 0);
  num_blocks = (header_info->num_samples / header_info->num_samples_per_block) + 1;
  data_chunk_size = header_info->block_size * num_blocks;
  /* 末尾のブロックの剰余サンプルサイズだけ減じる */
  tail_block_num_samples = header_info->num_samples % header_info->num_samples_per_block;
  tail_block_size = IMAADPCM_CALCULATE_DATASIZE_BYTE(header_info->num_samples_per_block - tail_block_num_samples, header_info->bits_per_sample);
  data_chunk_size -= tail_block_size;

  /* 書き出し用ポインタ設定 */
  data_pos = data;

  /* RIFFチャンクID */
  ByteArray_PutUint8(data_pos, 'R');
  ByteArray_PutUint8(data_pos, 'I');
  ByteArray_PutUint8(data_pos, 'F');
  ByteArray_PutUint8(data_pos, 'F');
  /* RIFFチャンクサイズ */
  ByteArray_PutUint32LE(data_pos, IMAADPCMWAVENCODER_HEADER_SIZE + data_chunk_size - 8);
  /* WAVEチャンクID */
  ByteArray_PutUint8(data_pos, 'W');
  ByteArray_PutUint8(data_pos, 'A');
  ByteArray_PutUint8(data_pos, 'V');
  ByteArray_PutUint8(data_pos, 'E');
  /* FMTチャンクID */
  ByteArray_PutUint8(data_pos, 'f');
  ByteArray_PutUint8(data_pos, 'm');
  ByteArray_PutUint8(data_pos, 't');
  ByteArray_PutUint8(data_pos, ' ');
  /* FMTチャンクサイズは20で決め打ち */
  ByteArray_PutUint32LE(data_pos, 20);
  /* WAVEフォーマットタイプ: IMA-ADPCM(17)で決め打ち */
  ByteArray_PutUint16LE(data_pos, 17);
  /* チャンネル数 */
  if (header_info->num_channels > IMAADPCM_MAX_NUM_CHANNELS) {
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }
  ByteArray_PutUint16LE(data_pos, header_info->num_channels);
  /* サンプリングレート */
  ByteArray_PutUint32LE(data_pos, header_info->sampling_rate);
  /* データ速度[byte/sec] */
  ByteArray_PutUint32LE(data_pos, header_info->bytes_per_sec);
  /* ブロックサイズ */
  ByteArray_PutUint16LE(data_pos, header_info->block_size);
  /* サンプルあたりビット数: 4で決め打ち */
  if (header_info->bits_per_sample != IMAADPCM_BITS_PER_SAMPLE) {
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }
  ByteArray_PutUint16LE(data_pos, header_info->bits_per_sample);
  /* fmtチャンクのエキストラサイズ: 2で決め打ち */
  ByteArray_PutUint16LE(data_pos, 2);
  /* ブロックあたりサンプル数 */
  ByteArray_PutUint16LE(data_pos, header_info->num_samples_per_block);

  /* FACTチャンクID */
  ByteArray_PutUint8(data_pos, 'f');
  ByteArray_PutUint8(data_pos, 'a');
  ByteArray_PutUint8(data_pos, 'c');
  ByteArray_PutUint8(data_pos, 't');
  /* FACTチャンクのエキストラサイズ: 4で決め打ち */
  ByteArray_PutUint32LE(data_pos, 4);
  /* サンプル数 */
  ByteArray_PutUint32LE(data_pos, header_info->num_samples);

  /* その他のチャンクは書き出さず、すぐにdataチャンクへ */

  /* dataチャンクID */
  ByteArray_PutUint8(data_pos, 'd');
  ByteArray_PutUint8(data_pos, 'a');
  ByteArray_PutUint8(data_pos, 't');
  ByteArray_PutUint8(data_pos, 'a');
  /* データチャンクサイズ */
  ByteArray_PutUint32LE(data_pos, data_chunk_size);

  /* 成功終了 */
  return IMAADPCM_APIRESULT_OK;
}

/* エンコーダワークサイズ計算 */
int32_t IMAADPCMWAVEncoder_CalculateWorkSize(void)
{
  return IMAADPCM_ALIGNMENT + sizeof(struct IMAADPCMWAVEncoder);
}

/* エンコーダハンドル作成 */
struct IMAADPCMWAVEncoder *IMAADPCMWAVEncoder_Create(void *work, int32_t work_size)
{
  struct IMAADPCMWAVEncoder *encoder;
  uint8_t *work_ptr;
  uint32_t alloced_by_malloc = 0;

  /* 領域自前確保の場合 */
  if ((work == NULL) && (work_size == 0)) {
    work_size = IMAADPCMWAVEncoder_CalculateWorkSize();
    work = malloc((uint32_t)work_size);
    alloced_by_malloc = 1;
  }

  /* 引数チェック */
  if ((work == NULL) || (work_size < IMAADPCMWAVEncoder_CalculateWorkSize())) {
    return NULL;
  }

  work_ptr = (uint8_t *)work;

  /* アラインメントを揃えてから構造体を配置 */
  work_ptr = (uint8_t *)IMAADPCM_ROUND_UP((uintptr_t)work_ptr, IMAADPCM_ALIGNMENT);
  encoder = (struct IMAADPCMWAVEncoder *)work_ptr;

  /* ハンドルの中身を0初期化 */
  memset(encoder, 0, sizeof(struct IMAADPCMWAVEncoder));

  /* パラメータは未セット状態に */
  encoder->set_parameter = 0;

  /* 自前確保の場合はメモリを記憶しておく */
  encoder->work = alloced_by_malloc ? work : NULL;

  return encoder;
}

/* エンコーダハンドル破棄 */
void IMAADPCMWAVEncoder_Destroy(struct IMAADPCMWAVEncoder *encoder)
{
  if (encoder != NULL) {
    /* 自分で領域確保していたら破棄 */
    if (encoder->work != NULL) {
      free(encoder->work);
    }
  }
}

/* 1サンプルエンコード */
static uint8_t IMAADPCMCoreEncoder_EncodeSample(
    struct IMAADPCMCoreEncoder *encoder, int16_t sample)
{
  uint8_t nibble;
  int8_t idx;
  int32_t prev, diff, qdiff, delta, stepsize, diffabs, sign;

  assert(encoder != NULL);
  
  /* 頻繁に参照する変数をオート変数に受ける */
  prev = encoder->prev_sample;
  idx = encoder->stepsize_index;

  /* ステップサイズの取得 */
  stepsize = IMAADPCM_stepsize_table[idx];

  /* 差分 */
  diff = sample - prev;
  sign = diff < 0;
  diffabs = sign ? -diff : diff;

  /* 差分を符号表現に変換 */
  /* nibble = sign(diff) * round(|diff| * 4 / stepsize) */
  nibble = (uint8_t)IMAADPCM_MIN_VAL((diffabs << 2) / stepsize, 7);
  /* nibbleの最上位ビットは符号ビット */
  nibble |= sign ? 0x8 : 0x0;

  /* 量子化した差分を計算 */
  delta = nibble & 7;
  qdiff = (stepsize * ((delta << 1) + 1)) >> 3;

  /* TODO: ここで量子化誤差が出る。観察から始める */

  /* 量子化した差分を加える */
  if (sign) {
    prev -= qdiff;
  } else {
    prev += qdiff;
  }
  prev = IMAADPCM_INNER_VAL(prev, -32768, 32767);

  /* インデックス更新 */
  idx += IMAADPCM_index_table[nibble];
  idx = IMAADPCM_INNER_VAL(idx, 0, 88);

  /* 計算結果の反映 */
  encoder->prev_sample = (int16_t)prev;
  encoder->stepsize_index = idx;

  return nibble;
}

/* モノラルブロックのエンコード */
static IMAADPCMError IMAADPCMWAVEncoder_EncodeBlockMono(
    struct IMAADPCMCoreEncoder *core_encoder,
    const int16_t *const *input, uint32_t num_samples,
    uint8_t *data, uint32_t data_size, uint32_t *output_size)
{
  uint8_t u8buf;
  uint8_t nibble[2];
  uint32_t smpl;
  uint8_t *data_pos = data;

  /* 引数チェック */
  if ((core_encoder == NULL) || (input == NULL)
      || (data == NULL) || (output_size == NULL)) {
    return IMAADPCM_ERROR_INVALID_ARGUMENT;
  }

  /* 十分なデータサイズがあるか確認 */
  if (data_size < (num_samples / 2 + 4)) {
    return IMAADPCM_ERROR_INSUFFICIENT_DATA;
  }

  /* 先頭サンプルをエンコーダにセット */
  core_encoder->prev_sample = input[0][0];

  /* ブロックヘッダエンコード */
  ByteArray_PutUint16LE(data_pos, core_encoder->prev_sample);
  ByteArray_PutUint8(data_pos, core_encoder->stepsize_index);
  ByteArray_PutUint8(data_pos, 0); /* reserved */

  /* ブロックデータエンコード */
  for (smpl = 1; smpl < num_samples; smpl += 2) {
    assert((uint32_t)(data_pos - data) < data_size);
    nibble[0] = IMAADPCMCoreEncoder_EncodeSample(core_encoder, input[0][smpl + 0]);
    nibble[1] = IMAADPCMCoreEncoder_EncodeSample(core_encoder, input[0][smpl + 1]);
    assert((nibble[0] <= 0xF) && (nibble[1] <= 0xF));
    u8buf = (uint8_t)((nibble[0] << 0) | (nibble[1] << 4));
    ByteArray_PutUint8(data_pos, u8buf);
  }

  /* 書き出しサイズをセット */
  (*output_size) = (uint32_t)(data_pos - data);
  return IMAADPCM_ERROR_OK;
}

/* ステレオブロックのエンコード */
static IMAADPCMError IMAADPCMWAVEncoder_EncodeBlockStereo(
    struct IMAADPCMCoreEncoder *core_encoder,
    const int16_t *const *input, uint32_t num_samples,
    uint8_t *data, uint32_t data_size, uint32_t *output_size)
{
  uint32_t u32buf;
  uint8_t nibble[8];
  uint32_t ch, smpl;
  uint8_t *data_pos = data;

  /* 引数チェック */
  if ((core_encoder == NULL) || (input == NULL)
      || (data == NULL) || (output_size == NULL)) {
    return IMAADPCM_ERROR_INVALID_ARGUMENT;
  }

  /* 十分なデータサイズがあるか確認 */
  if (data_size < (num_samples + 4)) {
    return IMAADPCM_ERROR_INSUFFICIENT_DATA;
  }

  /* 先頭サンプルをエンコーダにセット */
  for (ch = 0; ch < 2; ch++) {
    core_encoder[ch].prev_sample = input[ch][0];
  }

  /* ブロックヘッダエンコード */
  for (ch = 0; ch < 2; ch++) {
    ByteArray_PutUint16LE(data_pos, core_encoder[ch].prev_sample);
    ByteArray_PutUint8(data_pos, core_encoder[ch].stepsize_index);
    ByteArray_PutUint8(data_pos, 0); /* reserved */
  }

  /* ブロックデータエンコード */
  for (smpl = 1; smpl < num_samples; smpl += 8) {
    for (ch = 0; ch < 2; ch++) {
      assert((uint32_t)(data_pos - data) < data_size);
      nibble[0] = IMAADPCMCoreEncoder_EncodeSample(&(core_encoder[ch]), input[ch][smpl + 0]);
      nibble[1] = IMAADPCMCoreEncoder_EncodeSample(&(core_encoder[ch]), input[ch][smpl + 1]);
      nibble[2] = IMAADPCMCoreEncoder_EncodeSample(&(core_encoder[ch]), input[ch][smpl + 2]);
      nibble[3] = IMAADPCMCoreEncoder_EncodeSample(&(core_encoder[ch]), input[ch][smpl + 3]);
      nibble[4] = IMAADPCMCoreEncoder_EncodeSample(&(core_encoder[ch]), input[ch][smpl + 4]);
      nibble[5] = IMAADPCMCoreEncoder_EncodeSample(&(core_encoder[ch]), input[ch][smpl + 5]);
      nibble[6] = IMAADPCMCoreEncoder_EncodeSample(&(core_encoder[ch]), input[ch][smpl + 6]);
      nibble[7] = IMAADPCMCoreEncoder_EncodeSample(&(core_encoder[ch]), input[ch][smpl + 7]);
      assert((nibble[0] <= 0xF) && (nibble[1] <= 0xF) && (nibble[2] <= 0xF) && (nibble[3] <= 0xF)
          && (nibble[4] <= 0xF) && (nibble[5] <= 0xF) && (nibble[6] <= 0xF) && (nibble[7] <= 0xF));
      u32buf  = (uint32_t)(nibble[0] <<  0);
      u32buf |= (uint32_t)(nibble[1] <<  4);
      u32buf |= (uint32_t)(nibble[2] <<  8);
      u32buf |= (uint32_t)(nibble[3] << 12);
      u32buf |= (uint32_t)(nibble[4] << 16);
      u32buf |= (uint32_t)(nibble[5] << 20);
      u32buf |= (uint32_t)(nibble[6] << 24);
      u32buf |= (uint32_t)(nibble[7] << 28);
      ByteArray_PutUint32LE(data_pos, u32buf);
    }
  }

  /* 書き出しサイズをセット */
  (*output_size) = (uint32_t)(data_pos - data);
  return IMAADPCM_ERROR_OK;
}

/* 単一データブロックエンコード */
static IMAADPCMApiResult IMAADPCMWAVEncoder_EncodeBlock(
    struct IMAADPCMWAVEncoder *encoder,
    const int16_t *const *input, uint32_t num_samples, 
    uint8_t *data, uint32_t data_size, uint32_t *output_size)
{
  IMAADPCMError err;
  const struct IMAADPCMWAVEncodeParameter *enc_param;

  /* 引数チェック */
  if ((encoder == NULL) || (data == NULL)
      || (input == NULL) || (output_size == NULL)) {
    return IMAADPCM_APIRESULT_INVALID_ARGUMENT;
  }
  enc_param = &(encoder->encode_paramemter);

  /* ブロックデコード */
  switch (enc_param->num_channels) {
    case 1:
      err = IMAADPCMWAVEncoder_EncodeBlockMono(encoder->core_encoder, 
          input, num_samples, data, data_size, output_size);
      break;
    case 2:
      err = IMAADPCMWAVEncoder_EncodeBlockStereo(encoder->core_encoder, 
          input, num_samples, data, data_size, output_size);
      break;
    default:
      return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }

  /* デコード時のエラーハンドル */
  if (err != IMAADPCM_ERROR_OK) {
    switch (err) {
      case IMAADPCM_ERROR_INVALID_ARGUMENT:
        return IMAADPCM_APIRESULT_INVALID_ARGUMENT;
      case IMAADPCM_ERROR_INVALID_FORMAT:
        return IMAADPCM_APIRESULT_INVALID_FORMAT;
      case IMAADPCM_ERROR_INSUFFICIENT_BUFFER:
        return IMAADPCM_APIRESULT_INSUFFICIENT_BUFFER;
      default:
        return IMAADPCM_APIRESULT_NG;
    }
  }

  return IMAADPCM_APIRESULT_OK;
}

/* エンコードパラメータをヘッダに変換 */
static IMAADPCMError IMAADPCMWAVEncoder_ConvertParameterToHeader(
    const struct IMAADPCMWAVEncodeParameter *enc_param, uint32_t num_samples,
    struct IMAADPCMWAVHeaderInfo *header_info)
{
  uint32_t block_data_size;
  struct IMAADPCMWAVHeaderInfo tmp_header = {0, };

  /* 引数チェック */
  if ((enc_param == NULL) || (header_info == NULL)) {
    return IMAADPCM_ERROR_INVALID_ARGUMENT;
  }

  /* サンプルあたりビット数は4固定 */
  if (enc_param->bits_per_sample != IMAADPCM_BITS_PER_SAMPLE) {
    return IMAADPCM_ERROR_INVALID_FORMAT;
  }

  /* ヘッダサイズは決め打ち */
  tmp_header.header_size = IMAADPCMWAVENCODER_HEADER_SIZE;
  /* 総サンプル数 */
  tmp_header.num_samples = num_samples;

  /* そのままヘッダに入れられるメンバ */
  tmp_header.num_channels = enc_param->num_channels;
  tmp_header.sampling_rate = enc_param->sampling_rate;
  tmp_header.bits_per_sample = enc_param->bits_per_sample;
  tmp_header.block_size = enc_param->block_size;

  /* 計算が必要なメンバ */
  if (enc_param->block_size <= enc_param->num_channels * 4) {
    /* データを入れる領域がない */
    return IMAADPCM_ERROR_INVALID_FORMAT;
  }
  /* 4はチャンネルあたりのヘッダ領域サイズ */
  block_data_size = enc_param->block_size - enc_param->num_channels * 4;
  assert((block_data_size * 8) % (enc_param->bits_per_sample * enc_param->num_channels) == 0);
  assert((enc_param->bits_per_sample * enc_param->num_channels) != 0);
  tmp_header.num_samples_per_block = (uint16_t)((block_data_size * 8) / (enc_param->bits_per_sample * enc_param->num_channels));
  /* ヘッダに入っている分+1 */
  tmp_header.num_samples_per_block += 1;
  assert(tmp_header.num_samples_per_block != 0);
  tmp_header.bytes_per_sec = (enc_param->block_size * enc_param->sampling_rate) / tmp_header.num_samples_per_block;

  /* 成功終了 */
  (*header_info) = tmp_header;

  return IMAADPCM_ERROR_OK;
}

/* エンコードパラメータの設定 */
IMAADPCMApiResult IMAADPCMWAVEncoder_SetEncodeParameter(
    struct IMAADPCMWAVEncoder *encoder, const struct IMAADPCMWAVEncodeParameter *parameter)
{
  struct IMAADPCMWAVHeaderInfo tmp_header = {0, };

  /* 引数チェック */
  if ((encoder == NULL) || (parameter == NULL)) {
    return IMAADPCM_APIRESULT_INVALID_ARGUMENT;
  }

  /* パラメータ設定がおかしくないか、ヘッダへの変換を通じて確認 */
  /* 総サンプル数はダミー値を入れる */
  if (IMAADPCMWAVEncoder_ConvertParameterToHeader(parameter, 0, &tmp_header) != IMAADPCM_ERROR_OK) {
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }

  /* パラメータ設定 */
  encoder->encode_paramemter = (*parameter);

  /* パラメータ設定済みフラグを立てる */
  encoder->set_parameter = 1;

  return IMAADPCM_APIRESULT_OK;
}

/* ヘッダ含めファイル全体をエンコード */
IMAADPCMApiResult IMAADPCMWAVEncoder_EncodeWhole(
    struct IMAADPCMWAVEncoder *encoder,
    const int16_t *const *input, uint32_t num_samples,
    uint8_t *data, uint32_t data_size, uint32_t *output_size)
{
  IMAADPCMApiResult ret;
  uint32_t progress, ch, write_size, write_offset, num_encode_samples;
  uint8_t *data_pos;
  const int16_t *input_ptr[IMAADPCM_MAX_NUM_CHANNELS];
  struct IMAADPCMWAVHeaderInfo header = { 0, };

  /* 引数チェック */
  if ((encoder == NULL) || (input == NULL)
      || (data == NULL) || (output_size == NULL)) {
    return IMAADPCM_APIRESULT_INVALID_ARGUMENT;
  }

  /* パラメータ未セットではエンコードできない */
  if (encoder->set_parameter == 0) {
    return IMAADPCM_APIRESULT_PARAMETER_NOT_SET;
  }

  /* 書き出し位置を取得 */
  data_pos = data;

  /* エンコードパラメータをヘッダに変換 */
  if (IMAADPCMWAVEncoder_ConvertParameterToHeader(&(encoder->encode_paramemter), num_samples, &header) != IMAADPCM_ERROR_OK) {
    return IMAADPCM_APIRESULT_INVALID_FORMAT;
  }

  /* ヘッダエンコード */
  if ((ret = IMAADPCMWAVEncoder_EncodeHeader(&header, data_pos, data_size)) != IMAADPCM_APIRESULT_OK) {
    return ret;
  }

  progress = 0;
  write_offset = IMAADPCMWAVENCODER_HEADER_SIZE;
  data_pos = data + IMAADPCMWAVENCODER_HEADER_SIZE;
  while (progress < num_samples) {
    /* エンコードサンプル数の確定 */
    num_encode_samples 
      = IMAADPCM_MIN_VAL(header.num_samples_per_block, num_samples - progress);
    /* サンプル参照位置のセット */
    for (ch = 0; ch < header.num_channels; ch++) {
      input_ptr[ch] = &input[ch][progress];
    }

    /* ブロックエンコード */
    if ((ret = IMAADPCMWAVEncoder_EncodeBlock(encoder,
            input_ptr, num_encode_samples,
            data_pos, data_size - write_offset, &write_size)) != IMAADPCM_APIRESULT_OK) {
      return ret;
    }

    /* 進捗更新 */
    data_pos      += write_size;
    write_offset  += write_size;
    progress      += num_encode_samples;
    assert(write_size <= header.block_size);
    assert(write_offset <= data_size);
  }

  /* 成功終了 */
  (*output_size) = write_offset;
  return IMAADPCM_APIRESULT_OK;
}

