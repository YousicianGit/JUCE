#pragma once

namespace juce
{
class JUCE_API MidiSetupListener
{
public:
    virtual ~MidiSetupListener() { }

    virtual void midiDevicesChanged() = 0;
};

class JUCE_API MidiSetup
{
public:
    static bool supportsMidi();
    static void addListener(MidiSetupListener* const listener);
    static void removeListener(MidiSetupListener* const listener);
};
}
