
#if JUCE_ANDROID

//==============================================================================
#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD) \
FIELD (bufferIndex,                    "bufferIndex",                    "I") \
FIELD (dataSize,                       "dataSize",                       "I") \
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
        if (!stream) { return; }
        
        JNIEnv * env = getEnv();
        
        String filename = stream->getFile().getFullPathName();
        const LocalRef<jstring> jFilename (javaString (filename));
        decoder = GlobalRef(env->NewObject(AudioDecoder, AudioDecoder.constructor, jFilename.get()));
        jobject error = decoder.callObjectMethod(AudioDecoder.getError);
        if (error)
        {
            // TODO, log error?
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
        decoder.callVoidMethod(AudioDecoder.release);
        decoder.clear();
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
            if (samplesLeftInCurrentResult <= 0)
            {
                if (!decode())
                {
                    // EOF, should we assert something here?
                    break;
                }
            }
            
            int numSamples = std::min(samplesLeftToCopy, samplesLeftInCurrentResult);
            ReadHelper<AudioData::Int32, AudioData::Int16, AudioData::LittleEndian>::read (destSamples, startOffsetInDestBuffer, numDestChannels,
                                                                                           currentData, numChannels, numSamples);
            samplesLeftToCopy -= numSamples;
            samplesLeftInCurrentResult -= numSamples;
            lastReadPosition += numSamples;
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
        
        // TODO take channel count into account
        samplesLeftInCurrentResult = env->GetIntField(readResult, AudioDecoderReadResult.dataSize) / 2;
        jobject byteBuffer = env->GetObjectField(readResult, AudioDecoderReadResult.dataBuffer);
        currentData = static_cast<int16_t *>(env->GetDirectBufferAddress(byteBuffer));
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
