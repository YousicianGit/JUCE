/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2015 - ROLI Ltd.

   Permission is granted to use this software under the terms of either:
   a) the GPL v2 (or any later version)
   b) the Affero GPL v3

   Details of these licenses can be found at: www.gnu.org/licenses

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.juce.com for more information.

  ==============================================================================
*/

AudioFormatReaderSource::AudioFormatReaderSource (AudioFormatReader* const r,
                                                  const bool deleteReaderWhenThisIsDeleted)
    : reader (r, deleteReaderWhenThisIsDeleted),
      nextPlayPos (0),
      looping (false)
{
    jassert (reader != nullptr);
}

AudioFormatReaderSource::~AudioFormatReaderSource() {}

int64 AudioFormatReaderSource::getTotalLength() const                   { return reader->lengthInSamples; }
void AudioFormatReaderSource::setNextReadPosition (int64 newPosition)   { nextPlayPos = newPosition; }
void AudioFormatReaderSource::setLooping (bool shouldLoop)              { looping = shouldLoop; }

int64 AudioFormatReaderSource::getNextReadPosition() const
{
    return looping ? nextPlayPos % reader->lengthInSamples
                   : nextPlayPos;
}

void AudioFormatReaderSource::prepareToPlay (int /*samplesPerBlockExpected*/, double /*sampleRate*/) {}
void AudioFormatReaderSource::releaseResources() {}

void AudioFormatReaderSource::getNextAudioBlock (const AudioSourceChannelInfo& info)
{
    if (info.numSamples > 0)
    {
        if (looping)
        {
            int read = 0;
            while (read < info.numSamples)
            {
                const int64 start = nextPlayPos % reader->lengthInSamples;
                const int numReadable = jmin((int)(reader->lengthInSamples - start), info.numSamples - read);
                
                reader->read (info.buffer, info.startSample + read,
                              numReadable, start, true, true);
                
                read += numReadable;
                nextPlayPos = (start + numReadable) % reader->lengthInSamples;
                
            }
        }
        else
        {
            reader->read (info.buffer, info.startSample,
                          info.numSamples, nextPlayPos, true, true);
            nextPlayPos += info.numSamples;
        }
    }
}
