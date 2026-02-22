#pragma once

#include <lib/subghz/protocols/base.h>

#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include "ws_generic.h"
#include <lib/subghz/blocks/math.h>

#define WS_PROTOCOL_NEXUS_HOMEDICS_NAME "Nexus-HoMedics"

typedef struct WSProtocolDecoderNexus_HoMedics WSProtocolDecoderNexus_HoMedics;
typedef struct WSProtocolEncoderNexus_HoMedics WSProtocolEncoderNexus_HoMedics;

extern const SubGhzProtocolDecoder ws_protocol_nexus_homedics_decoder;
extern const SubGhzProtocolEncoder ws_protocol_nexus_homedics_encoder;
extern const SubGhzProtocol ws_protocol_nexus_homedics;

void* ws_protocol_decoder_nexus_homedics_alloc(SubGhzEnvironment* environment);
void ws_protocol_decoder_nexus_homedics_free(void* context);
void ws_protocol_decoder_nexus_homedics_reset(void* context);
void ws_protocol_decoder_nexus_homedics_feed(void* context, bool level, uint32_t duration);
uint8_t ws_protocol_decoder_nexus_homedics_get_hash_data(void* context);
SubGhzProtocolStatus ws_protocol_decoder_nexus_homedics_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus
    ws_protocol_decoder_nexus_homedics_deserialize(void* context, FlipperFormat* flipper_format);
void ws_protocol_decoder_nexus_homedics_get_string(void* context, FuriString* output);

