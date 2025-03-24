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

enum
{
    U_ISOFS_SUPER_MAGIC = 0x9660,   // linux/iso_fs.h
    U_MSDOS_SUPER_MAGIC = 0x4d44,   // linux/msdos_fs.h
    U_NFS_SUPER_MAGIC = 0x6969,     // linux/nfs_fs.h
    U_SMB_SUPER_MAGIC = 0x517B      // linux/smb_fs.h
};

bool File::isOnCDRomDrive() const
{
    return false;
}

bool File::isOnHardDisk() const
{
    // This is assuming we run WASM in browser.
    return false;
}

bool File::isOnRemovableDrive() const
{
    return false;
}

String File::getVersion() const
{
    return {};
}

File File::getSpecialLocation (const SpecialLocationType type)
{
    // None of these actually exist in browser
    jassertfalse;
    switch (type)
    {
        case userHomeDirectory:
        case userDocumentsDirectory:
        case userMusicDirectory:
        case userMoviesDirectory:
        case userPicturesDirectory:
        case userDesktopDirectory:
        case userApplicationDataDirectory:
        case commonDocumentsDirectory:
            return File ("/");

        case tempDirectory:
            return File ("/tmp");

        case invokedExecutableFile:
        case currentExecutableFile:
        case currentApplicationFile:
        case hostApplicationPath:
        {
            return {};
        }

        default:
            jassertfalse; // unknown type?
            break;
    }

    return {};
}

//==============================================================================
bool File::moveToTrash() const
{
    if (! exists())
        return true;

    if (isDirectory())
        return deleteRecursively();
    else
        return deleteFile();
}

//==============================================================================
static bool isFileExecutable (const String& filename)
{
    return false;
}
} // namespace juce
