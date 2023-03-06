package com.rmsl.juce;

import android.media.midi.MidiDevice;

public interface MidiDeviceManager
{
    String[] getJuceAndroidMidiOutputDeviceNameAndIDs();

    String[] getJuceAndroidMidiInputDeviceNameAndIDs();

    JuceMidiSupport.JuceMidiPort openMidiInputPortWithID(int deviceID, long host);

    JuceMidiSupport.JuceMidiPort openMidiOutputPortWithID(int deviceID);

    void injectMidiDevice(MidiDevice theDevice);

    public void detach () throws Throwable;
}
