#include "ima_adpcm.h"
#include "byte_array.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* アラインメント */
#define IMAADPCM_ALIGNMENT 16

/* nの倍数への切り上げ */
#define IMAADPCM_ROUND_UP(val, n) ((((val) + ((n) - 1)) / (n)) * (n))

/* 最大値を選択 */
#define IMAADPCM_MAX_VAL(a, b) (((a) > (b)) ? (a) : (b))

/* 最小値を選択 */
#define IMAADPCM_MIN_VAL(a, b) (((a) < (b)) ? (a) : (b))

/* min以上max未満に制限 */
#define IMAADPCM_INNER_VAL(val, min, max) IMAADPCM_MAX_VAL(min, IMAADPCM_MIN_VAL(max, val))

/* FourCCの一致確認 */
#define IMAADPCM_CHECK_FOURCC(u32lebuf, c1, c2, c3, c4) \
  ((u32lebuf) == ((c1 << 0) | (c2 << 8) | (c3 << 16) | (c4 << 24)))

/* 内部エラー型 */
typedef enum IMAADPCMErrorTag {
  IMAADPCM_ERROR_OK = 0,              /* OK */
  IMAADPCM_ERROR_NG,                  /* 分類不能な失敗 */
  IMAADPCM_ERROR_INVALID_ARGUMENT,    /* 不正な引数 */
  IMAADPCM_ERROR_INVALID_FORMAT,      /* 不正なフォーマット       */
  IMAADPCM_ERROR_INSUFFICIENT_BUFFER  /* バッファサイズが足りない */
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

/* インデックス変動テーブル */
static const int8_t IMAADPCM_index_table[16]
  = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

/* ステップサイズ量子化テーブル */
static const uint16_t IMAADPCM_stepsize_table[89] 
  = { 7, 8, 9, 10, 11, 12, 13, 14, 
    16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60,
    66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209,
    230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878,
    2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
    5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635,
    13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767 };

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
  if (data_size < u32buf) {
    fprintf(stderr, "Data size too small. fmt chunk size:%d data size:%d \n", u32buf, data_size);
    return IMAADPCM_APIRESULT_INSUFFICIENT_BUFFER;
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
  int32_t smp;
  int8_t  idx;
  int32_t diff, delta;
  int32_t stepsize;

  assert(decoder != NULL);

  /* 頻繁に参照する変数をオート変数に受ける */
  smp = decoder->sample_val;
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
    smp -= diff;
  } else {
    smp += diff;
  }

  /* 16bit幅にクリップ */
  smp = IMAADPCM_INNER_VAL(smp, -32768, 32767);

  /* 計算結果の反映 */
  decoder->sample_val = (int16_t)smp;
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
      || (buffer == NULL) || (read_size == NULL)) {
    return IMAADPCM_ERROR_INVALID_ARGUMENT;
  }

  /* バッファサイズチェック */
  if (buffer_num_samples < num_decode_samples) {
    return IMAADPCM_ERROR_INSUFFICIENT_BUFFER;
  }

  /* 指定サンプル分デコードしきれるか確認 */
  if (num_decode_samples > 2 * (data_size - 4)) {
    return IMAADPCM_ERROR_INSUFFICIENT_BUFFER;
  }

  /* ブロックヘッダデコード */
  ByteArray_GetUint16LE(read_pos, (uint16_t *)&(core_decoder->sample_val));
  ByteArray_GetUint8(read_pos, (uint8_t *)&(core_decoder->stepsize_index));
  ByteArray_GetUint8(read_pos, &u8buf); /* reserved */
  assert(u8buf == 0);

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
    assert(reserved == 0);
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
IMAADPCMApiResult IMAADPCMWAVDecoder_DecodeBlock(
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
  read_offset = 0;
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
