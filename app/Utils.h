#include <atlsafe.h>
#include <iostream>
#include <windows.h>
#include <vector>
#include <fstream>
#include <atlimage.h>


namespace MyUtils {
    void TakeScreenShot(const std::string& path)
    {   
        keybd_event(VK_SNAPSHOT, 0x45, KEYEVENTF_EXTENDEDKEY, 0);
        keybd_event(VK_SNAPSHOT, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

        HBITMAP hBitmap;

        OpenClipboard(NULL);
        hBitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
        CloseClipboard();

        std::vector<BYTE> buf;
        IStream* stream = NULL;
        HRESULT hr = CreateStreamOnHGlobal(0, TRUE, &stream);
        CImage image;
        ULARGE_INTEGER liSize;
        image.Attach(hBitmap);
        image.Save(stream, Gdiplus::ImageFormatJPEG);
        IStream_Size(stream, &liSize);
        DWORD len = liSize.LowPart;
        IStream_Reset(stream);
        buf.resize(len);
        IStream_Read(stream, &buf[0], len);
        stream->Release();

        std::fstream fi;
        fi.open(path, std::fstream::binary | std::fstream::out);
        fi.write(reinterpret_cast<const char*>(&buf[0]), buf.size() * sizeof(BYTE));
        fi.close();
    }
}
