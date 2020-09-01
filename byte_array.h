#ifndef BYTEARRAY_H_INCLUDED
#define BYTEARRAY_H_INCLUDED

#include <stdint.h>

/* 1バイト読み出し */
#define ByteArray_ReadUint8(p_array)                    \
  (uint8_t)((p_array)[0])

/* 2バイト読み出し（ビッグエンディアン） */
#define ByteArray_ReadUint16BE(p_array)                 \
  (uint16_t)(                                           \
   (((uint16_t)((p_array)[0])) << 8) |                  \
   (((uint16_t)((p_array)[1])) << 0)                    \
  )

/* 4バイト読み出し（ビッグエンディアン） */
#define ByteArray_ReadUint32BE(p_array)                 \
  (uint32_t)(                                           \
   (((uint32_t)((p_array)[0])) << 24) |                 \
   (((uint32_t)((p_array)[1])) << 16) |                 \
   (((uint32_t)((p_array)[2])) <<  8) |                 \
   (((uint32_t)((p_array)[3])) <<  0)                   \
  )

/* 2バイト読み出し（リトルエンディアン） */
#define ByteArray_ReadUint16LE(p_array)                 \
  (uint16_t)(                                           \
   (((uint16_t)((p_array)[0])) << 0) |                  \
   (((uint16_t)((p_array)[1])) << 8)                    \
  )

/* 4バイト読み出し（リトルエンディアン） */
#define ByteArray_ReadUint32LE(p_array)                 \
  (uint32_t)(                                           \
   (((uint32_t)((p_array)[0])) <<  0) |                 \
   (((uint32_t)((p_array)[1])) <<  8) |                 \
   (((uint32_t)((p_array)[2])) << 16) |                 \
   (((uint32_t)((p_array)[3])) << 24)                   \
  )

/* 1バイト取得 */
#define ByteArray_GetUint8(p_array, p_u8val) {          \
  (*(p_u8val)) = ByteArray_ReadUint8(p_array);          \
  (p_array) += 1;                                       \
}

/* 2バイト取得（ビッグエンディアン） */
#define ByteArray_GetUint16BE(p_array, p_u16val) {      \
  (*(p_u16val)) = ByteArray_ReadUint16BE(p_array);      \
  (p_array) += 2;                                       \
}

/* 4バイト取得（ビッグエンディアン） */
#define ByteArray_GetUint32BE(p_array, p_u32val) {      \
  (*(p_u32val)) = ByteArray_ReadUint32BE(p_array);      \
  (p_array) += 4;                                       \
}

/* 2バイト取得（リトルエンディアン） */
#define ByteArray_GetUint16LE(p_array, p_u16val) {      \
  (*(p_u16val)) = ByteArray_ReadUint16LE(p_array);      \
  (p_array) += 2;                                       \
}

/* 4バイト取得（リトルエンディアン） */
#define ByteArray_GetUint32LE(p_array, p_u32val) {      \
  (*(p_u32val)) = ByteArray_ReadUint32LE(p_array);      \
  (p_array) += 4;                                       \
}

/* 1バイト書き出し */
#define ByteArray_WriteUint8(p_array, u8val)   {        \
  ((p_array)[0]) = (uint8_t)(u8val);                    \
}

/* 2バイト書き出し（ビッグエンディアン） */
#define ByteArray_WriteUint16BE(p_array, u16val) {      \
  ((p_array)[0]) = (uint8_t)(((u16val) >> 8) & 0xFF);   \
  ((p_array)[1]) = (uint8_t)(((u16val) >> 0) & 0xFF);   \
}

/* 4バイト書き出し（ビッグエンディアン） */
#define ByteArray_WriteUint32BE(p_array, u32val) {      \
  ((p_array)[0]) = (uint8_t)(((u32val) >> 24) & 0xFF);  \
  ((p_array)[1]) = (uint8_t)(((u32val) >> 16) & 0xFF);  \
  ((p_array)[2]) = (uint8_t)(((u32val) >>  8) & 0xFF);  \
  ((p_array)[3]) = (uint8_t)(((u32val) >>  0) & 0xFF);  \
}

/* 2バイト書き出し（リトルエンディアン） */
#define ByteArray_WriteUint16LE(p_array, u16val) {      \
  ((p_array)[0]) = (uint8_t)(((u16val) >> 0) & 0xFF);   \
  ((p_array)[1]) = (uint8_t)(((u16val) >> 8) & 0xFF);   \
}

/* 4バイト書き出し（リトルエンディアン） */
#define ByteArray_WriteUint32LE(p_array, u32val) {      \
  ((p_array)[0]) = (uint8_t)(((u32val) >>  0) & 0xFF);  \
  ((p_array)[1]) = (uint8_t)(((u32val) >>  8) & 0xFF);  \
  ((p_array)[2]) = (uint8_t)(((u32val) >> 16) & 0xFF);  \
  ((p_array)[3]) = (uint8_t)(((u32val) >> 24) & 0xFF);  \
}

/* 1バイト出力 */
#define ByteArray_PutUint8(p_array, u8val) {            \
  ByteArray_WriteUint8(p_array, u8val);                 \
  (p_array) += 1;                                       \
}

/* 2バイト出力（ビッグエンディアン） */
#define ByteArray_PutUint16BE(p_array, u16val) {        \
  ByteArray_WriteUint16BE(p_array, u16val);             \
  (p_array) += 2;                                       \
}

/* 4バイト出力（ビッグエンディアン） */
#define ByteArray_PutUint32BE(p_array, u32val) {        \
  ByteArray_WriteUint32BE(p_array, u32val);             \
  (p_array) += 4;                                       \
}

/* 2バイト出力（リトルエンディアン） */
#define ByteArray_PutUint16LE(p_array, u16val) {        \
  ByteArray_WriteUint16LE(p_array, u16val);             \
  (p_array) += 2;                                       \
}

/* 4バイト出力（リトルエンディアン） */
#define ByteArray_PutUint32LE(p_array, u32val) {        \
  ByteArray_WriteUint32LE(p_array, u32val);             \
  (p_array) += 4;                                       \
}

#endif /* BYTEARRAY_H_INCLUDED */
