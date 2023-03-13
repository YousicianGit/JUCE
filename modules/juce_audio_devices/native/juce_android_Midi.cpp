/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2022 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

namespace juce
{
#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
 STATICMETHOD (getAndroidMidiDeviceManager, "getAndroidMidiDeviceManager", "(Landroid/content/Context;)Lcom/rmsl/juce/MidiDeviceManager;") \

DECLARE_JNI_CLASS (JuceMidiSupport, "com/rmsl/juce/JuceMidiSupport")
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
 METHOD (getJuceAndroidMidiInputDeviceNameAndIDs,  "getJuceAndroidMidiInputDeviceNameAndIDs",  "()[Ljava/lang/String;") \
 METHOD (getJuceAndroidMidiOutputDeviceNameAndIDs, "getJuceAndroidMidiOutputDeviceNameAndIDs", "()[Ljava/lang/String;") \
 METHOD (openMidiInputPortWithID,                  "openMidiInputPortWithID",                  "(IJ)Lcom/rmsl/juce/JuceMidiSupport$JuceMidiPort;") \
 METHOD (openMidiOutputPortWithID,                 "openMidiOutputPortWithID",                 "(I)Lcom/rmsl/juce/JuceMidiSupport$JuceMidiPort;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (MidiDeviceManager, "com/rmsl/juce/MidiDeviceManager", 23)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
 METHOD (start,    "start",    "()V") \
 METHOD (stop,     "stop",     "()V") \
 METHOD (close,    "close",    "()V") \
 METHOD (sendMidi, "sendMidi", "([BII)V") \
 METHOD (getName,  "getName",  "()Ljava/lang/String;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (JuceMidiPort, "com/rmsl/juce/JuceMidiSupport$JuceMidiPort", 23)
#undef JNI_CLASS_MEMBERS

//==============================================================================
class MidiInput::Pimpl
{
public:
    Pimpl (MidiInput* midiInput, int deviceID, juce::MidiInputCallback* midiInputCallback, jobject deviceManager)
        : juceMidiInput (midiInput), callback (midiInputCallback), midiConcatenator (2048),
          javaMidiDevice (LocalRef<jobject>(getEnv()->CallObjectMethod (deviceManager, MidiDeviceManager.openMidiInputPortWithID,
                                                                        (jint) deviceID, (jlong) this)))
    {
    }

    ~Pimpl()
    {
        if (jobject d = javaMidiDevice.get())
        {
            getEnv()->CallVoidMethod (d, JuceMidiPort.close);
            javaMidiDevice.clear();
        }
    }

    bool isOpen() const noexcept
    {
        return javaMidiDevice != nullptr;
    }

    void start()
    {
        if (jobject d = javaMidiDevice.get())
            getEnv()->CallVoidMethod (d, JuceMidiPort.start);
    }

    void stop()
    {
        if (jobject d = javaMidiDevice.get())
            getEnv()->CallVoidMethod (d, JuceMidiPort.stop);

        callback = nullptr;
    }

    String getName() const noexcept
    {
        if (jobject d = javaMidiDevice.get())
            return juceString (LocalRef<jstring> ((jstring) getEnv()->CallObjectMethod (d, JuceMidiPort.getName)));

        return {};
    }

    void handleMidi (jbyteArray byteArray, jlong offset, jint len, jlong timestamp)
    {
        auto* env = getEnv();

        jassert (byteArray != nullptr);
        auto* data = env->GetByteArrayElements (byteArray, nullptr);

        HeapBlock<uint8> buffer (static_cast<size_t> (len));
        std::memcpy (buffer.get(), data + offset, static_cast<size_t> (len));

        midiConcatenator.pushMidiData (buffer.get(),
                                       len, static_cast<double> (timestamp) * 1.0e-9,
                                       juceMidiInput, *callback);

        env->ReleaseByteArrayElements (byteArray, data, 0);
    }

private:
    MidiInput* juceMidiInput;
    MidiInputCallback* callback;
    MidiDataConcatenator midiConcatenator;
    GlobalRef javaMidiDevice;
};

extern "C" JNIEXPORT void Java_com_rmsl_juce_JuceMidiSupport_00024JuceMidiInputPort_handleReceive(
    JNIEnv*, jobject, jlong host, jbyteArray byteArray, jint offset, jint len, jlong timestamp)
{
    auto* myself = reinterpret_cast<MidiInput::Pimpl*> (host);

    myself->handleMidi (byteArray, offset, len, timestamp);
}

//==============================================================================
class MidiOutput::Pimpl
{
public:
    Pimpl (const LocalRef<jobject>& midiDevice)
        : javaMidiDevice (midiDevice)
    {
    }

    ~Pimpl()
    {
        if (jobject d = javaMidiDevice.get())
        {
            getEnv()->CallVoidMethod (d, JuceMidiPort.close);
            javaMidiDevice.clear();
        }
    }

    void send (jbyteArray byteArray, jint offset, jint len)
    {
        if (jobject d = javaMidiDevice.get())
            getEnv()->CallVoidMethod (d,
                                      JuceMidiPort.sendMidi,
                                      byteArray, offset, len);
    }

    String getName() const noexcept
    {
        if (jobject d = javaMidiDevice.get())
            return juceString (LocalRef<jstring> ((jstring) getEnv()->CallObjectMethod (d, JuceMidiPort.getName)));

        return {};
    }

private:
    GlobalRef javaMidiDevice;
};

//==============================================================================
class AndroidMidiDeviceManager
{
public:
    AndroidMidiDeviceManager()
        : deviceManager (LocalRef<jobject>(getEnv()->CallStaticObjectMethod (JuceMidiSupport,
                                                                             JuceMidiSupport.getAndroidMidiDeviceManager,
                                                                             getAppContext().get())))
    {
    }

    Array<MidiDeviceInfo> getDevices (bool input)
    {
        if (jobject dm = deviceManager.get())
        {
            jobjectArray jDeviceNameAndIDs
                = (jobjectArray) getEnv()->CallObjectMethod (dm, input ? MidiDeviceManager.getJuceAndroidMidiInputDeviceNameAndIDs
                                                                       : MidiDeviceManager.getJuceAndroidMidiOutputDeviceNameAndIDs);

            // Create a local reference as converting this to a JUCE string will call into JNI
            LocalRef<jobjectArray> localDeviceNameAndIDs (jDeviceNameAndIDs);

            auto deviceNameAndIDs = javaStringArrayToJuce (localDeviceNameAndIDs);
            deviceNameAndIDs.appendNumbersToDuplicates (false, false, CharPointer_UTF8 ("-"), CharPointer_UTF8 (""));

            Array<MidiDeviceInfo> devices;

            for (int i = 0; i < deviceNameAndIDs.size(); i += 2)
                devices.add ({ deviceNameAndIDs[i], deviceNameAndIDs[i + 1] });

            return devices;
        }

        return {};
    }

    MidiInput::Pimpl* openMidiInputPortWithID (int deviceID, MidiInput* juceMidiInput, juce::MidiInputCallback* callback)
    {
        if (auto dm = deviceManager.get())
        {
            auto androidMidiInput = std::make_unique<MidiInput::Pimpl> (juceMidiInput, deviceID, callback, dm);

            if (androidMidiInput->isOpen())
                return androidMidiInput.release();
        }

        return nullptr;
    }

    MidiOutput::Pimpl* openMidiOutputPortWithID (int deviceID)
    {
        if (auto dm = deviceManager.get())
            if (auto javaMidiPort = getEnv()->CallObjectMethod (dm, MidiDeviceManager.openMidiOutputPortWithID, (jint) deviceID))
                return new MidiOutput::Pimpl (LocalRef<jobject>(javaMidiPort));

        return nullptr;
    }

private:
    GlobalRef deviceManager;
};

//==============================================================================
Array<MidiDeviceInfo> MidiInput::getAvailableDevices()
{
    if (getAndroidSDKVersion() < 23)
        return {};

    AndroidMidiDeviceManager manager;
    return manager.getDevices (true);
}

MidiDeviceInfo MidiInput::getDefaultDevice()
{
    if (getAndroidSDKVersion() < 23)
        return {};

    return getAvailableDevices().getFirst();
}

std::unique_ptr<MidiInput> MidiInput::openDevice (const String& deviceIdentifier, MidiInputCallback* callback)
{
    if (getAndroidSDKVersion() < 23 || deviceIdentifier.isEmpty())
        return {};

    AndroidMidiDeviceManager manager;

    std::unique_ptr<MidiInput> midiInput (new MidiInput ({}, deviceIdentifier));

    if (auto* port = manager.openMidiInputPortWithID (deviceIdentifier.getIntValue(), midiInput.get(), callback))
    {
        midiInput->internal.reset (port);
        midiInput->setName (port->getName());

        return midiInput;
    }

    return {};
}

StringArray MidiInput::getDevices()
{
    if (getAndroidSDKVersion() < 23)
        return {};

    StringArray deviceNames;

    for (auto& d : getAvailableDevices())
        deviceNames.add (d.name);

    return deviceNames;
}

int MidiInput::getDefaultDeviceIndex()
{
    return (getAndroidSDKVersion() < 23 ? -1 : 0);
}

std::unique_ptr<MidiInput> MidiInput::openDevice (int index, MidiInputCallback* callback)
{
    return openDevice (getAvailableDevices()[index].identifier, callback);
}

MidiInput::MidiInput (const String& deviceName, const String& deviceIdentifier)
    : deviceInfo (deviceName, deviceIdentifier)
{
}

MidiInput::~MidiInput() = default;

void MidiInput::start()
{
    if (auto* mi = internal.get())
        mi->start();
}

void MidiInput::stop()
{
    if (auto* mi = internal.get())
        mi->stop();
}

//==============================================================================
Array<MidiDeviceInfo> MidiOutput::getAvailableDevices()
{
    if (getAndroidSDKVersion() < 23)
        return {};

    AndroidMidiDeviceManager manager;
    return manager.getDevices (false);
}

MidiDeviceInfo MidiOutput::getDefaultDevice()
{
    if (getAndroidSDKVersion() < 23)
        return {};

    return getAvailableDevices().getFirst();
}

std::unique_ptr<MidiOutput> MidiOutput::openDevice (const String& deviceIdentifier)
{
    if (getAndroidSDKVersion() < 23 || deviceIdentifier.isEmpty())
        return {};

    AndroidMidiDeviceManager manager;

    if (auto* port = manager.openMidiOutputPortWithID (deviceIdentifier.getIntValue()))
    {
        std::unique_ptr<MidiOutput> midiOutput (new MidiOutput ({}, deviceIdentifier));
        midiOutput->internal.reset (port);
        midiOutput->setName (port->getName());

        return midiOutput;
    }

    return {};
}

StringArray MidiOutput::getDevices()
{
    if (getAndroidSDKVersion() < 23)
        return {};

    StringArray deviceNames;

    for (auto& d : getAvailableDevices())
        deviceNames.add (d.name);

    return deviceNames;
}

int MidiOutput::getDefaultDeviceIndex()
{
    return (getAndroidSDKVersion() < 23 ? -1 : 0);
}

std::unique_ptr<MidiOutput> MidiOutput::openDevice (int index)
{
    return openDevice (getAvailableDevices()[index].identifier);
}

MidiOutput::~MidiOutput()
{
    stopBackgroundThread();
}

void MidiOutput::sendMessageNow (const MidiMessage& message)
{
    if (auto* androidMidi = internal.get())
    {
        auto* env = getEnv();
        auto messageSize = message.getRawDataSize();

        LocalRef<jbyteArray> messageContent (env->NewByteArray (messageSize));
        auto content = messageContent.get();

        auto* rawBytes = env->GetByteArrayElements (content, nullptr);
        std::memcpy (rawBytes, message.getRawData(), static_cast<size_t> (messageSize));
        env->ReleaseByteArrayElements (content, rawBytes, 0);

        androidMidi->send (content, (jint) 0, (jint) messageSize);
    }
}

} // namespace juce
