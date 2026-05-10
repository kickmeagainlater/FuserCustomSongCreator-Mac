#include "flac_to_ogg.h"

#include <FLAC/stream_decoder.h>
#include <vorbis/vorbisenc.h>

#include <cstring>
#include <cstdio>

namespace {

struct EncCtx {
    bool initialized = false;
    bool ok = true;
    std::string err;

    ogg_stream_state os;
    vorbis_info vi;
    vorbis_comment vc;
    vorbis_dsp_state vd;
    vorbis_block vb;

    std::vector<uint8_t>* out = nullptr;

    void writePage(const ogg_page& og) {
        out->insert(out->end(), og.header, og.header + og.header_len);
        out->insert(out->end(), og.body,   og.body   + og.body_len);
    }

    bool init(unsigned channels, unsigned sampleRate, float quality) {
        vorbis_info_init(&vi);
        if (vorbis_encode_init_vbr(&vi, channels, sampleRate, quality) != 0) {
            err = "vorbis_encode_init_vbr failed";
            return false;
        }
        vorbis_comment_init(&vc);
        vorbis_comment_add_tag(&vc, "ENCODER", "FuserCustomSongCreator");
        vorbis_analysis_init(&vd, &vi);
        vorbis_block_init(&vd, &vb);

        srand(0x12345678);
        ogg_stream_init(&os, 0xC0FFEE);

        ogg_packet header, header_comm, header_code;
        vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
        ogg_stream_packetin(&os, &header);
        ogg_stream_packetin(&os, &header_comm);
        ogg_stream_packetin(&os, &header_code);

        ogg_page og;
        while (ogg_stream_flush(&os, &og)) writePage(og);

        initialized = true;
        return true;
    }

    void flushBlocks(bool drain) {
        ogg_packet op;
        while (vorbis_analysis_blockout(&vd, &vb) == 1) {
            vorbis_analysis(&vb, nullptr);
            vorbis_bitrate_addblock(&vb);
            while (vorbis_bitrate_flushpacket(&vd, &op)) {
                ogg_stream_packetin(&os, &op);
                ogg_page og;
                while (true) {
                    int r = drain ? ogg_stream_flush(&os, &og) : ogg_stream_pageout(&os, &og);
                    if (r == 0) break;
                    writePage(og);
                    if (ogg_page_eos(&og)) break;
                }
            }
        }
    }

    void finish() {
        if (!initialized) return;
        vorbis_analysis_wrote(&vd, 0);
        flushBlocks(true);
    }

    ~EncCtx() {
        if (initialized) {
            ogg_stream_clear(&os);
            vorbis_block_clear(&vb);
            vorbis_dsp_clear(&vd);
            vorbis_comment_clear(&vc);
            vorbis_info_clear(&vi);
        }
    }
};

FLAC__StreamDecoderWriteStatus flacWriteCb(const FLAC__StreamDecoder*,
                                           const FLAC__Frame* frame,
                                           const FLAC__int32* const buffer[],
                                           void* client_data) {
    auto* ctx = static_cast<EncCtx*>(client_data);
    if (!ctx->ok) return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

    unsigned channels = frame->header.channels;
    unsigned bps      = frame->header.bits_per_sample;
    unsigned sr       = frame->header.sample_rate;
    unsigned blockSz  = frame->header.blocksize;

    if (!ctx->initialized) {
        if (!ctx->init(channels, sr, 0.5f)) {
            ctx->ok = false;
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }
    }

    float scale = 1.0f / static_cast<float>(1u << (bps - 1));
    float** out = vorbis_analysis_buffer(&ctx->vd, blockSz);
    for (unsigned ch = 0; ch < channels; ++ch) {
        const FLAC__int32* src = buffer[ch];
        float* dst = out[ch];
        for (unsigned i = 0; i < blockSz; ++i) dst[i] = src[i] * scale;
    }
    vorbis_analysis_wrote(&ctx->vd, blockSz);
    ctx->flushBlocks(false);

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void flacErrorCb(const FLAC__StreamDecoder*, FLAC__StreamDecoderErrorStatus status, void* client_data) {
    auto* ctx = static_cast<EncCtx*>(client_data);
    ctx->ok = false;
    ctx->err = std::string("FLAC decode error: ") + FLAC__StreamDecoderErrorStatusString[status];
}

void flacMetadataCb(const FLAC__StreamDecoder*, const FLAC__StreamMetadata*, void*) {}

} // namespace

bool convertFlacToOggVorbis(const std::string& flacPath,
                            std::vector<uint8_t>& outOggData,
                            std::string& err) {
    EncCtx ctx;
    ctx.out = &outOggData;
    outOggData.clear();

    FLAC__StreamDecoder* dec = FLAC__stream_decoder_new();
    if (!dec) { err = "FLAC__stream_decoder_new failed"; return false; }

    FLAC__stream_decoder_set_md5_checking(dec, false);

    auto initStatus = FLAC__stream_decoder_init_file(
        dec, flacPath.c_str(), flacWriteCb, flacMetadataCb, flacErrorCb, &ctx);
    if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        err = std::string("FLAC init failed: ") + FLAC__StreamDecoderInitStatusString[initStatus];
        FLAC__stream_decoder_delete(dec);
        return false;
    }

    bool decodeOk = FLAC__stream_decoder_process_until_end_of_stream(dec);
    FLAC__stream_decoder_finish(dec);
    FLAC__stream_decoder_delete(dec);

    if (!decodeOk || !ctx.ok) {
        if (err.empty()) err = ctx.err.empty() ? "FLAC decoding failed" : ctx.err;
        return false;
    }
    if (!ctx.initialized) {
        err = "FLAC file produced no audio frames";
        return false;
    }

    ctx.finish();
    return true;
}
