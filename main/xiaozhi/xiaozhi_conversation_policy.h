// 定义小智对话退出与说话期间唤醒打断的纯判定规则。
#pragma once

#include <stdint.h>
#include <string.h>

// 对话模式使用独立的 VOIP AEC 管线。WakeNet 只在待唤醒管线中运行，
// 所以 TTS 期间的打断由服务器对设备侧 AEC 输出执行 VAD，而不是 WakeNet。
inline constexpr bool kXiaozhiWakeInterruptDuringTtsEnabled = false;
inline constexpr uint32_t kXiaozhiWakeInterruptArmDelayMs = 1000;
inline constexpr uint32_t kXiaozhiEmptyReplyContinuationMs = 12000;
inline constexpr uint32_t kXiaozhiTtsFinalFrameGraceMs = 700;
inline constexpr uint32_t kXiaozhiTtsPlaybackTailSettleMs = 120;
static_assert(kXiaozhiTtsFinalFrameGraceMs > kXiaozhiTtsPlaybackTailSettleMs,
              "TTS final-frame grace must exceed playback tail settling time");

constexpr bool xiaozhi_wake_interrupt_allowed(bool server_speaking,
                                              bool tts_stop_pending,
                                              bool tts_start_known,
                                              uint32_t speaking_elapsed_ms)
{
    return kXiaozhiWakeInterruptDuringTtsEnabled &&
           server_speaking &&
           !tts_stop_pending &&
           tts_start_known &&
           speaking_elapsed_ms >= kXiaozhiWakeInterruptArmDelayMs;
}

constexpr bool xiaozhi_microphone_uplink_allowed(bool server_speaking,
                                                 bool tts_stop_pending)
{
    (void)server_speaking;
    (void)tts_stop_pending;
    // 仅在实时对话循环中调用。保持 AEC 输出连续上行，服务器才能在
    // 扬声器播放期间检测近场人声并发送 tts/stop 完成自然打断。
    return true;
}

constexpr bool xiaozhi_turn_reply_is_empty(bool user_text_received,
                                           bool assistant_text_received,
                                           bool assistant_audio_received)
{
    return user_text_received &&
           !assistant_text_received &&
           !assistant_audio_received;
}

constexpr bool xiaozhi_tts_final_frames_settled(uint32_t now_tick,
                                                uint32_t stop_received_tick,
                                                uint32_t last_audio_tick,
                                                uint32_t final_frame_grace_ticks,
                                                uint32_t playback_tail_settle_ticks)
{
    if (stop_received_tick != 0 &&
        now_tick - stop_received_tick < final_frame_grace_ticks) {
        return false;
    }
    return last_audio_tick == 0 ||
           now_tick - last_audio_tick >= playback_tail_settle_ticks;
}

inline bool xiaozhi_user_requested_exit(const char *text)
{
    if (!text || text[0] == '\0') {
        return false;
    }
    constexpr const char *kExplicitExitPhrases[] = {
        "关闭小智",
        "停止小智",
        "退出小智",
        "结束小智",
        "关闭对话",
        "停止对话",
        "退出对话",
        "结束对话",
    };
    for (const char *phrase : kExplicitExitPhrases) {
        if (strstr(text, phrase)) {
            return true;
        }
    }
    constexpr const char *kStandaloneExitCommands[] = {
        "关闭", "关闭。", "关闭！", "关闭？",
        "停止", "停止。", "停止！", "停止？",
        "退出", "退出。", "退出！", "退出？",
        "结束", "结束。", "结束！", "结束？",
        "退下", "退下。", "退下！", "退下？",
        "退下吧", "退下吧。", "退下吧！", "退下吧？",
        "你退下吧", "你退下吧。", "你退下吧！", "你退下吧？",
    };
    // 单独的结束动词代表退出；带“闹钟”等宾语的命令继续交给 MCP。
    for (const char *command : kStandaloneExitCommands) {
        if (strcmp(text, command) == 0) {
            return true;
        }
    }
    return false;
}
