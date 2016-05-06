#if JUCE_ANDROID

//==============================================================================
/**
 Android only - This uses the MediaCodec class to read any audio
 format that the system has a codec for.
 
 This should be able to understand formats such as mp3, m4a, etc.
 
 @see AudioFormat
 */
class JUCE_API  AndroidAudioFormat     : public AudioFormat
{
public:
    //==============================================================================
    /** Creates a format object. */
    AndroidAudioFormat();
    
    /** Destructor. */
    ~AndroidAudioFormat();
    
    //==============================================================================
    Array<int> getPossibleSampleRates() override;
    Array<int> getPossibleBitDepths() override;
    bool canDoStereo() override;
    bool canDoMono() override;
    
    //==============================================================================
    AudioFormatReader* createReaderFor (InputStream*,
                                        bool deleteStreamIfOpeningFails) override;
    
    AudioFormatWriter* createWriterFor (OutputStream*,
                                        double sampleRateToUse,
                                        unsigned int numberOfChannels,
                                        int bitsPerSample,
                                        const StringPairArray& metadataValues,
                                        int qualityOptionIndex) override;
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AndroidAudioFormat)
};

#endif
