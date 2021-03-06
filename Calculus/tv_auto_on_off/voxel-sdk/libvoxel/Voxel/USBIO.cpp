/*
 * TI Voxel Lib component.
 *
 * Copyright (c) 2014 Texas Instruments Inc.
 */

#include "USBIO.h"
#include "Logger.h"
#include "USBSystem.h"

#ifdef LINUX
#include "USBSystemPrivateLinux.h"
#elif defined(WINDOWS)
#include "USBSystemPrivateWindows.h"
#include <Windows.h>
#define __USB200_H__ // Disable redefinition of _USB_* structs in CyAPI.h
#include "CyAPI.h"
#endif


namespace Voxel
{
  
class USBIO::USBIOPrivate
{
public:
#ifdef LINUX
typedef libusb_device_handle *USBHandle;
typedef libusb_device *USBDeviceHandle;
#elif defined(WINDOWS)
typedef HANDLE USBDeviceHandle;
typedef CCyUSBDevice *USBHandle;
#endif
  USBHandle handle;
  USBDeviceHandle deviceHandle;
  
  USBSystem sys;
  
  DevicePtr device;
  
  bool _initialized;

  long lastTransferSize = 0;
  
  USBIOPrivate(DevicePtr device);
  inline bool isInitialized() { return _initialized && handle != 0; }
  
  bool controlTransfer(Direction direction, RequestType requestType, RecipientType recipientType, uint8_t request, uint16_t value, uint16_t index, 
                       uint8_t *data = 0, uint16_t length = 0, long timeout = 1000);
  
  bool bulkTransfer(uint8_t endpoint, uint8_t *data, long toTransferLength, long &transferredLength, long timeout = 1000);
  
  bool resetBulkEndPoint(uint8_t endpoint);
  
  ~USBIOPrivate();
};

USBIO::USBIOPrivate::USBIOPrivate(DevicePtr device): device(device), handle(0), _initialized(false)
#ifdef LINUX
  , deviceHandle(0)
#elif defined(WINDOWS)
  , deviceHandle(INVALID_HANDLE_VALUE)
#endif
{
  if(device->interfaceID() != Device::USB)
  {
    logger(LOG_ERROR) << "USBIO: cannot download to a non-USB device" << std::endl;
    return;
  }
  
  USBDevice &d = (USBDevice &)*device;
  
  if(!sys.isInitialized())
  {
    logger(LOG_ERROR) << "USBIO: USBSystem init failed." << std::endl;
    return;
  }
  
#ifdef LINUX
  int rc;
  
  deviceHandle = sys.getUSBSystemPrivate().getDeviceHandle(d);
  
  if(deviceHandle)
  {
    if((rc = libusb_open(deviceHandle, &handle)) == LIBUSB_SUCCESS)
    {
      int channelID = device->channelID();
      
      if(channelID < 0) channelID = 0;
      
      if((rc = libusb_claim_interface(handle, channelID)) != LIBUSB_SUCCESS)
      {
        logger(LOG_ERROR) << "USBIO: " << libusb_strerror((libusb_error)rc) << std::endl;
        return;
      }
      
      _initialized = true;
    }
    else
    {
      logger(LOG_ERROR) << "USBIO: " << libusb_strerror((libusb_error)rc) << std::endl;
      return;
    }
  }
  else
  {
    logger(LOG_ERROR) << "USBIO: Failed to get device handle. Check that device is connected and is accessible from current user." << std::endl;
    return;
  }
#elif defined(WINDOWS)
  String devicePath = sys.getDeviceNode(d);
  
  if (!devicePath.size())
  {
    logger(LOG_ERROR) << "USBIO: Could not get device path for '" << d.id() << "'" << std::endl;
    return;
  }
  
  deviceHandle = CreateFile(devicePath.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
  
  if (deviceHandle == INVALID_HANDLE_VALUE)
  {
    logger(LOG_ERROR) << "USBIO: Failed to open device path '" << devicePath << "' for device '" << d.id() << "'" << std::endl;
    return;
  }
  
  handle = new CCyUSBDevice(deviceHandle);
  
  if(handle->IsOpen())
  {
    _initialized = true;
  }
#endif
}

USBIO::USBIOPrivate::~USBIOPrivate()
{
#ifdef LINUX
  if(_initialized)
  {
    libusb_release_interface(handle, device->channelID());
    _initialized = false;
  }
  
  if(handle)
    libusb_close(handle);
    
  if(deviceHandle)
    libusb_unref_device(deviceHandle);
  
  
#elif defined(WINDOWS)
  if (handle)
    delete handle;
  
  if(deviceHandle != INVALID_HANDLE_VALUE)
    CloseHandle(deviceHandle);
#endif
}

bool USBIO::USBIOPrivate::controlTransfer(Direction direction, RequestType requestType, RecipientType recipientType, uint8_t request, uint16_t value, uint16_t index, uint8_t *data, uint16_t length, long timeout)
{
#ifdef LINUX
  int status;
  status = libusb_control_transfer(handle,
                                   (direction << 7) | (requestType << 5) | recipientType,
                                   request,
                                   value,
                                   index,
                                   data, length, timeout);
  if (status != length)
  {
    logger(LOG_ERROR) << "USBIO: Control transfer issue: Status " << status << std::endl;
    return false;
  }
  
  return true;
#elif defined(WINDOWS)
  CCyControlEndPoint *ept = handle->ControlEndPt;
  
  ept->Target = (CTL_XFER_TGT_TYPE)recipientType;
  ept->ReqType = (CTL_XFER_REQ_TYPE)requestType;
  ept->Direction = (CTL_XFER_DIR_TYPE)direction;
  ept->ReqCode = request;
  ept->Value = value;
  ept->TimeOut = timeout;
  ept->Index = index;
  long l = length;
  if (!ept->XferData(data, l, NULL))
  {
    logger(LOG_ERROR) << "USBIO: Control transfer issue." << std::endl;
    return false;
  }
  
  return true;
#endif
}

bool USBIO::USBIOPrivate::bulkTransfer(uint8_t endpoint, uint8_t *data, long toTransferLength, long &transferredLength, long timeout)
{
#ifdef LINUX
  int transferred;
  int rc = libusb_bulk_transfer(handle, endpoint, data, toTransferLength, &transferred, timeout);
  
  if(rc != 0 && rc != LIBUSB_ERROR_TIMEOUT)
  {
    logger(LOG_ERROR) << "USBIO: Bulk transfer failed." << std::endl;
    return false;
  }
  
  transferredLength = transferred;
  lastTransferSize = toTransferLength;
  
  return true;
#elif defined(WINDOWS)
  
  CCyBulkEndPoint *dataEP;
  
  if(endpoint & 0x80)
  {
    dataEP = handle->BulkInEndPt;

    if (lastTransferSize != toTransferLength)
      dataEP->SetXferSize(nearestPowerOf2(toTransferLength));
  }
  else
    dataEP = handle->BulkOutEndPt;
  
  dataEP->TimeOut = timeout;
  transferredLength = toTransferLength;
  lastTransferSize = toTransferLength;
  
  if (!dataEP->XferData(data, transferredLength, NULL))
  {
    logger(LOG_ERROR) << "USBIO: Could not transfer '" << transferredLength << "' bytes" << std::endl;
    return false;
  }
  return true;
#endif
}

bool USBIO::USBIOPrivate::resetBulkEndPoint(uint8_t endpoint)
{
#ifdef LINUX
  return libusb_clear_halt(handle, endpoint) == 0;
#elif defined(WINDOWS)
  CCyBulkEndPoint *dataEP;
  
  if(endpoint & 0x80)
    dataEP = handle->BulkInEndPt;
  else
    dataEP = handle->BulkOutEndPt;
  return dataEP->Reset();
#endif
}


bool USBIO::isInitialized()
{
  return _usbIOPrivate->isInitialized();
}


USBIO::USBIO(DevicePtr device): _usbIOPrivate(new USBIOPrivate(device))
{
}

bool USBIO::controlTransfer(Direction direction, RequestType requestType, RecipientType recipientType, uint8_t request, uint16_t value, uint16_t index, uint8_t *data, uint16_t length, long timeout)
{
  if(!_usbIOPrivate->isInitialized())
  {
    logger(LOG_ERROR) << "USBIO: Not initialized." << std::endl;
    return false;
  }
  
  return _usbIOPrivate->controlTransfer(direction, requestType, recipientType, request, value, index, data, length, timeout);
}

bool USBIO::bulkTransfer(uint8_t endpoint, uint8_t *data, long toTransferLength, long &transferredLength, long timeout)
{
  if(!_usbIOPrivate->isInitialized())
  {
    logger(LOG_ERROR) << "USBIO: Not initialized." << std::endl;
    return false;
  }
  
  return _usbIOPrivate->bulkTransfer(endpoint, data, toTransferLength, transferredLength, timeout);
}

bool USBIO::resetBulkEndPoint(uint8_t endpoint)
{
  if(!_usbIOPrivate->isInitialized())
  {
    logger(LOG_ERROR) << "USBIO: Not initialized." << std::endl;
    return false;
  }
  
  return _usbIOPrivate->resetBulkEndPoint(endpoint);
}

USBSystem &USBIO::getUSBSystem()
{
  return _usbIOPrivate->sys;
}


  
}