#include "nexus_homedics.h"

#define TAG "WSProtocolNexus_HoMedics"

/*
 * HoMedics 433 MHz sensor variant seen with Nexus-like framing.
 *
 * Captured timing characteristics:
 * - short mark around 430-480 us
 * - bit gap around 2000 us for one symbol
 * - bit gap around 4000 us for the other symbol
 * - frame sync gap around 8950 us
 * - payload is 36 bits, repeated
 */

static const SubGhzBlockConst ws_protocol_nexus_homedics_const = {
    .te_short = 500,
    .te_long = 4000,
    .te_delta = 250,
    .min_count_bit_for_found = 36,
};

struct WSProtocolDecoderNexus_HoMedics {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    WSBlockGeneric generic;
};

struct WSProtocolEncoderNexus_HoMedics {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    WSBlockGeneric generic;
};

typedef enum {
    Nexus_HoMedicsDecoderStepReset = 0,
    Nexus_HoMedicsDecoderStepSaveDuration,
    Nexus_HoMedicsDecoderStepCheckDuration,
} Nexus_HoMedicsDecoderStep;

const SubGhzProtocolDecoder ws_protocol_nexus_homedics_decoder = {
    .alloc = ws_protocol_decoder_nexus_homedics_alloc,
    .free = ws_protocol_decoder_nexus_homedics_free,

    .feed = ws_protocol_decoder_nexus_homedics_feed,
    .reset = ws_protocol_decoder_nexus_homedics_reset,

    .get_hash_data = ws_protocol_decoder_nexus_homedics_get_hash_data,
    .serialize = ws_protocol_decoder_nexus_homedics_serialize,
    .deserialize = ws_protocol_decoder_nexus_homedics_deserialize,
    .get_string = ws_protocol_decoder_nexus_homedics_get_string,
};

const SubGhzProtocolEncoder ws_protocol_nexus_homedics_encoder = {
    .alloc = NULL,
    .free = NULL,

    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol ws_protocol_nexus_homedics = {
    .name = WS_PROTOCOL_NEXUS_HOMEDICS_NAME,
    .type = SubGhzProtocolWeatherStation,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_315 | SubGhzProtocolFlag_868 |
            SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable,

    .decoder = &ws_protocol_nexus_homedics_decoder,
    .encoder = &ws_protocol_nexus_homedics_encoder,
};

void* ws_protocol_decoder_nexus_homedics_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    WSProtocolDecoderNexus_HoMedics* instance = malloc(sizeof(WSProtocolDecoderNexus_HoMedics));
    instance->base.protocol = &ws_protocol_nexus_homedics;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void ws_protocol_decoder_nexus_homedics_free(void* context) {
    furi_assert(context);
    WSProtocolDecoderNexus_HoMedics* instance = context;
    free(instance);
}

void ws_protocol_decoder_nexus_homedics_reset(void* context) {
    furi_assert(context);
    WSProtocolDecoderNexus_HoMedics* instance = context;
    instance->decoder.parser_step = Nexus_HoMedicsDecoderStepReset;
}

static bool ws_protocol_nexus_homedics_check(WSProtocolDecoderNexus_HoMedics* instance) {
    if(instance->decoder.decode_count_bit != ws_protocol_nexus_homedics_const.min_count_bit_for_found) {
        return false;
    }

    uint64_t mask36 = 0xFFFFFFFFFULL;
    uint64_t payload = instance->decoder.decode_data & mask36;

    // Minimal noise rejection: don't accept all-zero/all-one payloads.
    if((payload == 0) || (payload == mask36)) {
        return false;
    }

    return true;
}

/**
 * Best-effort Nexus-compatible field extraction for HoMedics payload.
 * Humidity may be invalid for temperature-only variants.
 */
static void ws_protocol_nexus_homedics_remote_controller(WSBlockGeneric* instance) {
    instance->id = (instance->data >> 28) & 0xFF;
    instance->battery_low = !((instance->data >> 27) & 1);
    instance->channel = ((instance->data >> 24) & 0x03) + 1;
    instance->btn = WS_NO_BTN;

    if(!((instance->data >> 23) & 1)) {
        instance->temp = (float)((instance->data >> 12) & 0x07FF) / 10.0f;
    } else {
        instance->temp = (float)((~(instance->data >> 12) & 0x07FF) + 1) / -10.0f;
    }

    instance->humidity = instance->data & 0xFF;
    if(instance->humidity > 95)
        instance->humidity = 95;
    else if(instance->humidity < 20)
        instance->humidity = 20;
}

void ws_protocol_decoder_nexus_homedics_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    WSProtocolDecoderNexus_HoMedics* instance = context;

    switch(instance->decoder.parser_step) {
    case Nexus_HoMedicsDecoderStepReset:
        if((!level) && (DURATION_DIFF(duration, ws_protocol_nexus_homedics_const.te_short * 18) <
                        ws_protocol_nexus_homedics_const.te_delta * 8)) {
            // Found frame sync around 9000 us
            instance->decoder.parser_step = Nexus_HoMedicsDecoderStepSaveDuration;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
        }
        break;

    case Nexus_HoMedicsDecoderStepSaveDuration:
        if(level) {
            instance->decoder.te_last = duration;
            instance->decoder.parser_step = Nexus_HoMedicsDecoderStepCheckDuration;
        } else {
            instance->decoder.parser_step = Nexus_HoMedicsDecoderStepReset;
        }
        break;

    case Nexus_HoMedicsDecoderStepCheckDuration:
        if(!level) {
            if(DURATION_DIFF(duration, ws_protocol_nexus_homedics_const.te_short * 18) <
               ws_protocol_nexus_homedics_const.te_delta * 8) {
                // Found next frame sync
                instance->decoder.parser_step = Nexus_HoMedicsDecoderStepReset;
                if(ws_protocol_nexus_homedics_check(instance)) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                    ws_protocol_nexus_homedics_remote_controller(&instance->generic);
                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                    instance->decoder.parser_step = Nexus_HoMedicsDecoderStepCheckDuration;
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;

                break;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, ws_protocol_nexus_homedics_const.te_short) <
                 ws_protocol_nexus_homedics_const.te_delta) &&
                (DURATION_DIFF(duration, ws_protocol_nexus_homedics_const.te_short * 4) <
                 ws_protocol_nexus_homedics_const.te_delta * 4)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = Nexus_HoMedicsDecoderStepSaveDuration;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, ws_protocol_nexus_homedics_const.te_short) <
                 ws_protocol_nexus_homedics_const.te_delta) &&
                (DURATION_DIFF(duration, ws_protocol_nexus_homedics_const.te_short * 8) <
                 ws_protocol_nexus_homedics_const.te_delta * 8)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = Nexus_HoMedicsDecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = Nexus_HoMedicsDecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = Nexus_HoMedicsDecoderStepReset;
        }
        break;
    }
}

uint8_t ws_protocol_decoder_nexus_homedics_get_hash_data(void* context) {
    furi_assert(context);
    WSProtocolDecoderNexus_HoMedics* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus ws_protocol_decoder_nexus_homedics_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    WSProtocolDecoderNexus_HoMedics* instance = context;
    return ws_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus ws_protocol_decoder_nexus_homedics_deserialize(
    void* context,
    FlipperFormat* flipper_format) {
    furi_assert(context);
    WSProtocolDecoderNexus_HoMedics* instance = context;
    return ws_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, ws_protocol_nexus_homedics_const.min_count_bit_for_found);
}

void ws_protocol_decoder_nexus_homedics_get_string(void* context, FuriString* output) {
    furi_assert(context);
    WSProtocolDecoderNexus_HoMedics* instance = context;
    furi_string_printf(
        output,
        "%s %dbit\r\n"
        "Key:0x%lX%08lX\r\n"
        "Sn:0x%lX Ch:%d  Bat:%d\r\n"
        "Temp:%3.1f C Hum:%d%%",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        (uint32_t)(instance->generic.data >> 32),
        (uint32_t)(instance->generic.data),
        instance->generic.id,
        instance->generic.channel,
        instance->generic.battery_low,
        (double)instance->generic.temp,
        instance->generic.humidity);
}
