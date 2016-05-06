
#if JUCE_ANDROID

//==============================================================================
#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD) \
FIELD (bufferIndex,                    "bufferIndex",                    "I") \
FIELD (dataSize,                       "dataSize",                       "I") \
FIELD (dataOffset,                     "dataOffset",                     "I") \
FIELD (presentationTimeInMicroseconds, "presentationTimeInMicroseconds", "J") \
FIELD (dataBuffer,                     "dataBuffer",                     "Ljava/nio/ByteBuffer;")

DECLARE_JNI_CLASS (AudioDecoderReadResult, "com/yousician/yousiciandsp/AudioDecoderReadResult");
#undef JNI_CLASS_MEMBERS

//==============================================================================
#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD) \
METHOD (constructor,             "<init>",                  "(Ljava/lang/String;)V") \
METHOD (getError,                "getError",                "()Ljava/lang/String;") \
METHOD (getDurationMicroseconds, "getDurationMicroseconds", "()J") \
METHOD (getSampleRate,           "getSampleRate",           "()I") \
METHOD (getChannelCount,         "getChannelCount",         "()I") \
METHOD (release,                 "release",                 "()V") \
METHOD (seek,                    "seek",                    "(J)V") \
METHOD (releaseBuffer,           "releaseBuffer",           "(I)V") \
METHOD (decode,                  "decode",                  "()Lcom/yousician/yousiciandsp/AudioDecoderReadResult;")

DECLARE_JNI_CLASS (AudioDecoder, "com/yousician/yousiciandsp/AudioDecoder");
#undef JNI_CLASS_MEMBERS

//==============================================================================
namespace
{
    const char* const androidAudioFormatName = "Android MediaCodec supported file";
    StringRef androidAudioExtensions = ".mp3 .flac .ogg .aac .ogg .wav";
}

//==============================================================================
class AndroidAudioReader : public AudioFormatReader
{
public:
    AndroidAudioReader (InputStream* const inp)
    : AudioFormatReader (inp, androidAudioFormatName),
    ok (false), lastReadPosition (0),
    samplesLeftInCurrentResult(0), currentData(nullptr)
    {
        FileInputStream * stream = dynamic_cast<FileInputStream *>(inp);
        if (!stream)
        {
            DBG("Trying to open file with AndroidAudioFormat, which is not a FileInputStream");
            return;
        }
        
        JNIEnv * env = getEnv();
        
        String filename = stream->getFile().getFullPathName();
        const LocalRef<jstring> jFilename (javaString (filename));
        decoder = GlobalRef(env->NewObject(AudioDecoder, AudioDecoder.constructor, jFilename.get()));
        jstring error = (jstring)decoder.callObjectMethod(AudioDecoder.getError);
        if (error)
        {
            DBG("AndroidAudioFormat returned error from Java: " << juceString(error));
            return;
        }
        
        sampleRate = decoder.callIntMethod(AudioDecoder.getSampleRate);
        long lengthInUs = decoder.callLongMethod(AudioDecoder.getDurationMicroseconds);
        lengthInSamples = lengthInUs * sampleRate / (1000 * 1000);
        numChannels = decoder.callIntMethod(AudioDecoder.getChannelCount);
        
        usesFloatingPointData = false;
        
        ok = true;
    }
    
    ~AndroidAudioReader()
    {
        releaseCurrentBuffer();
        if (decoder.get())
        {
            decoder.callVoidMethod(AudioDecoder.release);
            decoder.clear();
        }
    }
    
    //==============================================================================
    bool readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      int64 startSampleInFile, int numSamples) override
    {
        clearSamplesBeyondAvailableLength (destSamples, numDestChannels, startOffsetInDestBuffer,
                                           startSampleInFile, numSamples, lengthInSamples);
        
        if (numSamples <= 0)
        {
            return true;
        }
        
        JNIEnv * env = getEnv();
        
        if (lastReadPosition != startSampleInFile)
        {
            releaseCurrentBuffer();
            decoder.callVoidMethod(AudioDecoder.seek, static_cast<jlong>((1000.0 * 1000 * startSampleInFile) / sampleRate));
        }
        
        int samplesLeftToCopy = numSamples;
        while (samplesLeftToCopy > 0)
        {
            while (samplesLeftInCurrentResult <= 0)
            {
                if (!decode())
                {
                    // EOF, should we assert something here?
                    return true;
                }
            }
            
            int samplesAvailable = std::min(samplesLeftToCopy, samplesLeftInCurrentResult);
            ReadHelper<AudioData::Int32, AudioData::Int16, AudioData::LittleEndian>::read (destSamples, startOffsetInDestBuffer, numDestChannels,
                                                                                           currentData, numChannels, samplesAvailable);
            samplesLeftToCopy -= samplesAvailable;
            samplesLeftInCurrentResult -= samplesAvailable;
            lastReadPosition += samplesAvailable;
        }
        
        return true;
    }
    
    bool ok;
    
private:
    
    void releaseCurrentBuffer()
    {
        if (!readResult.get()) { return; }
        
        JNIEnv * env = getEnv();
        int bufferIndex = env->GetIntField(readResult, AudioDecoderReadResult.bufferIndex);
        decoder.callVoidMethod(AudioDecoder.releaseBuffer, (jint) bufferIndex);
        readResult.clear();
        samplesLeftInCurrentResult = 0;
        currentData = nullptr;
    }
    
    bool decode()
    {
        JNIEnv * env = getEnv();
        
        releaseCurrentBuffer();
        readResult = GlobalRef(decoder.callObjectMethod(AudioDecoder.decode));
        if (!readResult)
        {
            // EOF
            return false;
        }
        
        // Data is 16-bit integers
        int bytesPerSample = 2 * numChannels;
        samplesLeftInCurrentResult = env->GetIntField(readResult, AudioDecoderReadResult.dataSize) / bytesPerSample;
        
        if (samplesLeftInCurrentResult == 0) { return true; }

        long positionInUs = env->GetLongField(readResult, AudioDecoderReadResult.presentationTimeInMicroseconds);
        lastReadPosition = positionInUs * sampleRate / (1000 * 1000);
        
        jobject byteBuffer = env->GetObjectField(readResult, AudioDecoderReadResult.dataBuffer);
        jint dataOffset = env->GetIntField(readResult, AudioDecoderReadResult.dataOffset);
        currentData = reinterpret_cast<int16_t *>(static_cast<char *>(env->GetDirectBufferAddress(byteBuffer)) + dataOffset);
        
        return true;
    }
    
private:
    
    GlobalRef decoder;
    GlobalRef readResult;
    int       samplesLeftInCurrentResult;
    
    int64 lastReadPosition;
    int16_t const * currentData;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AndroidAudioReader)
};

//==============================================================================
AndroidAudioFormat::AndroidAudioFormat()
: AudioFormat (androidAudioFormatName, androidAudioExtensions)
{
}

AndroidAudioFormat::~AndroidAudioFormat() {}

Array<int> AndroidAudioFormat::getPossibleSampleRates()    { return Array<int>(); }
Array<int> AndroidAudioFormat::getPossibleBitDepths()      { return Array<int>(); }

bool AndroidAudioFormat::canDoStereo()     { return true; }
bool AndroidAudioFormat::canDoMono()       { return true; }

//==============================================================================
AudioFormatReader* AndroidAudioFormat::createReaderFor (InputStream* sourceStream,
                                                     bool deleteStreamIfOpeningFails)
{
    ScopedPointer<AndroidAudioReader> r (new AndroidAudioReader (sourceStream));
    
    if (r->ok)
        return r.release();
    
    if (! deleteStreamIfOpeningFails)
        r->input = nullptr;
    
    return nullptr;
}

AudioFormatWriter* AndroidAudioFormat::createWriterFor (OutputStream*,
                                                     double /*sampleRateToUse*/,
                                                     unsigned int /*numberOfChannels*/,
                                                     int /*bitsPerSample*/,
                                                     const StringPairArray& /*metadataValues*/,
                                                     int /*qualityOptionIndex*/)
{
    jassertfalse; // not yet implemented!
    return nullptr;
}

#endif
