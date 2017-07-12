 /*
  * Copyright (C) 2015 NXP Semiconductors
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *      http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */
#ifndef DATA_TYPES_H
#define DATA_TYPES_H
#include <stdint.h>
#include <stdbool.h>
#ifndef NULL
#define NULL     0
#endif

/*
#ifndef false
#define false  0
#endif
*/

#ifndef FALSE
#define FALSE  0
#endif

typedef uint32_t        TIME_STAMP;

/*
#ifndef true
#define true   (!false)
#endif
*/

#ifndef TRUE
#define TRUE   (!FALSE)
#endif

typedef unsigned char   UBYTE;

#define STATUS_SUCCESS     0x00
#define STATUS_OK          0x00
#define STATUS_UPTO_DATE   0x01
#define STATUS_FAILED      0x03
#define STATUS_INUSE       0x04
#define STATUS_FILE_NOT_FOUND  0x05

#define EE_ERROR_OPEN_FAIL -1

typedef uint8_t tJBL_STATUS;

#endif
