//
// audio_ios.mm — iOS audio session activation.
//
// iOS requires an AVAudioSession to be active before OpenAL can produce
// sound. Without this, alcOpenDevice may succeed but playback is silent
// (or the device fails to open entirely on some iOS versions).
//

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

namespace platform {

void ios_audio_init()
{
    NSError *error = nil;
    AVAudioSession *session = [AVAudioSession sharedInstance];

    //Ambient: mixes with other audio, no recording, obeys silent switch.
    //Use Playback category to ensure sound plays even in silent mode.
    [session setCategory:AVAudioSessionCategoryPlayback
              withOptions:AVAudioSessionCategoryOptionMixWithOthers
                    error:&error];

    [session setActive:YES error:&error];

    if (error)
        fprintf(stderr, "OH_AUDIO: AVAudioSession error: %s\n",
                [[error localizedDescription] UTF8String]);
    else
        fprintf(stderr, "OH_AUDIO: AVAudioSession activated (Playback+MixWithOthers)\n");
}

} // namespace platform
