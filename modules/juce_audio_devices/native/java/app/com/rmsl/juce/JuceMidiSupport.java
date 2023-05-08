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

package com.rmsl.juce;


import android.content.Context;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiDeviceStatus;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.util.Log;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;

import static android.content.Context.MIDI_SERVICE;

import com.yousician.yousiciannative.MidiSupport;
import com.yousician.yousiciannative.MidiUsbToJuce;

public class JuceMidiSupport
{
    // Yousician patch: All Bluetooth awareness by JUCE has been removed to allow selecting which
    // keyboard is paired and to allow the app to work with keyboards paired using other apps.

    //==============================================================================
    public interface JuceMidiPort
    {
        boolean isInputPort ();

        // start, stop does nothing on an output port
        void start ();

        void stop ();

        void close ();

        // send will do nothing on an input port
        void sendMidi (byte[] msg, int offset, int count);

        String getName ();
    }

    //==============================================================================
    public static class JuceMidiInputPort extends MidiReceiver implements JuceMidiPort
    {
        public static native void handleReceive (long host, byte[] msg, int offset, int count, long timestamp);

        public JuceMidiInputPort (JuceMidiDeviceManager mm, MidiOutputPort actualPort, MidiPortPath portPathToUse, long hostToUse)
        {
            owner = mm;
            androidPort = actualPort;
            portPath = portPathToUse;
            juceHost = hostToUse;
            isConnected = false;
        }

        @Override
        protected void finalize () throws Throwable
        {
            close ();
            super.finalize ();
        }

        @Override
        public boolean isInputPort ()
        {
            return true;
        }

        @Override
        public void start ()
        {
            if (owner != null && androidPort != null && !isConnected)
            {
                androidPort.connect (this);
                isConnected = true;
            }
        }

        @Override
        public void stop ()
        {
            if (owner != null && androidPort != null && isConnected)
            {
                androidPort.disconnect (this);
                isConnected = false;
            }
        }

        @Override
        public void close ()
        {
            if (androidPort != null)
            {
                try
                {
                    androidPort.close ();
                } catch (IOException exception)
                {
                    Log.d ("JUCE", "IO Exception while closing port");
                }
            }

            if (owner != null)
                owner.removePort (portPath);

            owner = null;
            androidPort = null;
        }

        @Override
        public void onSend (byte[] msg, int offset, int count, long timestamp)
        {
            if (count > 0)
                handleReceive (juceHost, msg, offset, count, timestamp);
        }

        @Override
        public void onFlush ()
        {}

        @Override
        public void sendMidi (byte[] msg, int offset, int count)
        {
        }

        @Override
        public String getName ()
        {
            return owner.getPortName (portPath);
        }

        JuceMidiDeviceManager owner;
        MidiOutputPort androidPort;
        MidiPortPath portPath;
        long juceHost;
        boolean isConnected;
    }

    public static class JuceMidiOutputPort implements JuceMidiPort
    {
        public JuceMidiOutputPort (JuceMidiDeviceManager mm, MidiInputPort actualPort, MidiPortPath portPathToUse)
        {
            owner = mm;
            androidPort = actualPort;
            portPath = portPathToUse;
        }

        @Override
        protected void finalize () throws Throwable
        {
            close ();
            super.finalize ();
        }

        @Override
        public boolean isInputPort ()
        {
            return false;
        }

        @Override
        public void start ()
        {
        }

        @Override
        public void stop ()
        {
        }

        @Override
        public void sendMidi (byte[] msg, int offset, int count)
        {
            if (androidPort != null)
            {
                try
                {
                    androidPort.send (msg, offset, count);
                } catch (IOException exception)
                {
                    Log.d ("JUCE", "send midi had IO exception");
                }
            }
        }

        @Override
        public void close ()
        {
            if (androidPort != null)
            {
                try
                {
                    androidPort.close ();
                } catch (IOException exception)
                {
                    Log.d ("JUCE", "IO Exception while closing port");
                }
            }

            if (owner != null)
                owner.removePort (portPath);

            owner = null;
            androidPort = null;
        }

        @Override
        public String getName ()
        {
            return owner.getPortName (portPath);
        }

        JuceMidiDeviceManager owner;
        MidiInputPort androidPort;
        MidiPortPath portPath;
    }

    private static class MidiPortPath extends Object
    {
        public MidiPortPath (int deviceIdToUse, boolean direction, int androidIndex)
        {
            deviceId = deviceIdToUse;
            isInput = direction;
            portIndex = androidIndex;
        }

        public int deviceId;
        public int portIndex;
        public boolean isInput;

        @Override
        public int hashCode ()
        {
            Integer i = new Integer ((deviceId * 128) + (portIndex < 128 ? portIndex : 127));
            return i.hashCode () * (isInput ? -1 : 1);
        }

        @Override
        public boolean equals (Object obj)
        {
            if (obj == null)
                return false;

            if (getClass () != obj.getClass ())
                return false;

            MidiPortPath other = (MidiPortPath) obj;
            return (portIndex == other.portIndex && isInput == other.isInput && deviceId == other.deviceId);
        }
    }

    //==============================================================================
    public static class JuceMidiDeviceManager extends MidiManager.DeviceCallback implements MidiManager.OnDeviceOpenedListener, MidiDeviceManager
    {
        //==============================================================================
        private class MidiDeviceOpenTask extends java.util.TimerTask
        {
            public MidiDeviceOpenTask (JuceMidiDeviceManager deviceManager, MidiDevice device)
            {
                owner = deviceManager;
                midiDevice = device;
            }

            @Override
            public boolean cancel ()
            {
                synchronized (MidiDeviceOpenTask.class)
                {
                    owner = null;
                    boolean retval = super.cancel ();

                    if (midiDevice != null)
                    {
                        try
                        {
                            midiDevice.close ();
                        } catch (IOException e)
                        {
                        }

                        midiDevice = null;
                    }

                    return retval;
                }
            }

            public int getID ()
            {
                return midiDevice.getInfo ().getId ();
            }

            @Override
            public void run ()
            {
                synchronized (MidiDeviceOpenTask.class)
                {
                    if (owner != null && midiDevice != null)
                        owner.onDeviceOpenedDelayed (midiDevice);
                }
            }

            private JuceMidiDeviceManager owner;
            private MidiDevice midiDevice;
        }

        //==============================================================================
        public JuceMidiDeviceManager(Context contextToUse)
        {
            appContext = contextToUse;
            manager = (MidiManager) appContext.getSystemService (MIDI_SERVICE);

            if (manager == null)
            {
                Log.d ("JUCE", "MidiDeviceManager error: could not get MidiManager system service");
                return;
            }

            openPorts = new HashMap<MidiPortPath, WeakReference<JuceMidiPort>> ();
            midiDevices = new ArrayList<MidiDevice> ();
            openTasks = new HashMap<Integer, MidiDeviceOpenTask> ();

            MidiDeviceInfo[] foundDevices = manager.getDevices ();
            for (MidiDeviceInfo info : foundDevices)
                onDeviceAdded (info);

            manager.registerDeviceCallback (this, null);
        }

        @Override
        public void detach () throws Throwable
        {
            manager.unregisterDeviceCallback (this);

            synchronized (JuceMidiDeviceManager.class)
            {
                for (Integer deviceID : openTasks.keySet ())
                    openTasks.get (deviceID).cancel ();

                openTasks = null;
            }

            for (MidiPortPath key : openPorts.keySet ())
                openPorts.get (key).get ().close ();

            openPorts = null;

            for (MidiDevice device : midiDevices)
            {
                device.close ();
            }

            midiDevices.clear ();

        }

        protected void finalize () throws Throwable
        {
            detach();
            super.finalize ();
        }

        @Override
        public String[] getJuceAndroidMidiOutputDeviceNameAndIDs()
        {
            return getJuceAndroidMidiDeviceNameAndIDs (MidiDeviceInfo.PortInfo.TYPE_OUTPUT);
        }

        @Override
        public String[] getJuceAndroidMidiInputDeviceNameAndIDs()
        {
            return getJuceAndroidMidiDeviceNameAndIDs (MidiDeviceInfo.PortInfo.TYPE_INPUT);
        }

        private String[] getJuceAndroidMidiDeviceNameAndIDs (int portType)
        {
            // only update the list when JUCE asks for a new list
            synchronized (JuceMidiDeviceManager.class)
            {
                deviceInfos = getDeviceInfos ();
            }

            ArrayList<String> portNameAndIDs = new ArrayList<String> ();

            for (MidiPortPath portInfo  : getAllPorts (portType))
            {
                portNameAndIDs.add (getPortName (portInfo));
                portNameAndIDs.add (Integer.toString (portInfo.hashCode ()));
            }

            String[] names = new String[portNameAndIDs.size ()];
            return portNameAndIDs.toArray (names);
        }

        private JuceMidiPort openMidiPortWithID (int deviceID, long host, boolean isInput)
        {
            synchronized (JuceMidiDeviceManager.class)
            {
                int portTypeToFind = (isInput ? MidiDeviceInfo.PortInfo.TYPE_INPUT : MidiDeviceInfo.PortInfo.TYPE_OUTPUT);
                MidiPortPath portInfo = getPortPathForID (portTypeToFind, deviceID);

                if (portInfo != null)
                {
                    // ports must be opened exclusively!
                    if (openPorts.containsKey (portInfo))
                        return null;

                    MidiDevice device = getMidiDevicePairForId (portInfo.deviceId);

                    if (device != null)
                    {
                        if (device != null)
                        {
                            JuceMidiPort juceMidiPort = null;

                            if (isInput)
                            {
                                MidiOutputPort outputPort = device.openOutputPort (portInfo.portIndex);

                                if (outputPort != null)
                                    juceMidiPort = new JuceMidiInputPort (this, outputPort, portInfo, host);
                            } else
                            {
                                MidiInputPort inputPort = device.openInputPort (portInfo.portIndex);

                                if (inputPort != null)
                                    juceMidiPort = new JuceMidiOutputPort (this, inputPort, portInfo);
                            }

                            if (juceMidiPort != null)
                            {
                                openPorts.put (portInfo, new WeakReference<JuceMidiPort> (juceMidiPort));

                                return juceMidiPort;
                            }
                        }
                    }
                }
            }

            return null;
        }

        @Override
        public JuceMidiPort openMidiInputPortWithID(int deviceID, long host)
        {
            return openMidiPortWithID (deviceID, host, true);
        }

        @Override
        public JuceMidiPort openMidiOutputPortWithID(int deviceID)
        {
            return openMidiPortWithID (deviceID, 0, false);
        }

        public void removePort (MidiPortPath path)
        {
            openPorts.remove (path);
        }

        @Override
        public void onDeviceAdded (MidiDeviceInfo info)
        {
            manager.openDevice (info, this, null);
        }

        @Override
        public void onDeviceRemoved(MidiDeviceInfo info)
        {
            boolean deviceWasRemoved = false;

            synchronized (JuceMidiDeviceManager.class)
            {
                MidiDevice midiDevice = getMidiDevicePairForId (info.getId ());

                if (midiDevice != null)
                {
                    // close all ports that use this device
                    boolean removedPort = true;

                    while (removedPort == true)
                    {
                        removedPort = false;
                        for (MidiPortPath key : openPorts.keySet ())
                        {
                            if (key.deviceId == info.getId ())
                            {
                                openPorts.get (key).get ().close ();
                                removedPort = true;
                                break;
                            }
                        }
                    }

                    midiDevices.remove (midiDevice);
                    deviceWasRemoved = true;
                }
            }

            if (deviceWasRemoved) {
                midiDevicesChanged();
            }
        }

        @Override
        public void onDeviceStatusChanged (MidiDeviceStatus status)
        {
        }

        @Override
        public void onDeviceOpened (MidiDevice theDevice)
        {
            synchronized (JuceMidiDeviceManager.class)
            {
                MidiDeviceInfo info = theDevice.getInfo ();
                int deviceID = info.getId ();

                if (!openTasks.containsKey (deviceID))
                {
                    MidiDeviceOpenTask openTask = new MidiDeviceOpenTask (this, theDevice);
                    openTasks.put (deviceID, openTask);

                    new java.util.Timer ().schedule (openTask, 100);
                }
            }
        }

        public void onDeviceOpenedDelayed (MidiDevice theDevice)
        {
            boolean deviceWasAdded = false;

            synchronized (JuceMidiDeviceManager.class)
            {
                int deviceID = theDevice.getInfo ().getId ();

                if (openTasks.containsKey (deviceID))
                {
                    if (!midiDevices.contains (theDevice))
                    {
                        openTasks.remove (deviceID);
                        midiDevices.add (theDevice);
                        deviceWasAdded = true;
                    }
                } else
                {
                    // unpair was called in the mean time
                    MidiDeviceInfo info = theDevice.getInfo ();
                    try
                    {
                        theDevice.close ();
                    } catch (IOException e)
                    {
                    }
                }
            }

            if (deviceWasAdded) {
                midiDevicesChanged();
            }
        }

        @Override
        public void injectMidiDevice(MidiDevice theDevice)
        {
            synchronized (JuceMidiDeviceManager.class)
            {
                // Fake JUCE pairing process for already paired device
                int deviceID = theDevice.getInfo().getId();
                openTasks.put(deviceID, new MidiDeviceOpenTask(this, theDevice));
            }

            onDeviceOpenedDelayed(theDevice);
        }

        public String getPortName (MidiPortPath path)
        {
            int portTypeToFind = (path.isInput ? MidiDeviceInfo.PortInfo.TYPE_INPUT : MidiDeviceInfo.PortInfo.TYPE_OUTPUT);

            synchronized (JuceMidiDeviceManager.class)
            {
                for (MidiDeviceInfo info : deviceInfos)
                {
                    int localIndex = 0;
                    if (info.getId () == path.deviceId)
                    {
                        for (MidiDeviceInfo.PortInfo portInfo : info.getPorts ())
                        {
                            int portType = portInfo.getType ();
                            if (portType == portTypeToFind)
                            {
                                int portIndex = portInfo.getPortNumber ();
                                if (portIndex == path.portIndex)
                                {
                                    String portName = portInfo.getName ();
                                    if (portName.isEmpty ())
                                        portName = (String) info.getProperties ().get (info.PROPERTY_NAME);

                                    return portName;
                                }
                            }
                        }
                    }
                }
            }

            return "";
        }

        public ArrayList<MidiPortPath> getAllPorts (int portType)
        {
            ArrayList<MidiPortPath> ports = new ArrayList<MidiPortPath> ();

            for (MidiDeviceInfo info : deviceInfos)
                for (MidiDeviceInfo.PortInfo portInfo : info.getPorts ())
                    if (portInfo.getType () == portType)
                        ports.add (new MidiPortPath (info.getId (), (portType == MidiDeviceInfo.PortInfo.TYPE_INPUT),
                                                     portInfo.getPortNumber ()));

            return ports;
        }

        public MidiPortPath getPortPathForID (int portType, int deviceID)
        {
            for (MidiPortPath port : getAllPorts (portType))
                if (port.hashCode () == deviceID)
                    return port;

            return null;
        }

        private MidiDeviceInfo[] getDeviceInfos ()
        {
            synchronized (JuceMidiDeviceManager.class)
            {
                MidiDeviceInfo[] infos = new MidiDeviceInfo[midiDevices.size ()];

                int idx = 0;
                for (MidiDevice midiDevice : midiDevices)
                    infos[idx++] = midiDevice.getInfo ();

                return infos;
            }
        }

        private MidiDevice getMidiDevicePairForId (int deviceId)
        {
            synchronized (JuceMidiDeviceManager.class)
            {
                for (MidiDevice midiDevice : midiDevices)
                    if (midiDevice.getInfo ().getId () == deviceId)
                        return midiDevice;
            }

            return null;
        }

        private MidiManager manager;
        private HashMap<Integer, MidiDeviceOpenTask> openTasks;
        private ArrayList<MidiDevice> midiDevices;
        private MidiDeviceInfo[] deviceInfos;
        private HashMap<MidiPortPath, WeakReference<JuceMidiPort>> openPorts;
        private Context appContext = null;
    }

    public static MidiDeviceManager getAndroidMidiDeviceManager (Context context)
    {
        synchronized (JuceMidiSupport.class)
        {
            if (midiDeviceManager == null) {
                MidiSupport midiSupport = MidiSupport.getInstance();
                if (midiSupport.isAndroidMidiSupported()) {
                    if (midiSupport.hasFallbackMidiDriver() && midiSupport.isMidiFallbackDriverEnabled())
                    {
                        midiDeviceManager = new MidiUsbToJuce.MidiDeviceManager(context);
                    }
                    else
                    {
                        midiDeviceManager = new JuceMidiDeviceManager(context);
                    }
                }
                else if (midiSupport.isMidiSupported())
                {
                    // If we're here, only fallback driver is supported
                    midiDeviceManager = new MidiUsbToJuce.MidiDeviceManager(context);
                }

            }
        }

        return midiDeviceManager;
    }

    public static void resetAndroidMidiDeviceManager(Context context)
    {
        synchronized (JuceMidiSupport.class)
        {
            if (midiDeviceManager != null)
            {
                try
                {
                    midiDeviceManager.detach();
                }
                catch (Throwable tr)
                {
                    Log.e("JuceMidiSupport", "Detaching midiDeviceManager", tr);
                }
                midiDeviceManager = null;
            }
        }

        getAndroidMidiDeviceManager(context);
    }

    private static MidiDeviceManager midiDeviceManager = null;
    public static native void midiDevicesChanged();
}
