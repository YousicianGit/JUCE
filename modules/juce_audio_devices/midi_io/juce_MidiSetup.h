

#ifndef JUCE_MIDISETUP_H_INCLUDED
#define JUCE_MIDISETUP_H_INCLUDED


//==============================================================================
/**

 */
class JUCE_API  MidiSetupListener
{
public:
    virtual void midiDevicesChanged() = 0;
};

//==============================================================================
/**

 */
class JUCE_API  MidiSetup
{
public:
    static bool supportsMidi ();
    static void addListener (MidiSetupListener * const listener);
    static void removeListener (MidiSetupListener * const listener);
};


#endif /* JUCE_MIDISETUP_H_INCLUDED */
