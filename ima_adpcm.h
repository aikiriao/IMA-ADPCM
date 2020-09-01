#ifndef IMAADPCMWAV_H_INCLDED
#define IMAADPCMWAV_H_INCLDED

#include <stdint.h>

/* 処理可能な最大チャンネル数 */
#define IMAADPCMWAV_MAX_NUM_CHANNELS 2

/* API結果型 */
typedef enum IMAADPCMWAVApiResultTag {
  IMAADPCMWAV_APIRESULT_OK = 0,              /* 成功                     */
  IMAADPCMWAV_APIRESULT_INVALID_ARGUMENT,    /* 無効な引数               */
  IMAADPCMWAV_APIRESULT_INVALID_FORMAT,      /* 不正なフォーマット       */
  IMAADPCMWAV_APIRESULT_INSUFFICIENT_BUFFER, /* バッファサイズが足りない */
  IMAADPCMWAV_APIRESULT_NG                   /* 分類不能な失敗           */
} IMAADPCMWAVApiResult; 

/* IMA-ADPCM形式のwavファイルのヘッダ情報 */
struct IMAADPCMWAVHeaderInfo {
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
struct IMAADPCMWAVDecoder;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ヘッダデコード */
IMAADPCMWAVApiResult IMAADPCMWAVDecoder_DecodeHeader(
    const uint8_t *data, uint32_t data_size, struct IMAADPCMWAVHeaderInfo *header_info);

/* ワークサイズ計算 */
int32_t IMAADPCMWAVDecoder_CalculateWorkSize(void);

/* デコーダハンドル作成 */
struct IMAADPCMWAVDecoder *IMAADPCMWAVDecoder_Create(void *work, int32_t work_size);

/* デコーダハンドル破棄 */
void IMAADPCMWAVDecoder_Destroy(struct IMAADPCMWAVDecoder *decoder);

/* ヘッダ含めファイル全体をデコード */
IMAADPCMWAVApiResult IMAADPCMWAVDecoder_DecodeWhole(
    struct IMAADPCMWAVDecoder *decoder, const uint8_t *data, uint32_t data_size,
    int16_t **buffer, uint32_t buffer_num_channels, uint32_t *output_num_samples);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* IMAADPCMWAV_H_INCLDED */
