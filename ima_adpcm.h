#ifndef IMAADPCM_H_INCLDED
#define IMAADPCM_H_INCLDED

#include <stdint.h>

/* 処理可能な最大チャンネル数 */
#define IMAADPCM_MAX_NUM_CHANNELS 2

/* API結果型 */
typedef enum IMAADPCMApiResultTag {
  IMAADPCM_APIRESULT_OK = 0,              /* 成功                     */
  IMAADPCM_APIRESULT_INVALID_ARGUMENT,    /* 無効な引数               */
  IMAADPCM_APIRESULT_INVALID_FORMAT,      /* 不正なフォーマット       */
  IMAADPCM_APIRESULT_INSUFFICIENT_BUFFER, /* バッファサイズが足りない */
  IMAADPCM_APIRESULT_NG                   /* 分類不能な失敗           */
} IMAADPCMApiResult; 

/* IMA-ADPCM形式のwavファイルのヘッダ情報 */
struct IMAADPCMHeaderInfo {
  uint16_t num_channels;          /* チャンネル数                                 */
  uint32_t sampling_rate;         /* サンプリングレート                           */
  uint32_t bytes_per_sec;         /* データ速度[byte/sec]                         */
  uint16_t block_size;            /* ブロックサイズ                               */
  uint16_t bits_per_sample;       /* サンプルあたりビット数                       */
  uint16_t num_samples_per_block; /* ブロックあたりサンプル数                     */
  uint32_t num_samples;           /* サンプル数                                   */
  uint32_t header_size;           /* ファイル先頭からdata領域先頭までのオフセット */
};

/* デコーダハンドル */
struct IMAADPCMDecoder;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ヘッダデコード */
IMAADPCMApiResult IMAADPCMDecoder_DecodeHeader(
    const uint8_t *data, uint32_t data_size, struct IMAADPCMHeaderInfo *header_info);

/* ワークサイズ計算 */
int32_t IMAADPCMDecoder_CalculateWorkSize(void);

/* デコードハンドル作成 */
struct IMAADPCMDecoder *IMAADPCMDecoder_Create(void *work, int32_t work_size);

/* デコードハンドル破棄 */
void IMAADPCMDecoder_Destroy(struct IMAADPCMDecoder *decoder);

/* ヘッダ含めファイル全体をデコード */
IMAADPCMApiResult IMAADPCMDecoder_DecodeWhole(
    struct IMAADPCMDecoder *decoder, const uint8_t *data, uint32_t data_size,
    int16_t **buffer, uint32_t buffer_num_channels, uint32_t *output_num_samples);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* IMAADPCM_H_INCLDED */
