/*******************************************************************************
*   Ledger Ethereum App
*   (c) 2016-2019 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

// Adapted from https://github.com/LedgerHQ/ledger-app-eth/src_common/ethUstream.c

#include <stdbool.h>
#include <stdint.h>

#include "rlp.h"
#define PRINTF(x, ...) {}

bool rlpCanDecode(uint8_t *buffer, uint32_t bufferLength, bool *valid) {
    if (*buffer <= 0x7f) {
    } else if (*buffer <= 0xb7) {
    } else if (*buffer <= 0xbf) {
        if (bufferLength < (1 + (*buffer - 0xb7))) {
            return false;
        }
        if (*buffer > 0xbb) {
            *valid = false; // arbitrary 32 bits length limitation
            return true;
        }
    } else if (*buffer <= 0xf7) {
    } else {
        if (bufferLength < (1 + (*buffer - 0xf7))) {
            return false;
        }
        if (*buffer > 0xfb) {
            *valid = false; // arbitrary 32 bits length limitation
            return true;
        }
    }
    *valid = true;
    return true;
}

static void processContent(txContext_t *context) {
    // Keep the full length for sanity checks, move to the next field
    if (!context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_CONTENT\n");
        THROW(EXCEPTION);
    }
    context->dataLength = context->currentFieldLength;
    context->currentField++;
    context->processingField = false;
}

void copyTxData(txContext_t *context, uint8_t *out, uint32_t length) {
    if (context->commandLength < length) {
        PRINTF("copyTxData Underflow\n");
        THROW(EXCEPTION);
    }
    if (out != NULL) {
        os_memmove(out, context->workBuffer, length);
    }
    context->workBuffer += length;
    context->commandLength -= length;
    if (context->processingField) {
        context->currentFieldPos += length;
    }
}

static void processNonce(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_NONCE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_NONCE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
                (context->commandLength <
                 ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, NULL, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
}

static void processStartGas(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_STARTGAS\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_STARTGAS %d\n",
               context->currentFieldLength);
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
                (context->commandLength <
                 ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        copyTxData(context,
                   context->content->startgas.value + context->currentFieldPos,
                   copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->startgas.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

static void processGasprice(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_GASPRICE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_GASPRICE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
                (context->commandLength <
                 ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        copyTxData(context,
                   context->content->gasprice.value + context->currentFieldPos,
                   copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->gasprice.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

bool rlpDecodeLength(uint8_t *buffer, uint32_t bufferLength,
                     uint32_t *fieldLength, uint32_t *offset, bool *list) {
    if (*buffer <= 0x7f) {
        *offset = 0;
        *fieldLength = 1;
        *list = false;
    } else if (*buffer <= 0xb7) {
        *offset = 1;
        *fieldLength = *buffer - 0x80;
        *list = false;
    } else if (*buffer <= 0xbf) {
        *offset = 1 + (*buffer - 0xb7);
        *list = false;
        switch (*buffer) {
            case 0xb8:
                *fieldLength = *(buffer + 1);
                break;
            case 0xb9:
                *fieldLength = (*(buffer + 1) << 8) + *(buffer + 2);
                break;
            case 0xba:
                *fieldLength =
                        (*(buffer + 1) << 16) + (*(buffer + 2) << 8) + *(buffer + 3);
                break;
            case 0xbb:
                *fieldLength = (*(buffer + 1) << 24) + (*(buffer + 2) << 16) +
                               (*(buffer + 3) << 8) + *(buffer + 4);
                break;
            default:
                return false; // arbitrary 32 bits length limitation
        }
    } else if (*buffer <= 0xf7) {
        *offset = 1;
        *fieldLength = *buffer - 0xc0;
        *list = true;
    } else {
        *offset = 1 + (*buffer - 0xf7);
        *list = true;
        switch (*buffer) {
            case 0xf8:
                *fieldLength = *(buffer + 1);
                break;
            case 0xf9:
                *fieldLength = (*(buffer + 1) << 8) + *(buffer + 2);
                break;
            case 0xfa:
                *fieldLength =
                        (*(buffer + 1) << 16) + (*(buffer + 2) << 8) + *(buffer + 3);
                break;
            case 0xfb:
                *fieldLength = (*(buffer + 1) << 24) + (*(buffer + 2) << 16) +
                               (*(buffer + 3) << 8) + *(buffer + 4);
                break;
            default:
                return false; // arbitrary 32 bits length limitation
        }
    }

    return true;
}

uint8_t readTxByte(txContext_t *context) {
    uint8_t data;
    data = *context->workBuffer;
    context->workBuffer++;
    context->commandLength--;
    if (context->processingField) {
        context->currentFieldPos++;
    }

    return data;
}

static void processValue(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_VALUE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_VALUE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
                (context->commandLength <
                 ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        copyTxData(context,
                   context->content->value.value + context->currentFieldPos,
                   copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->value.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

static void processFromShard(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for FROM SHARD \n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > 4) {
        PRINTF("Invalid length for FROM SHARD %d\n",
               context->currentFieldLength);
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
                (context->commandLength <
                 ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        copyTxData(context,
                   (uint8_t *)&context->content->fromShard + context->currentFieldPos,
                   copySize);

        //adjust from big endian to little endian
        uint32_t shardId = 0;
        uint8_t *shardIdArray = (uint8_t *) &context->content->fromShard;
        for(uint32_t i = 0 ; i < context->currentFieldLength;  i++ ) {
            shardId <<= 8;
            shardId |= shardIdArray[i];
        }
        context->content->fromShard = shardId;
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->destinationLength = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

static void processToShard(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for TO SHARD \n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT32) {
        PRINTF("Invalid length for TO SHARD %d\n",
               context->currentFieldLength);
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
                (context->commandLength <
                 ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        copyTxData(context,
                   (uint8_t *)&context->content->toShard + context->currentFieldPos,
                   copySize);

        //adjust from big endian to little endian
        uint32_t shardId = 0;
        uint8_t *shardIdArray = (uint8_t *) &context->content->toShard;
        for(uint32_t i = 0 ; i < context->currentFieldLength;  i++ ) {
            shardId <<= 8;
            shardId |= shardIdArray[i];
        }
        context->content->toShard = shardId;
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->destinationLength = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

static void processTo(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_TO\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_ADDRESS) {
        PRINTF("Invalid length for RLP_TO\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
                (context->commandLength <
                 ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        copyTxData(context,
                   context->content->destination + context->currentFieldPos,
                   copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->destinationLength = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}


int processTx(txContext_t *context) {
    for (;;) {
        if (context->currentField == TX_RLP_DONE) {
            return 0;
        }

        if (context->commandLength == 0) {
            return 1;
        }

        if (!context->processingField) {
            bool canDecode = false;
            uint32_t offset;
            while (context->commandLength != 0) {
                bool valid;
                // Feed the RLP buffer until the length can be decoded
                if (context->commandLength < 1) {
                    return -1;
                }
                context->rlpBuffer[context->rlpBufferPos++] =
                        readTxByte(context);
                if (rlpCanDecode(context->rlpBuffer, context->rlpBufferPos,
                                 &valid)) {
                    // Can decode now, if valid
                    if (!valid) {
                        return -1;
                    }
                    canDecode = true;
                    break;
                }
                // Cannot decode yet
                // Sanity check
                if (context->rlpBufferPos == sizeof(context->rlpBuffer)) {
                    return -1;
                }
            }
            if (!canDecode) {
                return 1;
            }
            // Ready to process this field
            if (!rlpDecodeLength(context->rlpBuffer, context->rlpBufferPos,
                                 &context->currentFieldLength, &offset,
                                 &context->currentFieldIsList)) {
                return -1;
            }
            if (offset == 0) {
                // Hack for single byte, self encoded
                context->workBuffer--;
                context->commandLength++;
                context->fieldSingleByte = true;
            } else {
                context->fieldSingleByte = false;
            }
            context->currentFieldPos = 0;
            context->rlpBufferPos = 0;
            context->processingField = true;
        }


        switch (context->currentField) {
            case TX_RLP_CONTENT:
                processContent(context);
                break;
            case TX_RLP_NONCE:
                processNonce(context);
                break;
            case TX_RLP_GASPRICE:
                processGasprice(context);
                break;
            case TX_RLP_STARTGAS:
                processStartGas(context);
                break;
            case TX_RLP_FROMSHARD:
                processFromShard(context);
                break;
            case TX_RLP_TOSHARD:
                processToShard(context);
                break;
            case TX_RLP_TO:
                processTo(context);
                break;
            case TX_RLP_AMOUNT:
                processValue(context);
                return 0;
            default:
                return -1;
        }
    }
}