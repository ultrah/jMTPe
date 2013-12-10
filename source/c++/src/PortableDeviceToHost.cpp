/*
 * Copyright 2013 Philipp Ebert
 * 
 * This file is part of JMTP.
 * 
 * JTMP is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation, either version 3 of 
 * the License, or any later version.
 * 
 * JMTP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU LesserGeneral Public 
 * License along with JMTP. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <atlbase.h>
#include <atlstr.h>

#include <iostream>


#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include <PortableDeviceApi.h>

#include "jmtp.h"
#include "jmtp_PortableDeviceToHostImpl32.h"

using namespace std;

static inline IPortableDevice* GetPortableDevice(JNIEnv* env, jobject obj)
{
	return (IPortableDevice*)GetComReferencePointer(env, obj, "pDevice");
}

// Copies data from a source stream to a destination stream using the
// specified cbTransferSize as the temporary buffer size.
HRESULT StreamCopy(IStream *pDestStream, IStream *pSourceStream, DWORD cbTransferSize, DWORD *pcbWritten)
{
    HRESULT hr = S_OK;

    // Allocate a temporary buffer (of Optimal transfer size) for the read results to
    // be written to.
    BYTE*   pObjectData = new (std::nothrow) BYTE[cbTransferSize];
    if (pObjectData != NULL)
    {
        DWORD cbTotalBytesRead    = 0;
        DWORD cbTotalBytesWritten = 0;

        DWORD cbBytesRead    = 0;
        DWORD cbBytesWritten = 0;

        // Read until the number of bytes returned from the source stream is 0, or
        // an error occured during transfer.
        do
        {
            // Read object data from the source stream
            hr = pSourceStream->Read(pObjectData, cbTransferSize, &cbBytesRead);
            if (FAILED(hr))
            {
                printf("! Failed to read %d bytes from the source stream, hr = 0x%lx\n",cbTransferSize, hr);
            }

            // Write object data to the destination stream
            if (SUCCEEDED(hr))
            {
                cbTotalBytesRead += cbBytesRead; // Calculating total bytes read from device for debugging purposes only

                hr = pDestStream->Write(pObjectData, cbBytesRead, &cbBytesWritten);
                if (FAILED(hr))
                {
                    printf("! Failed to write %d bytes of object data to the destination stream, hr = 0x%lx\n",cbBytesRead, hr);
                }

                if (SUCCEEDED(hr))
                {
                    cbTotalBytesWritten += cbBytesWritten; // Calculating total bytes written to the file for debugging purposes only
                }
            }

            // Output Read/Write operation information only if we have received data and if no error has occured so far.
            if (SUCCEEDED(hr) && (cbBytesRead > 0))
            {
                //printf("Read %d bytes from the source stream...Wrote %d bytes to the destination stream...\n", cbBytesRead, cbBytesWritten);
            }

        } while (SUCCEEDED(hr) && (cbBytesRead > 0));

        // If the caller supplied a pcbWritten parameter and we
        // and we are successful, set it to cbTotalBytesWritten
        // before exiting.
        if ((SUCCEEDED(hr)) && (pcbWritten != NULL))
        {
            *pcbWritten = cbTotalBytesWritten;
        }

        // Remember to delete the temporary transfer buffer
        delete [] pObjectData;
        pObjectData = NULL;
    }
    else
    {
        printf("! Failed to allocate %d bytes for the temporary transfer buffer.\n", cbTransferSize);
    }

    return hr;
}

// Reads a string property from the IPortableDeviceProperties
// interface and returns it in the form of a CAtlStringW
HRESULT GetStringValue(IPortableDeviceProperties* pProperties, PCWSTR pszObjectID, REFPROPERTYKEY key, CAtlStringW &strStringValue)
{
    CComPtr<IPortableDeviceValues>        pObjectProperties;
    CComPtr<IPortableDeviceKeyCollection> pPropertiesToRead;

    // 1) CoCreate an IPortableDeviceKeyCollection interface to hold the the property key we wish to read.
    HRESULT hr = CoCreateInstance(CLSID_PortableDeviceKeyCollection, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pPropertiesToRead));

    // 2) Populate the IPortableDeviceKeyCollection with the keys we wish to read.
    // NOTE: We are not handling any special error cases here so we can proceed with
    // adding as many of the target properties as we can.
    if (SUCCEEDED(hr))
    {
        if (pPropertiesToRead != NULL)
        {
            HRESULT hrTemp = S_OK;
            hrTemp = pPropertiesToRead->Add(key);

            if (FAILED(hrTemp))
                printf("! Failed to add PROPERTYKEY to IPortableDeviceKeyCollection, hr= 0x%lx\n", hrTemp);
        }
    }

    // 3) Call GetValues() passing the collection of specified PROPERTYKEYs.
    if (SUCCEEDED(hr))
        hr = pProperties->GetValues(pszObjectID, pPropertiesToRead, &pObjectProperties);

    // 4) Extract the string value from the returned property collection
    if (SUCCEEDED(hr))
    {
        PWSTR pszStringValue = NULL;
        hr = pObjectProperties->GetStringValue(key, &pszStringValue);

        if (SUCCEEDED(hr))
            strStringValue = pszStringValue; // assign the newly read string to the CAtlStringW out parameter
        else
            printf("! Failed to find property in IPortableDeviceValues, hr = 0x%lx\n",hr);

        CoTaskMemFree(pszStringValue);
        pszStringValue = NULL;
    }

    return hr;
}

std::wstring JavaToWSZ(JNIEnv* env, jstring string)
{
    std::wstring value;

    const jchar *raw = env->GetStringChars(string, 0);
    jsize len = env->GetStringLength(string);
    const jchar *temp = raw;

    value.assign(raw, raw + len);

    env->ReleaseStringChars(string, raw);

    return value;
}




// JNIEXPORT void JNICALL CopyFromPortableDeviceToHost(const WCHAR *	, IPortableDevice *pDevice)
JNIEXPORT void JNICALL Java_jmtp_PortableDeviceToHostImpl32_copyFromPortableDeviceToHost(JNIEnv * env, jobject jobj, jstring jstr, jstring jpath, jobject deviceObj)
{

 IPortableDevice* pDevice = GetPortableDevice(env, deviceObj);

  if (pDevice == NULL)
  {
    printf("! A NULL IPortableDevice interface pointer was received\n");
    //return -1;
  }

  wstring szSelection = JavaToWSZ(env, jstr);

  wstring dir = JavaToWSZ(env, jpath);

  //printf("%ls\n\n",szSelection.c_str());
  //wcout << szSelection << endl << endl;

  HRESULT hr = S_OK;
  CComPtr<IPortableDeviceContent> pContent;
  CComPtr<IPortableDeviceResources> pResources;
  CComPtr<IPortableDeviceProperties> pProperties;
  CComPtr<IStream> pObjectDataStream;
  CComPtr<IStream> pFinalFileStream;
  DWORD cbOptimalTransferSize = 0;
  CAtlStringW strOriginalFileName;

 
  if (SUCCEEDED(hr))
  {
    hr = pDevice->Content(&pContent);
    if (FAILED(hr))
      printf("! Failed to get IPortableDeviceContent from IPortableDevice, hr = 0x%lx\n",hr);
  }

  if (SUCCEEDED(hr))
  {
    hr = pContent->Transfer(&pResources);
    if (FAILED(hr))
      printf("! Failed to get IPortableDeviceResources from IPortableDeviceContent, hr = 0x%lx\n",hr);
  }

  
  if (SUCCEEDED(hr))
  {
    hr = pContent->Properties(&pProperties);
    if (SUCCEEDED(hr))
    {
      hr = GetStringValue(pProperties, szSelection.c_str(), WPD_OBJECT_ORIGINAL_FILE_NAME, strOriginalFileName);
      if (FAILED(hr))
      {
		  printf("! Failed to read WPD_OBJECT_ORIGINAL_FILE_NAME on object '%ws', hr = 0x%lx\n", szSelection.c_str(), hr);
        strOriginalFileName.Format(L"%ws.data", szSelection.c_str());
        printf("* Creating a filename '%ws' as a default.\n", (PWSTR)strOriginalFileName.GetString());
        // set the HRESULT to S_OK, so we can continue
        // with our newly generated temporary file name
        hr = S_OK;
      }
    }
    else
    {
      printf("! Failed to get IPortableDeviceProperties from IPortableDeviceContent, hr = 0x%lx\n", hr);
    }
  }

  if (SUCCEEDED(hr))
  {
    hr = pResources->GetStream(szSelection.c_str(), WPD_RESOURCE_DEFAULT, STGM_READ, &cbOptimalTransferSize, &pObjectDataStream);
    if (FAILED(hr))
      printf("! Failed to get IStream (representing object data on the device) from IPortableDeviceResources, hr = 0x%lx\n",hr);
  }

  if (SUCCEEDED(hr))
  {
    std::wstring combined = dir;
    if (!combined.empty() && combined.back() != '\\')
        combined += '\\';
    combined += strOriginalFileName;

    hr = SHCreateStreamOnFile(combined.c_str(), STGM_CREATE|STGM_WRITE, &pFinalFileStream);
    if (FAILED(hr))
      printf("! Failed to create a temporary file named (%ws) to transfer object (%ws), hr = 0x%lx\n",(PWSTR)strOriginalFileName.GetString(), szSelection.c_str(), hr);
  }

  if (SUCCEEDED(hr))
  {
    DWORD cbTotalBytesWritten = 0;
    hr = StreamCopy(pFinalFileStream, pObjectDataStream, cbOptimalTransferSize, &cbTotalBytesWritten);

    if (FAILED(hr))
      printf("! Failed to transfer object from device, hr = 0x%lx\n",hr);
    //else
      //printf("* Transferred object '%ws' to '%ws'.\n", szSelection.c_str(), (PWSTR)strOriginalFileName.GetString());
  }

  fflush(stdout);
  //return hr;
}
