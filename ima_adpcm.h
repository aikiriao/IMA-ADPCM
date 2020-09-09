#ifndef IMAADPCM_H_INCLDED
#define IMAADPCM_H_INCLDED

#include <stdint.h>

/* 処理可能な最大チャンネル数 */
#define IMAADPCM_MAX_NUM_CHANNELS 2

/* サンプルあたりビット数は4で固定 */
#define IMAADPCM_BITS_PER_SAMPLE  4

/* API結果型 */
typedef enum IMAADPCMApiResultTag {
  IMAADPCM_APIRESULT_OK = 0,              /* 成功                         */
  IMAADPCM_APIRESULT_INVALID_ARGUMENT,    /* 無効な引数                   */
  IMAADPCM_APIRESULT_INVALID_FORMAT,      /* 不正なフォーマット           */
  /* FIXME: BUFFER, DATAの使い方がごっちゃになってるかも
   * BUFFERは出力側（受け）のデータバッファ、DATAは入力のデータ */
  IMAADPCM_APIRESULT_INSUFFICIENT_BUFFER, /* バッファサイズが足りない     */
  IMAADPCM_APIRESULT_INSUFFICIENT_DATA,   /* データが足りない             */
  IMAADPCM_APIRESULT_PARAMETER_NOT_SET,   /* パラメータがセットされてない */
  IMAADPCM_APIRESULT_NG                   /* 分類不能な失敗               */
} IMAADPCMApiResult; 

/* IMA-ADPCM形式のwavファイルのヘッダ情報 */
struct IMAADPCMWAVHeaderInfo {
  uint16_t num_channels;          /* チャンネル数                                 */
  uint32_t sampling_rate;         /* サンプリングレート                           */
  uint32_t bytes_per_sec;         /* データ速度[byte/sec]                         */
  uint16_t block_size;            /* ブロックサイズ                               */
  uint16_t bits_per_sample;       /* サンプルあたりビット数                       */
  uint16_t num_samples_per_block; /* ブロックあたりサンプル数                     */
  uint32_t num_samples;           /* 1チャンネルあたり総サンプル数                */
  uint32_t header_size;           /* ファイル先頭からdata領域先頭までのオフセット */
};

/* エンコードパラメータ */
struct IMAADPCMWAVEncodeParameter {
  uint16_t num_channels;          /* チャンネル数                                 */
  uint32_t sampling_rate;         /* サンプリングレート                           */
  uint16_t bits_per_sample;       /* サンプルあたりビット数（今の所4で固定）      */
  uint16_t block_size;            /* ブロックサイズ[byte]                         */
};

/* デコーダハンドル */
struct IMAADPCMWAVDecoder;

/* エンコーダハンドル */
struct IMAADPCMWAVEncoder;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ヘッダデコード */
IMAADPCMApiResult IMAADPCMWAVDecoder_DecodeHeader(
    const uint8_t *data, uint32_t data_size, struct IMAADPCMWAVHeaderInfo *header_info);

/* ヘッダエンコード */
IMAADPCMApiResult IMAADPCMWAVEncoder_EncodeHeader(
    const struct IMAADPCMWAVHeaderInfo *header_info, uint8_t *data, uint32_t data_size);

/* デコーダワークサイズ計算 */
int32_t IMAADPCMWAVDecoder_CalculateWorkSize(void);

/* デコーダハンドル作成 */
struct IMAADPCMWAVDecoder *IMAADPCMWAVDecoder_Create(void *work, int32_t work_size);

/* デコーダハンドル破棄 */
void IMAADPCMWAVDecoder_Destroy(struct IMAADPCMWAVDecoder *decoder);

/* ヘッダ含めファイル全体をデコード */
IMAADPCMApiResult IMAADPCMWAVDecoder_DecodeWhole(
    struct IMAADPCMWAVDecoder *decoder,
    const uint8_t *data, uint32_t data_size,
    int16_t **buffer, uint32_t buffer_num_channels, uint32_t buffer_num_samples);

/* エンコーダワークサイズ計算 */
int32_t IMAADPCMWAVEncoder_CalculateWorkSize(void);

/* エンコーダハンドル作成 */
struct IMAADPCMWAVEncoder *IMAADPCMWAVEncoder_Create(void *work, int32_t work_size);

/* エンコーダハンドル破棄 */
void IMAADPCMWAVEncoder_Destroy(struct IMAADPCMWAVEncoder *encoder);

/* エンコードパラメータの設定 */
IMAADPCMApiResult IMAADPCMWAVEncoder_SetEncodeParameter(
    struct IMAADPCMWAVEncoder *encoder, const struct IMAADPCMWAVEncodeParameter *parameter);

/* ヘッダ含めファイル全体をエンコード */
IMAADPCMApiResult IMAADPCMWAVEncoder_EncodeWhole(
    struct IMAADPCMWAVEncoder *encoder,
    const int16_t *const *input, uint32_t num_samples,
    uint8_t *data, uint32_t data_size, uint32_t *output_size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* IMAADPCM_H_INCLDED */
